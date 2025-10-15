#ifndef BRIGHTNESS_CONFIG_H
#define BRIGHTNESS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Brightness curve types
typedef enum {
    BRIGHTNESS_CURVE_LINEAR = 0,
    BRIGHTNESS_CURVE_LOGARITHMIC,
    BRIGHTNESS_CURVE_EXPONENTIAL,
    BRIGHTNESS_CURVE_MAX
} brightness_curve_type_t;

// Light sensor range configuration
typedef struct {
    float lux_min;
    float lux_max;
    uint8_t brightness_min;
    uint8_t brightness_max;
} light_range_config_t;

// Complete light sensor configuration
typedef struct {
    light_range_config_t very_dark;
    light_range_config_t dim;
    light_range_config_t normal;
    light_range_config_t bright;
    light_range_config_t very_bright;
} light_sensor_config_t;

// Potentiometer configuration
typedef struct {
    uint8_t brightness_min;
    uint8_t brightness_max;
    brightness_curve_type_t curve_type;
    uint8_t safety_limit_max;  // Maximum safe PWM value (default: 80)
} potentiometer_config_t;

// Combined brightness configuration
typedef struct {
    light_sensor_config_t light_sensor;
    potentiometer_config_t potentiometer;
    uint8_t global_minimum_brightness;  // Configurable safety minimum (default: 3)
    bool is_initialized;
    bool use_halb_centric_style;  // HALB-centric time expression style (default: false)
} brightness_config_t;

// Default configuration values
#define DEFAULT_LIGHT_SENSOR_CONFIG { \
    .very_dark = {1.0f, 10.0f, 5, 30}, \
    .dim = {10.0f, 50.0f, 30, 60}, \
    .normal = {50.0f, 200.0f, 60, 120}, \
    .bright = {200.0f, 500.0f, 120, 200}, \
    .very_bright = {500.0f, 1500.0f, 200, 255} \
}

#define DEFAULT_POTENTIOMETER_CONFIG { \
    .brightness_min = 5, \
    .brightness_max = 255, \
    .curve_type = BRIGHTNESS_CURVE_LINEAR, \
    .safety_limit_max = 80 \
}

#define DEFAULT_GLOBAL_MINIMUM_BRIGHTNESS 3  // Configurable safety minimum

// Global brightness configuration instance
extern brightness_config_t g_brightness_config;

// Configuration management functions
esp_err_t brightness_config_init(void);
esp_err_t brightness_config_save_to_nvs(void);
esp_err_t brightness_config_load_from_nvs(void);
esp_err_t brightness_config_reset_to_defaults(void);

// Light sensor configuration functions
esp_err_t brightness_config_set_light_range(const char* range_name, const light_range_config_t* config);
esp_err_t brightness_config_get_light_range(const char* range_name, light_range_config_t* config);

// Potentiometer configuration functions
esp_err_t brightness_config_set_potentiometer(const potentiometer_config_t* config);
esp_err_t brightness_config_get_potentiometer(potentiometer_config_t* config);
uint8_t brightness_config_get_safety_limit_max(void);

// Utility functions
uint8_t brightness_config_calculate_light_brightness(float lux);
uint8_t brightness_config_calculate_potentiometer_brightness(uint8_t raw_value);
const char* brightness_curve_type_to_string(brightness_curve_type_t type);
brightness_curve_type_t brightness_curve_type_from_string(const char* str);

// Time expression style configuration functions
bool brightness_config_get_halb_centric_style(void);
esp_err_t brightness_config_set_halb_centric_style(bool enabled);

#ifdef __cplusplus
}
#endif

#endif // BRIGHTNESS_CONFIG_H