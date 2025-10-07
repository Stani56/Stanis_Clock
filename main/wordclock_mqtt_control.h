#ifndef WORDCLOCK_MQTT_CONTROL_H
#define WORDCLOCK_MQTT_CONTROL_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "transition_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// External variables for MQTT access
extern uint8_t global_brightness;
extern uint8_t potentiometer_individual;
extern bool transition_system_enabled;
extern transition_curve_t transition_curve_fadein;
extern transition_curve_t transition_curve_fadeout;

// MQTT control functions
esp_err_t set_transition_duration(uint16_t duration_ms);
esp_err_t set_transition_curves(const char* fadein_curve, const char* fadeout_curve);
esp_err_t set_individual_brightness(uint8_t brightness);
esp_err_t set_global_brightness(uint8_t brightness);

// Display refresh function
void refresh_current_display(void);

// Transition control
void transition_manager_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_MQTT_CONTROL_H