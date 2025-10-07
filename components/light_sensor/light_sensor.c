#include "light_sensor.h"
#include "i2c_devices.h"  // Use existing I2C system
#include "brightness_config.h"  // Use configurable brightness curves
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "light_sensor";

// Light sensor state
typedef struct {
    bool initialized;
    light_reading_t last_reading;
    float indoor_threshold;
    float bright_threshold;
} light_sensor_t;

static light_sensor_t light_sensor = {0};

esp_err_t light_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing BH1750 light sensor");
    
    // Set default brightness curve thresholds
    light_sensor.indoor_threshold = LIGHT_THRESHOLD_INDOOR;
    light_sensor.bright_threshold = LIGHT_THRESHOLD_BRIGHT;
    
    uint8_t cmd;
    esp_err_t ret;
    
    // Power on the sensor
    cmd = BH1750_POWER_ON;
    ret = bh1750_write_command(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on BH1750: %s", esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset the sensor
    cmd = BH1750_RESET;
    ret = bh1750_write_command(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset BH1750: %s", esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Configure continuous high resolution mode
    cmd = BH1750_CONTINUOUS_HIGH_RES;
    ret = bh1750_write_command(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure BH1750 continuous mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for first measurement to be ready
    vTaskDelay(pdMS_TO_TICKS(200));
    
    light_sensor.initialized = true;
    ESP_LOGI(TAG, "BH1750 initialized successfully");
    
    return ESP_OK;
}

esp_err_t light_sensor_deinit(void)
{
    if (!light_sensor.initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing BH1750 light sensor");
    
    // Power down the sensor
    uint8_t cmd = BH1750_POWER_DOWN;
    esp_err_t ret = bh1750_write_command(cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to power down BH1750: %s", esp_err_to_name(ret));
    }
    
    light_sensor.initialized = false;
    return ESP_OK;
}

esp_err_t light_sensor_read_raw_lux(float *lux)
{
    if (!light_sensor.initialized || lux == NULL) {
        ESP_LOGE(TAG, "Light sensor not initialized or invalid parameter");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t data[2];
    esp_err_t ret = bh1750_read_data(data, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read from BH1750: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uint16_t raw_data = (data[0] << 8) | data[1];
    *lux = raw_data / 1.2f;  // BH1750 conversion factor
    
    return ESP_OK;
}

esp_err_t light_sensor_read(light_reading_t *reading)
{
    if (!light_sensor.initialized || reading == NULL) {
        ESP_LOGE(TAG, "Light sensor not initialized or invalid parameter");
        return ESP_ERR_INVALID_STATE;
    }
    
    float lux;
    esp_err_t ret = light_sensor_read_raw_lux(&lux);
    if (ret != ESP_OK) {
        reading->valid = false;
        return ret;
    }
    
    // Populate reading structure
    reading->lux_value = lux;
    reading->global_brightness = light_lux_to_brightness(lux);
    reading->valid = true;
    reading->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Store as last reading
    light_sensor.last_reading = *reading;
    
    // Production mode: Verbose logging disabled to reduce spam
    // For debugging, uncomment: ESP_LOGI(TAG, "Light reading: %.1f lux → Global brightness %d", reading->lux_value, reading->global_brightness);
    
    return ESP_OK;
}

uint8_t light_lux_to_brightness(float lux)
{
    // Clamp lux to sensor range
    if (lux < LIGHT_SENSOR_MIN_LUX) {
        lux = LIGHT_SENSOR_MIN_LUX;
    } else if (lux > LIGHT_SENSOR_MAX_LUX) {
        lux = LIGHT_SENSOR_MAX_LUX;
    }
    
    uint8_t brightness;
    
    // Use configurable brightness curves from brightness_config system
    brightness = brightness_config_calculate_light_brightness(lux);
    
    // Final bounds check
    if (brightness < LIGHT_BRIGHTNESS_MIN) {
        brightness = LIGHT_BRIGHTNESS_MIN;
    } else if (brightness > LIGHT_BRIGHTNESS_MAX) {
        brightness = LIGHT_BRIGHTNESS_MAX;
    }
    
    return brightness;
}

esp_err_t light_update_global_brightness(void)
{
    if (!light_sensor.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    light_reading_t reading;
    esp_err_t ret = light_sensor_read(&reading);
    if (ret != ESP_OK || !reading.valid) {
        ESP_LOGW(TAG, "Failed to read light sensor for brightness update: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Apply global brightness immediately using existing function
    extern esp_err_t set_global_brightness(uint8_t brightness);
    ret = set_global_brightness(reading.global_brightness);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Global brightness updated: %.1f lux → %d/255", 
                 reading.lux_value, reading.global_brightness);
    } else {
        ESP_LOGW(TAG, "Failed to set global brightness: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t light_set_brightness_curve(float indoor_lux, float bright_lux)
{
    if (indoor_lux <= 0 || bright_lux <= indoor_lux) {
        ESP_LOGE(TAG, "Invalid brightness curve parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    light_sensor.indoor_threshold = indoor_lux;
    light_sensor.bright_threshold = bright_lux;
    
    ESP_LOGI(TAG, "Brightness curve updated: Indoor=%.1f lux, Bright=%.1f lux", 
             indoor_lux, bright_lux);
    
    return ESP_OK;
}

bool light_sensor_is_initialized(void)
{
    return light_sensor.initialized;
}