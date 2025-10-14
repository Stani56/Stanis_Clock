/**
 * @file wordclock.c
 * @brief Main application for ESP32 German Word Clock with IoT integration
 * 
 * This is the refactored main application that uses modular components for
 * display, brightness, and transition control. The system features:
 * - German time display with proper grammar
 * - Dual brightness control (potentiometer + light sensor)
 * - Smooth LED transitions
 * - Complete IoT integration (WiFi, NTP, MQTT, Home Assistant)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_clk_tree.h"
#include "esp_heap_caps.h"
#include "soc/rtc.h"

// Hardware components
#include "i2c_devices.h"
#include "wordclock_time.h"
#include "adc_manager.h"
#include "light_sensor.h"
#include "button_manager.h"
#include "status_led_manager.h"

// Refactored modules
#include "wordclock_display.h"
#include "wordclock_brightness.h"
#include "wordclock_transitions.h"

// Network components
#include "nvs_manager.h"
#include "wifi_manager.h"
#include "ntp_manager.h"
#include "mqtt_manager.h"
#include "web_server.h"

// Advanced features
#include "brightness_config.h"
#include "mqtt_discovery.h"
#include "thread_safety.h"
#include "wordclock_mqtt_handlers.h"
#include "led_validation.h"

static const char *TAG = "wordclock";

// CPU frequency for performance monitoring
static uint32_t cpu_freq_mhz = 0;

// System test functions
#ifdef CONFIG_RUN_SYSTEM_TESTS
static void test_all_systems(void);
#endif
static void test_live_word_clock(void);

// Helper function to get time from RTC
static esp_err_t wordclock_time_get(wordclock_time_t *time)
{
    return ds3231_get_time_struct(time);
}

// Helper function to sync RTC with NTP
static esp_err_t ntp_manager_sync_rtc(void)
{
    return ntp_sync_to_rtc();
}

/**
 * @brief Initialize hardware subsystems
 */
static esp_err_t initialize_hardware(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING HARDWARE ===");
    
    // Initialize I2C buses and devices
    esp_err_t ret = i2c_devices_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C devices");
        return ret;
    }
    
    // Scan I2C buses for diagnostics
    i2c_scan_bus(I2C_LEDS_MASTER_PORT);
    i2c_scan_bus(I2C_SENSORS_MASTER_PORT);
    
    // Initialize display system and clear all LEDs
    ret = wordclock_display_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Display initialization failed - continuing with degraded display");
    }

    // Run TLC diagnostic test on suspicious devices
    ESP_LOGI(TAG, "Running TLC diagnostic test...");
    ret = tlc_diagnostic_test();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️  TLC diagnostic test detected readback issues");
        ESP_LOGW(TAG, "Validation may report false positives - display writes are working correctly");
    } else {
        ESP_LOGI(TAG, "✅ TLC diagnostic test passed - readback verified");
    }

    // Initialize brightness control (ADC + light sensor)
    ret = brightness_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize brightness control");
        return ret;
    }
    
    // Initialize transition system
    ret = transitions_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize transitions - continuing with instant mode");
    }
    
    // Initialize button manager
    ret = button_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Button manager init failed - continuing without button control");
    }
    
    // Initialize status LED manager
    ret = status_led_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Status LED manager init failed - continuing without status LEDs");
    }
    
    // Start status LED monitoring task
    ret = status_led_start_task();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Status LED task start failed");
    }
    
    ESP_LOGI(TAG, "✅ Hardware initialization complete");
    return ESP_OK;
}

/**
 * @brief Initialize network subsystems
 */
static esp_err_t initialize_network(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING NETWORK ===");
    
    // Initialize NVS for configuration storage
    esp_err_t ret = nvs_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS");
        return ret;
    }
    
    // Initialize brightness configuration
    ret = brightness_config_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to init brightness config - using defaults");
    }
    
    // Early NTP manager init (timezone only)
    ret = ntp_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize NTP manager");
    }
    
    // Initialize WiFi manager
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return ret;
    }
    
    // Check for stored WiFi credentials
    char ssid[64] = {0};
    char password[64] = {0};
    
    // Try to load credentials
    ret = nvs_manager_load_wifi_credentials(ssid, password, sizeof(ssid), sizeof(password));
    bool has_creds = (ret == ESP_OK && strlen(ssid) > 0);
    
    if (has_creds) {
        ESP_LOGI(TAG, "Found stored WiFi credentials for SSID: %s", ssid);
        
        // Check if WiFi is already connected (from previous boot cycle)
        if (thread_safe_get_wifi_connected()) {
            ESP_LOGI(TAG, "WiFi already connected - skipping reconnection");
        } else {
            wifi_manager_init_sta(ssid, password);
            ret = wifi_manager_start_with_credentials();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to connect with stored credentials");
                has_creds = false;
            }
        }
    }
    
    if (!has_creds && !thread_safe_get_wifi_connected()) {
        ESP_LOGI(TAG, "Starting WiFi in AP mode for configuration");
        wifi_manager_init_ap();
        
        // Start web server for configuration
        httpd_handle_t server = web_server_start();
        if (server == NULL) {
            ESP_LOGW(TAG, "Failed to start web server");
        }
    }
    
    // Initialize MQTT manager  
    ret = mqtt_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT manager init failed - continuing without MQTT");
    }
    
    // Initialize MQTT handlers
    ret = mqtt_handlers_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT handlers init failed - continuing with basic MQTT");
    }
    
    // Initialize MQTT discovery
    ret = mqtt_discovery_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT discovery init failed");
    }

    // Initialize LED validation system
    ret = led_validation_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED validation init failed - continuing without validation");
    } else {
        // Start validation task
        ret = start_validation_task();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start validation task");
        } else {
            ESP_LOGI(TAG, "✅ LED validation system started");
        }
    }

    ESP_LOGI(TAG, "✅ Network initialization complete");
    return ESP_OK;
}

/**
 * @brief Test all hardware systems
 */
#ifdef CONFIG_RUN_SYSTEM_TESTS
static void test_all_systems(void)
{
    ESP_LOGI(TAG, "=== RUNNING SYSTEM TESTS ===");
    
    // Test TLC59116 LED controllers
    ESP_LOGI(TAG, "Testing individual LED control...");
    for (uint8_t row = 0; row < WORDCLOCK_ROWS; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            tlc_set_matrix_led(row, col, 50);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            tlc_set_matrix_led(row, col, 0);
        }
    }
    
    // Test minute indicators
    ESP_LOGI(TAG, "Testing minute indicators...");
    for (uint8_t i = 0; i <= 4; i++) {
        set_minute_indicators(led_state, i, 80);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    tlc_turn_off_all_leds();
    
    // Test global brightness
    ESP_LOGI(TAG, "Testing global brightness control...");
    display_test_time();
    uint8_t brightness_levels[] = {255, 180, 120, 80, 40, 20, 255};
    for (size_t i = 0; i < sizeof(brightness_levels); i++) {
        tlc_set_global_brightness(brightness_levels[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Test German time display
    test_german_time_display();
    
    // Test brightness control
    test_brightness_control();
    
    ESP_LOGI(TAG, "=== ALL TESTS COMPLETE ===");
}
#endif // CONFIG_RUN_SYSTEM_TESTS

/**
 * @brief Live word clock operation with RTC
 */
static void test_live_word_clock(void)
{
    ESP_LOGI(TAG, "=== STARTING LIVE WORD CLOCK ===");
    ESP_LOGI(TAG, "Reading time from DS3231 and displaying German words");
    
    wordclock_time_t current_time;
    wordclock_time_t previous_time = {0};
    uint32_t loop_count = 0;
    bool first_loop = true;

    // Start light sensor monitoring
    esp_err_t ret = start_light_sensor_monitoring();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Light sensor monitoring started");
    }

    while (1) {
        loop_count++;

        // Read current time from RTC
        ret = wordclock_time_get(&current_time);
        if (ret == ESP_OK) {
            // Check if display will actually change (5-minute words or hour change)
            bool should_validate = first_loop;  // Always validate on first display

            if (!first_loop) {
                // Calculate base minutes (5-minute floor)
                uint8_t old_base = (previous_time.minutes / 5) * 5;
                uint8_t new_base = (current_time.minutes / 5) * 5;

                // Calculate display hours (accounting for 25+ minute rule)
                uint8_t old_display_hour = (old_base >= 25) ? (previous_time.hours + 1) % 12 : previous_time.hours % 12;
                uint8_t new_display_hour = (new_base >= 25) ? (current_time.hours + 1) % 12 : current_time.hours % 12;

                // Check if words actually change
                should_validate = (old_base != new_base) ||
                                (old_display_hour != new_display_hour) ||
                                ((old_base == 0) != (new_base == 0)); // UHR appears/disappears
            }

            // Display time with transitions
            display_german_time_with_transitions(&current_time);

            // Only validate if display actually changed
            if (should_validate) {
                // Sync LED state after transitions
                sync_led_state_after_transitions();

                // Trigger validation immediately after transition (auto-increment pointer is fresh)
                trigger_validation_post_transition();
            }

            // Update previous time
            previous_time = current_time;
            first_loop = false;

            // Log time occasionally
            if (loop_count % 20 == 0) {
                ESP_LOGI(TAG, "Current time: %02d:%02d:%02d",
                         current_time.hours, current_time.minutes, current_time.seconds);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read RTC - displaying test time");
            display_test_time();
        }

        // Update brightness from potentiometer every 3rd loop (15 seconds) - increased responsiveness
        if (loop_count % 3 == 0) {
            update_brightness_from_potentiometer();
        }

        // Network status logging
        if (loop_count % 20 == 0) {
            if (thread_safe_get_wifi_connected()) {
                ESP_LOGI(TAG, "WiFi Status: Connected");
            }

            if (thread_safe_get_ntp_synced()) {
                ESP_LOGI(TAG, "NTP Status: Synchronized");

                // Sync RTC with NTP time if available
                ret = ntp_manager_sync_rtc();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "RTC synchronized with NTP time");
                }
            }

            if (thread_safe_get_mqtt_connected()) {
                ESP_LOGI(TAG, "MQTT Status: Connected");

                // Publish sensor status
                mqtt_publish_sensor_status();

                // Publish heartbeat with NTP info
                mqtt_publish_heartbeat_with_ntp();
            }
        }

        // 5 second update interval
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    // Log system information
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "ESP32 GERMAN WORD CLOCK - PRODUCTION VERSION");
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap: %ld bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %ld bytes", esp_get_minimum_free_heap_size());
    
    // Get CPU frequency
    rtc_cpu_freq_config_t freq_config;
    rtc_clk_cpu_freq_get_config(&freq_config);
    cpu_freq_mhz = freq_config.freq_mhz;
    ESP_LOGI(TAG, "CPU Frequency: %lu MHz", cpu_freq_mhz);
    
    // Initialize thread safety first
    esp_err_t ret = thread_safety_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize thread safety: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize hardware
    ret = initialize_hardware();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware initialization failed");
        return;
    }
    
    // Initialize network
    ret = initialize_network();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Network initialization failed - continuing offline");
    }
    
    // Wait for network services to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Optional: Run system tests
    #ifdef CONFIG_RUN_SYSTEM_TESTS
    test_all_systems();
    #endif
    
    // Start live word clock operation
    test_live_word_clock();
    
    // Should never reach here
    ESP_LOGE(TAG, "Main loop exited unexpectedly!");
}
