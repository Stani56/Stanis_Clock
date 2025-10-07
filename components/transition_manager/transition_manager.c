#include "transition_manager.h"
#include "animation_curves.h"
#include "i2c_devices.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include <string.h>

// Include thread safety from main directory (relative path)
#include "../../main/thread_safety.h"

static const char *TAG = "transition_manager";

// Global system state
static transition_system_t g_system = {0};

// LED transition states (external dependencies will be added later)
// For now, we'll define the interface but not the storage
#define WORDCLOCK_ROWS 10
#define WORDCLOCK_COLS 16
static led_transition_t g_led_transitions[WORDCLOCK_ROWS][WORDCLOCK_COLS] = {0};

// Animation task handle
static TaskHandle_t g_animation_task = NULL;
static bool g_animation_running = false;

// Default configuration
static const transition_config_t default_config = {
    .enabled = true,
    .default_type = TRANSITION_TYPE_SEQUENTIAL,
    .fade_duration = 1500,  // Slow, elegant transitions (1.5 seconds)
    .hold_duration = 200,
    .curve = CURVE_EASE_OUT,
    .max_concurrent = 50  // Increased for German word clock (ensure no overflow)
};

// Forward declarations
static void animation_task(void *pvParameters);
static esp_err_t update_led_brightness_instant(uint8_t row, uint8_t col, uint8_t brightness);
static uint32_t get_time_ms(void);

esp_err_t transition_manager_init(void)
{
    ESP_LOGI(TAG, "=== TRANSITION MANAGER INIT ===");
    ESP_LOGI(TAG, "Initializing smooth transition system with guaranteed fallback");
    
    // Acquire transition mutex for initialization
    if (thread_safe_transition_lock(pdMS_TO_TICKS(5000)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire transition mutex for init");
        return ESP_ERR_TIMEOUT;
    }
    
    // Initialize system state
    memset(&g_system, 0, sizeof(transition_system_t));
    memcpy(&g_system.config, &default_config, sizeof(transition_config_t));
    
    // Initialize LED transition states
    memset(g_led_transitions, 0, sizeof(g_led_transitions));
    
    // Initialize all LEDs to IDLE state
    for (int row = 0; row < WORDCLOCK_ROWS; row++) {
        for (int col = 0; col < WORDCLOCK_COLS; col++) {
            g_led_transitions[row][col].state = TRANSITION_STATE_IDLE;
            g_led_transitions[row][col].current_brightness = 0;
            g_led_transitions[row][col].target_brightness = 0;
            g_led_transitions[row][col].curve = CURVE_LINEAR;
        }
    }
    
    // Set initial system state
    g_system.active = false;
    g_system.fallback_mode = false;
    g_system.concurrent_count = 0;
    g_system.last_update = get_time_ms();
    
    thread_safe_transition_unlock();
    
    ESP_LOGI(TAG, "✅ Transition manager initialized successfully [BUILD_FIX_v2]");
    ESP_LOGI(TAG, "System enabled: %s", g_system.config.enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Fallback mode: %s", g_system.fallback_mode ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "transition_manager_is_enabled(): %s", transition_manager_is_enabled() ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Default transition type: %d", g_system.config.default_type);
    ESP_LOGI(TAG, "Default curve: %s", animation_curve_get_name(g_system.config.curve));
    ESP_LOGI(TAG, "🔧 CRITICAL: Max concurrent transitions: %d", g_system.config.max_concurrent);
    ESP_LOGI(TAG, "🔧 CRITICAL: Fade duration: %d ms", g_system.config.fade_duration);
    
    return ESP_OK;
}

void transition_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing transition manager");
    
    // Stop animation task first
    transition_manager_stop();
    
    // Complete all transitions instantly
    transition_manager_complete_all();
    
    // Reset system state
    memset(&g_system, 0, sizeof(transition_system_t));
    memset(g_led_transitions, 0, sizeof(g_led_transitions));
    
    ESP_LOGI(TAG, "Transition manager deinitialized");
}

esp_err_t transition_manager_start(void)
{
    ESP_LOGI(TAG, "=== STARTING TRANSITION ANIMATION TASK ===");
    
    if (g_animation_task != NULL) {
        ESP_LOGW(TAG, "Animation task already running");
        return ESP_OK;
    }
    
    // CRITICAL: Set running flag BEFORE creating task to avoid race condition
    g_animation_running = true;
    
    // Create animation task
    BaseType_t task_created = xTaskCreate(
        animation_task,
        "transition_anim",
        4096,                    // Stack size
        NULL,                    // Parameters
        5,                       // Priority (same as main)
        &g_animation_task
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "❌ Failed to create animation task - ENABLING FALLBACK MODE");
        g_system.fallback_mode = true;
        g_animation_running = false;  // Reset on failure
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Animation task started successfully");
    return ESP_OK;
}

void transition_manager_stop(void)
{
    ESP_LOGI(TAG, "Stopping transition animation task");
    
    if (g_animation_task != NULL) {
        g_animation_running = false;
        // Task will delete itself safely
        // Wait a moment for task to self-delete
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Animation task stop requested");
}

esp_err_t transition_request_led(const transition_request_t *request)
{
    // Input validation
    if (request == NULL) {
        ESP_LOGE(TAG, "Invalid transition request (NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (request->row >= WORDCLOCK_ROWS || request->col >= WORDCLOCK_COLS) {
        ESP_LOGE(TAG, "Invalid LED position (%d,%d)", request->row, request->col);
        return ESP_ERR_INVALID_ARG;
    }
    
    // GUARANTEED FALLBACK: If system disabled or in fallback mode, update instantly
    if (!g_system.config.enabled || g_system.fallback_mode) {
        ESP_LOGW(TAG, "🚨 FALLBACK MODE ACTIVE: config.enabled=%s, fallback_mode=%s - LED (%d,%d) instant update", 
                 g_system.config.enabled ? "true" : "false",
                 g_system.fallback_mode ? "true" : "false",
                 request->row, request->col);
        return update_led_brightness_instant(request->row, request->col, request->to_brightness);
    }
    
    // Check if we're at max concurrent transitions
    // FORCE HIGH LIMIT: Use 50 instead of config value to ensure no overflow
    if (g_system.concurrent_count >= 50) {
        ESP_LOGW(TAG, "Max concurrent transitions reached (50, current=%d), using instant update", 
                 g_system.concurrent_count);
        return update_led_brightness_instant(request->row, request->col, request->to_brightness);
    }
    
    // Set up LED transition
    led_transition_t *led = &g_led_transitions[request->row][request->col];
    
    // If LED is already transitioning, complete it first
    if (led->state != TRANSITION_STATE_IDLE && led->state != TRANSITION_STATE_COMPLETE) {
        led->current_brightness = led->target_brightness;
        g_system.concurrent_count--;
    }
    
    // Configure new transition
    led->start_brightness = request->from_brightness;
    led->current_brightness = request->from_brightness;
    led->target_brightness = request->to_brightness;
    led->state = TRANSITION_STATE_FADE_IN; // Simple fade for now
    led->start_time = get_time_ms() + request->delay;
    led->duration = request->duration;
    led->curve = request->curve;
    
    // Validate curve
    if (!animation_curve_is_valid(led->curve)) {
        ESP_LOGW(TAG, "Invalid curve %d, using linear", led->curve);
        led->curve = CURVE_LINEAR;
    }
    
    // Update system state
    g_system.concurrent_count++;
    g_system.active = true;
    
    ESP_LOGI(TAG, "✅ Transition started: LED(%d,%d) %d→%d over %dms using %s curve [concurrent=%d]", 
             request->row, request->col, request->from_brightness, request->to_brightness,
             request->duration, animation_curve_get_name(led->curve), g_system.concurrent_count);
    
    return ESP_OK;
}

esp_err_t transition_request_batch(const transition_request_t *requests, size_t count)
{
    if (requests == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = ESP_OK;
    
    // Process each request, continue even if some fail (fallback behavior)
    for (size_t i = 0; i < count; i++) {
        esp_err_t req_result = transition_request_led(&requests[i]);
        if (req_result != ESP_OK) {
            ESP_LOGW(TAG, "Batch request %zu failed: %s", i, esp_err_to_name(req_result));
            result = req_result; // Record error but continue
        }
    }
    
    ESP_LOGI(TAG, "Batch transition requested: %zu LEDs", count);
    return result;
}

bool transition_manager_is_active(void)
{
    return g_system.active && g_system.concurrent_count > 0;
}

void transition_manager_complete_all(void)
{
    ESP_LOGI(TAG, "=== COMPLETING ALL TRANSITIONS INSTANTLY ===");
    ESP_LOGI(TAG, "Current concurrent count: %d", g_system.concurrent_count);
    
    uint8_t completed_count = 0;
    uint8_t active_count = 0;
    
    for (int row = 0; row < WORDCLOCK_ROWS; row++) {
        for (int col = 0; col < WORDCLOCK_COLS; col++) {
            led_transition_t *led = &g_led_transitions[row][col];
            
            if (led->state != TRANSITION_STATE_IDLE) {
                active_count++;
                
                if (led->state != TRANSITION_STATE_COMPLETE) {
                    // Complete transition instantly
                    led->current_brightness = led->target_brightness;
                    led->state = TRANSITION_STATE_COMPLETE;
                    
                    // Update actual LED hardware
                    update_led_brightness_instant(row, col, led->target_brightness);
                    completed_count++;
                }
                
                // Reset to IDLE state
                led->state = TRANSITION_STATE_IDLE;
            }
        }
    }
    
    // CRITICAL: Force reset system state completely
    g_system.active = false;
    g_system.concurrent_count = 0;
    
    ESP_LOGI(TAG, "Reset complete: %d active found, %d completed instantly, concurrent_count forced to 0", 
             active_count, completed_count);
}

void transition_manager_set_enabled(bool enabled)
{
    bool previous = g_system.config.enabled;
    g_system.config.enabled = enabled;
    
    ESP_LOGI(TAG, "Transitions %s → %s", 
             previous ? "ENABLED" : "DISABLED",
             enabled ? "ENABLED" : "DISABLED");
    
    if (!enabled) {
        // Complete all transitions when disabling
        transition_manager_complete_all();
    }
}

bool transition_manager_is_enabled(void)
{
    return g_system.config.enabled && !g_system.fallback_mode;
}

esp_err_t transition_manager_set_config(const transition_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate configuration values
    if (config->fade_duration < TRANSITION_MIN_DURATION_MS || 
        config->fade_duration > TRANSITION_MAX_DURATION_MS) {
        ESP_LOGE(TAG, "Invalid fade duration: %d ms", config->fade_duration);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!animation_curve_is_valid(config->curve)) {
        ESP_LOGE(TAG, "Invalid curve type: %d", config->curve);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Apply new configuration
    memcpy(&g_system.config, config, sizeof(transition_config_t));
    
    ESP_LOGI(TAG, "Configuration updated:");
    ESP_LOGI(TAG, "  Enabled: %s", config->enabled ? "true" : "false");
    ESP_LOGI(TAG, "  Fade duration: %d ms", config->fade_duration);
    ESP_LOGI(TAG, "  Hold duration: %d ms", config->hold_duration);
    ESP_LOGI(TAG, "  Curve: %s", animation_curve_get_name(config->curve));
    
    return ESP_OK;
}

esp_err_t transition_manager_get_config(transition_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &g_system.config, sizeof(transition_config_t));
    return ESP_OK;
}

bool transition_manager_is_fallback_mode(void)
{
    return g_system.fallback_mode;
}

void transition_manager_enable_fallback_mode(void)
{
    ESP_LOGW(TAG, "🚨 ENABLING EMERGENCY FALLBACK MODE 🚨");
    ESP_LOGW(TAG, "All transitions will be instant from now on");
    
    g_system.fallback_mode = true;
    
    // Complete all current transitions instantly
    transition_manager_complete_all();
}

esp_err_t transition_manager_try_exit_fallback(void)
{
    if (!g_system.fallback_mode) {
        return ESP_OK; // Already not in fallback mode
    }
    
    ESP_LOGI(TAG, "Attempting to exit fallback mode...");
    
    // Check if conditions allow exiting fallback
    if (g_animation_task == NULL) {
        ESP_LOGW(TAG, "Cannot exit fallback: animation task not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Try to exit fallback mode
    g_system.fallback_mode = false;
    ESP_LOGI(TAG, "✅ Successfully exited fallback mode");
    
    return ESP_OK;
}

esp_err_t transition_manager_get_stats(uint8_t *active_transitions, 
                                      float *update_rate, 
                                      uint8_t *cpu_usage)
{
    if (active_transitions) *active_transitions = g_system.concurrent_count;
    if (update_rate) *update_rate = 1000.0f / TRANSITION_UPDATE_INTERVAL_MS; // 20Hz
    if (cpu_usage) *cpu_usage = g_system.concurrent_count * 2; // Rough estimate
    
    return ESP_OK;
}

// Private helper functions

static void animation_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🎬 Animation task started - 20Hz update rate");
    ESP_LOGI(TAG, "🎬 Animation task running=%s, animation_task_handle=%p", 
             g_animation_running ? "true" : "false", (void*)g_animation_task);
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t update_interval = pdMS_TO_TICKS(TRANSITION_UPDATE_INTERVAL_MS);
    
    uint32_t loop_counter = 0;
    
    while (1) {  // CRITICAL: FreeRTOS tasks must never return
        // Check if task should stop
        if (!g_animation_running) {
            ESP_LOGI(TAG, "Animation task stopping - deleting self");
            g_animation_task = NULL;
            vTaskDelete(NULL);  // Delete self safely
            return;  // Should never reach here
        }
        
        loop_counter++;
        uint32_t current_time = get_time_ms();
        bool any_active = false;
        
        // Log every 100 loops (5 seconds at 20Hz) when transitions are active
        if (loop_counter % 100 == 0 && g_system.concurrent_count > 0) {
            ESP_LOGI(TAG, "🎬 Animation loop #%lu: %d active transitions", 
                     loop_counter, g_system.concurrent_count);
        }
        
        // Update all active transitions
        for (int row = 0; row < WORDCLOCK_ROWS; row++) {
            for (int col = 0; col < WORDCLOCK_COLS; col++) {
                led_transition_t *led = &g_led_transitions[row][col];
                
                if (led->state == TRANSITION_STATE_IDLE) {
                    continue;
                }
                
                // Reset completed transitions to IDLE (counter already decremented)
                if (led->state == TRANSITION_STATE_COMPLETE) {
                    led->state = TRANSITION_STATE_IDLE;
                    led->current_brightness = led->target_brightness;  // Ensure final state
                    continue;
                }
                
                // Check if transition should start (handle delay)
                if (current_time < led->start_time) {
                    any_active = true;
                    continue;
                }
                
                // Calculate progress
                uint32_t elapsed = current_time - led->start_time;
                if (elapsed >= led->duration) {
                    // Transition complete - use curve calculation at 100% progress instead of direct jump
                    uint8_t final_brightness = animation_curve_calculate(
                        led->start_brightness,
                        led->target_brightness,
                        1.0f,  // 100% progress
                        led->curve
                    );
                    
                    // DEBUG: Log completion details to identify brightness jumps
                    ESP_LOGI(TAG, "🏁 COMPLETION LED(%d,%d): elapsed=%lums, duration=%lums, current=%d→final=%d, target=%d", 
                             row, col, (unsigned long)elapsed, (unsigned long)led->duration, led->current_brightness, final_brightness, led->target_brightness);
                    
                    led->current_brightness = final_brightness;
                    led->state = TRANSITION_STATE_COMPLETE;
                    update_led_brightness_instant(row, col, final_brightness);
                    
                    // Decrement counter with debug logging
                    if (g_system.concurrent_count > 0) {
                        g_system.concurrent_count--;
                        ESP_LOGD(TAG, "Transition complete LED(%d,%d): concurrent_count=%d", 
                                row, col, g_system.concurrent_count);
                    } else {
                        ESP_LOGW(TAG, "⚠️ Concurrent counter already at 0 when completing LED(%d,%d)", row, col);
                    }
                } else {
                    // Transition in progress
                    float progress = (float)elapsed / (float)led->duration;
                    uint8_t new_brightness = animation_curve_calculate(
                        led->start_brightness,
                        led->target_brightness,
                        progress,
                        led->curve
                    );
                    
                    if (new_brightness != led->current_brightness) {
                        led->current_brightness = new_brightness;
                        
                        // DEBUG: Log brightness updates when needed
                        ESP_LOGD(TAG, "LED(%d,%d): progress=%.3f, brightness=%d", row, col, progress, new_brightness);
                        
                        update_led_brightness_instant(row, col, new_brightness);
                    }
                    
                    any_active = true;
                }
            }
        }
        
        // Update system state
        g_system.active = any_active;
        g_system.last_update = current_time;
        
        // Sleep until next update
        vTaskDelayUntil(&last_wake_time, update_interval);
    }
    
    // Should never reach here due to vTaskDelete above
    ESP_LOGE(TAG, "Animation task reached end - this should never happen!");
    g_animation_task = NULL;
}

// LED hardware update - integrated with actual TLC59116 control
static esp_err_t update_led_brightness_instant(uint8_t row, uint8_t col, uint8_t brightness)
{
    // Validate position
    if (row >= WORDCLOCK_ROWS || col >= WORDCLOCK_COLS) {
        ESP_LOGE(TAG, "Invalid LED position (%d,%d)", row, col);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use existing LED matrix control function
    esp_err_t ret = tlc_set_matrix_led(row, col, brightness);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update LED(%d,%d) to brightness %d: %s", 
                 row, col, brightness, esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "🔧 Hardware update: LED(%d,%d) → brightness %d", row, col, brightness);
    }
    
    return ret;
}

static uint32_t get_time_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}