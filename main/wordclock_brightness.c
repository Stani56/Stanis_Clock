/**
 * @file wordclock_brightness.c
 * @brief Dual brightness control system for wordclock
 * 
 * This module manages the dual brightness control system:
 * - Individual LED brightness (potentiometer controlled)
 * - Global brightness (light sensor controlled)
 * 
 * The system supports instant response to ambient light changes and
 * user-adjustable brightness via potentiometer.
 */

#include "wordclock_brightness.h"
#include "i2c_devices.h"
#include "adc_manager.h"
#include "light_sensor.h"
#include "wordclock_display.h"
#include "thread_safety.h"
#include "transition_manager.h"
#include "brightness_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <inttypes.h>

static const char *TAG = "wordclock_brightness";

// Brightness control variables (non-static for MQTT access)
uint8_t global_brightness = 180;         // Global brightness (light sensor controlled) - increased from 120
uint8_t potentiometer_individual = 60;   // Individual brightness (potentiometer controlled) - increased from 32

// Light sensor task control
static TaskHandle_t light_sensor_task_handle = NULL;
static bool light_sensor_task_running = false;

// Light sensor configuration
#define LIGHT_SENSOR_TASK_INTERVAL_MS    100    // 10Hz monitoring for instant response
#define LIGHT_CHANGE_THRESHOLD_PERCENT    10     // 10% change triggers update
#define LIGHT_SENSOR_SMOOTHING_SAMPLES    10    // Number of samples to average
#define LIGHT_UPDATE_COOLDOWN_MS          500   // Minimum time between updates

/**
 * @brief Initialize brightness control subsystems
 */
esp_err_t brightness_control_init(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING BRIGHTNESS CONTROL ===");
    
    // Initialize ADC for potentiometer
    esp_err_t ret = adc_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize light sensor
    ret = light_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Light sensor initialization failed - continuing without ambient control");
        // Not critical - continue without light sensor
    }
    
    // Set initial brightness values with safety limit checking
    uint8_t safety_limit = brightness_config_get_safety_limit_max();
    if (potentiometer_individual > safety_limit) {
        ESP_LOGW(TAG, "Initial individual brightness %d exceeds safety limit %d, clamping", 
                 potentiometer_individual, safety_limit);
        potentiometer_individual = safety_limit;
    }
    
    ret = tlc_set_individual_brightness(potentiometer_individual);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Initial individual brightness: %d (safety limit: %d)", 
                 potentiometer_individual, safety_limit);
    }
    
    ret = tlc_set_global_brightness(global_brightness);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Initial global brightness: %d", global_brightness);
        
        // Show effective brightness calculation for transparency
        uint16_t effective_brightness = (potentiometer_individual * global_brightness) / 255;
        ESP_LOGI(TAG, "📊 Effective display brightness: (%d × %d) ÷ 255 = %d (~%d%% of maximum)", 
                 potentiometer_individual, global_brightness, effective_brightness,
                 (effective_brightness * 100) / 255);
    }
    
    ESP_LOGI(TAG, "✅ Brightness control initialized");
    return ESP_OK;
}

/**
 * @brief Set individual LED brightness via MQTT control
 */
esp_err_t set_individual_brightness(uint8_t brightness)
{
    // Check minimum brightness
    if (brightness < 5) {
        ESP_LOGE(TAG, "Invalid individual brightness: %d (minimum is 5)", brightness);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check safety limit maximum
    uint8_t safety_limit = brightness_config_get_safety_limit_max();
    if (brightness > safety_limit) {
        ESP_LOGW(TAG, "Individual brightness %d exceeds safety limit %d, clamping", brightness, safety_limit);
        brightness = safety_limit;
    }
    
    potentiometer_individual = brightness;
    
    // Update TLC controllers with new individual brightness
    esp_err_t ret = tlc_set_individual_brightness(brightness);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "💡 Individual brightness updated to %d", brightness);
        
        // Refresh current display with new brightness
        refresh_current_display();
    } else {
        ESP_LOGW(TAG, "Failed to update individual brightness: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Set global brightness (usually from light sensor)
 */
esp_err_t set_global_brightness(uint8_t brightness)
{
    // Don't update during active transitions to prevent jumps
    if (transition_manager_is_active()) {
        ESP_LOGD(TAG, "Skipping global brightness update - transition active");
        return ESP_OK;
    }
    
    global_brightness = brightness;
    
    esp_err_t ret = tlc_set_global_brightness(brightness);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "🌞 Global brightness updated to %d", brightness);
    } else {
        ESP_LOGW(TAG, "Failed to update global brightness: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Update brightness from potentiometer reading
 */
esp_err_t update_brightness_from_potentiometer(void)
{
    adc_reading_t reading;
    esp_err_t ret = adc_read_potentiometer_averaged(&reading);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read potentiometer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Convert voltage to brightness (5-80 range)
    uint8_t new_brightness = adc_voltage_to_brightness(reading.voltage_mv);
    
    // Apply safety limit enforcement 
    uint8_t safety_limit = brightness_config_get_safety_limit_max();
    if (new_brightness > safety_limit) {
        ESP_LOGD(TAG, "Potentiometer brightness %d exceeds safety limit %d, clamping", new_brightness, safety_limit);
        new_brightness = safety_limit;
    }
    
    // Only update if changed by more than threshold (1 point)
    if (abs((int)new_brightness - (int)potentiometer_individual) > 1) {
        ESP_LOGI(TAG, "🎛️ Potentiometer: %" PRIu32 "mV → brightness %d (safety limit: %d)", 
                 reading.voltage_mv, new_brightness, safety_limit);
        
        // Show effective brightness change
        uint16_t old_effective = (potentiometer_individual * global_brightness) / 255;
        uint16_t new_effective = (new_brightness * global_brightness) / 255;
        ESP_LOGI(TAG, "📊 Potentiometer effect: individual %d→%d, effective %d→%d",
                 potentiometer_individual, new_brightness, old_effective, new_effective);
        
        ret = set_individual_brightness(new_brightness);
    }
    
    return ret;
}

/**
 * @brief Light sensor monitoring task for instant ambient response
 */
static void light_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🌞 Light sensor task started - 10Hz monitoring");
    
    light_reading_t reading;
    float last_lux = 0.0f;
    uint32_t last_update_time = 0;
    float lux_samples[LIGHT_SENSOR_SMOOTHING_SAMPLES] = {0};
    int sample_index = 0;
    bool samples_filled = false;
    
    // Grace period for system stabilization - reduced for better user experience
    vTaskDelay(pdMS_TO_TICKS(3000));  // 3 seconds delay for stabilization (reduced from 10)
    ESP_LOGI(TAG, "🌞 Light sensor grace period complete - starting monitoring");
    
    while (light_sensor_task_running) {
        // Read light sensor
        esp_err_t ret = light_sensor_read(&reading);
        if (ret == ESP_OK) {
            // Store sample for averaging
            lux_samples[sample_index] = reading.lux_value;
            sample_index = (sample_index + 1) % LIGHT_SENSOR_SMOOTHING_SAMPLES;
            if (sample_index == 0) samples_filled = true;
            
            // Calculate average lux
            float avg_lux = 0;
            int sample_count = samples_filled ? LIGHT_SENSOR_SMOOTHING_SAMPLES : sample_index + 1;
            for (int i = 0; i < sample_count; i++) {
                avg_lux += lux_samples[i];
            }
            avg_lux /= sample_count;
            
            // Check for significant change (±10%)
            float change_percent = fabsf((avg_lux - last_lux) / (last_lux + 1.0f)) * 100.0f;
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t time_since_update = current_time - last_update_time;
            
            if (change_percent >= LIGHT_CHANGE_THRESHOLD_PERCENT && 
                time_since_update >= LIGHT_UPDATE_COOLDOWN_MS) {
                
                // Don't update during transitions - wait for completion
                if (transition_manager_is_active()) {
                    ESP_LOGD(TAG, "Light change detected (%.1f%%) but transition active - deferring", 
                             change_percent);
                } else {
                    ESP_LOGI(TAG, "🌞 Significant light change: %.1f lux → %.1f lux (%.1f%% change)",
                             last_lux, avg_lux, change_percent);
                    
                    // Calculate and apply new brightness
                    uint8_t new_brightness = light_lux_to_brightness(avg_lux);
                    uint8_t old_global = global_brightness;
                    set_global_brightness(new_brightness);
                    
                    // Show brightness transition for debugging
                    uint16_t old_effective = (potentiometer_individual * old_global) / 255;
                    uint16_t new_effective = (potentiometer_individual * new_brightness) / 255;
                    ESP_LOGI(TAG, "📊 Brightness transition: global %d→%d, effective %d→%d (%+d%%)",
                             old_global, new_brightness, old_effective, new_effective,
                             ((int)new_effective - (int)old_effective) * 100 / old_effective);
                    
                    last_lux = avg_lux;
                    last_update_time = current_time;
                }
            }
        } else {
            ESP_LOGW(TAG, "Light sensor read failed: %s", esp_err_to_name(ret));
        }
        
        // 10Hz monitoring rate
        vTaskDelay(pdMS_TO_TICKS(LIGHT_SENSOR_TASK_INTERVAL_MS));
    }
    
    ESP_LOGI(TAG, "Light sensor task stopping");
    light_sensor_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Start light sensor monitoring task
 */
esp_err_t start_light_sensor_monitoring(void)
{
    if (light_sensor_task_handle != NULL) {
        ESP_LOGW(TAG, "Light sensor task already running");
        return ESP_OK;
    }
    
    light_sensor_task_running = true;
    
    BaseType_t ret = xTaskCreate(
        light_sensor_task,
        "light_sensor",
        3072,  // Stack size
        NULL,  // Parameters
        3,     // Priority
        &light_sensor_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create light sensor task");
        light_sensor_task_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Light sensor monitoring started");
    return ESP_OK;
}

/**
 * @brief Stop light sensor monitoring
 */
void stop_light_sensor_monitoring(void)
{
    if (light_sensor_task_handle != NULL) {
        light_sensor_task_running = false;
        // Task will delete itself
        vTaskDelay(pdMS_TO_TICKS(200));  // Give task time to stop
    }
}

/**
 * @brief Test brightness control system
 */
void test_brightness_control(void)
{
    ESP_LOGI(TAG, "=== TESTING BRIGHTNESS CONTROL ===");
    
    // Test potentiometer reading
    ESP_LOGI(TAG, "Testing potentiometer...");
    for (int i = 0; i < 5; i++) {
        update_brightness_from_potentiometer();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Test global brightness changes
    ESP_LOGI(TAG, "Testing global brightness...");
    uint8_t test_values[] = {20, 60, 120, 180, 255};
    for (size_t i = 0; i < sizeof(test_values); i++) {
        ESP_LOGI(TAG, "Setting global brightness to %d", test_values[i]);
        set_global_brightness(test_values[i]);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // Reset to normal
    set_global_brightness(120);
    ESP_LOGI(TAG, "=== BRIGHTNESS CONTROL TEST COMPLETE ===");
}