/**
 * @file wordclock_mqtt_handlers.c
 * @brief MQTT command handling implementation for wordclock
 * 
 * Phase 2.1: Interface layer for MQTT command handling.
 * This module provides the API structure for centralized MQTT command processing.
 * Current implementation provides interface definitions with delegation to existing
 * mqtt_manager.c implementations. In Phase 2.2, this will contain full implementations.
 */

#include "wordclock_mqtt_handlers.h"
#include "wordclock_display.h"
#include "wordclock_brightness.h"
#include "wordclock_transitions.h"
#include "brightness_config.h"
#include "thread_safety.h"
#include "audio_manager.h"  // ESP32-S3: Built-in MAX98357A on GPIO 5/6/7
#include "filesystem_manager.h"
#include "chime_manager.h"  // ESP32-S3: Westminster chimes (Phase 2.3)
#include "mqtt_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wordclock_mqtt_handlers";

/**
 * @brief Initialize MQTT handlers interface
 */
esp_err_t mqtt_handlers_init(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING MQTT HANDLERS INTERFACE ===");
    
    // Phase 2.1: Interface layer initialization
    // Provides the module structure for Phase 2.2 implementation
    
    ESP_LOGI(TAG, "✅ MQTT handlers interface initialized (Phase 2.1)");
    return ESP_OK;
}

/**
 * @brief Handle basic wordclock commands (status, restart, etc.)
 * Phase 2.1: Implementation of basic commands that can be safely extracted
 */
esp_err_t mqtt_handle_basic_commands(const char* command)
{
    if (!command) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Processing basic command: %s", command);
    
    if (strcmp(command, "status") == 0) {
        ESP_LOGI(TAG, "Status command - delegating to existing implementation");
        return ESP_OK;
    } else if (strcmp(command, "restart") == 0) {
        ESP_LOGI(TAG, "🔄 System restart requested via MQTT");
        esp_restart();
        return ESP_OK;
    } else if (strcmp(command, "test_transitions_start") == 0) {
        return start_transition_test_mode();
    } else if (strcmp(command, "test_transitions_stop") == 0) {
        return stop_transition_test_mode();
    } else if (strcmp(command, "test_transitions_status") == 0) {
        bool active = is_transition_test_mode();
        ESP_LOGI(TAG, "Transition test mode: %s", active ? "ACTIVE" : "INACTIVE");
        return ESP_OK;
    } else if (strcmp(command, "test_audio") == 0) {
        ESP_LOGI(TAG, "🔊 Audio test tone requested via MQTT");
        esp_err_t ret = audio_play_test_tone();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Test tone playing (440Hz for 2 seconds)");
        } else {
            ESP_LOGE(TAG, "❌ Test tone failed: %s", esp_err_to_name(ret));
        }
        return ret;
    } else if (strncmp(command, "set_time ", 9) == 0) {
        // Parse time format HH:MM
        const char* time_str = command + 9;
        int hours, minutes;
        if (sscanf(time_str, "%d:%d", &hours, &minutes) == 2) {
            if (hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59) {
                wordclock_time_t test_time = {
                    .hours = hours,
                    .minutes = minutes,
                    .seconds = 0,
                    .day = 1,
                    .month = 1, 
                    .year = 24
                };
                
                // Use display function with transitions
                esp_err_t ret = display_german_time_with_transitions(&test_time);
                ESP_LOGI(TAG, "⏰ Set time to %02d:%02d", hours, minutes);
                return ret;
            }
        }
        ESP_LOGW(TAG, "Invalid time format: %s (expected HH:MM)", time_str);
        return ESP_ERR_INVALID_ARG;
    }

    // ============================================================================
    // Westminster Chime Commands (Phase 2.3)
    // ============================================================================
    else if (strcmp(command, "chimes_enable") == 0) {
        ESP_LOGI(TAG, "🔔 Enabling Westminster chimes");
        esp_err_t ret = chime_manager_enable(true);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Chimes enabled");
        } else {
            ESP_LOGE(TAG, "❌ Failed to enable chimes: %s", esp_err_to_name(ret));
        }
        return ret;
    } else if (strcmp(command, "chimes_disable") == 0) {
        ESP_LOGI(TAG, "🔕 Disabling Westminster chimes");
        esp_err_t ret = chime_manager_enable(false);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Chimes disabled");
        } else {
            ESP_LOGE(TAG, "❌ Failed to disable chimes: %s", esp_err_to_name(ret));
        }
        return ret;
    } else if (strcmp(command, "chimes_status") == 0) {
        bool enabled = chime_manager_is_enabled();
        chime_config_t config;
        if (chime_manager_get_config(&config) == ESP_OK) {
            ESP_LOGI(TAG, "🔔 Chime Status:");
            ESP_LOGI(TAG, "  Enabled: %s", enabled ? "YES" : "NO");
            ESP_LOGI(TAG, "  Quiet hours: %02d:00 - %02d:00", config.quiet_start_hour, config.quiet_end_hour);
            ESP_LOGI(TAG, "  Volume: %d%%", config.volume);
            ESP_LOGI(TAG, "  Chime set: %s", config.chime_set);
        }
        return ESP_OK;
    } else if (strcmp(command, "chimes_test_quarter") == 0) {
        ESP_LOGI(TAG, "🔔 Testing quarter-past chime");
        return chime_manager_play_test(CHIME_QUARTER_PAST);
    } else if (strcmp(command, "chimes_test_half") == 0) {
        ESP_LOGI(TAG, "🔔 Testing half-past chime");
        return chime_manager_play_test(CHIME_HALF_PAST);
    } else if (strcmp(command, "chimes_test_quarter_to") == 0) {
        ESP_LOGI(TAG, "🔔 Testing quarter-to chime");
        return chime_manager_play_test(CHIME_QUARTER_TO);
    } else if (strcmp(command, "chimes_test_hour") == 0) {
        ESP_LOGI(TAG, "🔔 Testing hour chime");
        return chime_manager_play_test(CHIME_HOUR);
    } else if (strcmp(command, "chimes_test_strike") == 0) {
        ESP_LOGI(TAG, "🔔 Testing single strike");
        return chime_manager_play_test(CHIME_TEST_SINGLE);
    } else if (strncmp(command, "chimes_quiet_hours ", 19) == 0) {
        // Parse format: chimes_quiet_hours HH:MM-HH:MM (e.g., "chimes_quiet_hours 22:00-07:00")
        const char* hours_str = command + 19;
        int start_hour, start_min, end_hour, end_min;
        if (sscanf(hours_str, "%d:%d-%d:%d", &start_hour, &start_min, &end_hour, &end_min) == 4) {
            if (start_hour >= 0 && start_hour <= 23 && end_hour >= 0 && end_hour <= 23) {
                esp_err_t ret = chime_manager_set_quiet_hours(start_hour, end_hour);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Quiet hours set: %02d:00 - %02d:00", start_hour, end_hour);
                } else {
                    ESP_LOGE(TAG, "❌ Failed to set quiet hours: %s", esp_err_to_name(ret));
                }
                return ret;
            }
        }
        ESP_LOGW(TAG, "Invalid quiet hours format: %s (expected HH:MM-HH:MM)", hours_str);
        return ESP_ERR_INVALID_ARG;
    } else if (strncmp(command, "chimes_set ", 11) == 0) {
        // Format: chimes_set <set_name> (e.g., "chimes_set westminster")
        const char* set_name = command + 11;
        esp_err_t ret = chime_manager_set_chime_set(set_name);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Chime set changed to: %s", set_name);
        } else {
            ESP_LOGE(TAG, "❌ Failed to set chime set: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    // Audio commands DISABLED (test_audio, play_audio, list_audio_files)
    // Reason: ESP32 cannot handle WiFi+MQTT+I2S concurrently
    // These will be re-enabled on ESP32-S3 hardware

    ESP_LOGW(TAG, "Unknown command: %s", command);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Handle brightness control commands
 * Phase 2.1: Interface to existing brightness control
 */
esp_err_t mqtt_handle_brightness_commands(uint8_t individual_brightness, uint8_t global_brightness)
{
    ESP_LOGI(TAG, "💡 Brightness control: individual=%d, global=%d", individual_brightness, global_brightness);
    
    esp_err_t ret = ESP_OK;
    
    if (individual_brightness > 0) {
        ret = set_individual_brightness(individual_brightness);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set individual brightness: %s", esp_err_to_name(ret));
        }
    }
    
    if (global_brightness > 0) {
        ret = set_global_brightness(global_brightness);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set global brightness: %s", esp_err_to_name(ret));
        }
    }
    
    return ret;
}

/**
 * @brief Handle transition control commands
 * Phase 2.1: Interface to existing transition control
 */
esp_err_t mqtt_handle_transition_commands(uint16_t duration_ms, const char* fadein_curve, const char* fadeout_curve, bool enable)
{
    ESP_LOGI(TAG, "🎬 Transition control: duration=%dms, enable=%s", duration_ms, enable ? "true" : "false");
    
    esp_err_t ret = ESP_OK;
    
    // Enable/disable transitions
    if (transition_system_enabled != enable) {
        transition_system_enabled = enable;
        ESP_LOGI(TAG, "Transitions %s", enable ? "enabled" : "disabled");
    }
    
    // Set duration if provided
    if (duration_ms > 0) {
        ret = set_transition_duration(duration_ms);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set transition duration: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Set curves if provided
    if (fadein_curve || fadeout_curve) {
        ret = set_transition_curves(fadein_curve, fadeout_curve);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set transition curves: %s", esp_err_to_name(ret));
        }
    }
    
    return ret;
}

// Phase 2.1 Interface Functions - Provide API structure for Phase 2.2

esp_err_t mqtt_handle_command(const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "🔧 MQTT Command Interface (Phase 2.1): %.*s", payload_len, payload);
    
    // Phase 2.1: Interface layer only - actual processing still in mqtt_manager.c
    // This provides the API structure for Phase 2.2 full implementation
    // For now, return success to maintain system compatibility
    
    return ESP_OK;
}

esp_err_t mqtt_handle_transition_control(const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "🎬 Transition Control Interface (Phase 2.1): %.*s", payload_len, payload);
    
    // Phase 2.1: Interface layer only
    // In Phase 2.2, this will parse JSON and call mqtt_handle_transition_commands()
    
    return ESP_OK;
}

esp_err_t mqtt_handle_brightness_control(const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "💡 Brightness Control Interface (Phase 2.1): %.*s", payload_len, payload);
    
    // Phase 2.1: Interface layer only
    // In Phase 2.2, this will parse JSON and call mqtt_handle_brightness_commands()
    
    return ESP_OK;
}

esp_err_t mqtt_handle_brightness_config_set(const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "⚙️ Brightness Config Set Interface (Phase 2.1)");
    
    // Phase 2.1: Interface layer only
    // In Phase 2.2, this will handle brightness configuration updates directly
    
    return ESP_OK;
}

esp_err_t mqtt_handle_brightness_config_get(const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "⚙️ Brightness Config Get Interface (Phase 2.1)");
    
    // Phase 2.1: Interface layer only
    // In Phase 2.2, this will publish brightness configuration status
    
    return ESP_OK;
}

esp_err_t mqtt_handle_brightness_config_reset(void)
{
    ESP_LOGI(TAG, "⚙️ Brightness Config Reset Interface (Phase 2.1)");
    
    // Phase 2.1: This can be implemented immediately as it's self-contained
    esp_err_t ret = brightness_config_reset_to_defaults();
    if (ret == ESP_OK) {
        ret = brightness_config_save_to_nvs();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Brightness configuration reset to defaults");
            // Phase 2.2: Will call wordclock_mqtt_publish_brightness_config_status()
        }
    }
    
    return ret;
}

// Phase 2.2 Interface Functions - Publishing functions with unique names to avoid conflicts

esp_err_t wordclock_mqtt_publish_system_status(void)
{
    ESP_LOGI(TAG, "📊 Publishing system status (Phase 2.2 interface)");
    
    // Phase 2.1: Interface placeholder
    // Phase 2.2: Will implement direct system status publishing
    
    return ESP_OK;
}

esp_err_t wordclock_mqtt_publish_sensor_status(void)
{
    ESP_LOGI(TAG, "📊 Publishing sensor status (Phase 2.2 interface)");
    
    // Phase 2.1: Interface placeholder
    // Phase 2.2: Will implement direct sensor status publishing
    
    return ESP_OK;
}

esp_err_t wordclock_mqtt_publish_heartbeat_with_ntp(void)
{
    ESP_LOGD(TAG, "💓 Publishing heartbeat with NTP status (Phase 2.2 interface)");
    
    // Phase 2.1: Interface placeholder
    // Phase 2.2: Will implement direct heartbeat publishing with NTP status
    
    return ESP_OK;
}

esp_err_t wordclock_mqtt_publish_brightness_status(uint8_t individual, uint8_t global)
{
    ESP_LOGD(TAG, "💡 Publishing brightness status (Phase 2.2 interface): individual=%d, global=%d", individual, global);
    
    // Phase 2.1: Interface placeholder
    // Phase 2.2: Will implement direct brightness status publishing
    
    return ESP_OK;
}

esp_err_t wordclock_mqtt_publish_brightness_config_status(void)
{
    ESP_LOGD(TAG, "⚙️ Publishing brightness configuration status (Phase 2.2 interface)");
    
    // Phase 2.1: Interface placeholder
    // Phase 2.2: Will implement direct brightness configuration publishing
    
    return ESP_OK;
}