#include "animation_curves.h"
#include <math.h>
#include "esp_log.h"

static const char *TAG = "animation_curves";

// Curve name lookup table
static const char* curve_names[] = {
    "Linear",
    "Ease-In", 
    "Ease-Out",
    "Ease-In-Out",
    "Bounce"
};

uint8_t animation_curve_calculate(uint8_t start_brightness, 
                                 uint8_t end_brightness,
                                 float progress, 
                                 transition_curve_t curve)
{
    // Input validation and fallback
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    
    // GUARANTEED FALLBACK: If invalid curve or calculation fails, use linear
    float adjusted_progress = progress;
    
    switch (curve) {
        case CURVE_LINEAR:
            // Already set to progress
            break;
            
        case CURVE_EASE_OUT:
            adjusted_progress = animation_curve_ease_out(progress);
            break;
            
        case CURVE_EASE_IN:
            adjusted_progress = animation_curve_ease_in(progress);
            break;
            
        case CURVE_EASE_IN_OUT:
            adjusted_progress = animation_curve_ease_in_out(progress);
            break;
            
        case CURVE_BOUNCE:
            adjusted_progress = animation_curve_bounce(progress);
            break;
            
        default:
            // FALLBACK: Unknown curve type, use linear
            ESP_LOGW(TAG, "Unknown curve type %d, using linear", curve);
            adjusted_progress = progress;
            break;
    }
    
    // Validate adjusted progress and fallback if needed
    if (adjusted_progress < 0.0f || adjusted_progress > 1.0f || isnan(adjusted_progress)) {
        ESP_LOGW(TAG, "Invalid adjusted progress %.3f, using linear fallback", adjusted_progress);
        adjusted_progress = progress;
    }
    
    // Calculate final brightness value
    float brightness_diff = (float)(end_brightness - start_brightness);
    float result = (float)start_brightness + (brightness_diff * adjusted_progress);
    
    // DEBUG: Log curve calculations when needed (commented out for production)
    // ESP_LOGD(TAG, "Curve calc: progress=%.3f→%.3f, result=%d", progress, adjusted_progress, (uint8_t)result);
    
    // Clamp to valid range
    if (result < 0.0f) result = 0.0f;
    if (result > 255.0f) result = 255.0f;
    
    return (uint8_t)result;
}

uint8_t animation_curve_linear_fast(uint8_t start, uint8_t end, uint16_t progress_1000)
{
    // Fast integer-only linear interpolation
    // progress_1000: 0-1000 represents 0.0-1.0
    
    if (progress_1000 >= 1000) return end;
    if (progress_1000 == 0) return start;
    
    int32_t diff = (int32_t)end - (int32_t)start;
    int32_t result = (int32_t)start + ((diff * progress_1000) / 1000);
    
    // Clamp to valid range
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    
    return (uint8_t)result;
}

float animation_curve_ease_out(float progress)
{
    // Ease-out: f(t) = 1 - (1-t)²
    // Natural deceleration
    float inv = 1.0f - progress;
    return 1.0f - (inv * inv);
}

float animation_curve_ease_in(float progress)
{
    // Ease-in: f(t) = t²
    // Natural acceleration
    return progress * progress;
}

float animation_curve_ease_in_out(float progress)
{
    // Ease-in-out: combination of ease-in and ease-out
    if (progress < 0.5f) {
        // First half: ease-in
        return 2.0f * progress * progress;
    } else {
        // Second half: ease-out
        float t = (progress - 0.5f) * 2.0f;
        float ease_out = 1.0f - ((1.0f - t) * (1.0f - t));
        return 0.5f + (ease_out * 0.5f);
    }
}

float animation_curve_bounce(float progress)
{
    // Bounce effect: slight overshoot with settle
    // Uses sine wave for smooth bounce
    if (progress >= 1.0f) return 1.0f;
    
    // Base eased progress
    float base = animation_curve_ease_out(progress);
    
    // Add bounce component (diminishing sine wave)
    float bounce_amplitude = 0.1f * (1.0f - progress); // Diminishing amplitude
    float bounce_frequency = 8.0f; // Bounce frequency
    float bounce = sinf(progress * bounce_frequency * M_PI) * bounce_amplitude;
    
    float result = base + bounce;
    
    // Ensure result stays in valid range
    if (result < 0.0f) result = 0.0f;
    if (result > 1.0f) result = 1.0f;
    
    return result;
}

bool animation_curve_is_valid(transition_curve_t curve)
{
    return (curve >= 0 && curve < CURVE_COUNT);
}

const char* animation_curve_get_name(transition_curve_t curve)
{
    if (!animation_curve_is_valid(curve)) {
        return "Invalid";
    }
    return curve_names[curve];
}