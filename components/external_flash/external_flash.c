/**
 * @file external_flash.c
 * @brief W25Q64 External SPI Flash Driver Implementation
 *
 * Driver for Winbond W25Q64 8MB SPI flash memory chip.
 * Uses HSPI bus (SPI2_HOST) with GPIO pins 12, 13, 14, 15.
 *
 * @note GPIO pins verified for ESP32-PICO-KIT V4.1
 * @note No conflicts with existing I2C buses (GPIO 18/19, 25/26)
 */

#include "external_flash.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "spi_flash_mmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "external_flash";

// ========================================================================
// W25Q64 COMMAND CODES
// ========================================================================

#define CMD_WRITE_ENABLE        0x06    /**< Enable write operations */
#define CMD_READ_STATUS_REG1    0x05    /**< Read status register 1 */
#define CMD_READ_STATUS_REG2    0x35    /**< Read status register 2 */
#define CMD_WRITE_STATUS_REG    0x01    /**< Write status register */
#define CMD_PAGE_PROGRAM        0x02    /**< Page program (write) */
#define CMD_QUAD_PAGE_PROGRAM   0x32    /**< Quad page program */
#define CMD_SECTOR_ERASE        0x20    /**< Sector erase (4KB) */
#define CMD_BLOCK_ERASE_32K     0x52    /**< Block erase (32KB) */
#define CMD_BLOCK_ERASE_64K     0xD8    /**< Block erase (64KB) */
#define CMD_CHIP_ERASE          0xC7    /**< Chip erase (full chip) */
#define CMD_ERASE_SUSPEND       0x75    /**< Suspend erase operation */
#define CMD_ERASE_RESUME        0x7A    /**< Resume erase operation */
#define CMD_POWER_DOWN          0xB9    /**< Enter power-down mode */
#define CMD_RELEASE_POWER_DOWN  0xAB    /**< Release power-down */
#define CMD_READ_DATA           0x03    /**< Read data bytes */
#define CMD_FAST_READ           0x0B    /**< Fast read data bytes */
#define CMD_READ_JEDEC_ID       0x9F    /**< Read JEDEC ID */
#define CMD_READ_UNIQUE_ID      0x4B    /**< Read unique ID */

// Status register bits
#define STATUS_BUSY_BIT         0x01    /**< Bit 0: BUSY */
#define STATUS_WEL_BIT          0x02    /**< Bit 1: Write Enable Latch */

// ========================================================================
// GPIO PIN DEFINITIONS (ESP32-PICO-KIT V4.1)
// ========================================================================

#define PIN_NUM_MISO            12      /**< SPI MISO (Master In, Slave Out) */
#define PIN_NUM_MOSI            13      /**< SPI MOSI (Master Out, Slave In) */
#define PIN_NUM_CLK             14      /**< SPI Clock */
#define PIN_NUM_CS              15      /**< SPI Chip Select (has internal pull-up) */

// ========================================================================
// SPI CONFIGURATION
// ========================================================================

#define SPI_CLOCK_SPEED_HZ      (10 * 1000 * 1000)  /**< 10 MHz (conservative) */
#define SPI_MAX_TRANSFER_SIZE   4096                /**< Maximum SPI transfer size */

// ========================================================================
// PRIVATE STATE
// ========================================================================

static spi_device_handle_t spi_handle = NULL;      /**< SPI device handle */
static esp_flash_t *esp_flash_handle = NULL;        /**< ESP-IDF flash handle for partition API */
static bool initialized = false;                    /**< Initialization flag */

// Statistics tracking
static uint32_t stats_reads = 0;
static uint32_t stats_writes = 0;
static uint32_t stats_erases = 0;

// ========================================================================
// PRIVATE FUNCTION PROTOTYPES
// ========================================================================

static esp_err_t send_write_enable(void);
static esp_err_t wait_ready_internal(uint32_t timeout_ms);
static esp_err_t read_status_internal(uint8_t *status);

// ========================================================================
// INITIALIZATION
// ========================================================================

esp_err_t external_flash_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "External flash already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing W25Q64 external flash...");

    // Configure SPI bus
    spi_bus_config_t bus_cfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,        // Not using quad mode
        .quadhd_io_num = -1,        // Not using quad mode
        .max_transfer_sz = SPI_MAX_TRANSFER_SIZE
    };

    // Initialize SPI bus (SPI2_HOST = HSPI)
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus already initialized (not an error)");
    }

    // Configure SPI device (W25Q64 chip)
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_CLOCK_SPEED_HZ,
        .mode = 0,                  // SPI mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,            // Transaction queue size
        .flags = SPI_DEVICE_HALFDUPLEX,  // Allow different TX/RX lengths
        .pre_cb = NULL,
        .post_cb = NULL
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for chip to be ready
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read and verify JEDEC ID
    uint8_t manufacturer, mem_type, capacity;
    ret = external_flash_read_jedec_id(&manufacturer, &mem_type, &capacity);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read JEDEC ID: %s", esp_err_to_name(ret));
        spi_bus_remove_device(spi_handle);
        spi_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "JEDEC ID: Manufacturer=0x%02X, MemType=0x%02X, Capacity=0x%02X",
             manufacturer, mem_type, capacity);

    // Verify chip is W25Q64
    if (manufacturer != W25Q64_JEDEC_MFG ||
        mem_type != W25Q64_JEDEC_MEM_TYPE ||
        capacity != W25Q64_JEDEC_CAPACITY) {
        ESP_LOGE(TAG, "Unexpected flash chip! Expected W25Q64 (0xEF 0x40 0x17)");
        spi_bus_remove_device(spi_handle);
        spi_handle = NULL;
        return ESP_FAIL;
    }

    initialized = true;
    ESP_LOGI(TAG, "✅ W25Q64 initialized successfully (8 MB)");
    ESP_LOGI(TAG, "GPIO: MISO=%d, MOSI=%d, CLK=%d, CS=%d",
             PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    return ESP_OK;
}

// ========================================================================
// READ OPERATIONS
// ========================================================================

esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (buffer == NULL) {
        ESP_LOGE(TAG, "NULL buffer provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (address + size > EXTERNAL_FLASH_SIZE) {
        ESP_LOGE(TAG, "Read beyond flash size (addr=0x%06lX, size=%u)", address, size);
        return ESP_ERR_INVALID_ARG;
    }

    if (size == 0) {
        return ESP_OK;  // Nothing to read
    }

    // Wait for flash to be ready
    esp_err_t ret = wait_ready_internal(1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flash busy timeout before read");
        return ret;
    }

    // Prepare command: CMD + 24-bit address
    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    spi_transaction_t trans = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = 0,
        .rxlength = size * 8,  // In bits
        .tx_buffer = cmd,
        .rx_buffer = buffer
    };

    // Send command + address, then receive data
    trans.length = sizeof(cmd) * 8;  // Send 4 bytes (cmd + addr)

    ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read transaction failed: %s", esp_err_to_name(ret));
        return ret;
    }

    stats_reads++;
    return ESP_OK;
}

// ========================================================================
// WRITE OPERATIONS
// ========================================================================

esp_err_t external_flash_write(uint32_t address, const void *data, size_t size)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL) {
        ESP_LOGE(TAG, "NULL data provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (address + size > EXTERNAL_FLASH_SIZE) {
        ESP_LOGE(TAG, "Write beyond flash size (addr=0x%06lX, size=%u)", address, size);
        return ESP_ERR_INVALID_ARG;
    }

    if (size == 0) {
        return ESP_OK;  // Nothing to write
    }

    const uint8_t *data_ptr = (const uint8_t *)data;
    size_t remaining = size;
    uint32_t current_address = address;

    while (remaining > 0) {
        // Calculate bytes to write in this page
        uint32_t page_offset = current_address & (EXTERNAL_FLASH_PAGE_SIZE - 1);
        size_t bytes_this_page = EXTERNAL_FLASH_PAGE_SIZE - page_offset;
        if (bytes_this_page > remaining) {
            bytes_this_page = remaining;
        }

        // Wait for flash to be ready
        esp_err_t ret = wait_ready_internal(1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Flash busy timeout before write");
            return ret;
        }

        // Send write enable command
        ret = send_write_enable();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Write enable failed");
            return ret;
        }

        // Prepare page program command: CMD + 24-bit address + data
        uint8_t cmd[4] = {
            CMD_PAGE_PROGRAM,
            (current_address >> 16) & 0xFF,
            (current_address >> 8) & 0xFF,
            current_address & 0xFF
        };

        // Create transaction with command + data
        spi_transaction_t trans = {
            .flags = 0,
            .length = (4 + bytes_this_page) * 8,  // In bits
            .tx_buffer = NULL,  // We'll use tx_data for small transfers
            .rx_buffer = NULL
        };

        // Allocate buffer for command + data
        uint8_t *tx_buffer = heap_caps_malloc(4 + bytes_this_page, MALLOC_CAP_DMA);
        if (tx_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer");
            return ESP_ERR_NO_MEM;
        }

        memcpy(tx_buffer, cmd, 4);
        memcpy(tx_buffer + 4, data_ptr, bytes_this_page);
        trans.tx_buffer = tx_buffer;

        ret = spi_device_transmit(spi_handle, &trans);
        free(tx_buffer);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI write transaction failed: %s", esp_err_to_name(ret));
            return ret;
        }

        // Wait for write to complete (max 3ms per page)
        ret = wait_ready_internal(10);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Flash busy timeout after write");
            return ret;
        }

        // Update pointers
        data_ptr += bytes_this_page;
        current_address += bytes_this_page;
        remaining -= bytes_this_page;
    }

    stats_writes++;
    return ESP_OK;
}

// ========================================================================
// ERASE OPERATIONS
// ========================================================================

esp_err_t external_flash_erase_sector(uint32_t address)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (address >= EXTERNAL_FLASH_SIZE) {
        ESP_LOGE(TAG, "Erase address beyond flash size (addr=0x%06lX)", address);
        return ESP_ERR_INVALID_ARG;
    }

    // Align to sector boundary
    address &= ~(EXTERNAL_FLASH_SECTOR_SIZE - 1);

    ESP_LOGD(TAG, "Erasing sector at 0x%06lX", address);

    // Wait for flash to be ready
    esp_err_t ret = wait_ready_internal(1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flash busy timeout before erase");
        return ret;
    }

    // Send write enable command
    ret = send_write_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write enable failed");
        return ret;
    }

    // Prepare sector erase command: CMD + 24-bit address
    uint8_t cmd[4] = {
        CMD_SECTOR_ERASE,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    spi_transaction_t trans = {
        .flags = 0,
        .length = sizeof(cmd) * 8,  // In bits
        .tx_buffer = cmd,
        .rx_buffer = NULL
    };

    ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI erase transaction failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for erase to complete (typical: 45-400ms, max: 1000ms)
    ret = wait_ready_internal(1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flash busy timeout after erase (sector may be bad)");
        return ret;
    }

    stats_erases++;
    ESP_LOGD(TAG, "Sector erased successfully");
    return ESP_OK;
}

esp_err_t external_flash_erase_range(uint32_t start_address, size_t size)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Align start to sector boundary
    start_address &= ~(EXTERNAL_FLASH_SECTOR_SIZE - 1);

    // Calculate number of sectors
    uint32_t sectors = (size + EXTERNAL_FLASH_SECTOR_SIZE - 1) / EXTERNAL_FLASH_SECTOR_SIZE;

    ESP_LOGI(TAG, "Erasing %lu sectors starting at 0x%06lX", sectors, start_address);

    for (uint32_t i = 0; i < sectors; i++) {
        uint32_t sector_addr = start_address + (i * EXTERNAL_FLASH_SECTOR_SIZE);
        esp_err_t ret = external_flash_erase_sector(sector_addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase sector %lu at 0x%06lX", i, sector_addr);
            return ret;
        }
    }

    ESP_LOGI(TAG, "✅ Erased %lu sectors successfully", sectors);
    return ESP_OK;
}

// ========================================================================
// JEDEC ID OPERATIONS
// ========================================================================

esp_err_t external_flash_read_jedec_id(uint8_t *manufacturer,
                                       uint8_t *mem_type,
                                       uint8_t *capacity)
{
    if (!initialized && spi_handle == NULL) {
        // Allow reading JEDEC ID even before full initialization
        // (used during init process)
    }

    if (spi_handle == NULL) {
        ESP_LOGE(TAG, "SPI handle not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t id_data[3] = {0};

    // Use spi_transaction_ext_t with variable command for command-only transactions
    spi_transaction_ext_t trans = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD,
            .cmd = CMD_READ_JEDEC_ID,
            .rxlength = 24,     // 3 bytes response
            .rx_buffer = id_data
        },
        .command_bits = 8       // 8-bit command
    };

    esp_err_t ret = spi_device_transmit(spi_handle, (spi_transaction_t*)&trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read JEDEC ID: %s", esp_err_to_name(ret));
        return ret;
    }

    if (manufacturer) *manufacturer = id_data[0];
    if (mem_type) *mem_type = id_data[1];
    if (capacity) *capacity = id_data[2];

    return ESP_OK;
}

esp_err_t external_flash_verify_chip(void)
{
    if (!initialized || spi_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t mfg, mem, cap;
    esp_err_t ret = external_flash_read_jedec_id(&mfg, &mem, &cap);
    if (ret != ESP_OK) {
        return ret;
    }

    if (mfg == W25Q64_JEDEC_MFG &&
        mem == W25Q64_JEDEC_MEM_TYPE &&
        cap == W25Q64_JEDEC_CAPACITY) {
        ESP_LOGI(TAG, "✅ Verified W25Q64 chip");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "❌ Chip verification failed (got 0x%02X 0x%02X 0x%02X)", mfg, mem, cap);
    return ESP_FAIL;
}

// ========================================================================
// STATUS OPERATIONS
// ========================================================================

esp_err_t external_flash_read_status(uint8_t *status)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (status == NULL) {
        ESP_LOGE(TAG, "NULL status pointer");
        return ESP_ERR_INVALID_ARG;
    }

    return read_status_internal(status);
}

esp_err_t external_flash_wait_ready(uint32_t timeout_ms)
{
    if (!initialized || spi_handle == NULL) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return wait_ready_internal(timeout_ms);
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

uint32_t external_flash_get_size(void)
{
    return EXTERNAL_FLASH_SIZE;
}

bool external_flash_available(void)
{
    return initialized && (spi_handle != NULL);
}

void external_flash_get_stats(uint32_t *total_reads,
                             uint32_t *total_writes,
                             uint32_t *total_erases)
{
    if (total_reads) *total_reads = stats_reads;
    if (total_writes) *total_writes = stats_writes;
    if (total_erases) *total_erases = stats_erases;
}

void external_flash_reset_stats(void)
{
    stats_reads = 0;
    stats_writes = 0;
    stats_erases = 0;
    ESP_LOGI(TAG, "Statistics reset");
}

/**
 * @brief Register external flash as ESP partition for filesystem use
 *
 * This function registers the W25Q64 external flash with ESP-IDF's esp_flash API,
 * then creates a partition on it that can be used by LittleFS or other filesystems.
 *
 * The function uses spi_bus_add_flash_device() to add the flash to the existing
 * SPI bus (SPI2/HSPI), then registers it as a partition.
 */
esp_err_t external_flash_register_partition(const esp_partition_t **out_partition)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Flash not initialized - call external_flash_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (out_partition == NULL) {
        ESP_LOGE(TAG, "Output partition pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Check if already registered
    static const esp_partition_t *s_registered_partition = NULL;
    if (s_registered_partition != NULL) {
        ESP_LOGI(TAG, "Partition already registered, returning existing handle");
        *out_partition = s_registered_partition;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Registering external flash with ESP-IDF esp_flash API...");

    // Step 1: Configure esp_flash device on the existing SPI bus
    // Note: The SPI bus (SPI2/HSPI) is already initialized by external_flash_init()
    const esp_flash_spi_device_config_t flash_config = {
        .host_id = SPI2_HOST,
        .cs_id = 0,  // First CS on this SPI host
        .cs_io_num = PIN_NUM_CS,
        .io_mode = SPI_FLASH_DIO,  // Dual I/O mode
        .freq_mhz = 10,  // 10 MHz to match our custom driver
    };

    // Step 2: Add flash device to SPI bus and get esp_flash handle
    ESP_LOGI(TAG, "Adding flash device to SPI bus...");
    esp_err_t ret = spi_bus_add_flash_device(&esp_flash_handle, &flash_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add flash device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 3: Initialize the flash chip (probe and detect)
    ESP_LOGI(TAG, "Initializing esp_flash chip...");
    ret = esp_flash_init(esp_flash_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize esp_flash: %s", esp_err_to_name(ret));
        spi_bus_remove_flash_device(esp_flash_handle);
        esp_flash_handle = NULL;
        return ret;
    }

    // Step 4: Verify flash size
    uint32_t flash_size = 0;
    ret = esp_flash_get_size(esp_flash_handle, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "External flash size: %lu bytes (%.2f MB)",
                 (unsigned long)flash_size, flash_size / (1024.0 * 1024.0));
    }

    // Step 5: Register the entire flash as a partition
    ESP_LOGI(TAG, "Registering partition 'ext_storage'...");
    ret = esp_partition_register_external(
        esp_flash_handle,
        0,                                      // Start at offset 0
        EXTERNAL_FLASH_SIZE,                    // Use entire 8MB
        "ext_storage",                          // Partition label
        ESP_PARTITION_TYPE_DATA,                // Data partition
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,      // Generic data subtype
        &s_registered_partition
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register partition: %s", esp_err_to_name(ret));
        spi_bus_remove_flash_device(esp_flash_handle);
        esp_flash_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "✅ External flash registered as partition 'ext_storage'");
    ESP_LOGI(TAG, "   Size: 8 MB");
    ESP_LOGI(TAG, "   Type: DATA");
    ESP_LOGI(TAG, "   Ready for LittleFS mount");

    *out_partition = s_registered_partition;
    return ESP_OK;
}

// ========================================================================
// PRIVATE HELPER FUNCTIONS
// ========================================================================

static esp_err_t send_write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;

    spi_transaction_t trans = {
        .flags = 0,
        .length = 8,  // 1 byte
        .tx_buffer = &cmd,
        .rx_buffer = NULL
    };

    return spi_device_transmit(spi_handle, &trans);
}

static esp_err_t read_status_internal(uint8_t *status)
{
    uint8_t cmd = CMD_READ_STATUS_REG1;
    uint8_t status_byte = 0;

    spi_transaction_t trans = {
        .flags = 0,
        .length = 8,        // 1 byte command
        .rxlength = 8,      // 1 byte response
        .tx_buffer = &cmd,
        .rx_buffer = &status_byte
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret == ESP_OK) {
        *status = status_byte;
    }

    return ret;
}

static esp_err_t wait_ready_internal(uint32_t timeout_ms)
{
    uint8_t status;
    uint32_t start_time = xTaskGetTickCount();
    uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (1) {
        esp_err_t ret = read_status_internal(&status);
        if (ret != ESP_OK) {
            return ret;
        }

        // Check BUSY bit (bit 0)
        if ((status & STATUS_BUSY_BIT) == 0) {
            return ESP_OK;  // Ready
        }

        // Check timeout
        if ((xTaskGetTickCount() - start_time) >= timeout_ticks) {
            ESP_LOGE(TAG, "Timeout waiting for flash ready (status=0x%02X)", status);
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(1));  // Wait 1ms before retry
    }
}
