/**
 * @file sdcard_manager.h
 * @brief SD Card Manager for ESP32-S3 Audio Storage
 *
 * Manages SPI SD card interface for storing Westminster chime audio files.
 * Provides file system operations for audio streaming.
 *
 * Hardware Connections (ESP32-S3 DevKitC-1):
 * - GPIO 10 → CS (Chip Select)
 * - GPIO 11 → MOSI (Master Out Slave In)
 * - GPIO 12 → CLK (Clock)
 * - GPIO 13 → MISO (Master In Slave Out)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD Card Configuration
 */
#define SDCARD_MOUNT_POINT      "/sdcard"
#define SDCARD_MAX_FILES        20

/**
 * @brief GPIO Pin Definitions for SPI SD Card
 */
#define SDCARD_GPIO_MOSI        11     // SPI MOSI
#define SDCARD_GPIO_MISO        13     // SPI MISO
#define SDCARD_GPIO_CLK         12     // SPI Clock
#define SDCARD_GPIO_CS          10     // Chip Select

/**
 * @brief Initialize SD card and mount FAT filesystem
 *
 * Initializes SPI interface and mounts SD card at /sdcard.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Already initialized
 *      - ESP_FAIL: SD card not found or mount failed
 */
esp_err_t sdcard_manager_init(void);

/**
 * @brief Deinitialize SD card and unmount filesystem
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t sdcard_manager_deinit(void);

/**
 * @brief Check if a file exists on SD card
 *
 * @param filepath Full path to file (e.g., "/sdcard/chimes/quarter.pcm")
 *
 * @return
 *      - true: File exists
 *      - false: File does not exist or SD card not initialized
 */
bool sdcard_file_exists(const char *filepath);

/**
 * @brief Get file size in bytes
 *
 * @param filepath Full path to file
 * @param[out] size Pointer to store file size
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid filepath or size pointer
 *      - ESP_ERR_NOT_FOUND: File not found
 *      - ESP_ERR_INVALID_STATE: SD card not initialized
 */
esp_err_t sdcard_get_file_size(const char *filepath, size_t *size);

/**
 * @brief List files in a directory
 *
 * @param dirpath Directory path (e.g., "/sdcard/chimes")
 * @param[out] files Array to store filenames (caller must free)
 * @param[out] count Number of files found
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NOT_FOUND: Directory not found
 *      - ESP_ERR_NO_MEM: Failed to allocate memory
 *      - ESP_ERR_INVALID_STATE: SD card not initialized
 */
esp_err_t sdcard_list_files(const char *dirpath, char ***files, size_t *count);

/**
 * @brief Open file for reading
 *
 * @param filepath Full path to file
 *
 * @return
 *      - FILE*: File handle on success
 *      - NULL: Failed to open file
 */
FILE* sdcard_open_file(const char *filepath);

/**
 * @brief Close file
 *
 * @param file File handle from sdcard_open_file()
 */
void sdcard_close_file(FILE *file);

/**
 * @brief Check if SD card is initialized
 *
 * @return
 *      - true: SD card initialized and mounted
 *      - false: SD card not initialized
 */
bool sdcard_is_initialized(void);

/**
 * @brief Get SD card information
 *
 * @param[out] total_bytes Total card capacity in bytes
 * @param[out] used_bytes Used space in bytes
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: SD card not initialized
 */
esp_err_t sdcard_get_info(uint64_t *total_bytes, uint64_t *used_bytes);

#ifdef __cplusplus
}
#endif
