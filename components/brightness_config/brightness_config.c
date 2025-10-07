#include "brightness_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "brightness_config";

// Global brightness configuration
brightness_config_t g_brightness_config = {
    .light_sensor = DEFAULT_LIGHT_SENSOR_CONFIG,
    .potentiometer = DEFAULT_POTENTIOMETER_CONFIG,
    .is_initialized = false
};

// NVS namespace and keys
#define NVS_NAMESPACE "brightness"
#define NVS_KEY_LIGHT_SENSOR "light_sensor"
#define NVS_KEY_POTENTIOMETER "potentiometer"
#define NVS_KEY_VERSION "version"
#define CURRENT_CONFIG_VERSION 1

// Initialize brightness configuration
esp_err_t brightness_config_init(void)
{
    ESP_LOGI(TAG, "Initializing brightness configuration");
    
    // Try to load from NVS first
    esp_err_t ret = brightness_config_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load brightness config from NVS, using defaults");
        // Already initialized with defaults
    }
    
    g_brightness_config.is_initialized = true;
    
    ESP_LOGI(TAG, "Brightness configuration initialized");
    ESP_LOGI(TAG, "Light sensor ranges:");
    ESP_LOGI(TAG, "  Very dark: %.1f-%.1f lux → %d-%d brightness", 
             g_brightness_config.light_sensor.very_dark.lux_min,
             g_brightness_config.light_sensor.very_dark.lux_max,
             g_brightness_config.light_sensor.very_dark.brightness_min,
             g_brightness_config.light_sensor.very_dark.brightness_max);
    ESP_LOGI(TAG, "  Dim: %.1f-%.1f lux → %d-%d brightness",
             g_brightness_config.light_sensor.dim.lux_min,
             g_brightness_config.light_sensor.dim.lux_max,
             g_brightness_config.light_sensor.dim.brightness_min,
             g_brightness_config.light_sensor.dim.brightness_max);
    ESP_LOGI(TAG, "Potentiometer: %d-%d, curve: %s",
             g_brightness_config.potentiometer.brightness_min,
             g_brightness_config.potentiometer.brightness_max,
             brightness_curve_type_to_string(g_brightness_config.potentiometer.curve_type));
    
    return ESP_OK;
}

// Save configuration to NVS
esp_err_t brightness_config_save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Saving brightness configuration to NVS");
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save version
    uint32_t version = CURRENT_CONFIG_VERSION;
    ret = nvs_set_u32(nvs_handle, NVS_KEY_VERSION, version);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save version: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Save light sensor configuration as blob
    ret = nvs_set_blob(nvs_handle, NVS_KEY_LIGHT_SENSOR, 
                       &g_brightness_config.light_sensor, 
                       sizeof(light_sensor_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save light sensor config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Save potentiometer configuration as blob
    ret = nvs_set_blob(nvs_handle, NVS_KEY_POTENTIOMETER,
                       &g_brightness_config.potentiometer,
                       sizeof(potentiometer_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save potentiometer config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Brightness configuration saved to NVS successfully");
    }
    
    nvs_close(nvs_handle);
    return ret;
}

// Load configuration from NVS
esp_err_t brightness_config_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Loading brightness configuration from NVS");
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, will use defaults");
        return ret;
    }
    
    // Check version
    uint32_t version = 0;
    ret = nvs_get_u32(nvs_handle, NVS_KEY_VERSION, &version);
    if (ret != ESP_OK || version != CURRENT_CONFIG_VERSION) {
        ESP_LOGW(TAG, "Invalid or missing version, will use defaults");
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_VERSION;
    }
    
    // Load light sensor configuration
    size_t length = sizeof(light_sensor_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_LIGHT_SENSOR,
                       &g_brightness_config.light_sensor, &length);
    if (ret != ESP_OK || length != sizeof(light_sensor_config_t)) {
        ESP_LOGW(TAG, "Failed to load light sensor config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Load potentiometer configuration
    length = sizeof(potentiometer_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_POTENTIOMETER,
                       &g_brightness_config.potentiometer, &length);
    if (ret != ESP_OK || length != sizeof(potentiometer_config_t)) {
        ESP_LOGW(TAG, "Failed to load potentiometer config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    ESP_LOGI(TAG, "Brightness configuration loaded from NVS successfully");
    nvs_close(nvs_handle);
    return ESP_OK;
}

// Reset to default configuration
esp_err_t brightness_config_reset_to_defaults(void)
{
    ESP_LOGI(TAG, "Resetting brightness configuration to defaults");
    
    light_sensor_config_t default_light = DEFAULT_LIGHT_SENSOR_CONFIG;
    potentiometer_config_t default_pot = DEFAULT_POTENTIOMETER_CONFIG;
    
    // Debug: Show what we're resetting to
    ESP_LOGI(TAG, "Default very_bright: %.1f-%.1f lux → %d-%d brightness", 
             default_light.very_bright.lux_min, default_light.very_bright.lux_max,
             default_light.very_bright.brightness_min, default_light.very_bright.brightness_max);
    
    g_brightness_config.light_sensor = default_light;
    g_brightness_config.potentiometer = default_pot;
    
    // Debug: Verify it was set
    ESP_LOGI(TAG, "After reset - very_bright in memory: %.1f-%.1f lux → %d-%d brightness",
             g_brightness_config.light_sensor.very_bright.lux_min,
             g_brightness_config.light_sensor.very_bright.lux_max,
             g_brightness_config.light_sensor.very_bright.brightness_min,
             g_brightness_config.light_sensor.very_bright.brightness_max);
    
    // Save to NVS
    return brightness_config_save_to_nvs();
}

// Set light sensor range configuration
esp_err_t brightness_config_set_light_range(const char* range_name, const light_range_config_t* config)
{
    if (!range_name || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    light_range_config_t* target = NULL;
    
    if (strcmp(range_name, "very_dark") == 0) {
        target = &g_brightness_config.light_sensor.very_dark;
    } else if (strcmp(range_name, "dim") == 0) {
        target = &g_brightness_config.light_sensor.dim;
    } else if (strcmp(range_name, "normal") == 0) {
        target = &g_brightness_config.light_sensor.normal;
    } else if (strcmp(range_name, "bright") == 0) {
        target = &g_brightness_config.light_sensor.bright;
    } else if (strcmp(range_name, "very_bright") == 0) {
        target = &g_brightness_config.light_sensor.very_bright;
    } else {
        ESP_LOGE(TAG, "Unknown light range: %s", range_name);
        return ESP_ERR_INVALID_ARG;
    }
    
    *target = *config;
    ESP_LOGI(TAG, "Updated light range '%s': %.1f-%.1f lux → %d-%d brightness",
             range_name, config->lux_min, config->lux_max, 
             config->brightness_min, config->brightness_max);
    
    return ESP_OK;
}

// Get light sensor range configuration
esp_err_t brightness_config_get_light_range(const char* range_name, light_range_config_t* config)
{
    if (!range_name || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const light_range_config_t* source = NULL;
    
    if (strcmp(range_name, "very_dark") == 0) {
        source = &g_brightness_config.light_sensor.very_dark;
    } else if (strcmp(range_name, "dim") == 0) {
        source = &g_brightness_config.light_sensor.dim;
    } else if (strcmp(range_name, "normal") == 0) {
        source = &g_brightness_config.light_sensor.normal;
    } else if (strcmp(range_name, "bright") == 0) {
        source = &g_brightness_config.light_sensor.bright;
    } else if (strcmp(range_name, "very_bright") == 0) {
        source = &g_brightness_config.light_sensor.very_bright;
    } else {
        ESP_LOGE(TAG, "Unknown light range: %s", range_name);
        return ESP_ERR_INVALID_ARG;
    }
    
    *config = *source;
    return ESP_OK;
}

// Set potentiometer configuration
esp_err_t brightness_config_set_potentiometer(const potentiometer_config_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->brightness_min >= config->brightness_max) {
        ESP_LOGE(TAG, "Invalid potentiometer range: min >= max");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->curve_type >= BRIGHTNESS_CURVE_MAX) {
        ESP_LOGE(TAG, "Invalid curve type: %d", config->curve_type);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_brightness_config.potentiometer = *config;
    ESP_LOGI(TAG, "Updated potentiometer config: %d-%d, curve: %s",
             config->brightness_min, config->brightness_max,
             brightness_curve_type_to_string(config->curve_type));
    
    return ESP_OK;
}

// Get potentiometer configuration
esp_err_t brightness_config_get_potentiometer(potentiometer_config_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *config = g_brightness_config.potentiometer;
    return ESP_OK;
}

// Calculate brightness from lux using configured ranges
uint8_t brightness_config_calculate_light_brightness(float lux)
{
    const light_sensor_config_t* cfg = &g_brightness_config.light_sensor;
    uint8_t brightness;
    
    // Clamp lux to sensor range
    if (lux < cfg->very_dark.lux_min) {
        lux = cfg->very_dark.lux_min;
    } else if (lux > cfg->very_bright.lux_max) {
        lux = cfg->very_bright.lux_max;
    }
    
    // Determine which range we're in and interpolate
    if (lux <= cfg->very_dark.lux_max) {
        // Very dark range
        float ratio = (lux - cfg->very_dark.lux_min) / 
                     (cfg->very_dark.lux_max - cfg->very_dark.lux_min);
        brightness = cfg->very_dark.brightness_min + 
                    (uint8_t)(ratio * (cfg->very_dark.brightness_max - cfg->very_dark.brightness_min));
    } else if (lux <= cfg->dim.lux_max) {
        // Dim range
        float ratio = (lux - cfg->dim.lux_min) / 
                     (cfg->dim.lux_max - cfg->dim.lux_min);
        brightness = cfg->dim.brightness_min + 
                    (uint8_t)(ratio * (cfg->dim.brightness_max - cfg->dim.brightness_min));
    } else if (lux <= cfg->normal.lux_max) {
        // Normal range
        float ratio = (lux - cfg->normal.lux_min) / 
                     (cfg->normal.lux_max - cfg->normal.lux_min);
        brightness = cfg->normal.brightness_min + 
                    (uint8_t)(ratio * (cfg->normal.brightness_max - cfg->normal.brightness_min));
    } else if (lux <= cfg->bright.lux_max) {
        // Bright range
        float ratio = (lux - cfg->bright.lux_min) / 
                     (cfg->bright.lux_max - cfg->bright.lux_min);
        brightness = cfg->bright.brightness_min + 
                    (uint8_t)(ratio * (cfg->bright.brightness_max - cfg->bright.brightness_min));
    } else {
        // Very bright range
        float ratio = (lux - cfg->very_bright.lux_min) / 
                     (cfg->very_bright.lux_max - cfg->very_bright.lux_min);
        if (ratio > 1.0f) ratio = 1.0f;
        brightness = cfg->very_bright.brightness_min + 
                    (uint8_t)(ratio * (cfg->very_bright.brightness_max - cfg->very_bright.brightness_min));
    }
    
    // Final bounds check
    if (brightness < 1) brightness = 1;
    // Note: brightness is uint8_t, so it can't exceed 255
    
    return brightness;
}

// Calculate potentiometer brightness with curve type
uint8_t brightness_config_calculate_potentiometer_brightness(uint8_t raw_value)
{
    const potentiometer_config_t* cfg = &g_brightness_config.potentiometer;
    float normalized = raw_value / 255.0f;  // Normalize to 0-1
    float curved;
    
    switch (cfg->curve_type) {
        case BRIGHTNESS_CURVE_LINEAR:
            curved = normalized;
            break;
            
        case BRIGHTNESS_CURVE_LOGARITHMIC:
            // More sensitivity at low end
            curved = log10f(1.0f + 9.0f * normalized) / log10f(10.0f);
            break;
            
        case BRIGHTNESS_CURVE_EXPONENTIAL:
            // More sensitivity at high end
            curved = (powf(2.0f, normalized) - 1.0f) / 1.0f;
            break;
            
        default:
            curved = normalized;
            break;
    }
    
    // Map to configured range
    uint8_t brightness = cfg->brightness_min + 
                        (uint8_t)(curved * (cfg->brightness_max - cfg->brightness_min));
    
    // Bounds check
    if (brightness < cfg->brightness_min) brightness = cfg->brightness_min;
    if (brightness > cfg->brightness_max) brightness = cfg->brightness_max;
    
    return brightness;
}

// Convert curve type to string
const char* brightness_curve_type_to_string(brightness_curve_type_t type)
{
    switch (type) {
        case BRIGHTNESS_CURVE_LINEAR:
            return "linear";
        case BRIGHTNESS_CURVE_LOGARITHMIC:
            return "logarithmic";
        case BRIGHTNESS_CURVE_EXPONENTIAL:
            return "exponential";
        default:
            return "unknown";
    }
}

// Convert string to curve type
brightness_curve_type_t brightness_curve_type_from_string(const char* str)
{
    if (!str) return BRIGHTNESS_CURVE_LINEAR;
    
    if (strcmp(str, "linear") == 0) {
        return BRIGHTNESS_CURVE_LINEAR;
    } else if (strcmp(str, "logarithmic") == 0) {
        return BRIGHTNESS_CURVE_LOGARITHMIC;
    } else if (strcmp(str, "exponential") == 0) {
        return BRIGHTNESS_CURVE_EXPONENTIAL;
    }
    
    return BRIGHTNESS_CURVE_LINEAR;  // Default
}

// Get current safety limit maximum
uint8_t brightness_config_get_safety_limit_max(void)
{
    return g_brightness_config.potentiometer.safety_limit_max;
}