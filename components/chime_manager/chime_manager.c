/**
 * @file chime_manager.c
 * @brief Westminster Chime Manager Implementation
 */

#include "chime_manager.h"
#include "audio_manager.h"
#include "sdcard_manager.h"
#include "nvs_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

static const char *TAG = "chime_mgr";

// NVS keys for configuration
#define NVS_NAMESPACE           "chime_config"
#define NVS_KEY_ENABLED         "enabled"
#define NVS_KEY_QUIET_START     "quiet_start"
#define NVS_KEY_QUIET_END       "quiet_end"
#define NVS_KEY_VOLUME          "volume"
#define NVS_KEY_CHIME_SET       "chime_set"

// State
static bool initialized = false;
static chime_config_t config;
static uint8_t last_minute_played = 255;  // Track last chime to avoid duplicates

/**
 * @brief Load configuration from NVS
 */
static esp_err_t load_config(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        // Set defaults
        config.enabled = CHIME_DEFAULT_ENABLED;
        config.quiet_start_hour = CHIME_DEFAULT_QUIET_START;
        config.quiet_end_hour = CHIME_DEFAULT_QUIET_END;
        config.volume = CHIME_DEFAULT_VOLUME;
        strncpy(config.chime_set, CHIME_DEFAULT_SET, sizeof(config.chime_set) - 1);
        config.chime_set[sizeof(config.chime_set) - 1] = '\0';
        return ESP_OK;
    }

    // Load enabled
    uint8_t enabled_val = CHIME_DEFAULT_ENABLED ? 1 : 0;
    nvs_get_u8(handle, NVS_KEY_ENABLED, &enabled_val);
    config.enabled = (enabled_val != 0);

    // Load quiet hours
    nvs_get_u8(handle, NVS_KEY_QUIET_START, &config.quiet_start_hour);
    if (config.quiet_start_hour > 23) {
        config.quiet_start_hour = CHIME_DEFAULT_QUIET_START;
    }

    nvs_get_u8(handle, NVS_KEY_QUIET_END, &config.quiet_end_hour);
    if (config.quiet_end_hour > 23) {
        config.quiet_end_hour = CHIME_DEFAULT_QUIET_END;
    }

    // Load volume
    nvs_get_u8(handle, NVS_KEY_VOLUME, &config.volume);
    if (config.volume > 100) {
        config.volume = CHIME_DEFAULT_VOLUME;
    }

    // Load chime set
    size_t required_size = sizeof(config.chime_set);
    ret = nvs_get_str(handle, NVS_KEY_CHIME_SET, config.chime_set, &required_size);
    if (ret != ESP_OK) {
        strncpy(config.chime_set, CHIME_DEFAULT_SET, sizeof(config.chime_set) - 1);
        config.chime_set[sizeof(config.chime_set) - 1] = '\0';
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "Configuration loaded from NVS");
    ESP_LOGI(TAG, "  Enabled: %s", config.enabled ? "yes" : "no");
    ESP_LOGI(TAG, "  Quiet hours: %02d:00 - %02d:00", config.quiet_start_hour, config.quiet_end_hour);
    ESP_LOGI(TAG, "  Volume: %d%%", config.volume);
    ESP_LOGI(TAG, "  Chime set: %s", config.chime_set);

    return ESP_OK;
}

/**
 * @brief Save configuration to NVS
 */
esp_err_t chime_manager_save_config(void) {
    if (!initialized) {
        ESP_LOGW(TAG, "Cannot save config: not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "💾 Saving configuration to NVS namespace '%s'...", NVS_NAMESPACE);
    ESP_LOGI(TAG, "   enabled=%d, volume=%d, quiet=%d-%d, set=%s",
             config.enabled, config.volume, config.quiet_start_hour, config.quiet_end_hour, config.chime_set);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open NVS for writing: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    ESP_LOGI(TAG, "✅ NVS handle opened successfully");

    // Save all config values with individual error checking
    uint8_t enabled_val = config.enabled ? 1 : 0;
    ret = nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save enabled: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    ESP_LOGI(TAG, "   Saved enabled=%d", enabled_val);

    ret = nvs_set_u8(handle, NVS_KEY_QUIET_START, config.quiet_start_hour);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save quiet_start: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    ret = nvs_set_u8(handle, NVS_KEY_QUIET_END, config.quiet_end_hour);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save quiet_end: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    ret = nvs_set_u8(handle, NVS_KEY_VOLUME, config.volume);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save volume: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    ESP_LOGI(TAG, "   Saved volume=%d", config.volume);

    ret = nvs_set_str(handle, NVS_KEY_CHIME_SET, config.chime_set);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save chime_set: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    ESP_LOGI(TAG, "   All values written to NVS, committing...");

    // Commit changes
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to commit NVS: %s (0x%x)", esp_err_to_name(ret), ret);
        nvs_close(handle);
        return ret;
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "✅ Configuration saved to NVS successfully!");

    return ESP_OK;
}

/**
 * @brief Get filename for chime type
 */
static const char* get_chime_filename(chime_type_t type) {
    switch (type) {
        case CHIME_QUARTER_PAST:
            return CHIME_QUARTER_PAST_FILE;
        case CHIME_HALF_PAST:
            return CHIME_HALF_PAST_FILE;
        case CHIME_QUARTER_TO:
            return CHIME_QUARTER_TO_FILE;
        case CHIME_HOUR:
            return CHIME_HOUR_FILE;
        case CHIME_TEST_SINGLE:
            return CHIME_STRIKE_FILE;
        default:
            return NULL;
    }
}

/**
 * @brief Check if current hour is in quiet period
 */
static bool is_quiet_hour(uint8_t hour) {
    if (config.quiet_start_hour == config.quiet_end_hour) {
        return false;  // No quiet hours set
    }

    if (config.quiet_start_hour < config.quiet_end_hour) {
        // Normal case: e.g., 22:00 - 07:00 (overnight)
        return (hour >= config.quiet_start_hour && hour < config.quiet_end_hour);
    } else {
        // Wrapped case: e.g., 07:00 - 22:00 (daytime quiet - unusual but supported)
        return (hour >= config.quiet_start_hour || hour < config.quiet_end_hour);
    }
}

/**
 * @brief Play chime sequence (internal)
 */
static esp_err_t play_chime_internal(chime_type_t type, uint8_t hour) {
    char filepath[128];
    const char *filename = get_chime_filename(type);

    if (!filename) {
        ESP_LOGE(TAG, "Invalid chime type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    // Build full path: /sdcard/CHIMES/WESTMI~1/QUARTE~1.PCM (8.3 format)
    snprintf(filepath, sizeof(filepath), "%s/%s/%s",
             CHIME_BASE_PATH, CHIME_DIR_NAME, filename);

    ESP_LOGI(TAG, "🔔 Playing chime: %s (volume: %d%%)", filepath, config.volume);

    // Check if file exists
    if (!sdcard_file_exists(filepath)) {
        ESP_LOGE(TAG, "❌ Chime file not found: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Set volume before playback
    audio_set_volume(config.volume);

    // Play main chime sequence
    esp_err_t ret = audio_play_from_file(filepath);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to play chime: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ Chime playback started");

    // If hour chime, play hour strikes after main sequence
    if (type == CHIME_HOUR) {
        uint8_t strikes = (hour % 12 == 0) ? 12 : (hour % 12);
        ESP_LOGI(TAG, "🕐 Playing %d hour strike(s)", strikes);

        // Build strike file path (use CHIME_DIR_NAME for 8.3 format)
        snprintf(filepath, sizeof(filepath), "%s/%s/%s",
                 CHIME_BASE_PATH, CHIME_DIR_NAME, CHIME_STRIKE_FILE);

        // Check if strike file exists
        if (!sdcard_file_exists(filepath)) {
            ESP_LOGW(TAG, "⚠️  Strike file not found: %s (skipping hour strikes)", filepath);
            return ESP_OK;  // Main chime played successfully
        }

        // Wait for main chime to finish
        // TODO: Implement audio_is_playing() check
        // For now, use simple delay based on typical chime duration
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5 seconds pause before hour strikes

        // Play strikes with gaps
        for (uint8_t i = 0; i < strikes; i++) {
            ret = audio_play_from_file(filepath);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "⚠️  Failed to play strike %d/%d", i + 1, strikes);
                continue;
            }

            // Gap between strikes (except after last)
            if (i < strikes - 1) {
                vTaskDelay(pdMS_TO_TICKS(2000));  // 2 second gap
            }
        }

        ESP_LOGI(TAG, "✅ Hour strikes complete");
    }

    return ESP_OK;
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t chime_manager_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Chime manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing Westminster chime manager");

    // Load configuration from NVS
    esp_err_t ret = load_config();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
    }

    // Initialize state
    last_minute_played = 255;

    initialized = true;
    ESP_LOGI(TAG, "✅ Chime manager initialized");

    return ESP_OK;
}

esp_err_t chime_manager_deinit(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing chime manager");

    // Save configuration
    chime_manager_save_config();

    initialized = false;
    ESP_LOGI(TAG, "✅ Chime manager deinitialized");

    return ESP_OK;
}

esp_err_t chime_manager_enable(bool enabled) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (config.enabled != enabled) {
        config.enabled = enabled;
        ESP_LOGI(TAG, "Chimes %s", enabled ? "ENABLED" : "DISABLED");

        // Save to NVS
        chime_manager_save_config();
    }

    return ESP_OK;
}

bool chime_manager_is_enabled(void) {
    return initialized && config.enabled;
}

esp_err_t chime_manager_set_quiet_hours(uint8_t start_hour, uint8_t end_hour) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (start_hour > 23 || end_hour > 23) {
        ESP_LOGE(TAG, "Invalid quiet hours: %d-%d (must be 0-23)", start_hour, end_hour);
        return ESP_ERR_INVALID_ARG;
    }

    config.quiet_start_hour = start_hour;
    config.quiet_end_hour = end_hour;

    ESP_LOGI(TAG, "Quiet hours set: %02d:00 - %02d:00", start_hour, end_hour);

    // Save to NVS
    chime_manager_save_config();

    return ESP_OK;
}

esp_err_t chime_manager_get_quiet_hours(uint8_t *start_hour, uint8_t *end_hour) {
    if (!initialized || !start_hour || !end_hour) {
        return ESP_ERR_INVALID_ARG;
    }

    *start_hour = config.quiet_start_hour;
    *end_hour = config.quiet_end_hour;

    return ESP_OK;
}

esp_err_t chime_manager_set_chime_set(const char *chime_set) {
    if (!initialized || !chime_set) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(chime_set) >= sizeof(config.chime_set)) {
        ESP_LOGE(TAG, "Chime set name too long (max %zu chars)", sizeof(config.chime_set) - 1);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(config.chime_set, chime_set, sizeof(config.chime_set) - 1);
    config.chime_set[sizeof(config.chime_set) - 1] = '\0';

    ESP_LOGI(TAG, "Chime set changed to: %s", config.chime_set);

    // Save to NVS
    chime_manager_save_config();

    return ESP_OK;
}

const char* chime_manager_get_chime_set(void) {
    return initialized ? config.chime_set : CHIME_DEFAULT_SET;
}

esp_err_t chime_manager_set_volume(uint8_t volume) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (volume > 100) {
        ESP_LOGE(TAG, "Invalid volume: %d (must be 0-100)", volume);
        return ESP_ERR_INVALID_ARG;
    }

    config.volume = volume;
    ESP_LOGI(TAG, "Chime volume set to: %d%%", volume);

    // Apply volume to audio manager
    esp_err_t ret = audio_set_volume(volume);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set audio volume: %s", esp_err_to_name(ret));
    }

    // Save to NVS
    chime_manager_save_config();

    return ESP_OK;
}

uint8_t chime_manager_get_volume(void) {
    return initialized ? config.volume : CHIME_DEFAULT_VOLUME;
}

esp_err_t chime_manager_check_time(void) {
    // Get current time first for logging
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    uint8_t minute = timeinfo.tm_min;
    uint8_t hour = timeinfo.tm_hour;

    // ALWAYS log this to verify function is being called (every loop)
    if (minute % 15 == 0) {  // Only log on quarter hours to reduce spam
        ESP_LOGI(TAG, "🔍 check_time: %02d:%02d (init=%d, enabled=%d)",
                 hour, minute, initialized, config.enabled);
    }

    if (!initialized) {
        ESP_LOGW(TAG, "❌ check_time: not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if chimes enabled
    if (!config.enabled) {
        return ESP_OK;  // Not an error, just disabled
    }

    // Check if this is a chime minute (:00, :15, :30, :45)
    if (minute % 15 != 0) {
        return ESP_OK;  // Not a chime minute
    }

    ESP_LOGI(TAG, "🕐 Quarter-hour detected: %02d:%02d (enabled=true)", hour, minute);

    // Prevent duplicate chimes in same minute
    if (minute == last_minute_played) {
        ESP_LOGI(TAG, "⏭️  Already played chime for minute %d, skipping", minute);
        return ESP_OK;  // Already played this minute
    }

    // Check quiet hours
    if (is_quiet_hour(hour)) {
        ESP_LOGI(TAG, "🔇 Chime suppressed (quiet hours: %02d:00-%02d:00)",
                 config.quiet_start_hour, config.quiet_end_hour);
        last_minute_played = minute;
        return ESP_OK;
    }

    // Determine chime type
    chime_type_t type;
    switch (minute) {
        case 0:
            type = CHIME_HOUR;
            break;
        case 15:
            type = CHIME_QUARTER_PAST;
            break;
        case 30:
            type = CHIME_HALF_PAST;
            break;
        case 45:
            type = CHIME_QUARTER_TO;
            break;
        default:
            return ESP_OK;  // Shouldn't happen
    }

    ESP_LOGI(TAG, "🕐 Time for chime: %02d:%02d (type: %d)", hour, minute, type);

    // Play chime
    esp_err_t ret = play_chime_internal(type, hour);

    // Mark as played (even if failed, to avoid rapid retries)
    last_minute_played = minute;

    return ret;
}

esp_err_t chime_manager_play_test(chime_type_t type) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (type < 0 || type >= CHIME_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "🧪 Manual test chime requested (type: %d)", type);

    // Get current hour for strike count (if testing hour chime)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    return play_chime_internal(type, timeinfo.tm_hour);
}

esp_err_t chime_manager_get_config(chime_config_t *cfg) {
    if (!initialized || !cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(cfg, &config, sizeof(chime_config_t));
    return ESP_OK;
}

esp_err_t chime_manager_reset_config(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Resetting configuration to defaults");

    config.enabled = CHIME_DEFAULT_ENABLED;
    config.quiet_start_hour = CHIME_DEFAULT_QUIET_START;
    config.quiet_end_hour = CHIME_DEFAULT_QUIET_END;
    config.volume = CHIME_DEFAULT_VOLUME;
    strncpy(config.chime_set, CHIME_DEFAULT_SET, sizeof(config.chime_set) - 1);
    config.chime_set[sizeof(config.chime_set) - 1] = '\0';

    // Save to NVS
    return chime_manager_save_config();
}
