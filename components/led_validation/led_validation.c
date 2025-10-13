#include "led_validation.h"
#include "i2c_devices.h"
#include "wordclock_display.h"  // For build_led_state_matrix() and led_state[]
#include "wordclock_time.h"     // For wordclock_time_t
#include "thread_safety.h"      // For LED state mutex functions
#include "transition_manager.h" // For transition_manager_is_active()
#include "nvs_flash.h"          // For NVS configuration storage
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "led_validation";

// NVS namespace and keys
#define NVS_NAMESPACE "led_valid"
#define NVS_KEY_CONFIG "config"
#define NVS_KEY_VERSION "version"
#define NVS_KEY_RESTART_COUNTER "restart_cnt"
#define CURRENT_CONFIG_VERSION 1

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
// Configuration Functions
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
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ESP_LOGI(TAG, "Loading validation configuration from NVS");

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, will use defaults");
        return ret;
    }

    // Check version
    uint32_t version = 0;
    ret = nvs_get_u32(nvs_handle, NVS_KEY_VERSION, &version);
    if (ret != ESP_OK || version != CURRENT_CONFIG_VERSION) {
        ESP_LOGW(TAG, "Invalid or missing version, will use defaults");
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_VERSION;
    }

    // Load configuration as blob
    size_t length = sizeof(validation_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_CONFIG, config, &length);
    if (ret != ESP_OK || length != sizeof(validation_config_t)) {
        ESP_LOGW(TAG, "Failed to load validation config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ESP_LOGI(TAG, "Validation configuration loaded from NVS successfully");
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t save_validation_config(const validation_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ESP_LOGI(TAG, "Saving validation configuration to NVS");

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // Save version
    uint32_t version = CURRENT_CONFIG_VERSION;
    ret = nvs_set_u32(nvs_handle, NVS_KEY_VERSION, version);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save version: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Save configuration as blob
    ret = nvs_set_blob(nvs_handle, NVS_KEY_CONFIG, config, sizeof(validation_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save validation config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Validation configuration saved to NVS successfully");
    }

    nvs_close(nvs_handle);
    return ret;
}

// ============================================================================
// Statistics Functions
// ============================================================================

void update_statistics(validation_statistics_t *stats, failure_type_t failure)
{
    if (stats == NULL) {
        return;
    }

    // Always increment total validations
    stats->total_validations++;

    if (failure == FAILURE_NONE) {
        // Validation passed, reset consecutive failures
        stats->consecutive_failures = 0;
        return;
    }

    // Validation failed
    stats->validations_failed++;
    stats->consecutive_failures++;

    // Update max consecutive failures
    if (stats->consecutive_failures > stats->max_consecutive_failures) {
        stats->max_consecutive_failures = stats->consecutive_failures;
    }

    // Update failure counters by type
    switch (failure) {
        case FAILURE_HARDWARE_FAULT:
            stats->hardware_fault_count++;
            break;
        case FAILURE_I2C_BUS_FAILURE:
            stats->i2c_bus_failure_count++;
            break;
        case FAILURE_SYSTEMATIC_MISMATCH:
            stats->systematic_mismatch_count++;
            break;
        case FAILURE_PARTIAL_MISMATCH:
            stats->partial_mismatch_count++;
            break;
        case FAILURE_GRPPWM_MISMATCH:
            stats->grppwm_mismatch_count++;
            break;
        case FAILURE_SOFTWARE_ERROR:
            stats->software_error_count++;
            break;
        default:
            break;
    }

    // Update last failure time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats->last_failure_time = tv.tv_sec;

    // Update 24-hour failure count
    // Simple approach: increment counter (could be refined with time-based window)
    stats->failures_last_24h++;
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
    if (stats == NULL) {
        return 100;
    }

    int32_t score = 100;  // Start with perfect score

    // Deduct points for failures in last 24 hours
    // CRITICAL failures: -10 points each
    uint32_t critical_failures_24h = 0;
    if (stats->hardware_fault_count > 0) critical_failures_24h++;
    if (stats->i2c_bus_failure_count > 0) critical_failures_24h++;
    score -= (critical_failures_24h * 10);

    // HIGH severity: -5 points each
    score -= (stats->systematic_mismatch_count > 0) ? 5 : 0;

    // MEDIUM severity: -2 points each
    uint32_t medium_failures = stats->partial_mismatch_count + stats->grppwm_mismatch_count;
    score -= (medium_failures * 2);

    // LOW severity: -1 point each
    score -= (stats->software_error_count > 0) ? 1 : 0;

    // Recovery failure rate penalty: -20 if >10% failures
    if (stats->recovery_attempts > 0) {
        float recovery_failure_rate = (float)stats->recovery_failures / stats->recovery_attempts;
        if (recovery_failure_rate > 0.1f) {
            score -= 20;
        }
    }

    // Consecutive failures penalty: -5 per consecutive failure
    score -= (stats->consecutive_failures * 5);

    // Overall failure rate penalty (if > 5%)
    if (stats->total_validations > 0) {
        float failure_rate = (float)stats->validations_failed / stats->total_validations;
        if (failure_rate > 0.05f) {
            score -= 10;
        }
    }

    // Clamp score to 0-100
    if (score < 0) score = 0;
    if (score > 100) score = 100;

    return (uint8_t)score;
}

// ============================================================================
// Recovery Functions
// ============================================================================

bool recover_hardware_mismatch(const validation_result_enhanced_t *result)
{
    if (result == NULL || result->hardware_mismatch_count == 0) {
        return true;
    }

    ESP_LOGW(TAG, "Attempting to recover from hardware mismatch (%d LEDs)",
             result->hardware_mismatch_count);

    uint16_t recovered = 0;
    uint16_t failed = 0;

    // Rewrite mismatched LEDs
    for (uint16_t i = 0; i < result->hardware_mismatch_count && i < 50; i++) {
        const led_mismatch_t *mismatch = &result->mismatches[i];

        ESP_LOGD(TAG, "Rewriting LED[%d][%d]: expected=%d",
                 mismatch->row, mismatch->col, mismatch->expected);

        esp_err_t ret = tlc_set_matrix_led(mismatch->row, mismatch->col, mismatch->expected);
        if (ret == ESP_OK) {
            recovered++;
        } else {
            failed++;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    ESP_LOGI(TAG, "Hardware mismatch recovery: %d recovered, %d failed", recovered, failed);
    vTaskDelay(pdMS_TO_TICKS(50));

    return (failed == 0);
}

bool recover_hardware_fault(const validation_result_enhanced_t *result)
{
    if (result == NULL || !result->hardware_faults_detected) {
        return true;
    }

    ESP_LOGE(TAG, "Attempting to recover from hardware faults (%d devices)",
             result->devices_with_faults);

    uint8_t recovered = 0;

    for (uint8_t i = 0; i < 10; i++) {
        if (result->eflag_values[i][0] != 0 || result->eflag_values[i][1] != 0) {
            ESP_LOGE(TAG, "Device %d EFLAG: 0x%02X 0x%02X - resetting",
                     i, result->eflag_values[i][0], result->eflag_values[i][1]);

            if (tlc59116_reset_device(i) == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(10));
                if (tlc59116_init_device(i) == ESP_OK) {
                    recovered++;
                }
            }
        }
    }

    ESP_LOGI(TAG, "Hardware fault recovery: %d/%d devices recovered",
             recovered, result->devices_with_faults);
    return (recovered > 0);
}

bool recover_i2c_bus_failure(void)
{
    ESP_LOGE(TAG, "Attempting I2C bus recovery - reinitializing all devices");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (tlc59116_init_all() != ESP_OK) {
        return false;
    }

    // Verify recovery
    uint8_t test_pwm[16];
    uint8_t success_count = 0;
    for (uint8_t i = 0; i < 10; i++) {
        if (tlc_read_pwm_values(i, test_pwm) == ESP_OK) {
            success_count++;
        }
    }

    ESP_LOGI(TAG, "I2C recovery: %d/10 devices responsive", success_count);
    return (success_count >= 8);
}

bool recover_grppwm_mismatch(const validation_result_enhanced_t *result)
{
    if (result == NULL || !result->grppwm_mismatch) {
        return true;
    }

    ESP_LOGW(TAG, "Recovering GRPPWM mismatch (expected: %d)", result->expected_grppwm);

    if (tlc_set_global_brightness(result->expected_grppwm) != ESP_OK) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    // Verify
    uint8_t verify_grppwm[10];
    if (tlc_read_global_brightness(verify_grppwm) != ESP_OK) {
        return false;
    }

    uint8_t correct = 0;
    for (uint8_t i = 0; i < 10; i++) {
        if (verify_grppwm[i] == result->expected_grppwm) {
            correct++;
        }
    }

    ESP_LOGI(TAG, "GRPPWM recovery: %d/10 correct", correct);
    return (correct == 10);
}

bool attempt_recovery(
    const validation_result_enhanced_t *result,
    failure_type_t failure
)
{
    if (result == NULL) {
        return false;
    }

    ESP_LOGW(TAG, "=== RECOVERY: %s ===", get_failure_type_name(failure));

    bool success = false;
    uint8_t max_attempts = 3;

    if (xSemaphoreTake(g_stats_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        g_statistics.recovery_attempts++;
        xSemaphoreGive(g_stats_mutex);
    }

    for (uint8_t attempt = 1; attempt <= max_attempts; attempt++) {
        ESP_LOGI(TAG, "Attempt %d/%d", attempt, max_attempts);

        switch (failure) {
            case FAILURE_HARDWARE_FAULT:
                success = recover_hardware_fault(result);
                break;
            case FAILURE_I2C_BUS_FAILURE:
                success = recover_i2c_bus_failure();
                break;
            case FAILURE_SYSTEMATIC_MISMATCH:
            case FAILURE_PARTIAL_MISMATCH:
                success = recover_hardware_mismatch(result);
                break;
            case FAILURE_GRPPWM_MISMATCH:
                success = recover_grppwm_mismatch(result);
                break;
            default:
                success = true;
                break;
        }

        if (success) {
            ESP_LOGI(TAG, "=== RECOVERY SUCCESS ===");
            break;
        }

        if (attempt < max_attempts) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (xSemaphoreTake(g_stats_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        if (success) {
            g_statistics.recovery_successes++;
        } else {
            g_statistics.recovery_failures++;
            ESP_LOGE(TAG, "=== RECOVERY FAILED ===");
        }
        xSemaphoreGive(g_stats_mutex);
    }

    return success;
}

// ============================================================================
// Task Functions
// ============================================================================

static TaskHandle_t validation_task_handle = NULL;
static bool task_running = false;

static void validation_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Validation task started");

    while (task_running) {
        // Check if validation is enabled
        bool enabled = false;
        uint16_t interval_sec = 300;

        if (xSemaphoreTake(g_config_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            enabled = g_config.validation_enabled;
            interval_sec = g_config.validation_interval_sec;
            xSemaphoreGive(g_config_mutex);
        }

        if (!enabled) {
            ESP_LOGD(TAG, "Validation disabled, skipping");
            vTaskDelay(pdMS_TO_TICKS(60000)); // Check every minute
            continue;
        }

        // Check if transitions are currently active
        if (transition_manager_is_active()) {
            ESP_LOGD(TAG, "Transitions active, postponing validation by 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // Get current time for validation
        wordclock_time_t current_time;
        if (ds3231_get_time_struct(&current_time) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get current time, skipping validation");
            vTaskDelay(pdMS_TO_TICKS(interval_sec * 1000));
            continue;
        }

        // Run validation
        validation_result_enhanced_t result = validate_display_with_hardware(
            current_time.hours,
            current_time.minutes
        );

        // Classify failure
        failure_type_t failure = classify_failure(&result);

        // Update statistics
        if (xSemaphoreTake(g_stats_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            update_statistics(&g_statistics, failure);
            xSemaphoreGive(g_stats_mutex);
        }

        // Attempt recovery if needed
        if (failure != FAILURE_NONE) {
            bool recovered = attempt_recovery(&result, failure);

            if (!recovered) {
                // Check if restart is configured
                bool should_restart = false;
                if (xSemaphoreTake(g_config_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    should_restart = should_restart_on_failure(&g_config, failure);
                    xSemaphoreGive(g_config_mutex);
                }

                if (should_restart) {
                    ESP_LOGW(TAG, "Restart configured for %s, restarting in 5s...",
                             get_failure_type_name(failure));
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    esp_restart();
                }
            }
        }

        // Wait for next validation interval
        ESP_LOGD(TAG, "Next validation in %d seconds", interval_sec);
        vTaskDelay(pdMS_TO_TICKS(interval_sec * 1000));
    }

    ESP_LOGI(TAG, "Validation task stopped");
    validation_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t start_validation_task(void)
{
    if (validation_task_handle != NULL) {
        ESP_LOGW(TAG, "Validation task already running");
        return ESP_OK;
    }

    task_running = true;

    BaseType_t ret = xTaskCreate(
        validation_task,
        "led_validation",
        4096,
        NULL,
        5,
        &validation_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create validation task");
        task_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Validation task created successfully");
    return ESP_OK;
}

esp_err_t stop_validation_task(void)
{
    if (validation_task_handle == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping validation task...");
    task_running = false;

    // Wait for task to finish
    int timeout = 10;
    while (validation_task_handle != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (validation_task_handle != NULL) {
        ESP_LOGW(TAG, "Task did not stop gracefully, deleting");
        vTaskDelete(validation_task_handle);
        validation_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Validation task stopped");
    return ESP_OK;
}
