#ifndef WORDCLOCK_BRIGHTNESS_H
#define WORDCLOCK_BRIGHTNESS_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Brightness control variables
extern uint8_t global_brightness;
extern uint8_t potentiometer_individual;

// Initialize brightness control subsystems
esp_err_t brightness_control_init(void);

// Brightness control functions
esp_err_t set_individual_brightness(uint8_t brightness);
esp_err_t set_global_brightness(uint8_t brightness);

// Potentiometer control
esp_err_t update_brightness_from_potentiometer(void);

// Light sensor control
esp_err_t start_light_sensor_monitoring(void);
void stop_light_sensor_monitoring(void);

// Test functions
void test_brightness_control(void);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_BRIGHTNESS_H