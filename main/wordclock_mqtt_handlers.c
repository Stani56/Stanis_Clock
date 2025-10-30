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
#include "audio_manager.h"
#include "filesystem_manager.h"
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
    } else if (strcmp(command, "test_audio") == 0) {
        ESP_LOGI(TAG, "🔊 Test audio command received - playing 440Hz tone");
        esp_err_t ret = audio_play_test_tone();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Test tone playback started (2 seconds)");
            mqtt_publish_status("audio_test_tone_playing");
        } else {
            ESP_LOGE(TAG, "❌ Test tone playback failed: %s", esp_err_to_name(ret));
            mqtt_publish_status("audio_test_tone_failed");
        }
        return ret;
    } else if (strncmp(command, "play_audio:", 11) == 0) {
        // Extract filename from command (format: "play_audio:/storage/chimes/quarter.pcm")
        const char *filepath = command + 11;
        ESP_LOGI(TAG, "🔊 Play audio command received: %s", filepath);

        // Check if file exists
        if (filesystem_file_exists(filepath)) {
            esp_err_t ret = audio_play_from_file(filepath);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Audio playback started: %s", filepath);
                mqtt_publish_status("audio_playback_started");
            } else {
                ESP_LOGE(TAG, "❌ Audio playback failed: %s", esp_err_to_name(ret));
                mqtt_publish_status("audio_playback_failed");
            }
            return ret;
        } else {
            ESP_LOGW(TAG, "Audio file not found: %s", filepath);
            mqtt_publish_status("audio_file_not_found");
            return ESP_ERR_NOT_FOUND;
        }
    } else if (strcmp(command, "list_audio_files") == 0) {
        ESP_LOGI(TAG, "📁 List audio files command received");

        // List files in /storage/chimes directory
        filesystem_file_info_t file_list[20];
        size_t file_count = 0;

        esp_err_t ret = filesystem_list_files("/storage/chimes", file_list, 20, &file_count);
        if (ret == ESP_OK && file_count > 0) {
            ESP_LOGI(TAG, "Found %zu audio files:", file_count);
            for (size_t i = 0; i < file_count; i++) {
                ESP_LOGI(TAG, "  [%zu] %s (%zu bytes)", i, file_list[i].name, file_list[i].size);
            }

            // Log file count to MQTT
            char file_count_str[64];
            snprintf(file_count_str, sizeof(file_count_str), "audio_files_found_count_%zu", file_count);
            mqtt_publish_status(file_count_str);
        } else {
            ESP_LOGW(TAG, "No audio files found or error reading directory");
            mqtt_publish_status("audio_files_none_found");
        }
        return ret;
    }

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