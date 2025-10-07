#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// BH1750 Configuration
#define BH1750_ADDR 0x23  // Light sensor I2C address

// BH1750 Commands
#define BH1750_POWER_DOWN 0x00
#define BH1750_POWER_ON 0x01
#define BH1750_RESET 0x07
#define BH1750_CONTINUOUS_HIGH_RES 0x10
#define BH1750_CONTINUOUS_HIGH_RES2 0x11
#define BH1750_CONTINUOUS_LOW_RES 0x13
#define BH1750_ONE_TIME_HIGH_RES 0x20
#define BH1750_ONE_TIME_HIGH_RES2 0x21
#define BH1750_ONE_TIME_LOW_RES 0x23

// Light sensor configuration
#define LIGHT_SENSOR_TIMEOUT_MS 1000
#define LIGHT_SENSOR_MIN_LUX 1.0f      // Minimum detectable light
#define LIGHT_SENSOR_MAX_LUX 65535.0f  // Maximum sensor range

// Brightness mapping configuration
#define LIGHT_BRIGHTNESS_MIN 5         // Minimum global brightness (configurable minimum)
#define LIGHT_BRIGHTNESS_MAX 255       // Maximum global brightness (full bright)
#define LIGHT_THRESHOLD_INDOOR 50.0f   // Typical indoor lighting (lux)
#define LIGHT_THRESHOLD_BRIGHT 500.0f  // Bright indoor/outdoor (lux)

// Light sensor reading structure
typedef struct {
    float lux_value;            // Light intensity in lux
    uint8_t global_brightness;  // Calculated global brightness (20-255)
    bool valid;                 // Reading validity flag
    uint32_t timestamp_ms;      // Reading timestamp
} light_reading_t;

// Core functions
esp_err_t light_sensor_init(void);
esp_err_t light_sensor_deinit(void);
esp_err_t light_sensor_read(light_reading_t *reading);
esp_err_t light_sensor_read_raw_lux(float *lux);

// Brightness mapping functions
uint8_t light_lux_to_brightness(float lux);
bool light_sensor_is_initialized(void);

// Adaptive brightness functions
esp_err_t light_update_global_brightness(void);
esp_err_t light_set_brightness_curve(float indoor_lux, float bright_lux);

#ifdef __cplusplus
}
#endif

#endif  // LIGHT_SENSOR_H