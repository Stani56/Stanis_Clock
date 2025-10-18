/**
 * @file external_flash.h
 * @brief W25Q64 External SPI Flash Driver
 *
 * Driver for Winbond W25Q64 8MB SPI flash memory chip.
 * Uses HSPI bus (SPI2_HOST) with the following GPIO pins:
 * - GPIO 14: MOSI (Master Out, Slave In)
 * - GPIO 12: MISO (Master In, Slave Out)
 * - GPIO 13: SCK  (Serial Clock)
 * - GPIO 15: CS   (Chip Select - has internal pull-up)
 *
 * Features:
 * - 8MB (64 Mbit) capacity
 * - 4KB sector erase
 * - 256-byte page program
 * - SPI clock up to 104 MHz (running at 10 MHz conservative)
 * - JEDEC ID verification (0xEF 0x40 0x17)
 *
 * @note GPIO pins verified for ESP32-PICO-KIT V4.1
 * @note No conflicts with existing I2C buses (GPIO 18/19, 25/26)
 */

#ifndef EXTERNAL_FLASH_H
#define EXTERNAL_FLASH_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================================================================
// CONSTANTS
// ========================================================================

/** Total flash size in bytes (8 MB) */
#define EXTERNAL_FLASH_SIZE         (8 * 1024 * 1024)

/** Sector size in bytes (4 KB) */
#define EXTERNAL_FLASH_SECTOR_SIZE  4096

/** Page size in bytes (256 bytes) */
#define EXTERNAL_FLASH_PAGE_SIZE    256

/** Expected JEDEC ID for W25Q64 */
#define W25Q64_JEDEC_MFG            0xEF    /**< Manufacturer: Winbond */
#define W25Q64_JEDEC_MEM_TYPE       0x40    /**< Memory Type: SPI */
#define W25Q64_JEDEC_CAPACITY       0x17    /**< Capacity: 8 MB */


// ========================================================================
// PUBLIC API
// ========================================================================

/**
 * @brief Initialize external SPI flash (W25Q64)
 *
 * Performs the following:
 * - Initializes HSPI bus (SPI2_HOST)
 * - Configures GPIO pins (12, 13, 14, 15)
 * - Reads and verifies JEDEC ID
 * - Sets up SPI device handle
 *
 * @return
 *  - ESP_OK: Flash initialized successfully
 *  - ESP_ERR_INVALID_STATE: SPI bus already initialized (not an error)
 *  - ESP_FAIL: JEDEC ID verification failed
 *  - Other ESP_ERR_* codes from SPI driver
 *
 * @note Safe to call multiple times (checks if already initialized)
 * @note GPIO 15 (CS) has internal ~50kΩ pull-up, no external resistor needed
 */
esp_err_t external_flash_init(void);

/**
 * @brief Read data from external flash
 *
 * @param address Starting address to read from (0x000000 to 0x7FFFFF)
 * @param buffer Pointer to buffer to store read data
 * @param size Number of bytes to read
 *
 * @return
 *  - ESP_OK: Read successful
 *  - ESP_ERR_INVALID_STATE: Flash not initialized
 *  - ESP_ERR_INVALID_ARG: Invalid address, NULL buffer, or size beyond flash
 *  - Other ESP_ERR_* codes from SPI driver
 *
 * @note Can read across sector/page boundaries
 * @note No alignment requirements
 */
esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size);

/**
 * @brief Write data to external flash
 *
 * Writes data using page programming (256-byte pages).
 * Handles page boundary crossing automatically.
 *
 * @param address Starting address to write to (0x000000 to 0x7FFFFF)
 * @param data Pointer to data to write
 * @param size Number of bytes to write
 *
 * @return
 *  - ESP_OK: Write successful
 *  - ESP_ERR_INVALID_STATE: Flash not initialized
 *  - ESP_ERR_INVALID_ARG: Invalid address, NULL data, or size beyond flash
 *  - ESP_ERR_TIMEOUT: Write operation timed out
 *  - Other ESP_ERR_* codes from SPI driver
 *
 * @warning Flash sectors must be erased before writing!
 * @note Writing to non-erased flash may result in corrupted data
 * @note Handles page boundary crossing (max 256 bytes per page)
 */
esp_err_t external_flash_write(uint32_t address, const void *data, size_t size);

/**
 * @brief Erase a 4KB sector
 *
 * Erases a single 4KB sector. All bytes in the sector will be set to 0xFF.
 * Sector erase is required before writing new data.
 *
 * @param address Any address within the sector to erase (will be sector-aligned)
 *
 * @return
 *  - ESP_OK: Erase successful
 *  - ESP_ERR_INVALID_STATE: Flash not initialized
 *  - ESP_ERR_INVALID_ARG: Address beyond flash size
 *  - ESP_ERR_TIMEOUT: Erase operation timed out (>1000ms)
 *  - Other ESP_ERR_* codes from SPI driver
 *
 * @note Address does NOT need to be sector-aligned (will be aligned automatically)
 * @note Typical erase time: 45-400ms per sector
 * @note Timeout set to 1000ms for safety
 */
esp_err_t external_flash_erase_sector(uint32_t address);

/**
 * @brief Erase multiple consecutive sectors
 *
 * Helper function to erase multiple sectors at once.
 *
 * @param start_address Starting address (will be sector-aligned)
 * @param size Number of bytes to erase (will be rounded up to sector boundary)
 *
 * @return
 *  - ESP_OK: All sectors erased successfully
 *  - ESP_ERR_* codes from external_flash_erase_sector()
 *
 * @note Calculates required sectors automatically
 */
esp_err_t external_flash_erase_range(uint32_t start_address, size_t size);

/**
 * @brief Get flash chip size in bytes
 *
 * @return Flash size in bytes (8388608 for W25Q64)
 */
uint32_t external_flash_get_size(void);

/**
 * @brief Check if external flash is initialized and available
 *
 * @return
 *  - true: Flash initialized and ready
 *  - false: Flash not initialized or failed initialization
 */
bool external_flash_available(void);

/**
 * @brief Read and verify JEDEC ID
 *
 * Reads the 3-byte JEDEC ID and verifies it matches W25Q64.
 *
 * @param manufacturer Pointer to store manufacturer ID (0xEF for Winbond)
 * @param mem_type Pointer to store memory type (0x40 for SPI)
 * @param capacity Pointer to store capacity code (0x17 for 8MB)
 *
 * @return
 *  - ESP_OK: JEDEC ID read successfully
 *  - ESP_ERR_INVALID_STATE: Flash not initialized
 *  - ESP_ERR_INVALID_ARG: NULL pointer provided
 *  - Other ESP_ERR_* codes from SPI driver
 *
 * @note Can pass NULL for any parameter to ignore that value
 */
esp_err_t external_flash_read_jedec_id(uint8_t *manufacturer,
                                       uint8_t *mem_type,
                                       uint8_t *capacity);

/**
 * @brief Verify flash chip is W25Q64
 *
 * @return
 *  - ESP_OK: Chip verified as W25Q64
 *  - ESP_FAIL: Unexpected chip detected
 *  - ESP_ERR_INVALID_STATE: Flash not initialized
 */
esp_err_t external_flash_verify_chip(void);


// ========================================================================
// ADVANCED/DIAGNOSTIC FUNCTIONS
// ========================================================================

/**
 * @brief Read flash status register
 *
 * @param status Pointer to store status byte
 *
 * @return ESP_OK or error code
 *
 * Status Register Bits:
 * - Bit 0: BUSY (1 = busy, 0 = ready)
 * - Bit 1: WEL (Write Enable Latch)
 * - Bits 2-5: Block protect bits
 * - Bit 6: QE (Quad Enable)
 * - Bit 7: SRWD (Status Register Write Disable)
 */
esp_err_t external_flash_read_status(uint8_t *status);

/**
 * @brief Wait for flash to become ready
 *
 * Polls status register until BUSY bit clears.
 *
 * @param timeout_ms Maximum time to wait in milliseconds
 *
 * @return
 *  - ESP_OK: Flash ready
 *  - ESP_ERR_TIMEOUT: Timeout occurred while waiting
 */
esp_err_t external_flash_wait_ready(uint32_t timeout_ms);

/**
 * @brief Get flash statistics
 *
 * @param total_reads Pointer to store total read operations count
 * @param total_writes Pointer to store total write operations count
 * @param total_erases Pointer to store total erase operations count
 *
 * @note Pass NULL for any parameter to ignore
 */
void external_flash_get_stats(uint32_t *total_reads,
                             uint32_t *total_writes,
                             uint32_t *total_erases);

/**
 * @brief Reset flash statistics counters
 */
void external_flash_reset_stats(void);


// ========================================================================
// TEST FUNCTIONS
// ========================================================================

/**
 * @brief Run comprehensive test suite for external flash
 *
 * Performs 9 tests:
 * 1. Initialization and availability
 * 2. JEDEC ID verification
 * 3. Sector erase
 * 4. Basic write/read
 * 5. Page boundary handling
 * 6. Multi-sector erase range
 * 7. Alternating patterns (stress test)
 * 8. Large block (4KB) read/write
 * 9. Statistics tracking
 *
 * @return Number of failed tests (0 = all passed)
 *
 * @note Uses test sectors in reserved area (0x700000-0x703000)
 * @note Safe to run - will not interfere with chime data
 * @note Takes approximately 5-10 seconds to complete
 */
int external_flash_run_all_tests(void);

/**
 * @brief Quick smoke test (initialization + basic read/write)
 *
 * Fast test to verify basic flash functionality.
 * Tests: initialization, JEDEC ID, and basic write/read.
 *
 * @return true if basic functionality works, false otherwise
 *
 * @note Takes <1 second to complete
 * @note Run this after first initialization to verify hardware
 */
bool external_flash_quick_test(void);


#ifdef __cplusplus
}
#endif

#endif // EXTERNAL_FLASH_H
