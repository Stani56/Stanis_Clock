#include "led_validation.h"
#include "i2c_devices.h"
#include "wordclock_display.h"  // For build_led_state_matrix() and led_state[]
#include "wordclock_time.h"     // For wordclock_time_t
#include "thread_safety.h"      // For LED state mutex functions
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "led_validation";

// Global statistics (thread-safe access through functions)
static validation_statistics_t g_statistics = {0};
static SemaphoreHandle_t g_stats_mutex = NULL;

// Global configuration (thread-safe access through functions)
static validation_config_t g_config = {0};
static SemaphoreHandle_t g_config_mutex = NULL;

// ============================================================================
// Initialization
// ============================================================================

esp_err_t led_validation_init(void)
{
    ESP_LOGI(TAG, "Initializing LED validation system");

    // Create mutexes
    g_stats_mutex = xSemaphoreCreateMutex();
    if (g_stats_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create statistics mutex");
        return ESP_FAIL;
    }

    g_config_mutex = xSemaphoreCreateMutex();
    if (g_config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create configuration mutex");
        vSemaphoreDelete(g_stats_mutex);
        return ESP_FAIL;
    }

    // Load configuration from NVS
    esp_err_t ret = load_validation_config(&g_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS, using defaults");
        get_default_validation_config(&g_config);
    }

    ESP_LOGI(TAG, "LED validation initialized successfully");
    ESP_LOGI(TAG, "  Validation enabled: %s", g_config.validation_enabled ? "YES" : "NO");
    ESP_LOGI(TAG, "  Validation interval: %d seconds", g_config.validation_interval_sec);
    ESP_LOGI(TAG, "  Auto-restart on I2C failure: %s", g_config.restart_on_i2c_bus_failure ? "YES" : "NO");

    return ESP_OK;
}

// ============================================================================
// Core Validation Function
// ============================================================================

validation_result_enhanced_t validate_display_with_hardware(
    uint8_t current_hour,
    uint8_t current_minutes
)
{
    validation_result_enhanced_t result = {0};
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // ========================================================================
    // LEVEL 1: Software State Validation
    // ========================================================================

    // Generate expected state
    wordclock_time_t ref_time = {
        .hours = current_hour,
        .minutes = current_minutes,
        .seconds = 0
    };
    uint8_t expected_bitmap[WORDCLOCK_ROWS][WORDCLOCK_COLS] = {0};
    build_led_state_matrix(&ref_time, expected_bitmap);

    // Lock LED state for comparison
    if (thread_safe_led_state_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LED state mutex");
        result.is_valid = false;
        return result;
    }

    // Compare software state vs expected
    for (uint8_t row = 0; row < WORDCLOCK_ROWS; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            bool expected = (expected_bitmap[row][col] > 0);
            bool software = (led_state[row][col] > 0);

            if (expected != software) {
                result.software_errors++;
                if (result.software_errors <= 50) {
                    led_mismatch_t *mismatch = &result.mismatches[result.software_errors - 1];
                    mismatch->row = row;
                    mismatch->col = col;
                    mismatch->expected = expected_bitmap[row][col];
                    mismatch->software = led_state[row][col];
                    mismatch->hardware = 0; // Will be filled in Level 2
                }
            }
        }
    }

    thread_safe_led_state_unlock();

    result.software_valid = (result.software_errors == 0);

    ESP_LOGI(TAG, "Level 1 (Software): %s (%d errors)",
             result.software_valid ? "VALID" : "INVALID",
             result.software_errors);

    // ========================================================================
    // LEVEL 2: Hardware State Readback
    // ========================================================================

    uint8_t hardware_pwm[10][16];
    esp_err_t hw_ret = tlc_read_all_pwm_values(hardware_pwm);

    if (hw_ret == ESP_OK) {
        // Lock LED state for comparison
        if (thread_safe_led_state_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to lock LED state mutex for hardware comparison");
            result.hardware_valid = false;
        } else {
            // Compare hardware vs software state
            for (uint8_t row = 0; row < 10; row++) {
                for (uint8_t col = 0; col < 16; col++) {
                    if (hardware_pwm[row][col] == 0xFF) {
                        // Read failed for this device
                        result.hardware_read_failures++;
                        continue;
                    }

                    // Check if hardware matches software
                    // Allow small tolerance (±2) for timing issues during updates
                    int16_t diff = abs((int16_t)hardware_pwm[row][col] -
                                       (int16_t)led_state[row][col]);

                    if (diff > 2) {
                        result.hardware_mismatch_count++;

                        // Record mismatch
                        if (result.hardware_mismatch_count <= 50) {
                            led_mismatch_t *mismatch = &result.mismatches[result.hardware_mismatch_count - 1];
                            mismatch->row = row;
                            mismatch->col = col;
                            mismatch->expected = expected_bitmap[row][col];
                            mismatch->software = led_state[row][col];
                            mismatch->hardware = hardware_pwm[row][col];
                        }
                    }
                }
            }

            thread_safe_led_state_unlock();

            result.hardware_valid = (result.hardware_mismatch_count == 0 &&
                                     result.hardware_read_failures == 0);
        }
    } else {
        ESP_LOGE(TAG, "Hardware readback failed completely");
        result.hardware_valid = false;
        result.hardware_read_failures = 10; // All devices failed
    }

    ESP_LOGI(TAG, "Level 2 (Hardware): %s (mismatches: %d, read failures: %d)",
             result.hardware_valid ? "VALID" : "INVALID",
             result.hardware_mismatch_count,
             result.hardware_read_failures);

    // ========================================================================
    // LEVEL 3: Hardware Fault Detection
    // ========================================================================

    esp_err_t eflag_ret = tlc_read_error_flags(result.eflag_values);

    if (eflag_ret == ESP_FAIL) {
        result.hardware_faults_detected = true;

        // Count devices with faults
        for (uint8_t i = 0; i < 10; i++) {
            if (result.eflag_values[i][0] != 0 ||
                result.eflag_values[i][1] != 0) {
                result.devices_with_faults++;
            }
        }
    }

    // Verify GRPPWM consistency
    result.expected_grppwm = tlc_get_global_brightness();
    tlc_read_global_brightness(result.actual_grppwm);

    for (uint8_t i = 0; i < 10; i++) {
        if (result.actual_grppwm[i] != 0xFF &&
            result.actual_grppwm[i] != result.expected_grppwm) {
            result.grppwm_mismatch = true;
            break;
        }
    }

    ESP_LOGI(TAG, "Level 3 (Faults): hardware_faults=%s, grppwm_mismatch=%s",
             result.hardware_faults_detected ? "YES" : "NO",
             result.grppwm_mismatch ? "YES" : "NO");

    // ========================================================================
    // Overall Validation Result
    // ========================================================================

    result.is_valid = result.software_valid &&
                      result.hardware_valid &&
                      !result.hardware_faults_detected &&
                      !result.grppwm_mismatch;

    uint32_t end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    result.validation_time_ms = end_time - start_time;

    ESP_LOGI(TAG, "=== VALIDATION %s (%.0fms) ===",
             result.is_valid ? "PASSED" : "FAILED",
             (float)result.validation_time_ms);

    return result;
}

// ============================================================================
// Failure Classification
// ============================================================================

failure_type_t classify_failure(const validation_result_enhanced_t *result)
{
    if (result == NULL) {
        return FAILURE_NONE;
    }

    if (result->is_valid) {
        return FAILURE_NONE;
    }

    // CRITICAL: Hardware fault detected (LED open/short circuit)
    if (result->hardware_faults_detected) {
        return FAILURE_HARDWARE_FAULT;
    }

    // CRITICAL: Complete I2C bus failure (cannot read any device)
    if (result->hardware_read_failures >= 10) {
        return FAILURE_I2C_BUS_FAILURE;
    }

    // Calculate mismatch percentage
    uint16_t total_leds = WORDCLOCK_ROWS * WORDCLOCK_COLS; // 160 LEDs
    uint16_t mismatch_count = result->hardware_mismatch_count;
    float mismatch_percentage = (float)mismatch_count / total_leds * 100.0f;

    // HIGH: Systematic mismatch (>20% of LEDs)
    if (mismatch_percentage > 20.0f) {
        return FAILURE_SYSTEMATIC_MISMATCH;
    }

    // MEDIUM: Partial mismatch (1-20% of LEDs)
    if (mismatch_count > 0) {
        return FAILURE_PARTIAL_MISMATCH;
    }

    // MEDIUM: GRPPWM mismatch
    if (result->grppwm_mismatch) {
        return FAILURE_GRPPWM_MISMATCH;
    }

    // LOW: Software state error only (hardware is correct)
    if (result->software_errors > 0 && result->hardware_valid) {
        return FAILURE_SOFTWARE_ERROR;
    }

    // Unknown failure
    return FAILURE_NONE;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* get_failure_type_name(failure_type_t failure)
{
    switch (failure) {
        case FAILURE_NONE:
            return "NONE";
        case FAILURE_HARDWARE_FAULT:
            return "HARDWARE_FAULT";
        case FAILURE_I2C_BUS_FAILURE:
            return "I2C_BUS_FAILURE";
        case FAILURE_SYSTEMATIC_MISMATCH:
            return "SYSTEMATIC_MISMATCH";
        case FAILURE_PARTIAL_MISMATCH:
            return "PARTIAL_MISMATCH";
        case FAILURE_GRPPWM_MISMATCH:
            return "GRPPWM_MISMATCH";
        case FAILURE_SOFTWARE_ERROR:
            return "SOFTWARE_ERROR";
        default:
            return "UNKNOWN";
    }
}

bool should_restart_on_failure(
    const validation_config_t *config,
    failure_type_t failure
)
{
    if (config == NULL) {
        return false;
    }

    switch (failure) {
        case FAILURE_HARDWARE_FAULT:
            return config->restart_on_hardware_fault;
        case FAILURE_I2C_BUS_FAILURE:
            return config->restart_on_i2c_bus_failure;
        case FAILURE_SYSTEMATIC_MISMATCH:
            return config->restart_on_systematic_mismatch;
        case FAILURE_PARTIAL_MISMATCH:
            return config->restart_on_partial_mismatch;
        case FAILURE_GRPPWM_MISMATCH:
            return config->restart_on_grppwm_mismatch;
        case FAILURE_SOFTWARE_ERROR:
        case FAILURE_NONE:
        default:
            return false;
    }
}

// ============================================================================
// Configuration Functions (Stubs for now - will implement in Phase 3a)
// ============================================================================

void get_default_validation_config(validation_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->restart_on_hardware_fault = false;
    config->restart_on_i2c_bus_failure = true;  // Only I2C failure restarts by default
    config->restart_on_systematic_mismatch = false;
    config->restart_on_partial_mismatch = false;
    config->restart_on_grppwm_mismatch = false;
    config->max_restarts_per_session = 3;
    config->validation_interval_sec = 300;  // 5 minutes
    config->validation_enabled = true;
}

esp_err_t load_validation_config(validation_config_t *config)
{
    // TODO: Implement NVS load in Phase 3a
    // For now, just return error to trigger defaults
    return ESP_ERR_NOT_FOUND;
}

esp_err_t save_validation_config(const validation_config_t *config)
{
    // TODO: Implement NVS save in Phase 3a
    return ESP_OK;
}

// ============================================================================
// Statistics Functions (Stubs for now - will implement in Phase 3b)
// ============================================================================

void update_statistics(validation_statistics_t *stats, failure_type_t failure)
{
    // TODO: Implement in Phase 3b
}

void get_validation_statistics(validation_statistics_t *stats)
{
    if (stats == NULL) {
        return;
    }

    if (xSemaphoreTake(g_stats_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        memcpy(stats, &g_statistics, sizeof(validation_statistics_t));
        xSemaphoreGive(g_stats_mutex);
    }
}

uint8_t calculate_validation_health_score(const validation_statistics_t *stats)
{
    // TODO: Implement in Phase 3c
    return 100; // Perfect score for now
}

// ============================================================================
// Recovery Functions (Stubs for now - will implement in Phase 4a)
// ============================================================================

bool attempt_recovery(
    const validation_result_enhanced_t *result,
    failure_type_t failure
)
{
    // TODO: Implement in Phase 4b
    return false;
}

bool recover_hardware_mismatch(const validation_result_enhanced_t *result)
{
    // TODO: Implement in Phase 4a
    return false;
}

bool recover_hardware_fault(const validation_result_enhanced_t *result)
{
    // TODO: Implement in Phase 4a
    return false;
}

bool recover_i2c_bus_failure(void)
{
    // TODO: Implement in Phase 4a
    return false;
}

bool recover_grppwm_mismatch(const validation_result_enhanced_t *result)
{
    // TODO: Implement in Phase 4a
    return false;
}

// ============================================================================
// Task Functions (Stubs for now - will implement in Phase 5c)
// ============================================================================

esp_err_t start_validation_task(void)
{
    // TODO: Implement in Phase 5c
    ESP_LOGI(TAG, "Validation task start requested (not yet implemented)");
    return ESP_OK;
}

esp_err_t stop_validation_task(void)
{
    // TODO: Implement in Phase 5c
    return ESP_OK;
}
