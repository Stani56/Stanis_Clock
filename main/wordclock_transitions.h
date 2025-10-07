#ifndef WORDCLOCK_TRANSITIONS_H
#define WORDCLOCK_TRANSITIONS_H

#include "esp_err.h"
#include "wordclock_time.h"
#include "transition_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum transitions for German word clock
#define MAX_TRANSITION_REQUESTS 48

// Transition system variables
extern bool transition_system_enabled;
extern uint16_t transition_duration_ms;
extern transition_curve_t transition_curve_fadein;
extern transition_curve_t transition_curve_fadeout;

// Initialize transition coordination
esp_err_t transitions_init(void);

// Main transition functions
esp_err_t display_german_time_with_transitions(const wordclock_time_t *time);
esp_err_t build_transition_requests(const wordclock_time_t *old_time,
                                   const wordclock_time_t *new_time,
                                   transition_request_t *requests,
                                   size_t max_requests,
                                   size_t *request_count);

// Transition control
esp_err_t set_transition_duration(uint16_t duration_ms);
esp_err_t set_transition_curves(const char* fadein_curve, const char* fadeout_curve);

// Test functions
esp_err_t start_transition_test_mode(void);
esp_err_t stop_transition_test_mode(void);
bool is_transition_test_mode(void);

// Helper function
void sync_led_state_after_transitions(void);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_TRANSITIONS_H