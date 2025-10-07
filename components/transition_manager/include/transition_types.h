#ifndef TRANSITION_TYPES_H
#define TRANSITION_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// Transition system configuration
#define TRANSITION_MAX_DURATION_MS     2000
#define TRANSITION_MIN_DURATION_MS     200
#define TRANSITION_UPDATE_INTERVAL_MS  50    // 20Hz updates
#define TRANSITION_MAX_CONCURRENT      50    // Max simultaneous transitions (increased for German word clock)

// Animation curves
typedef enum {
    CURVE_LINEAR = 0,
    CURVE_EASE_IN,
    CURVE_EASE_OUT,
    CURVE_EASE_IN_OUT,
    CURVE_BOUNCE,
    CURVE_COUNT
} transition_curve_t;

// Transition states
typedef enum {
    TRANSITION_STATE_IDLE = 0,
    TRANSITION_STATE_FADE_OUT,
    TRANSITION_STATE_HOLD,
    TRANSITION_STATE_FADE_IN,
    TRANSITION_STATE_COMPLETE,
    TRANSITION_STATE_ERROR
} transition_state_t;

// Transition types for different word changes
typedef enum {
    TRANSITION_TYPE_INSTANT = 0,    // Fallback mode - no transitions
    TRANSITION_TYPE_SIMPLE,         // Direct crossfade
    TRANSITION_TYPE_SEQUENTIAL,     // Fade out → hold → fade in
    TRANSITION_TYPE_STAGGERED       // Complex timing
} transition_type_t;

// Individual LED transition data
typedef struct {
    uint8_t current_brightness;     // Current PWM value (0-255)
    uint8_t target_brightness;      // Target PWM value (0-255)
    uint8_t start_brightness;       // Starting PWM value for this transition
    transition_state_t state;       // Current transition state
    uint32_t start_time;           // Transition start time (ms)
    uint16_t duration;             // Transition duration (ms)
    transition_curve_t curve;      // Animation curve type
} led_transition_t;

// Transition configuration
typedef struct {
    bool enabled;                   // Master enable/disable
    transition_type_t default_type; // Default transition type
    uint16_t fade_duration;        // Default fade duration (ms)
    uint16_t hold_duration;        // Hold phase duration (ms)
    transition_curve_t curve;      // Default animation curve
    uint8_t max_concurrent;        // Max concurrent transitions
} transition_config_t;

// System-wide transition state
typedef struct {
    bool active;                   // Any transitions currently running
    bool fallback_mode;           // Emergency fallback to instant mode
    uint32_t last_update;         // Last animation update time
    uint8_t concurrent_count;     // Number of active transitions
    transition_config_t config;   // Current configuration
} transition_system_t;

// Transition request structure
typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t from_brightness;
    uint8_t to_brightness;
    uint16_t duration;
    transition_curve_t curve;
    uint32_t delay;               // Delay before starting (ms)
} transition_request_t;

#ifdef __cplusplus
}
#endif

#endif // TRANSITION_TYPES_H