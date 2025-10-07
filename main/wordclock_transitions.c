/**
 * @file wordclock_transitions.c
 * @brief Smooth LED transition coordination for wordclock
 * 
 * This module coordinates smooth LED transitions between time changes.
 * It manages transition requests, priority-based word coherence, and
 * interfaces with the transition manager for animation execution.
 */

#include "wordclock_transitions.h"
#include "wordclock_display.h"
#include "wordclock_brightness.h"
#include "i2c_devices.h"
#include "transition_manager.h"
#include "thread_safety.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wordclock_transitions";

// Transition system variables (non-static for MQTT access)
bool transition_system_enabled = true;
uint16_t transition_duration_ms = 2000;  // 2 seconds default
transition_curve_t transition_curve_fadein = CURVE_EASE_IN_OUT;
transition_curve_t transition_curve_fadeout = CURVE_EASE_IN_OUT;

// Static transition buffers to avoid stack allocation
static transition_request_t static_transition_requests[MAX_TRANSITION_REQUESTS];
static bool transition_buffer_in_use = false;
static portMUX_TYPE transition_buffer_spinlock = portMUX_INITIALIZER_UNLOCKED;

// LED state buffers for transition building
static uint8_t static_old_state[WORDCLOCK_ROWS][WORDCLOCK_COLS];
static uint8_t static_new_state[WORDCLOCK_ROWS][WORDCLOCK_COLS];

// Tracking variables
static wordclock_time_t previous_display_time = {0};
static bool first_display = true;

// Transition test system
static TaskHandle_t transition_test_task_handle = NULL;

/**
 * @brief Initialize transition coordination
 */
esp_err_t transitions_init(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING TRANSITION COORDINATION ===");
    
    // Initialize transition manager
    esp_err_t ret = transition_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize transition manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start animation task
    ret = transition_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start animation task: %s", esp_err_to_name(ret));
        // Continue anyway with fallback mode
    }
    
    ESP_LOGI(TAG, "✅ Transition coordination initialized");
    ESP_LOGI(TAG, "Duration: %dms, Fade-in: %d, Fade-out: %d", 
             transition_duration_ms, transition_curve_fadein, transition_curve_fadeout);
    
    return ESP_OK;
}

/**
 * @brief Sync LED state array after transitions complete
 */
void sync_led_state_after_transitions(void)
{
    if (!transition_manager_is_active()) {
        return;  // No sync needed if no transitions
    }
    
    // Wait for transitions to complete
    while (transition_manager_is_active()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Sync LED state array with actual hardware state
    ESP_LOGD(TAG, "🔄 Syncing LED state after transitions");
    
    // The LED state should already be correct from display_german_time
    // This function is mainly for ensuring consistency
}

/**
 * @brief Build transition requests with priority-based word coherence
 */
esp_err_t build_transition_requests(const wordclock_time_t *old_time,
                                   const wordclock_time_t *new_time,
                                   transition_request_t *requests,
                                   size_t max_requests,
                                   size_t *request_count)
{
    if (!old_time || !new_time || !requests || !request_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *request_count = 0;
    
    // Use static buffers with spinlock protection
    // Get current individual brightness before critical section
    uint8_t current_individual = tlc_get_individual_brightness();
    ESP_LOGI(TAG, "🔧 Building transitions with brightness %d", current_individual);
    
    portENTER_CRITICAL(&transition_buffer_spinlock);
    
    // Build old and new LED states
    build_led_state_matrix(old_time, static_old_state);
    build_led_state_matrix(new_time, static_new_state);
    
    // Priority 1: Hour words (rows 4-9) - Most important
    for (uint8_t row = 4; row <= 9 && *request_count < max_requests; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            if (static_old_state[row][col] != static_new_state[row][col]) {
                transition_request_t *req = &requests[*request_count];
                req->row = row;
                req->col = col;
                // Use actual current LED state to prevent brightness jumps/flashes
                req->from_brightness = led_state[row][col];  // Actual current hardware state
                req->to_brightness = static_new_state[row][col] ? current_individual : 0;
                req->duration = transition_duration_ms;
                req->delay = 0;
                req->curve = (req->to_brightness > req->from_brightness) ? 
                            transition_curve_fadein : transition_curve_fadeout;
                (*request_count)++;
                
                if (*request_count >= max_requests) break;
            }
        }
    }
    
    // Priority 2: Minute words and ES IST (rows 0-3)
    for (uint8_t row = 0; row <= 3 && *request_count < max_requests; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            if (static_old_state[row][col] != static_new_state[row][col]) {
                transition_request_t *req = &requests[*request_count];
                req->row = row;
                req->col = col;
                // Use actual current LED state to prevent brightness jumps/flashes
                req->from_brightness = led_state[row][col];  // Actual current hardware state
                req->to_brightness = static_new_state[row][col] ? current_individual : 0;
                req->duration = transition_duration_ms;
                req->delay = 0;
                req->curve = (req->to_brightness > req->from_brightness) ? 
                            transition_curve_fadein : transition_curve_fadeout;
                (*request_count)++;
                
                if (*request_count >= max_requests) break;
            }
        }
    }
    
    // Priority 3: Minute indicators (special case - shorter duration)
    if (*request_count < max_requests) {
        for (uint8_t i = 0; i < 4 && *request_count < max_requests; i++) {
            uint8_t row = 9;
            uint8_t col = 11 + i;
            if (static_old_state[row][col] != static_new_state[row][col]) {
                transition_request_t *req = &requests[*request_count];
                req->row = row;
                req->col = col;
                // Use actual current LED state to prevent brightness jumps/flashes
                req->from_brightness = led_state[row][col];  // Actual current hardware state
                req->to_brightness = static_new_state[row][col] ? current_individual : 0;
                req->duration = transition_duration_ms / 2;  // Faster for indicators
                req->delay = 0;
                req->curve = CURVE_LINEAR;  // Simple linear for indicators
                (*request_count)++;
            }
        }
    }
    
    portEXIT_CRITICAL(&transition_buffer_spinlock);
    
    // Handle overflow LEDs with instant updates
    if (*request_count >= max_requests) {
        int overflow_count = 0;
        ESP_LOGW(TAG, "🚨 Transition limit reached! Processing overflow LEDs instantly");
        
        for (uint8_t row = 0; row < WORDCLOCK_ROWS; row++) {
            for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
                if (static_old_state[row][col] != static_new_state[row][col]) {
                    // Check if this LED already has a transition
                    bool has_transition = false;
                    for (size_t i = 0; i < *request_count; i++) {
                        if (requests[i].row == row && requests[i].col == col) {
                            has_transition = true;
                            break;
                        }
                    }
                    
                    if (!has_transition) {
                        // Apply instant update for overflow LED
                        uint8_t target_brightness = static_new_state[row][col] ? current_individual : 0;
                        tlc_set_matrix_led(row, col, target_brightness);
                        led_state[row][col] = target_brightness;
                        overflow_count++;
                    }
                }
            }
        }
        
        ESP_LOGI(TAG, "🔧 HYBRID UPDATE: %zu smooth transitions + %d instant updates", 
                 *request_count, overflow_count);
    }
    
    ESP_LOGI(TAG, "🔧 Transition building complete: %zu/%zu transitions created", 
             *request_count, max_requests);
    
    return ESP_OK;
}

/**
 * @brief Display time with transition support
 */
esp_err_t display_german_time_with_transitions(const wordclock_time_t *time)
{
    if (!time) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // If transitions disabled or system not ready, use instant mode
    if (!transition_system_enabled || !transition_manager_is_enabled()) {
        ESP_LOGW(TAG, "🚨 TRANSITIONS DISABLED - using instant mode");
        
        esp_err_t ret = display_german_time(time);
        if (ret == ESP_OK) {
            previous_display_time = *time;
            first_display = false;
        }
        return ret;
    }
    
    // Only trigger transitions for actual word changes (5-minute intervals)
    if (!first_display) {
        uint8_t old_base = wordclock_calculate_base_minutes(previous_display_time.minutes);
        uint8_t new_base = wordclock_calculate_base_minutes(time->minutes);
        uint8_t old_hour = wordclock_get_display_hour(previous_display_time.hours, old_base);
        uint8_t new_hour = wordclock_get_display_hour(time->hours, new_base);
        
        // Check if words actually change
        bool words_changed = (old_base != new_base) || 
                            (old_hour != new_hour) || 
                            ((old_base == 0) != (new_base == 0)); // UHR appears/disappears
        
        // If only minute indicators change, use instant update
        if (!words_changed) {
            esp_err_t ret = display_german_time(time);
            if (ret == ESP_OK) {
                previous_display_time = *time;
            }
            return ret;
        }
    }
    
    // Protect transition buffer access
    portENTER_CRITICAL(&transition_buffer_spinlock);
    if (transition_buffer_in_use) {
        portEXIT_CRITICAL(&transition_buffer_spinlock);
        ESP_LOGW(TAG, "Transition buffer in use - falling back to instant mode");
        return display_german_time(time);
    }
    transition_buffer_in_use = true;
    portEXIT_CRITICAL(&transition_buffer_spinlock);
    
    // Build transition requests
    size_t request_count = 0;
    esp_err_t ret = build_transition_requests(
        first_display ? time : &previous_display_time,
        time,
        static_transition_requests,
        MAX_TRANSITION_REQUESTS,
        &request_count
    );
    
    if (ret == ESP_OK && request_count > 0) {
        ESP_LOGI(TAG, "🎬 Executing %zu LED transitions", request_count);
        
        // Submit batch transition request
        ret = transition_request_batch(static_transition_requests, request_count);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Transition request failed - falling back to instant mode");
            ret = display_german_time(time);
        }
    } else {
        // No transitions needed or error - use instant update
        ret = display_german_time(time);
    }
    
    // Update state tracking only (hardware updated by transitions or fallback)
    if (ret == ESP_OK) {
        // Don't call display_german_time() here - it would override transition hardware updates
        // LED state array will be updated by the transition system or fallback display_german_time()
        previous_display_time = *time;
        first_display = false;
    }
    
    // Release transition buffer
    portENTER_CRITICAL(&transition_buffer_spinlock);
    transition_buffer_in_use = false;
    portEXIT_CRITICAL(&transition_buffer_spinlock);
    
    return ret;
}

/**
 * @brief Set transition duration
 */
esp_err_t set_transition_duration(uint16_t duration_ms)
{
    if (duration_ms < 200 || duration_ms > 5000) {
        ESP_LOGE(TAG, "Invalid transition duration: %d ms (must be 200-5000)", duration_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    transition_duration_ms = duration_ms;
    ESP_LOGI(TAG, "🎬 Transition duration updated to %d ms", duration_ms);
    
    return ESP_OK;
}

/**
 * @brief Set transition curves
 */
esp_err_t set_transition_curves(const char* fadein_curve, const char* fadeout_curve)
{
    // Parse fade-in curve
    if (fadein_curve != NULL) {
        transition_curve_t new_fadein = CURVE_EASE_IN_OUT;
        
        if (strcmp(fadein_curve, "linear") == 0) {
            new_fadein = CURVE_LINEAR;
        } else if (strcmp(fadein_curve, "ease_in") == 0) {
            new_fadein = CURVE_EASE_IN;
        } else if (strcmp(fadein_curve, "ease_out") == 0) {
            new_fadein = CURVE_EASE_OUT;
        } else if (strcmp(fadein_curve, "ease_in_out") == 0) {
            new_fadein = CURVE_EASE_IN_OUT;
        } else if (strcmp(fadein_curve, "bounce") == 0) {
            new_fadein = CURVE_BOUNCE;
        }
        
        transition_curve_fadein = new_fadein;
        ESP_LOGI(TAG, "🎬 Fade-in curve updated to %s", fadein_curve);
    }
    
    // Parse fade-out curve
    if (fadeout_curve != NULL) {
        transition_curve_t new_fadeout = CURVE_EASE_IN_OUT;
        
        if (strcmp(fadeout_curve, "linear") == 0) {
            new_fadeout = CURVE_LINEAR;
        } else if (strcmp(fadeout_curve, "ease_in") == 0) {
            new_fadeout = CURVE_EASE_IN;
        } else if (strcmp(fadeout_curve, "ease_out") == 0) {
            new_fadeout = CURVE_EASE_OUT;
        } else if (strcmp(fadeout_curve, "ease_in_out") == 0) {
            new_fadeout = CURVE_EASE_IN_OUT;
        } else if (strcmp(fadeout_curve, "bounce") == 0) {
            new_fadeout = CURVE_BOUNCE;
        }
        
        transition_curve_fadeout = new_fadeout;
        ESP_LOGI(TAG, "🎬 Fade-out curve updated to %s", fadeout_curve);
    }
    
    return ESP_OK;
}

/**
 * @brief Transition test task - changes time every 10 seconds
 */
static void transition_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🎬 Transition test mode started - changing time every 10 seconds");
    
    // Test sequence of times that create interesting transitions
    wordclock_time_t test_times[] = {
        {14, 0, 0, 1, 1, 24},   // ZWEI UHR
        {14, 5, 0, 1, 1, 24},   // FÜNF NACH ZWEI
        {14, 10, 0, 1, 1, 24},  // ZEHN NACH ZWEI  
        {14, 15, 0, 1, 1, 24},  // VIERTEL NACH ZWEI
        {14, 20, 0, 1, 1, 24},  // ZWANZIG NACH ZWEI
        {14, 25, 0, 1, 1, 24},  // FÜNF VOR HALB DREI
        {14, 30, 0, 1, 1, 24},  // HALB DREI
        {14, 35, 0, 1, 1, 24},  // FÜNF NACH HALB DREI
        {14, 40, 0, 1, 1, 24},  // ZWANZIG VOR DREI
        {14, 45, 0, 1, 1, 24},  // VIERTEL VOR DREI
        {14, 50, 0, 1, 1, 24},  // ZEHN VOR DREI
        {14, 55, 0, 1, 1, 24},  // FÜNF VOR DREI
        {15, 0, 0, 1, 1, 24},   // DREI UHR
    };
    
    size_t test_index = 0;
    size_t num_tests = sizeof(test_times) / sizeof(test_times[0]);
    
    while (transition_test_task_handle != NULL) {
        ESP_LOGI(TAG, "🎬 TEST: Displaying %02d:%02d", 
                 test_times[test_index].hours, test_times[test_index].minutes);
        
        display_german_time_with_transitions(&test_times[test_index]);
        
        test_index = (test_index + 1) % num_tests;
        vTaskDelay(pdMS_TO_TICKS(10000));  // 10 second delay
    }
    
    ESP_LOGI(TAG, "Transition test task stopping");
    vTaskDelete(NULL);
}

/**
 * @brief Start transition test mode
 */
esp_err_t start_transition_test_mode(void)
{
    if (transition_test_task_handle != NULL) {
        ESP_LOGW(TAG, "Transition test already running");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(
        transition_test_task,
        "transition_test",
        4096,
        NULL,
        5,
        &transition_test_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create transition test task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Transition test mode started");
    return ESP_OK;
}

/**
 * @brief Stop transition test mode
 */
esp_err_t stop_transition_test_mode(void)
{
    if (transition_test_task_handle != NULL) {
        transition_test_task_handle = NULL;
        vTaskDelay(pdMS_TO_TICKS(100));  // Let task notice the change
        ESP_LOGI(TAG, "✅ Transition test mode stopped");
    }
    return ESP_OK;
}

/**
 * @brief Check if transition test mode is active
 */
bool is_transition_test_mode(void)
{
    return transition_test_task_handle != NULL;
}