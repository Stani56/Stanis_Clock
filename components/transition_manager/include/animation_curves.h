#ifndef ANIMATION_CURVES_H
#define ANIMATION_CURVES_H

#include <stdint.h>
#include "transition_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate interpolated brightness value using specified curve
 * 
 * GUARANTEED FALLBACK: If curve calculation fails, returns linear interpolation.
 * 
 * @param start_brightness Starting brightness (0-255)
 * @param end_brightness Target brightness (0-255)
 * @param progress Transition progress (0.0 to 1.0)
 * @param curve Animation curve type
 * @return Interpolated brightness value (0-255)
 */
uint8_t animation_curve_calculate(uint8_t start_brightness, 
                                 uint8_t end_brightness,
                                 float progress, 
                                 transition_curve_t curve);

/**
 * @brief Fast integer-based linear interpolation
 * 
 * Optimized version using only integer math for performance.
 * 
 * @param start Starting value (0-255)
 * @param end Target value (0-255)
 * @param progress Progress as integer (0-1000 represents 0.0-1.0)
 * @return Interpolated value (0-255)
 */
uint8_t animation_curve_linear_fast(uint8_t start, uint8_t end, uint16_t progress_1000);

/**
 * @brief Ease-out curve (natural deceleration)
 * 
 * @param progress Transition progress (0.0 to 1.0)
 * @return Curve-adjusted progress (0.0 to 1.0)
 */
float animation_curve_ease_out(float progress);

/**
 * @brief Ease-in curve (natural acceleration)
 * 
 * @param progress Transition progress (0.0 to 1.0)
 * @return Curve-adjusted progress (0.0 to 1.0)
 */
float animation_curve_ease_in(float progress);

/**
 * @brief Ease-in-out curve (acceleration then deceleration)
 * 
 * @param progress Transition progress (0.0 to 1.0)
 * @return Curve-adjusted progress (0.0 to 1.0)
 */
float animation_curve_ease_in_out(float progress);

/**
 * @brief Bounce curve (slight overshoot with settle)
 * 
 * @param progress Transition progress (0.0 to 1.0)
 * @return Curve-adjusted progress (0.0 to 1.0)
 */
float animation_curve_bounce(float progress);

/**
 * @brief Validate curve type
 * 
 * @param curve Curve type to validate
 * @return true if valid, false if invalid (use linear fallback)
 */
bool animation_curve_is_valid(transition_curve_t curve);

/**
 * @brief Get curve name for debugging/logging
 * 
 * @param curve Curve type
 * @return String name of curve
 */
const char* animation_curve_get_name(transition_curve_t curve);

#ifdef __cplusplus
}
#endif

#endif // ANIMATION_CURVES_H