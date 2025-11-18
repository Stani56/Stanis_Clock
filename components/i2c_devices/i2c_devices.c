#include "i2c_devices.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "thread_safety.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <string.h>

static const char *TAG = "i2c_devices";

// Global Variables
static uint8_t individual_led_brightness = 32;
static uint8_t current_global_brightness = 255;
static i2c_master_bus_handle_t leds_bus_handle = NULL;
static i2c_master_bus_handle_t sensors_bus_handle = NULL;
static i2c_master_dev_handle_t tlc_dev_handles[TLC59116_COUNT] = {NULL};
static i2c_master_dev_handle_t ds3231_dev_handle = NULL;
static i2c_master_dev_handle_t bh1750_dev_handle = NULL;

// TLC59116 device states
static tlc59116_device_t tlc_devices[TLC59116_COUNT] = {0};

// TLC59116 addresses
const uint8_t tlc_addresses[TLC59116_COUNT] = {
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6A
};

// TLC59116 hardware reset state
static bool tlc_reset_gpio_initialized = false;

// Helper Functions
static esp_err_t i2c_write_byte(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t value)
{
    i2c_master_dev_handle_t dev_handle = NULL;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Find TLC device handle
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_addresses[i] == device_addr) {
                dev_handle = tlc_dev_handles[i];
                break;
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == DS3231_ADDR) {
        dev_handle = ds3231_dev_handle;
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == BH1750_ADDR) {
        dev_handle = bh1750_dev_handle;
    }
    
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "Device handle not found for address 0x%02X", device_addr);
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t write_buffer[2] = {reg_addr, value};
    return i2c_master_transmit(dev_handle, write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(100));
}

static esp_err_t i2c_read_bytes(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev_handle = NULL;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Find TLC device handle
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_addresses[i] == device_addr) {
                dev_handle = tlc_dev_handles[i];
                break;
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == DS3231_ADDR) {
        dev_handle = ds3231_dev_handle;
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == BH1750_ADDR) {
        dev_handle = bh1750_dev_handle;
    }
    
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "Device handle not found for address 0x%02X", device_addr);
        return ESP_ERR_NOT_FOUND;
    }
    
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, pdMS_TO_TICKS(100));
}

// Core I2C Initialization
esp_err_t i2c_devices_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C devices with ESP-IDF 5.3.1 driver");
    ESP_LOGI(TAG, "I2C Frequencies: LEDs=%dkHz, Sensors=%dkHz", 
             I2C_LEDS_MASTER_FREQ_HZ/1000, I2C_SENSORS_MASTER_FREQ_HZ/1000);
    
    // Initialize LEDs I2C bus
    i2c_master_bus_config_t leds_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_LEDS_MASTER_PORT,
        .scl_io_num = I2C_LEDS_MASTER_SCL_IO,
        .sda_io_num = I2C_LEDS_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&leds_bus_config, &leds_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LEDs I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LEDs I2C bus initialized on port %d", I2C_LEDS_MASTER_PORT);
    
    // Initialize Sensors I2C bus
    i2c_master_bus_config_t sensors_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_SENSORS_MASTER_PORT,
        .scl_io_num = I2C_SENSORS_MASTER_SCL_IO,
        .sda_io_num = I2C_SENSORS_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ret = i2c_new_master_bus(&sensors_bus_config, &sensors_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create Sensors I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Sensors I2C bus initialized on port %d", I2C_SENSORS_MASTER_PORT);
    
    // Add TLC59116 devices
    for (int i = 0; i < TLC59116_COUNT; i++) {
        i2c_device_config_t tlc_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = tlc_addresses[i],
            .scl_speed_hz = I2C_LEDS_MASTER_FREQ_HZ,
        };
        
        ret = i2c_master_bus_add_device(leds_bus_handle, &tlc_dev_cfg, &tlc_dev_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add TLC59116 device %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "TLC59116 device %d added (0x%02X)", i, tlc_addresses[i]);
    }
    
    // Add DS3231 device
    i2c_device_config_t ds3231_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDR,
        .scl_speed_hz = I2C_SENSORS_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(sensors_bus_handle, &ds3231_dev_cfg, &ds3231_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add DS3231 device: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "DS3231 device added (0x%02X)", DS3231_ADDR);
    
    // Add BH1750 light sensor device
    i2c_device_config_t bh1750_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_ADDR,
        .scl_speed_hz = I2C_SENSORS_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(sensors_bus_handle, &bh1750_dev_cfg, &bh1750_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BH1750 device: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BH1750 device added (0x%02X)", BH1750_ADDR);

    // Initialize TLC59116 hardware reset GPIO (must be done before init_all)
    ret = tlc59116_reset_gpio_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TLC hardware reset GPIO initialized successfully");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "TLC hardware reset not available (hardware modification required)");
    } else {
        ESP_LOGW(TAG, "TLC hardware reset GPIO init failed: %s", esp_err_to_name(ret));
    }

    // Initialize all TLC59116 devices
    ret = tlc59116_init_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Some TLC59116 devices failed to initialize, continuing with available devices");
    }
    
    // Initialize DS3231 RTC
    ret = ds3231_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DS3231 initialization failed: %s", esp_err_to_name(ret));
    }
    
    // Initialize BH1750 light sensor
    ret = bh1750_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BH1750 initialization failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "I2C devices initialization completed");
    return ESP_OK;
}

// I2C Bus Scanner - Simplified to avoid timing conflicts
esp_err_t i2c_scan_bus(i2c_port_t port)
{
    ESP_LOGI(TAG, "Checking I2C bus %d device status...", port);
    int device_count = 0;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Check known TLC59116 devices only
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_devices[i].initialized) {
                ESP_LOGI(TAG, "TLC59116 device %d (0x%02X) - OK", i, tlc_addresses[i]);
                device_count++;
            } else {
                ESP_LOGI(TAG, "TLC59116 device %d (0x%02X) - Failed", i, tlc_addresses[i]);
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT) {
        // Test DS3231 with a simple read operation
        if (ds3231_dev_handle != NULL) {
            uint8_t test_data;
            esp_err_t ret = i2c_read_bytes(I2C_SENSORS_MASTER_PORT, DS3231_ADDR, DS3231_REG_SECONDS, &test_data, 1);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "DS3231 RTC (0x%02X) - OK", DS3231_ADDR);
                device_count++;
            } else {
                ESP_LOGI(TAG, "DS3231 RTC (0x%02X) - Failed", DS3231_ADDR);
            }
        }
    }
    
    ESP_LOGI(TAG, "I2C bus %d status check complete: %d devices working", port, device_count);
    return ESP_OK;
}

// TLC59116 LED Controller Functions
esp_err_t tlc59116_init_all(void)
{
    ESP_LOGI(TAG, "Initializing all TLC59116 devices");
    
    esp_err_t ret = ESP_OK;
    int success_count = 0;
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        esp_err_t device_ret = tlc59116_init_device(i);
        if (device_ret == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGW(TAG, "Failed to initialize TLC59116 device %d, continuing with others", i);
            ret = device_ret; // Keep last error
        }
    }
    
    ESP_LOGI(TAG, "TLC59116 initialization completed: %d/%d devices successful", success_count, TLC59116_COUNT);
    
    if (success_count == 0) {
        ESP_LOGE(TAG, "No TLC59116 devices initialized successfully");
        return ESP_FAIL;
    }
    
    return (success_count == TLC59116_COUNT) ? ESP_OK : ret;
}

esp_err_t tlc59116_init_device(uint8_t tlc_index)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (tlc_dev_handles[tlc_index] == NULL) {
        ESP_LOGE(TAG, "TLC59116 device handle %d not available", tlc_index);
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    uint8_t device_addr = tlc_addresses[tlc_index];
    
    ESP_LOGI(TAG, "Initializing TLC59116 device %d (0x%02X)", tlc_index, device_addr);
    
    // Reset device first
    ret = tlc59116_reset_device(tlc_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure MODE1 register: Normal mode, auto-increment enabled
    uint8_t mode1_val = TLC59116_MODE1_AI0 | TLC59116_MODE1_AI1; // Auto-increment for PWM registers
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, mode1_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure MODE1 for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure MODE2 register: Normal operation
    uint8_t mode2_val = 0x00; // Normal operation, no blinking
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE2, mode2_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure MODE2 for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Set all LEDs to GRPPWM mode for global brightness control (LEDOUT registers)
    uint8_t ledout_val = 0xFF; // 11111111 = GRPPWM mode for all 4 outputs per register
    for (int reg = 0; reg < 4; reg++) {
        ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_LEDOUT0 + reg, ledout_val);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure LEDOUT%d for TLC59116 device %d: %s", 
                     reg, tlc_index, esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Set all PWM channels to 0 (off)
    for (int ch = 0; ch < TLC59116_CHANNELS; ch++) {
        ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_PWM0 + ch, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set PWM%d for TLC59116 device %d: %s", 
                     ch, tlc_index, esp_err_to_name(ret));
            return ret;
        }
        tlc_devices[tlc_index].pwm_values[ch] = 0;
        tlc_devices[tlc_index].led_states[ch] = TLC59116_LED_GRPPWM;
    }
    
    // Set global brightness to match expected startup value (120) instead of 255
    // This prevents brightness jumps during startup when main sets GRPPWM to 120
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_GRPPWM, 120);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GRPPWM for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    tlc_devices[tlc_index].initialized = true;
    ESP_LOGI(TAG, "TLC59116 device %d initialized successfully", tlc_index);
    
    return ESP_OK;
}

esp_err_t tlc59116_reset_device(uint8_t tlc_index)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t device_addr = tlc_addresses[tlc_index];
    
    // Set MODE1 to software reset (sleep mode first, then wake up)
    esp_err_t ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, TLC59116_MODE1_SLEEP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sleep TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Sleep delay
    
    // Wake up device
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1)); // Wake-up delay
    
    return ESP_OK;
}

esp_err_t tlc_set_pwm(uint8_t tlc_index, uint8_t channel, uint8_t pwm_value)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }

    if (channel >= TLC59116_CHANNELS) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (!tlc_devices[tlc_index].initialized || tlc_dev_handles[tlc_index] == NULL) {
        // Silently skip uninitialized devices to avoid spam
        return ESP_ERR_INVALID_STATE;
    }

    // Lock I2C bus for atomic write operation
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to acquire I2C mutex for PWM write");
        return ESP_ERR_TIMEOUT;
    }

    // DEBUG: Log PWM writes when needed (can be commented out for production)
    // ESP_LOGD(TAG, "TLC PWM: device[%d] channel[%d] = %d", tlc_index, channel, pwm_value);

    // Use device handle directly - no lookup needed
    uint8_t write_buffer[2] = {TLC59116_PWM0 + channel, pwm_value};
    esp_err_t ret = i2c_master_transmit(tlc_dev_handles[tlc_index], write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));

    if (ret == ESP_OK) {
        tlc_devices[tlc_index].pwm_values[channel] = pwm_value;
        // Add small delay to ensure I2C timing stability and prevent brightness jumps
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    thread_safe_i2c_unlock();

    return ret;
}

esp_err_t tlc_set_global_brightness(uint8_t brightness)
{
    // DEBUG: Track global brightness changes when needed
    ESP_LOGD(TAG, "Global brightness change: %d → %d", current_global_brightness, brightness);

    // Lock I2C bus for atomic brightness update across all devices
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to acquire I2C mutex for global brightness");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    int success_count = 0;

    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized || tlc_dev_handles[i] == NULL) {
            continue;
        }

        // Use device handle directly
        uint8_t write_buffer[2] = {TLC59116_GRPPWM, brightness};
        esp_err_t device_ret = i2c_master_transmit(tlc_dev_handles[i], write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));

        if (device_ret == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGW(TAG, "Failed to set global brightness for TLC59116 device %d: %s",
                     i, esp_err_to_name(device_ret));
            ret = device_ret;
        }
    }

    thread_safe_i2c_unlock();

    ESP_LOGI(TAG, "Global brightness set to %d for %d devices", brightness, success_count);

    // Update global tracking variable only if at least one device succeeded
    if (success_count > 0) {
        current_global_brightness = brightness;
    }

    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_turn_off_all_leds(void)
{
    esp_err_t ret = ESP_OK;
    int success_count = 0;
    int error_count = 0;
    
    ESP_LOGI(TAG, "Turning off all LEDs");
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            continue;
        }
        
        // Set all PWM channels to 0 with error handling and timing
        for (int ch = 0; ch < TLC59116_CHANNELS; ch++) {
            esp_err_t ch_ret = tlc_set_pwm(i, ch, 0);
            if (ch_ret != ESP_OK) {
                error_count++;
                // Don't fail completely on individual channel errors
                if (error_count > 10) {
                    ESP_LOGW(TAG, "Too many I2C errors, stopping clear operation");
                    break;
                }
            } else {
                // Small delay between successful operations to prevent bus saturation
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        success_count++;
        
        // Break if too many errors
        if (error_count > 10) {
            break;
        }
    }
    
    if (error_count > 0) {
        ESP_LOGW(TAG, "LED clear completed with %d I2C errors for %d devices", error_count, success_count);
    } else {
        ESP_LOGI(TAG, "All LEDs turned off for %d devices", success_count);
    }
    
    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_read_all_status(uint8_t *status_array)
{
    if (status_array == NULL) {
        ESP_LOGE(TAG, "Status array is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            status_array[i] = 0xFF; // Mark as uninitialized
            continue;
        }
        
        uint8_t device_addr = tlc_addresses[i];
        esp_err_t device_ret = i2c_read_bytes(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_EFLAG1, &status_array[i], 1);
        
        if (device_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read status for TLC59116 device %d: %s", i, esp_err_to_name(device_ret));
            status_array[i] = 0xFF;
            ret = device_ret;
        }
    }
    
    return ret;
}

esp_err_t tlc_set_matrix_led(uint8_t row, uint8_t col, uint8_t brightness)
{
    if (row >= WORDCLOCK_ROWS) {
        ESP_LOGE(TAG, "Invalid row: %d", row);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (col >= WORDCLOCK_COLS) {
        ESP_LOGE(TAG, "Invalid column: %d", col);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate TLC device and channel from row/column
    uint8_t tlc_index = row;
    uint8_t channel = col;
    
    return tlc_set_pwm(tlc_index, channel, brightness);
}

esp_err_t tlc_set_matrix_led_auto(uint8_t row, uint8_t col)
{
    return tlc_set_matrix_led(row, col, individual_led_brightness);
}

esp_err_t tlc_set_minute_leds(uint8_t indicator_count)
{
    if (indicator_count > WORDCLOCK_MINUTE_LEDS) {
        ESP_LOGE(TAG, "Invalid indicator count: %d", indicator_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    // Minute indicator LEDs are at Row 9, Columns 11-14
    for (uint8_t i = 0; i < WORDCLOCK_MINUTE_LEDS; i++) {
        uint8_t brightness = (i < indicator_count) ? individual_led_brightness : 0;
        esp_err_t led_ret = tlc_set_matrix_led(9, 11 + i, brightness);
        if (led_ret != ESP_OK) {
            ret = led_ret;
        }
    }
    
    ESP_LOGI(TAG, "Minute indicator LEDs set: %d active", indicator_count);
    return ret;
}

esp_err_t tlc_set_individual_brightness(uint8_t brightness)
{
    individual_led_brightness = brightness;
    ESP_LOGI(TAG, "Individual LED brightness set to %d", brightness);
    return ESP_OK;
}

uint8_t tlc_get_individual_brightness(void)
{
    return individual_led_brightness;
}

uint8_t tlc_get_global_brightness(void)
{
    return current_global_brightness;
}

// DS3231 RTC Functions
esp_err_t ds3231_init(void)
{
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing DS3231 RTC");
    
    // Read control register to check device
    uint8_t control_reg;
    uint8_t reg_addr = DS3231_REG_CONTROL;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, &control_reg, 1, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read DS3231 control register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 control register: 0x%02X", control_reg);
    
    // Configure DS3231 for normal operation
    // Clear EOSC bit to enable oscillator, disable square wave output
    uint8_t control_val = 0x1C; // INTCN=1, RS2=1, RS1=1 (disable square wave)
    uint8_t control_write[2] = {DS3231_REG_CONTROL, control_val};
    ret = i2c_master_transmit(ds3231_dev_handle, control_write, sizeof(control_write), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure DS3231 control register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Clear status register
    uint8_t status_write[2] = {DS3231_REG_STATUS, 0x00};
    ret = i2c_master_transmit(ds3231_dev_handle, status_write, sizeof(status_write), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear DS3231 status register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 RTC initialized successfully");
    return ESP_OK;
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

esp_err_t ds3231_get_time_struct(wordclock_time_t *time)
{
    if (time == NULL) {
        ESP_LOGE(TAG, "Time structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }

    // Lock I2C bus for RTC read
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to acquire I2C mutex for RTC read");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t time_data[7];
    uint8_t reg_addr = DS3231_REG_SECONDS;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, time_data, 7, pdMS_TO_TICKS(1000));

    thread_safe_i2c_unlock();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from DS3231: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convert BCD to binary
    time->seconds = bcd_to_bin(time_data[0] & 0x7F);  // Mask out CH bit
    time->minutes = bcd_to_bin(time_data[1] & 0x7F);  // Mask out unused bits
    time->hours = bcd_to_bin(time_data[2] & 0x3F);    // Mask out 12/24h and AM/PM bits
    time->day = bcd_to_bin(time_data[4] & 0x3F);      // Mask out unused bits
    time->month = bcd_to_bin(time_data[5] & 0x1F);    // Mask out century bit
    time->year = bcd_to_bin(time_data[6]);

    return ESP_OK;
}

esp_err_t ds3231_set_time_struct(const wordclock_time_t *time)
{
    if (time == NULL) {
        ESP_LOGE(TAG, "Time structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate time values
    if (time->seconds > 59 || time->minutes > 59 || time->hours > 23 ||
        time->day < 1 || time->day > 31 || time->month < 1 || time->month > 12 ||
        time->year > 99) {
        ESP_LOGE(TAG, "Invalid time values");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert binary to BCD and write to DS3231
    uint8_t time_data[7];
    time_data[0] = bin_to_bcd(time->seconds);
    time_data[1] = bin_to_bcd(time->minutes);
    time_data[2] = bin_to_bcd(time->hours);    // 24-hour format
    time_data[3] = 1;                          // Day of week (not used)
    time_data[4] = bin_to_bcd(time->day);
    time_data[5] = bin_to_bcd(time->month);
    time_data[6] = bin_to_bcd(time->year);
    
    // Write all time data in one transaction
    uint8_t write_buffer[8];
    write_buffer[0] = DS3231_REG_SECONDS;  // Starting register
    memcpy(&write_buffer[1], time_data, 7); // Copy time data
    
    esp_err_t ret = i2c_master_transmit(ds3231_dev_handle, write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write time to DS3231: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 time set: %02d:%02d:%02d %02d/%02d/20%02d",
             time->hours, time->minutes, time->seconds,
             time->day, time->month, time->year);
    
    return ESP_OK;
}

esp_err_t ds3231_get_temperature(ds3231_temp_t *temperature)
{
    if (temperature == NULL) {
        ESP_LOGE(TAG, "Temperature structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t temp_data[2];
    uint8_t reg_addr = DS3231_REG_TEMP_MSB;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, temp_data, 2, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature from DS3231: %s", esp_err_to_name(ret));
        temperature->valid = false;
        return ret;
    }
    
    // Convert temperature data
    int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
    temperature->temperature = (float)temp_raw / 256.0f;
    temperature->valid = true;
    
    return ESP_OK;
}

// BH1750 Light Sensor Functions
esp_err_t bh1750_init(void)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing BH1750 light sensor");
    return ESP_OK;
}

esp_err_t bh1750_write_command(uint8_t command)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    return i2c_master_transmit(bh1750_dev_handle, &command, 1, pdMS_TO_TICKS(1000));
}

esp_err_t bh1750_read_data(uint8_t *data, size_t len)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }

    // Lock I2C bus for light sensor read
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to acquire I2C mutex for light sensor read");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = i2c_master_receive(bh1750_dev_handle, data, len, pdMS_TO_TICKS(1000));

    thread_safe_i2c_unlock();

    return ret;
}

// ============================================================================
// TLC59116 Register Read Functions (for LED validation)
// ============================================================================

esp_err_t tlc_read_pwm_values(uint8_t tlc_index, uint8_t pwm_values[16])
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }

    if (pwm_values == NULL) {
        ESP_LOGE(TAG, "PWM values array is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!tlc_devices[tlc_index].initialized) {
        ESP_LOGW(TAG, "TLC59116 device %d not initialized", tlc_index);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t device_addr = tlc_addresses[tlc_index];

    // Read each PWM register individually (byte-by-byte) to avoid auto-increment issues
    // The TLC59116 auto-increment pointer can get corrupted by differential writes,
    // so we explicitly read each register one at a time with NO AUTO-INCREMENT.
    //
    // CRITICAL: TLC59116 register address format (bits 7:5):
    //   Bit 7:6 = 00: No auto-increment (single register access)
    //   Bit 7:6 = 10: Auto-increment enabled
    // We must explicitly set bits 7:6 = 00 by masking with 0x1F!
    esp_err_t ret = ESP_OK;
    for (uint8_t ch = 0; ch < 16; ch++) {
        // Explicit register address with NO auto-increment (bits 7:5 = 000)
        uint8_t reg_addr = (TLC59116_PWM0 + ch) & 0x1F;  // Mask to disable auto-increment

        esp_err_t ch_ret = i2c_read_bytes(
            I2C_LEDS_MASTER_PORT,
            device_addr,
            reg_addr,  // No-auto-increment register address
            &pwm_values[ch],
            1  // Read single byte
        );

        if (ch_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read PWM%d from TLC %d: %s",
                     ch, tlc_index, esp_err_to_name(ch_ret));
            ret = ch_ret;
            // Continue reading other channels even if one fails
        }
    }

    // DEBUG: Print readback matrix for TLC device 0 (Row 0) to diagnose pointer issue
    if (tlc_index == 0) {
        ESP_LOGI(TAG, "🔍 TLC 0 (Row 0) PWM Readback:");
        ESP_LOGI(TAG, "  [0-7]:   %3d %3d %3d %3d %3d %3d %3d %3d",
                 pwm_values[0], pwm_values[1], pwm_values[2], pwm_values[3],
                 pwm_values[4], pwm_values[5], pwm_values[6], pwm_values[7]);
        ESP_LOGI(TAG, "  [8-15]:  %3d %3d %3d %3d %3d %3d %3d %3d",
                 pwm_values[8], pwm_values[9], pwm_values[10], pwm_values[11],
                 pwm_values[12], pwm_values[13], pwm_values[14], pwm_values[15]);

        // Check if we're reading LEDOUT registers (0xAA = 170) instead of PWM
        uint8_t aa_count = 0;
        for (int i = 0; i < 16; i++) {
            if (pwm_values[i] == 0xAA || pwm_values[i] == 170) aa_count++;
        }
        if (aa_count > 5) {
            ESP_LOGW(TAG, "⚠️  Likely reading LEDOUT registers (0xAA) instead of PWM! Count: %d/16", aa_count);
        }
    }

    return ret;
}

esp_err_t tlc_read_all_pwm_values(uint8_t hardware_state[10][16])
{
    if (hardware_state == NULL) {
        ESP_LOGE(TAG, "Hardware state array is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Lock I2C bus for exclusive validation readback
    // This prevents interference from light sensor, brightness updates, display writes, etc.
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for PWM readback");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    int success_count = 0;

    for (uint8_t row = 0; row < TLC59116_COUNT; row++) {
        esp_err_t row_ret = tlc_read_pwm_values(row, hardware_state[row]);

        if (row_ret == ESP_OK) {
            success_count++;
        } else {
            // Mark row as invalid on read failure
            memset(hardware_state[row], 0xFF, 16);
            ret = row_ret;
            ESP_LOGW(TAG, "Failed to read PWM from TLC %d: %s",
                     row, esp_err_to_name(row_ret));
        }

        // Small delay between devices to prevent I2C bus saturation
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    thread_safe_i2c_unlock();

    ESP_LOGI(TAG, "Hardware readback: %d/10 devices read successfully",
             success_count);

    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_read_global_brightness(uint8_t grppwm_values[10])
{
    if (grppwm_values == NULL) {
        ESP_LOGE(TAG, "GRPPWM values array is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Lock I2C bus for exclusive access during readback
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for GRPPWM readback");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    int success_count = 0;

    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            grppwm_values[i] = 0xFF;  // Invalid marker
            continue;
        }

        uint8_t device_addr = tlc_addresses[i];
        esp_err_t device_ret = i2c_read_bytes(
            I2C_LEDS_MASTER_PORT,
            device_addr,
            TLC59116_GRPPWM,
            &grppwm_values[i],
            1
        );

        if (device_ret == ESP_OK) {
            success_count++;
        } else {
            grppwm_values[i] = 0xFF;
            ret = device_ret;
            ESP_LOGW(TAG, "Failed to read GRPPWM from TLC %d: %s",
                     i, esp_err_to_name(device_ret));
        }
    }

    thread_safe_i2c_unlock();

    ESP_LOGI(TAG, "Global brightness readback: %d/10 devices read successfully",
             success_count);

    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_read_error_flags(uint8_t eflag_values[10][2])
{
    if (eflag_values == NULL) {
        ESP_LOGE(TAG, "EFLAG values array is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Lock I2C bus for exclusive access during readback
    if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire I2C mutex for EFLAG readback");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    bool errors_found = false;
    int success_count = 0;

    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            eflag_values[i][0] = 0xFF;
            eflag_values[i][1] = 0xFF;
            continue;
        }

        uint8_t device_addr = tlc_addresses[i];

        // Read EFLAG1 and EFLAG2 (0x1D and 0x1E)
        esp_err_t device_ret = i2c_read_bytes(
            I2C_LEDS_MASTER_PORT,
            device_addr,
            TLC59116_EFLAG1,
            eflag_values[i],
            2  // Read both EFLAG registers
        );

        if (device_ret != ESP_OK) {
            ret = device_ret;
            ESP_LOGW(TAG, "Failed to read EFLAG from TLC %d: %s",
                     i, esp_err_to_name(device_ret));
        } else {
            success_count++;
            // Non-zero EFLAG indicates hardware fault
            if (eflag_values[i][0] != 0 || eflag_values[i][1] != 0) {
                errors_found = true;
                ESP_LOGW(TAG, "TLC %d EFLAG errors: 0x%02X 0x%02X",
                         i, eflag_values[i][0], eflag_values[i][1]);
            }
        }
    }

    thread_safe_i2c_unlock();

    ESP_LOGI(TAG, "Error flag readback: %d/10 devices read, errors found: %s",
             success_count, errors_found ? "YES" : "NO");

    return errors_found ? ESP_FAIL : ret;
}

// ============================================================================
// TLC59116 Hardware Reset Functions (GPIO 4 - Shared Reset Line)
// ============================================================================

esp_err_t tlc59116_reset_gpio_init(void)
{
#if BOARD_TLC_HW_RESET_AVAILABLE
    ESP_LOGI(TAG, "Initializing TLC59116 hardware reset on GPIO %d", BOARD_TLC_RESET_GPIO);

    // Configure GPIO 4 as open-drain output with external pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_TLC_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT_OD,  // Open-drain mode (requires external pull-up)
        .pull_up_en = GPIO_PULLUP_DISABLE,  // External 10kΩ pull-up resistor
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure TLC RESET GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // Release RESET (HIGH via external pull-up)
    // Open-drain HIGH = tri-state (pulled HIGH by external resistor)
    gpio_set_level(BOARD_TLC_RESET_GPIO, 1);

    tlc_reset_gpio_initialized = true;
    ESP_LOGI(TAG, "✅ TLC hardware reset initialized (shared RESET line for all 10 devices)");
    ESP_LOGI(TAG, "   GPIO %d configured as open-drain output", BOARD_TLC_RESET_GPIO);
    ESP_LOGI(TAG, "   Reset pulse: %dµs, Stabilization: %dms",
             BOARD_TLC_RESET_PULSE_US, BOARD_TLC_RESET_STABILIZE_MS);

    return ESP_OK;
#else
    ESP_LOGW(TAG, "TLC hardware reset not available (BOARD_TLC_HW_RESET_AVAILABLE=0)");
    ESP_LOGW(TAG, "Hardware modification required - see board_config.h for instructions");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t tlc59116_hardware_reset_all(void)
{
#if BOARD_TLC_HW_RESET_AVAILABLE
    if (!tlc_reset_gpio_initialized) {
        ESP_LOGE(TAG, "Hardware reset GPIO not initialized - call tlc59116_reset_gpio_init() first");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "🔄 Performing hardware reset of all 10 TLC59116 devices...");
    ESP_LOGW(TAG, "   Asserting RESET (LOW) for %dµs on GPIO %d",
             BOARD_TLC_RESET_PULSE_US, BOARD_TLC_RESET_GPIO);

    // Assert RESET (active LOW) for 10µs
    // TLC59116 datasheet: minimum 50ns pulse width
    gpio_set_level(BOARD_TLC_RESET_GPIO, 0);
    esp_rom_delay_us(BOARD_TLC_RESET_PULSE_US);

    // Release RESET (HIGH via external pull-up)
    gpio_set_level(BOARD_TLC_RESET_GPIO, 1);

    ESP_LOGI(TAG, "   RESET released, waiting %dms for stabilization...",
             BOARD_TLC_RESET_STABILIZE_MS);

    // Wait for chip stabilization
    // TLC59116 datasheet: minimum 1ms required
    vTaskDelay(pdMS_TO_TICKS(BOARD_TLC_RESET_STABILIZE_MS));

    ESP_LOGI(TAG, "✅ TLC hardware reset complete");
    ESP_LOGI(TAG, "   All 10 devices reset, re-initialization required");

    return ESP_OK;
#else
    ESP_LOGE(TAG, "Hardware reset not available (feature disabled)");
    ESP_LOGE(TAG, "Enable BOARD_TLC_HW_RESET_AVAILABLE in board_config.h");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

bool tlc59116_has_hardware_reset(void)
{
#if BOARD_TLC_HW_RESET_AVAILABLE
    return tlc_reset_gpio_initialized;
#else
    return false;
#endif
}

//=============================================================================
// TLC59116 Hardware Reset Detection (Power Surge Recovery)
//=============================================================================

/**
 * @brief Read a single register from TLC59116
 *
 * This is a LOW-LEVEL function that reads hardware state directly.
 * Even if RAM is corrupted, the TLC chip holds the truth.
 *
 * @param tlc_index Device index (0-9)
 * @param reg_addr Register address (0x00-0x1B)
 * @param value Output: register value read
 * @return ESP_OK on success, ESP_FAIL on I2C error
 */
esp_err_t tlc_read_register(uint8_t tlc_index, uint8_t reg_addr, uint8_t *value)
{
    if (tlc_index >= TLC59116_COUNT || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get device handle
    i2c_master_dev_handle_t dev_handle = tlc_dev_handles[tlc_index];
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "TLC device %d not initialized", tlc_index);
        return ESP_ERR_INVALID_STATE;
    }

    // I2C read with thread safety
    thread_safe_i2c_lock(pdMS_TO_TICKS(1000));
    esp_err_t ret = i2c_master_transmit_receive(
        dev_handle,
        &reg_addr, 1,      // Write register address
        value, 1,          // Read 1 byte
        pdMS_TO_TICKS(100) // 100ms timeout
    );
    thread_safe_i2c_unlock();

    return ret;
}

/**
 * @brief Check if TLC controllers have been reset to hardware defaults
 *
 * CRITICAL: This function reads HARDWARE state, not RAM variables.
 * If MODE1 register = 0x01, the TLC chip was reset, regardless of
 * what our software thinks the state is.
 *
 * TLC59116 MODE1 register values:
 * - 0x01 = Hardware default after power-on/reset (OSC bit set)
 * - 0x00 = Normal operation (our init sets this)
 * - 0x81 = Sleep mode (if we use it)
 *
 * @param reset_devices Output: Array of bools indicating which devices were reset
 * @return Number of devices detected as reset (0 = all good)
 */
uint8_t tlc_detect_hardware_reset(bool reset_devices[TLC59116_COUNT])
{
    uint8_t reset_count = 0;

    ESP_LOGI(TAG, "🔍 Checking TLC controllers for hardware reset...");

    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        reset_devices[i] = false;

        uint8_t mode1_value = 0;
        esp_err_t ret = tlc_read_register(i, TLC59116_MODE1, &mode1_value);

        if (ret == ESP_OK) {
            // Check if MODE1 is at hardware default (0x01)
            // Bit 4 (OSC/SLEEP) = 1 is the key indicator of reset state
            if (mode1_value == 0x01 || (mode1_value & 0x10)) {
                ESP_LOGW(TAG, "  Device %d: MODE1=0x%02X → RESET DETECTED!", i, mode1_value);
                reset_devices[i] = true;
                reset_count++;
            } else {
                ESP_LOGD(TAG, "  Device %d: MODE1=0x%02X → OK (initialized)", i, mode1_value);
            }
        } else {
            ESP_LOGW(TAG, "  Device %d: Failed to read MODE1 (I2C error)", i);
            // Don't count as reset - might just be I2C glitch
        }

        vTaskDelay(pdMS_TO_TICKS(2)); // Small delay between reads
    }

    if (reset_count > 0) {
        ESP_LOGE(TAG, "⚠️  TLC RESET DETECTED: %d/%d devices", reset_count, TLC59116_COUNT);
    } else {
        ESP_LOGD(TAG, "✅ All TLC controllers in initialized state");
    }

    return reset_count;
}

/**
 * @brief Recover from TLC hardware reset
 *
 * This is the AUTOMATIC recovery that runs when reset is detected.
 * Same logic as manual tlc_hardware_reset MQTT command.
 *
 * Steps:
 * 1. Turn off all LEDs (clear corrupted state)
 * 2. Re-initialize all TLC devices
 * 3. Clear software LED cache
 * 4. Refresh current display
 *
 * @return ESP_OK on success
 */
esp_err_t tlc_automatic_recovery(void)
{
    ESP_LOGE(TAG, "🔧 Starting AUTOMATIC TLC recovery...");

    // Publish to MQTT so user knows recovery is happening
    extern void mqtt_publish_status(const char *status);
    mqtt_publish_status("tlc_auto_recovery_start");

    // Step 1: Turn off all LEDs (clear any corrupted state)
    ESP_LOGI(TAG, "  Step 1/4: Clearing all LEDs...");
    tlc_turn_off_all_leds();
    vTaskDelay(pdMS_TO_TICKS(20));

    // Step 2: Re-initialize all TLC devices
    ESP_LOGI(TAG, "  Step 2/4: Re-initializing TLC controllers...");
    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        esp_err_t ret = tlc59116_init_device(i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "    Device %d init failed: %s", i, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Step 3: Clear software LED state cache
    ESP_LOGI(TAG, "  Step 3/4: Clearing software LED cache...");
    extern uint8_t led_state[10][16];
    memset(led_state, 0, 10 * 16 * sizeof(uint8_t));

    // Step 4: Refresh current display
    ESP_LOGI(TAG, "  Step 4/4: Refreshing display...");
    extern esp_err_t ds3231_get_time_struct(wordclock_time_t *time);
    extern esp_err_t display_german_time(const wordclock_time_t *time);

    wordclock_time_t current_time;
    if (ds3231_get_time_struct(&current_time) == ESP_OK) {
        display_german_time(&current_time);
        ESP_LOGI(TAG, "✅ Automatic TLC recovery COMPLETE");
        mqtt_publish_status("tlc_auto_recovery_success");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ Failed to read RTC for display refresh");
        mqtt_publish_status("tlc_auto_recovery_partial");
        return ESP_FAIL;
    }
}

