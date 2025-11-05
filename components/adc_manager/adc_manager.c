#include "adc_manager.h"
#include "brightness_config.h"  // Use configurable potentiometer ranges
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "adc_manager";

// Global ADC manager instance
static adc_manager_t adc_mgr = {0};

esp_err_t adc_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC manager for potentiometer input");
    
    if (adc_mgr.initialized) {
        ESP_LOGW(TAG, "ADC manager already initialized");
        return ESP_OK;
    }
    
    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_POTENTIOMETER_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_mgr.adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC oneshot unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_POTENTIOMETER_BITWIDTH,
        .atten = ADC_POTENTIOMETER_ATTEN,
    };
    
    ret = adc_oneshot_config_channel(adc_mgr.adc_handle, ADC_POTENTIOMETER_CHANNEL, &channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_mgr.adc_handle);
        return ret;
    }
    
    // Initialize calibration
    ret = adc_calibration_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
        adc_mgr.calibration_enabled = false;
    } else {
        adc_mgr.calibration_enabled = true;
        ESP_LOGI(TAG, "ADC calibration initialized successfully");
    }
    
    adc_mgr.initialized = true;
    ESP_LOGI(TAG, "ADC manager initialized (GPIO 34, 12-bit, 3.3V range)");
    
    return ESP_OK;
}

esp_err_t adc_manager_deinit(void)
{
    if (!adc_mgr.initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing ADC manager");
    
    // Clean up calibration
    if (adc_mgr.calibration_enabled) {
        adc_calibration_deinit();
    }
    
    // Delete ADC unit
    if (adc_mgr.adc_handle) {
        adc_oneshot_del_unit(adc_mgr.adc_handle);
        adc_mgr.adc_handle = NULL;
    }
    
    memset(&adc_mgr, 0, sizeof(adc_manager_t));
    
    ESP_LOGI(TAG, "ADC manager deinitialized");
    return ESP_OK;
}

esp_err_t adc_read_potentiometer(adc_reading_t *reading)
{
    if (!adc_mgr.initialized || reading == NULL) {
        ESP_LOGE(TAG, "ADC manager not initialized or invalid reading pointer");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read raw ADC value
    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc_mgr.adc_handle, ADC_POTENTIOMETER_CHANNEL, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        reading->valid = false;
        return ret;
    }
    
    // Debug: Log raw reading details
    static int raw_debug_count = 0;
    if (raw_debug_count < 5) {
        ESP_LOGI(TAG, "Raw ADC Debug %d: raw_value=%d, handle=%p", 
                 raw_debug_count++, raw_value, adc_mgr.adc_handle);
    }
    
    // Ensure raw value is within expected range
    if (raw_value < 0) {
        raw_value = 0;
    } else if (raw_value > 4095) {
        raw_value = 4095;
    }
    
    reading->raw_value = (uint32_t)raw_value;
    reading->valid = true;
    
    // Convert to voltage
    if (adc_mgr.calibration_enabled) {
        int voltage_mv;
        ret = adc_cali_raw_to_voltage(adc_mgr.cali_handle, raw_value, &voltage_mv);
        if (ret == ESP_OK) {
            reading->voltage_mv = (uint32_t)voltage_mv;
        } else {
            // Fallback to linear conversion
            reading->voltage_mv = adc_raw_to_voltage(reading->raw_value);
            ESP_LOGW(TAG, "Calibration read failed, using linear conversion");
        }
    } else {
        reading->voltage_mv = adc_raw_to_voltage(reading->raw_value);
    }
    
    // Map to brightness value using brightness config
    // Convert voltage to normalized 0-255 range for brightness config
    uint8_t normalized_value = (uint8_t)((reading->voltage_mv * 255) / ADC_VOLTAGE_MAX);
    reading->brightness_value = brightness_config_calculate_potentiometer_brightness(normalized_value);
    
    // Store as last reading
    adc_mgr.last_reading = *reading;
    
    return ESP_OK;
}

esp_err_t adc_read_potentiometer_averaged(adc_reading_t *reading)
{
    if (!adc_mgr.initialized || reading == NULL) {
        ESP_LOGE(TAG, "ADC manager not initialized or invalid reading pointer");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t raw_sum = 0;
    uint32_t voltage_sum = 0;
    int valid_samples = 0;
    
    // Take multiple samples
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        adc_reading_t sample;
        esp_err_t ret = adc_read_potentiometer(&sample);
        
        if (ret == ESP_OK && sample.valid) {
            raw_sum += sample.raw_value;
            voltage_sum += sample.voltage_mv;
            valid_samples++;
        }
        
        // Small delay between samples (reduced for faster response)
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    if (valid_samples == 0) {
        ESP_LOGE(TAG, "No valid ADC samples obtained");
        reading->valid = false;
        return ESP_FAIL;
    }
    
    // Calculate averages
    reading->raw_value = raw_sum / valid_samples;
    reading->voltage_mv = voltage_sum / valid_samples;
    // Map to brightness value using brightness config
    // Convert voltage to normalized 0-255 range for brightness config
    uint8_t normalized_value = (uint8_t)((reading->voltage_mv * 255) / ADC_VOLTAGE_MAX);
    reading->brightness_value = brightness_config_calculate_potentiometer_brightness(normalized_value);
    reading->valid = true;
    
    // Store as last reading
    adc_mgr.last_reading = *reading;
    
    ESP_LOGI(TAG, "ADC averaged: raw=%lu, voltage=%lu mV, brightness=%d (%d samples)",
             reading->raw_value, reading->voltage_mv, reading->brightness_value, valid_samples);
    
    // Debug: Log individual sample details for first few readings
    static int debug_count = 0;
    if (debug_count < 3) {
        ESP_LOGI(TAG, "ADC Debug %d: Unit=%d, Channel=%d, Attenuation=%d, Bitwidth=%d", 
                 debug_count++, ADC_POTENTIOMETER_UNIT, ADC_POTENTIOMETER_CHANNEL, 
                 ADC_POTENTIOMETER_ATTEN, ADC_POTENTIOMETER_BITWIDTH);
    }
    
    return ESP_OK;
}

uint8_t adc_voltage_to_brightness(uint32_t voltage_mv)
{
    // Clamp voltage to expected range (ADC_VOLTAGE_MIN is 0, so skip negative check)
    if (voltage_mv > ADC_VOLTAGE_MAX) {
        voltage_mv = ADC_VOLTAGE_MAX;
    }
    
    // Linear mapping from voltage to brightness
    uint32_t voltage_range = ADC_VOLTAGE_MAX - ADC_VOLTAGE_MIN;
    uint32_t brightness_range = ADC_BRIGHTNESS_MAX - ADC_BRIGHTNESS_MIN;
    uint32_t voltage_offset = voltage_mv - ADC_VOLTAGE_MIN;
    
    uint32_t brightness = ADC_BRIGHTNESS_MIN + (voltage_offset * brightness_range) / voltage_range;
    
    // Ensure result is within bounds
    if (brightness < ADC_BRIGHTNESS_MIN) {
        brightness = ADC_BRIGHTNESS_MIN;
    } else if (brightness > ADC_BRIGHTNESS_MAX) {
        brightness = ADC_BRIGHTNESS_MAX;
    }
    
    return (uint8_t)brightness;
}

uint32_t adc_raw_to_voltage(uint32_t raw_value)
{
    // Linear conversion for 12-bit ADC with 3.3V reference
    // Raw range: 0-4095, Voltage range: 0-3300 mV
    uint32_t voltage_mv = (raw_value * ADC_VOLTAGE_MAX) / 4095;
    return voltage_mv;
}

bool adc_manager_is_initialized(void)
{
    return adc_mgr.initialized;
}

esp_err_t adc_calibration_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC calibration");
    esp_err_t ret = ESP_FAIL;

    // Try curve fitting first (ESP32-S3, ESP32-C3, ESP32-C2)
    #if CONFIG_ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_POTENTIOMETER_UNIT,
        .chan = ADC_POTENTIOMETER_CHANNEL,
        .atten = ADC_POTENTIOMETER_ATTEN,
        .bitwidth = ADC_POTENTIOMETER_BITWIDTH,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_mgr.cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration scheme (curve fitting) created");
        adc_mgr.calibration_enabled = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Curve fitting calibration failed: %s", esp_err_to_name(ret));
    #endif

    // Try line fitting calibration (ESP32, ESP32-S2)
    #if CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t line_config = {
        .unit_id = ADC_POTENTIOMETER_UNIT,
        .atten = ADC_POTENTIOMETER_ATTEN,
        .bitwidth = ADC_POTENTIOMETER_BITWIDTH,
    };

    ret = adc_cali_create_scheme_line_fitting(&line_config, &adc_mgr.cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration scheme (line fitting) created");
        adc_mgr.calibration_enabled = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Line fitting calibration also failed: %s", esp_err_to_name(ret));
    #endif

    adc_mgr.calibration_enabled = false;
    return ret;
}

void adc_calibration_deinit(void)
{
    if (adc_mgr.cali_handle) {
        ESP_LOGI(TAG, "Deinitializing ADC calibration");
        esp_err_t ret = ESP_FAIL;

        // Try curve fitting deletion first (ESP32-S3, ESP32-C3, ESP32-C2)
        #if CONFIG_ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_curve_fitting(adc_mgr.cali_handle);
        if (ret == ESP_OK) {
            adc_mgr.cali_handle = NULL;
            adc_mgr.calibration_enabled = false;
            return;
        }
        #endif

        // Try line fitting deletion (ESP32, ESP32-S2)
        #if CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        ret = adc_cali_delete_scheme_line_fitting(adc_mgr.cali_handle);
        if (ret == ESP_OK) {
            adc_mgr.cali_handle = NULL;
            adc_mgr.calibration_enabled = false;
            return;
        }
        #endif

        ESP_LOGW(TAG, "Failed to delete calibration scheme: %s", esp_err_to_name(ret));
        adc_mgr.cali_handle = NULL;
        adc_mgr.calibration_enabled = false;
    }
}

bool adc_calibration_available(void)
{
    return adc_mgr.calibration_enabled && (adc_mgr.cali_handle != NULL);
}

esp_err_t adc_test_gpio_connectivity(void)
{
    if (!adc_mgr.initialized) {
        ESP_LOGE(TAG, "ADC manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "=== GPIO 34 Connectivity Test ===");
    ESP_LOGI(TAG, "Testing GPIO 34 (ADC1_CH6) - Try connecting to GND, 3.3V, then potentiometer");
    
    for (int test = 0; test < 20; test++) {
        int raw_value;
        esp_err_t ret = adc_oneshot_read(adc_mgr.adc_handle, ADC_POTENTIOMETER_CHANNEL, &raw_value);
        
        if (ret == ESP_OK) {
            uint32_t voltage = adc_raw_to_voltage(raw_value);
            ESP_LOGI(TAG, "Test %d: Raw=%d (%.1f%%), Linear_Voltage=%lu mV", 
                     test + 1, raw_value, (raw_value * 100.0f) / 4095.0f, voltage);
            
            if (adc_mgr.calibration_enabled) {
                int cal_voltage_mv;
                esp_err_t cal_ret = adc_cali_raw_to_voltage(adc_mgr.cali_handle, raw_value, &cal_voltage_mv);
                if (cal_ret == ESP_OK) {
                    ESP_LOGI(TAG, "       Calibrated_Voltage=%d mV", cal_voltage_mv);
                }
            }
        } else {
            ESP_LOGE(TAG, "Test %d: ADC read failed: %s", test + 1, esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "GPIO connectivity test completed");
    ESP_LOGI(TAG, "Expected results:");
    ESP_LOGI(TAG, "- GND connection: Raw=0-50, Voltage=0-50mV");
    ESP_LOGI(TAG, "- 3.3V connection: Raw=4000-4095, Voltage=3200-3300mV");
    ESP_LOGI(TAG, "- Floating/unconnected: Raw varies randomly or stays at ~142mV");
    
    return ESP_OK;
}