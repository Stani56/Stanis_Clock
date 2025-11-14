/**
 * @file ota_manager.h
 * @brief OTA (Over-The-Air) Update Manager for ESP32-S3 Word Clock
 *
 * Provides secure firmware updates via HTTPS from GitHub Releases.
 * Features automatic rollback on boot failures and health check validation.
 *
 * @author Claude Code
 * @date 2025-11-07
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA firmware source selection
 */
typedef enum {
    OTA_SOURCE_GITHUB = 0,        ///< GitHub raw URLs (default)
    OTA_SOURCE_CLOUDFLARE = 1     ///< Cloudflare R2 CDN
} ota_source_t;

/**
 * @brief OTA update states
 */
typedef enum {
    OTA_STATE_IDLE = 0,           ///< No update in progress
    OTA_STATE_CHECKING,           ///< Checking for available updates
    OTA_STATE_DOWNLOADING,        ///< Downloading firmware from server
    OTA_STATE_VERIFYING,          ///< Verifying downloaded firmware
    OTA_STATE_FLASHING,           ///< Writing firmware to flash
    OTA_STATE_COMPLETE,           ///< Update complete, ready to reboot
    OTA_STATE_FAILED              ///< Update failed
} ota_state_t;

/**
 * @brief OTA error codes
 */
typedef enum {
    OTA_ERROR_NONE = 0,           ///< No error
    OTA_ERROR_NO_INTERNET,        ///< Cannot connect to update server
    OTA_ERROR_DOWNLOAD_FAILED,    ///< Download interrupted or failed
    OTA_ERROR_CHECKSUM_MISMATCH,  ///< Firmware checksum verification failed
    OTA_ERROR_FLASH_FAILED,       ///< Failed to write to flash
    OTA_ERROR_NO_UPDATE_PARTITION,///< No OTA partition available
    OTA_ERROR_INVALID_VERSION,    ///< New version is not newer
    OTA_ERROR_LOW_MEMORY,         ///< Not enough free memory
    OTA_ERROR_ALREADY_RUNNING     ///< Update already in progress
} ota_error_t;

/**
 * @brief OTA progress information
 */
typedef struct {
    ota_state_t state;            ///< Current OTA state
    ota_error_t error;            ///< Last error (if state is FAILED)
    uint8_t progress_percent;     ///< Download/flash progress (0-100%)
    uint32_t bytes_downloaded;    ///< Bytes downloaded so far
    uint32_t total_bytes;         ///< Total firmware size in bytes
    uint32_t time_elapsed_ms;     ///< Time elapsed since start (milliseconds)
    uint32_t time_remaining_ms;   ///< Estimated time remaining (milliseconds)
} ota_progress_t;

/**
 * @brief Firmware version information
 */
typedef struct {
    char version[32];             ///< Version string (e.g., "2.11.0")
    char binary_hash[9];          ///< First 8 chars of binary SHA-256 + null terminator
    char build_date[16];          ///< Build date (e.g., "2025-11-07")
    char idf_version[16];         ///< ESP-IDF version (e.g., "5.4.2")
    uint32_t size_bytes;          ///< Firmware size in bytes
    char sha256[65];              ///< SHA-256 checksum (64 hex chars + null terminator)
} firmware_version_t;

/**
 * @brief OTA progress callback function type
 *
 * Called periodically during download to report progress.
 * Can be used to publish progress to MQTT, update UI, etc.
 *
 * @param progress Current progress information
 */
typedef void (*ota_progress_callback_t)(const ota_progress_t *progress);

/**
 * @brief OTA update configuration
 */
typedef struct {
    const char *firmware_url;     ///< URL to download firmware from
    const char *version_url;      ///< URL to check version info (optional)
    bool auto_reboot;             ///< Automatically reboot after successful update
    uint32_t timeout_ms;          ///< Download timeout in milliseconds
    bool skip_version_check;      ///< Skip version comparison (force update)
    ota_progress_callback_t progress_callback;  ///< Optional progress callback (called every ~5%)
} ota_config_t;

// OTA Source URLs - Dual source support with automatic failover
// GitHub URLs (primary source - automatically updated on git push)
#define OTA_GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin"
#define OTA_GITHUB_VERSION_URL  "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json"

// Cloudflare R2 URLs (backup source - manual upload required)
#define OTA_CLOUDFLARE_FIRMWARE_URL "https://pub-3ef23ec5bae14629b323217785443321.r2.dev/wordclock.bin"
#define OTA_CLOUDFLARE_VERSION_URL  "https://pub-3ef23ec5bae14629b323217785443321.r2.dev/version.json"

// Default configuration
#define OTA_DEFAULT_SOURCE       OTA_SOURCE_GITHUB  // Default to GitHub
#define OTA_DEFAULT_TIMEOUT_MS   120000             // 2 minutes

/**
 * @brief Initialize OTA manager
 *
 * Must be called before any other OTA functions.
 * Checks partition table for OTA support.
 *
 * @return ESP_OK on success
 *         ESP_ERR_NOT_FOUND if no OTA partitions found
 *         ESP_FAIL on initialization failure
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Check if OTA partitions are available
 *
 * @return true if OTA is supported (partitions exist)
 *         false if OTA is not supported
 */
bool ota_manager_is_available(void);

/**
 * @brief Get current firmware version
 *
 * Reads version information from currently running firmware.
 *
 * @param[out] version Pointer to version structure to fill
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if version is NULL
 */
esp_err_t ota_get_current_version(firmware_version_t *version);

/**
 * @brief Check for available firmware updates
 *
 * Downloads version information from server and compares with current version.
 * Non-blocking - returns immediately with result.
 *
 * @param[out] available_version Pointer to version structure to fill with available version
 * @return ESP_OK if newer version available
 *         ESP_ERR_NOT_FOUND if no update available (already latest)
 *         ESP_FAIL on error checking for updates
 */
esp_err_t ota_check_for_updates(firmware_version_t *available_version);

/**
 * @brief Start OTA update with custom configuration
 *
 * Downloads and flashes new firmware. This is a blocking operation
 * that runs in a separate task. Use ota_get_progress() to monitor.
 *
 * @param config OTA configuration (NULL for defaults)
 * @return ESP_OK if update started successfully
 *         ESP_ERR_INVALID_STATE if update already in progress
 *         ESP_ERR_NO_MEM if not enough free memory
 *         ESP_FAIL on other errors
 */
esp_err_t ota_start_update(const ota_config_t *config);

/**
 * @brief Start OTA update with default settings
 *
 * Convenience function that uses default GitHub URL and settings.
 * Equivalent to ota_start_update(NULL).
 *
 * @return ESP_OK if update started successfully
 *         ESP_ERR_INVALID_STATE if update already in progress
 *         ESP_FAIL on error
 */
esp_err_t ota_start_update_default(void);

/**
 * @brief Get current OTA progress
 *
 * Can be called during an update to monitor progress.
 *
 * @param[out] progress Pointer to progress structure to fill
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if progress is NULL
 */
esp_err_t ota_get_progress(ota_progress_t *progress);

/**
 * @brief Get current OTA state
 *
 * @return Current OTA state
 */
ota_state_t ota_get_state(void);

/**
 * @brief Cancel ongoing OTA update
 *
 * Stops download and cleans up. Safe to call even if no update is running.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_cancel_update(void);

/**
 * @brief Check if this is first boot after OTA update
 *
 * Returns true if this is the first boot after an OTA update.
 * Used to trigger health checks and validation.
 *
 * @return true if first boot after OTA
 *         false otherwise
 */
bool ota_is_first_boot_after_update(void);

/**
 * @brief Mark current firmware as valid
 *
 * Call this after successful health checks on first boot after OTA.
 * Prevents automatic rollback on next reboot.
 *
 * IMPORTANT: Must be called after validating:
 * - WiFi connection works
 * - MQTT connection works
 * - I2C devices responding
 * - LEDs functioning
 *
 * @return ESP_OK on success
 *         ESP_FAIL on error
 */
esp_err_t ota_mark_app_valid(void);

/**
 * @brief Trigger rollback to previous firmware
 *
 * Forces rollback to previous partition on next reboot.
 * Use if health checks fail after OTA update.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_trigger_rollback(void);

/**
 * @brief Get name of partition currently running from
 *
 * @return Partition name (e.g., "factory", "ota_0")
 */
const char* ota_get_running_partition_name(void);

/**
 * @brief Get boot count since last successful update
 *
 * Used to detect boot loops. If boot count > 3, firmware may be broken.
 *
 * @return Boot count (1 on first boot, 2 on second, etc.)
 */
uint32_t ota_get_boot_count(void);

/**
 * @brief Perform system health checks
 *
 * Checks critical system components:
 * - WiFi connection
 * - MQTT connection
 * - I2C bus
 * - LED controllers
 * - RTC
 *
 * @return ESP_OK if all checks pass
 *         ESP_FAIL if any check fails
 */
esp_err_t ota_perform_health_checks(void);

/**
 * @brief Get human-readable error string
 *
 * @param error OTA error code
 * @return Error description string
 */
const char* ota_error_to_string(ota_error_t error);

/**
 * @brief Set preferred OTA firmware source
 *
 * Stores the preferred source in NVS. System will use this source
 * by default, with automatic failover to alternate source if download fails.
 *
 * @param source OTA_SOURCE_GITHUB or OTA_SOURCE_CLOUDFLARE
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if source is invalid
 *         ESP_FAIL on NVS write error
 */
esp_err_t ota_set_source(ota_source_t source);

/**
 * @brief Get preferred OTA firmware source
 *
 * Reads the stored preference from NVS.
 * Returns OTA_DEFAULT_SOURCE if no preference is stored.
 *
 * @return Current preferred OTA source
 */
ota_source_t ota_get_source(void);

/**
 * @brief Get OTA source as string
 *
 * Converts ota_source_t enum to human-readable string.
 *
 * @param source OTA source enum value
 * @return "github" or "cloudflare"
 */
const char* ota_source_to_string(ota_source_t source);

/**
 * @brief Parse OTA source from string
 *
 * Converts string to ota_source_t enum.
 *
 * @param source_str String "github" or "cloudflare"
 * @param[out] source Pointer to store parsed source
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if string is invalid
 */
esp_err_t ota_source_from_string(const char *source_str, ota_source_t *source);

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H
