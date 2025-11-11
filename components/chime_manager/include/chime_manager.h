/**
 * @file chime_manager.h
 * @brief Westminster Chime Manager for ESP32 Word Clock
 *
 * Manages automatic Westminster quarter-hour chime playback based on RTC time.
 * Supports quiet hours, multiple chime sets, and MQTT control.
 */

#ifndef CHIME_MANAGER_H
#define CHIME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Chime types for Westminster quarter-hour system
 */
typedef enum {
    CHIME_QUARTER_PAST,     // :15 - 4 notes (E♭-D♭-C-B♭)
    CHIME_HALF_PAST,        // :30 - 8 notes
    CHIME_QUARTER_TO,       // :45 - 12 notes
    CHIME_HOUR,             // :00 - 16 notes + hour strikes
    CHIME_TEST_SINGLE,      // Test: single bell strike
    CHIME_TYPE_MAX
} chime_type_t;

/**
 * @brief Chime configuration stored in NVS
 */
typedef struct {
    bool enabled;                   // Master enable/disable
    uint8_t quiet_start_hour;       // Quiet hours start (0-23, e.g., 22 = 10 PM)
    uint8_t quiet_end_hour;         // Quiet hours end (0-23, e.g., 7 = 7 AM)
    uint8_t volume;                 // Volume 0-100 (future: volume control)
    char chime_set[32];             // Chime set name ("westminster", "bigben", etc.)
} chime_config_t;

/**
 * @brief Default configuration values
 */
#define CHIME_DEFAULT_ENABLED       false
#define CHIME_DEFAULT_QUIET_START   22      // 10 PM
#define CHIME_DEFAULT_QUIET_END     7       // 7 AM
#define CHIME_DEFAULT_VOLUME        80      // 80%
#define CHIME_DEFAULT_SET           "WESTMINSTER"  // Uppercase by convention

/**
 * @brief SD card chime file paths
 * Note: Using long filenames with UTF-8 encoding support
 * FAT32 is case-insensitive but case-preserving (UPPERCASE used by convention)
 */
#define CHIME_BASE_PATH             "/sdcard/CHIMES"
#define CHIME_DIR_NAME              "WESTMINSTER"         // Long filename support
#define CHIME_QUARTER_PAST_FILE     "QUARTER_PAST.PCM"   // Long filename
#define CHIME_HALF_PAST_FILE        "HALF_PAST.PCM"      // Long filename
#define CHIME_QUARTER_TO_FILE       "QUARTER_TO.PCM"     // Long filename
#define CHIME_HOUR_FILE             "HOUR.PCM"
#define CHIME_STRIKE_FILE           "STRIKE.PCM"

/**
 * @brief Initialize chime manager
 *
 * Loads configuration from NVS, initializes state tracking.
 * Does NOT start automatic chiming - call chime_manager_enable(true) to start.
 *
 * @return ESP_OK on success
 *         ESP_FAIL on error
 */
esp_err_t chime_manager_init(void);

/**
 * @brief Deinitialize chime manager
 *
 * Saves configuration to NVS, stops any active playback.
 *
 * @return ESP_OK on success
 */
esp_err_t chime_manager_deinit(void);

/**
 * @brief Enable or disable automatic chiming
 *
 * @param enabled true to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t chime_manager_enable(bool enabled);

/**
 * @brief Check if chimes are enabled
 *
 * @return true if enabled, false otherwise
 */
bool chime_manager_is_enabled(void);

/**
 * @brief Set quiet hours (chimes suppressed during this period)
 *
 * @param start_hour Starting hour (0-23)
 * @param end_hour Ending hour (0-23)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if hours out of range
 */
esp_err_t chime_manager_set_quiet_hours(uint8_t start_hour, uint8_t end_hour);

/**
 * @brief Get current quiet hours setting
 *
 * @param start_hour Pointer to store start hour
 * @param end_hour Pointer to store end hour
 * @return ESP_OK on success
 */
esp_err_t chime_manager_get_quiet_hours(uint8_t *start_hour, uint8_t *end_hour);

/**
 * @brief Set chime set (e.g., "westminster", "bigben")
 *
 * Changes which subdirectory in /sdcard/chimes/ is used.
 *
 * @param chime_set Name of chime set (max 31 chars)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if name too long
 */
esp_err_t chime_manager_set_chime_set(const char *chime_set);

/**
 * @brief Get current chime set name
 *
 * @return Pointer to chime set name string (do not free)
 */
const char* chime_manager_get_chime_set(void);

/**
 * @brief Set chime volume
 *
 * Controls software volume scaling for chime audio playback.
 * Note: This is in addition to hardware gain set on MAX98357A.
 *
 * @param volume Volume level (0-100%)
 *               0 = mute, 100 = full scale
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if volume out of range
 */
esp_err_t chime_manager_set_volume(uint8_t volume);

/**
 * @brief Get current chime volume
 *
 * @return Current volume level (0-100%)
 */
uint8_t chime_manager_get_volume(void);

/**
 * @brief Check time and play chime if appropriate
 *
 * Called every minute by main loop. Checks if:
 * - Chimes enabled
 * - Current minute is :00, :15, :30, or :45
 * - Not in quiet hours
 * - Plays appropriate chime sequence
 *
 * @return ESP_OK if chime played or check completed
 *         ESP_FAIL if error occurred
 */
esp_err_t chime_manager_check_time(void);

/**
 * @brief Manually play a specific chime (for testing)
 *
 * Ignores enabled/quiet hours settings. Useful for MQTT test commands.
 *
 * @param type Chime type to play
 * @return ESP_OK on success
 *         ESP_ERR_NOT_FOUND if chime file missing
 *         ESP_FAIL on playback error
 */
esp_err_t chime_manager_play_test(chime_type_t type);

/**
 * @brief Get current configuration
 *
 * @param config Pointer to store configuration
 * @return ESP_OK on success
 */
esp_err_t chime_manager_get_config(chime_config_t *config);

/**
 * @brief Reset configuration to defaults
 *
 * Resets all settings to default values and saves to NVS.
 *
 * @return ESP_OK on success
 */
esp_err_t chime_manager_reset_config(void);

/**
 * @brief Save current configuration to NVS
 *
 * Normally called automatically, but can be called manually.
 *
 * @return ESP_OK on success
 */
esp_err_t chime_manager_save_config(void);

#ifdef __cplusplus
}
#endif

#endif // CHIME_MANAGER_H
