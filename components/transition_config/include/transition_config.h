#ifndef TRANSITION_CONFIG_H
#define TRANSITION_CONFIG_H

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Transition curve types (must match transition_manager.h)
typedef enum {
    TRANSITION_CURVE_LINEAR = 0,
    TRANSITION_CURVE_EASE_IN,
    TRANSITION_CURVE_EASE_OUT,
    TRANSITION_CURVE_EASE_IN_OUT,
    TRANSITION_CURVE_BOUNCE,
    TRANSITION_CURVE_MAX
} transition_curve_type_t;

// Transition configuration structure
typedef struct {
    uint16_t duration_ms;           // Animation duration (200-5000ms)
    bool enabled;                   // Smooth transitions on/off
    transition_curve_type_t fadein_curve;   // Fade-in animation curve
    transition_curve_type_t fadeout_curve;  // Fade-out animation curve
    bool is_initialized;            // Initialization flag
} transition_config_t;

// Default configuration values
#define DEFAULT_TRANSITION_DURATION     1500
#define DEFAULT_TRANSITION_ENABLED      true
#define DEFAULT_FADEIN_CURVE           TRANSITION_CURVE_EASE_IN
#define DEFAULT_FADEOUT_CURVE          TRANSITION_CURVE_EASE_OUT

#define DEFAULT_TRANSITION_CONFIG { \
    .duration_ms = DEFAULT_TRANSITION_DURATION, \
    .enabled = DEFAULT_TRANSITION_ENABLED, \
    .fadein_curve = DEFAULT_FADEIN_CURVE, \
    .fadeout_curve = DEFAULT_FADEOUT_CURVE, \
    .is_initialized = false \
}

// NVS Configuration
#define NVS_TRANSITION_NAMESPACE    "transition_config"
#define NVS_TRANSITION_VERSION_KEY  "version"
#define NVS_TRANSITION_CONFIG_KEY   "config"
#define TRANSITION_CONFIG_VERSION   1

// Global configuration instance
extern transition_config_t g_transition_config;

// Function prototypes
esp_err_t transition_config_init(void);
esp_err_t transition_config_save_to_nvs(void);
esp_err_t transition_config_load_from_nvs(void);
esp_err_t transition_config_reset_to_defaults(void);

// Configuration setters/getters
esp_err_t transition_config_set_duration(uint16_t duration_ms);
esp_err_t transition_config_set_enabled(bool enabled);
esp_err_t transition_config_set_curves(transition_curve_type_t fadein, transition_curve_type_t fadeout);

uint16_t transition_config_get_duration(void);
bool transition_config_get_enabled(void);
transition_curve_type_t transition_config_get_fadein_curve(void);
transition_curve_type_t transition_config_get_fadeout_curve(void);

// Utility functions
const char* transition_curve_type_to_string(transition_curve_type_t type);
transition_curve_type_t transition_curve_type_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif // TRANSITION_CONFIG_H