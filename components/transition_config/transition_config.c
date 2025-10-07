#include "transition_config.h"
#include <string.h>

static const char *TAG = "transition_config";

// Global transition configuration
transition_config_t g_transition_config = DEFAULT_TRANSITION_CONFIG;

// Initialize transition configuration
esp_err_t transition_config_init(void)
{
    ESP_LOGI(TAG, "Initializing transition configuration");
    
    // Try to load from NVS first
    esp_err_t ret = transition_config_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load transition config from NVS, using defaults");
        // Already initialized with defaults
    }
    
    g_transition_config.is_initialized = true;
    
    ESP_LOGI(TAG, "Transition configuration initialized:");
    ESP_LOGI(TAG, "  Duration: %d ms", g_transition_config.duration_ms);
    ESP_LOGI(TAG, "  Enabled: %s", g_transition_config.enabled ? "true" : "false");
    ESP_LOGI(TAG, "  Fade-in curve: %s", transition_curve_type_to_string(g_transition_config.fadein_curve));
    ESP_LOGI(TAG, "  Fade-out curve: %s", transition_curve_type_to_string(g_transition_config.fadeout_curve));
    
    return ESP_OK;
}

// Save configuration to NVS
esp_err_t transition_config_save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Saving transition configuration to NVS");
    
    ret = nvs_open(NVS_TRANSITION_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save version
    uint32_t version = TRANSITION_CONFIG_VERSION;
    ret = nvs_set_u32(nvs_handle, NVS_TRANSITION_VERSION_KEY, version);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save version: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Save configuration as blob (excluding is_initialized flag)
    typedef struct {
        uint16_t duration_ms;
        bool enabled;
        transition_curve_type_t fadein_curve;
        transition_curve_type_t fadeout_curve;
    } transition_config_nvs_t;
    
    transition_config_nvs_t nvs_config = {
        .duration_ms = g_transition_config.duration_ms,
        .enabled = g_transition_config.enabled,
        .fadein_curve = g_transition_config.fadein_curve,
        .fadeout_curve = g_transition_config.fadeout_curve
    };
    
    ret = nvs_set_blob(nvs_handle, NVS_TRANSITION_CONFIG_KEY, 
                       &nvs_config, sizeof(transition_config_nvs_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save transition config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Transition configuration saved to NVS successfully");
    }
    
    nvs_close(nvs_handle);
    return ret;
}

// Load configuration from NVS
esp_err_t transition_config_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Loading transition configuration from NVS");
    
    ret = nvs_open(NVS_TRANSITION_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, will use defaults");
        return ret;
    }
    
    // Check version
    uint32_t version = 0;
    ret = nvs_get_u32(nvs_handle, NVS_TRANSITION_VERSION_KEY, &version);
    if (ret != ESP_OK || version != TRANSITION_CONFIG_VERSION) {
        ESP_LOGW(TAG, "Invalid or missing version, will use defaults");
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_VERSION;
    }
    
    // Load configuration blob
    typedef struct {
        uint16_t duration_ms;
        bool enabled;
        transition_curve_type_t fadein_curve;
        transition_curve_type_t fadeout_curve;
    } transition_config_nvs_t;
    
    transition_config_nvs_t nvs_config;
    size_t length = sizeof(transition_config_nvs_t);
    ret = nvs_get_blob(nvs_handle, NVS_TRANSITION_CONFIG_KEY, &nvs_config, &length);
    if (ret != ESP_OK || length != sizeof(transition_config_nvs_t)) {
        ESP_LOGW(TAG, "Failed to load transition config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Validate loaded values
    if (nvs_config.duration_ms < 200 || nvs_config.duration_ms > 5000) {
        ESP_LOGW(TAG, "Invalid duration %d, using default", nvs_config.duration_ms);
        nvs_config.duration_ms = DEFAULT_TRANSITION_DURATION;
    }
    
    if (nvs_config.fadein_curve >= TRANSITION_CURVE_MAX) {
        ESP_LOGW(TAG, "Invalid fadein curve %d, using default", nvs_config.fadein_curve);
        nvs_config.fadein_curve = DEFAULT_FADEIN_CURVE;
    }
    
    if (nvs_config.fadeout_curve >= TRANSITION_CURVE_MAX) {
        ESP_LOGW(TAG, "Invalid fadeout curve %d, using default", nvs_config.fadeout_curve);
        nvs_config.fadeout_curve = DEFAULT_FADEOUT_CURVE;
    }
    
    // Apply loaded configuration
    g_transition_config.duration_ms = nvs_config.duration_ms;
    g_transition_config.enabled = nvs_config.enabled;
    g_transition_config.fadein_curve = nvs_config.fadein_curve;
    g_transition_config.fadeout_curve = nvs_config.fadeout_curve;
    
    ESP_LOGI(TAG, "Transition configuration loaded from NVS successfully");
    nvs_close(nvs_handle);
    return ESP_OK;
}

// Reset to default configuration
esp_err_t transition_config_reset_to_defaults(void)
{
    ESP_LOGI(TAG, "Resetting transition configuration to defaults");
    
    g_transition_config.duration_ms = DEFAULT_TRANSITION_DURATION;
    g_transition_config.enabled = DEFAULT_TRANSITION_ENABLED;
    g_transition_config.fadein_curve = DEFAULT_FADEIN_CURVE;
    g_transition_config.fadeout_curve = DEFAULT_FADEOUT_CURVE;
    
    // Save to NVS
    return transition_config_save_to_nvs();
}

// Configuration setters with validation
esp_err_t transition_config_set_duration(uint16_t duration_ms)
{
    if (duration_ms < 200 || duration_ms > 5000) {
        ESP_LOGE(TAG, "Invalid transition duration: %d (must be 200-5000ms)", duration_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_transition_config.duration_ms = duration_ms;
    ESP_LOGI(TAG, "Transition duration set to %d ms", duration_ms);
    
    return ESP_OK;
}

esp_err_t transition_config_set_enabled(bool enabled)
{
    g_transition_config.enabled = enabled;
    ESP_LOGI(TAG, "Transitions %s", enabled ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t transition_config_set_curves(transition_curve_type_t fadein, transition_curve_type_t fadeout)
{
    if (fadein >= TRANSITION_CURVE_MAX || fadeout >= TRANSITION_CURVE_MAX) {
        ESP_LOGE(TAG, "Invalid curve types: fadein=%d, fadeout=%d", fadein, fadeout);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_transition_config.fadein_curve = fadein;
    g_transition_config.fadeout_curve = fadeout;
    ESP_LOGI(TAG, "Transition curves set: fadein=%s, fadeout=%s",
             transition_curve_type_to_string(fadein),
             transition_curve_type_to_string(fadeout));
    
    return ESP_OK;
}

// Configuration getters
uint16_t transition_config_get_duration(void)
{
    return g_transition_config.duration_ms;
}

bool transition_config_get_enabled(void)
{
    return g_transition_config.enabled;
}

transition_curve_type_t transition_config_get_fadein_curve(void)
{
    return g_transition_config.fadein_curve;
}

transition_curve_type_t transition_config_get_fadeout_curve(void)
{
    return g_transition_config.fadeout_curve;
}

// Utility functions
const char* transition_curve_type_to_string(transition_curve_type_t type)
{
    switch (type) {
        case TRANSITION_CURVE_LINEAR:     return "linear";
        case TRANSITION_CURVE_EASE_IN:    return "ease_in";
        case TRANSITION_CURVE_EASE_OUT:   return "ease_out";
        case TRANSITION_CURVE_EASE_IN_OUT: return "ease_in_out";
        case TRANSITION_CURVE_BOUNCE:     return "bounce";
        default:                          return "unknown";
    }
}

transition_curve_type_t transition_curve_type_from_string(const char* str)
{
    if (!str) return TRANSITION_CURVE_LINEAR;
    
    if (strcmp(str, "linear") == 0) {
        return TRANSITION_CURVE_LINEAR;
    } else if (strcmp(str, "ease_in") == 0) {
        return TRANSITION_CURVE_EASE_IN;
    } else if (strcmp(str, "ease_out") == 0) {
        return TRANSITION_CURVE_EASE_OUT;
    } else if (strcmp(str, "ease_in_out") == 0) {
        return TRANSITION_CURVE_EASE_IN_OUT;
    } else if (strcmp(str, "bounce") == 0) {
        return TRANSITION_CURVE_BOUNCE;
    }
    
    return TRANSITION_CURVE_LINEAR;  // Default
}