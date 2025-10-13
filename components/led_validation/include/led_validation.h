#ifndef LED_VALIDATION_H
#define LED_VALIDATION_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Failure Type Definitions
// ============================================================================

typedef enum {
    FAILURE_NONE = 0,
    FAILURE_HARDWARE_FAULT,          // EFLAG shows open/short circuit
    FAILURE_I2C_BUS_FAILURE,         // Cannot read any TLC devices
    FAILURE_SYSTEMATIC_MISMATCH,     // >20% LEDs mismatched
    FAILURE_PARTIAL_MISMATCH,        // 1-20% LEDs mismatched
    FAILURE_GRPPWM_MISMATCH,         // Global brightness wrong
    FAILURE_SOFTWARE_ERROR,          // Software array inconsistent
} failure_type_t;

// ============================================================================
// Configuration Structures
// ============================================================================

typedef struct {
    bool restart_on_hardware_fault;
    bool restart_on_i2c_bus_failure;
    bool restart_on_systematic_mismatch;
    bool restart_on_partial_mismatch;
    bool restart_on_grppwm_mismatch;
    uint8_t max_restarts_per_session;
    uint16_t validation_interval_sec;
    bool validation_enabled;
} validation_config_t;

// ============================================================================
// Statistics Structures
// ============================================================================

typedef struct {
    // Failure counters (lifetime)
    uint32_t hardware_fault_count;
    uint32_t i2c_bus_failure_count;
    uint32_t systematic_mismatch_count;
    uint32_t partial_mismatch_count;
    uint32_t grppwm_mismatch_count;
    uint32_t software_error_count;

    // Recovery success rates
    uint32_t recovery_attempts;
    uint32_t recovery_successes;
    uint32_t recovery_failures;

    // Recent failure tracking
    uint8_t failures_last_24h;
    time_t last_failure_time;

    // Consecutive failures
    uint8_t consecutive_failures;
    uint8_t max_consecutive_failures;

    // Restart counters
    uint32_t automatic_restarts;
    time_t last_restart_time;

    // Total validations
    uint32_t total_validations;
    uint32_t validations_failed;

} validation_statistics_t;

// ============================================================================
// Validation Result Structures
// ============================================================================

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t expected;
    uint8_t software;
    uint8_t hardware;
} led_mismatch_t;

typedef struct {
    // Software validation results
    bool software_valid;
    uint8_t software_errors;

    // Hardware readback results
    bool hardware_valid;
    uint8_t hardware_mismatch_count;
    uint8_t hardware_read_failures;

    // Hardware fault detection
    bool hardware_faults_detected;
    uint8_t devices_with_faults;

    // Detailed error tracking
    led_mismatch_t mismatches[50];

    // GRPPWM verification
    uint8_t expected_grppwm;
    uint8_t actual_grppwm[10];
    bool grppwm_mismatch;

    // Hardware fault details
    uint8_t eflag_values[10][2];

    // Overall status
    bool is_valid;

    // Timing
    uint32_t validation_time_ms;

} validation_result_enhanced_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Run diagnostic test on suspicious TLC devices at startup
 *
 * Tests write/readback on TLC devices that show frequent validation mismatches
 * (rows 0, 1, 3, 4 = addresses 0x60, 0x61, 0x63, 0x64)
 *
 * @return esp_err_t ESP_OK if all tests pass, ESP_FAIL if readback issues detected
 */
esp_err_t tlc_diagnostic_test(void);

/**
 * @brief Initialize LED validation system
 *
 * Loads configuration from NVS, initializes statistics
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_validation_init(void);

/**
 * @brief Complete LED display validation with hardware readback
 *
 * Performs comprehensive validation:
 * 1. Software state vs expected state (~2ms)
 * 2. Hardware PWM readback vs software state (~80ms)
 * 3. Hardware fault detection via EFLAG registers (~30ms)
 *
 * Total execution time: ~110ms
 * Recommended frequency: Every 5 minutes
 *
 * @param current_hour Current hour (1-12)
 * @param current_minutes Current minutes (0-59)
 * @return validation_result_enhanced_t Complete validation results
 */
validation_result_enhanced_t validate_display_with_hardware(
    uint8_t current_hour,
    uint8_t current_minutes
);

/**
 * @brief Classify failure type based on validation result
 *
 * @param result Validation result structure
 * @return failure_type_t Classified failure type
 */
failure_type_t classify_failure(const validation_result_enhanced_t *result);

/**
 * @brief Attempt recovery based on failure type
 *
 * @param result Validation result
 * @param failure Failure type
 * @return bool True if recovery successful
 */
bool attempt_recovery(
    const validation_result_enhanced_t *result,
    failure_type_t failure
);

/**
 * @brief Calculate system health score (0-100)
 *
 * @param stats Statistics structure
 * @return uint8_t Health score
 */
uint8_t calculate_validation_health_score(const validation_statistics_t *stats);

// ============================================================================
// Configuration Functions
// ============================================================================

/**
 * @brief Load validation configuration from NVS
 *
 * @param config Configuration structure to populate
 * @return esp_err_t ESP_OK on success
 */
esp_err_t load_validation_config(validation_config_t *config);

/**
 * @brief Save validation configuration to NVS
 *
 * @param config Configuration structure to save
 * @return esp_err_t ESP_OK on success
 */
esp_err_t save_validation_config(const validation_config_t *config);

/**
 * @brief Get default validation configuration
 *
 * @param config Configuration structure to populate with defaults
 */
void get_default_validation_config(validation_config_t *config);

// ============================================================================
// Statistics Functions
// ============================================================================

/**
 * @brief Update statistics after validation
 *
 * @param stats Statistics structure
 * @param failure Failure type (FAILURE_NONE if validation passed)
 */
void update_statistics(validation_statistics_t *stats, failure_type_t failure);

/**
 * @brief Get current validation statistics
 *
 * @param stats Statistics structure to populate
 */
void get_validation_statistics(validation_statistics_t *stats);

// ============================================================================
// Recovery Functions
// ============================================================================

/**
 * @brief Recover from hardware mismatch
 *
 * @param result Validation result with mismatch details
 * @return bool True if recovery successful
 */
bool recover_hardware_mismatch(const validation_result_enhanced_t *result);

/**
 * @brief Recover from hardware fault
 *
 * @param result Validation result with fault details
 * @return bool True if recovery successful
 */
bool recover_hardware_fault(const validation_result_enhanced_t *result);

/**
 * @brief Recover from I2C bus failure
 *
 * @return bool True if recovery successful
 */
bool recover_i2c_bus_failure(void);

/**
 * @brief Recover from GRPPWM mismatch
 *
 * @param result Validation result with GRPPWM details
 * @return bool True if recovery successful
 */
bool recover_grppwm_mismatch(const validation_result_enhanced_t *result);

// ============================================================================
// Task Functions
// ============================================================================

/**
 * @brief Start periodic validation task
 *
 * Creates FreeRTOS task that runs validation every 5 minutes
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t start_validation_task(void);

/**
 * @brief Stop periodic validation task
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t stop_validation_task(void);

/**
 * @brief Trigger validation immediately after transition completes
 *
 * This function should be called right after sync_led_state_after_transitions().
 * It validates hardware PWM state when auto-increment pointer is fresh from
 * sequential transition writes, providing most reliable readback.
 *
 * Benefits of post-transition validation:
 * - Auto-increment pointer in known state (just wrote all LEDs sequentially)
 * - Display stable for ~5 minutes until next time change
 * - Current time and brightness are known
 * - Can detect both incorrect active LEDs AND accidentally lit unused LEDs
 *
 * @return esp_err_t ESP_OK if validation triggered successfully
 */
esp_err_t trigger_validation_post_transition(void);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get failure type name as string
 *
 * @param failure Failure type
 * @return const char* Failure type name
 */
const char* get_failure_type_name(failure_type_t failure);

/**
 * @brief Check if restart is configured for failure type
 *
 * @param config Configuration
 * @param failure Failure type
 * @return bool True if restart is configured
 */
bool should_restart_on_failure(
    const validation_config_t *config,
    failure_type_t failure
);

#ifdef __cplusplus
}
#endif

#endif // LED_VALIDATION_H
