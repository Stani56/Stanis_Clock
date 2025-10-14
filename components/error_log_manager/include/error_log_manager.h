#ifndef ERROR_LOG_MANAGER_H
#define ERROR_LOG_MANAGER_H

#include <stdint.h>
#include <time.h>
#include <string.h>
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#define ERROR_LOG_MAX_ENTRIES       50      // Circular buffer size
#define ERROR_LOG_CONTEXT_SIZE      32      // Bytes for error-specific context
#define ERROR_LOG_MESSAGE_SIZE      64      // Bytes for human-readable message
#define ERROR_LOG_NVS_NAMESPACE     "error_log"

// ============================================================================
// Error Sources
// ============================================================================

typedef enum {
    ERROR_SOURCE_UNKNOWN            = 0x00,
    ERROR_SOURCE_LED_VALIDATION     = 0x01,
    ERROR_SOURCE_I2C_BUS            = 0x02,
    ERROR_SOURCE_WIFI               = 0x03,
    ERROR_SOURCE_MQTT               = 0x04,
    ERROR_SOURCE_NTP                = 0x05,
    ERROR_SOURCE_SYSTEM             = 0x06,
    ERROR_SOURCE_POWER              = 0x07,
    ERROR_SOURCE_SENSOR             = 0x08,
    ERROR_SOURCE_DISPLAY            = 0x09,
    ERROR_SOURCE_TRANSITION         = 0x0A,
    ERROR_SOURCE_BRIGHTNESS         = 0x0B,
} error_source_t;

// ============================================================================
// Error Severity
// ============================================================================

typedef enum {
    ERROR_SEVERITY_INFO             = 0x00,
    ERROR_SEVERITY_WARNING          = 0x01,
    ERROR_SEVERITY_ERROR            = 0x02,
    ERROR_SEVERITY_CRITICAL         = 0x03,
} error_severity_t;

// ============================================================================
// Error Log Entry Structure
// ============================================================================

typedef struct {
    time_t timestamp;                       // Unix timestamp (from DS3231 RTC)
    uint32_t uptime_sec;                    // System uptime when error occurred
    uint8_t error_source;                   // error_source_t
    uint8_t error_severity;                 // error_severity_t
    uint16_t error_code;                    // Component-specific error code
    uint8_t context[ERROR_LOG_CONTEXT_SIZE]; // Error-specific context data
    char message[ERROR_LOG_MESSAGE_SIZE];   // Human-readable error message
} error_log_entry_t;

// Total size: 8 + 4 + 1 + 1 + 2 + 32 + 64 = 112 bytes

// ============================================================================
// Context Structures (for packing into error_log_entry_t.context)
// ============================================================================

// LED Validation Error Context
typedef struct {
    uint8_t mismatch_count;                 // Number of mismatched LEDs
    uint8_t failure_type;                   // failure_type_t from led_validation
    uint8_t affected_rows[10];              // Bitmask of affected rows
    uint8_t reserved[21];
} __attribute__((packed)) validation_error_context_t;

// I2C Error Context
typedef struct {
    uint8_t device_addr;                    // I2C device address
    uint8_t register_addr;                  // Register address
    uint16_t esp_error_code;                // ESP-IDF error code
    uint8_t retry_count;                    // Number of retries attempted
    uint8_t reserved[27];
} __attribute__((packed)) i2c_error_context_t;

// WiFi Error Context
typedef struct {
    uint8_t disconnect_reason;              // WiFi disconnect reason code
    int8_t rssi;                            // Signal strength at disconnect
    uint8_t channel;                        // WiFi channel
    uint8_t reconnect_attempts;             // Number of reconnect attempts
    uint8_t reserved[28];
} __attribute__((packed)) wifi_error_context_t;

// MQTT Error Context
typedef struct {
    uint16_t mqtt_error_code;               // MQTT-specific error code
    uint8_t connection_state;               // Connection state at error
    uint8_t reconnect_attempts;             // Number of reconnect attempts
    uint8_t reserved[28];
} __attribute__((packed)) mqtt_error_context_t;

// ============================================================================
// Error Statistics Structure
// ============================================================================

typedef struct {
    // Total error counts by source
    uint32_t total_errors;
    uint32_t validation_errors;
    uint32_t i2c_errors;
    uint32_t wifi_errors;
    uint32_t mqtt_errors;
    uint32_t ntp_errors;
    uint32_t system_errors;

    // Total error counts by severity
    uint32_t info_count;
    uint32_t warning_count;
    uint32_t error_count;
    uint32_t critical_count;

    // Last error timestamps
    time_t last_validation_error;
    time_t last_i2c_error;
    time_t last_wifi_error;
    time_t last_mqtt_error;

    // Ring buffer metadata
    uint16_t current_entry_count;           // Number of entries in log (0-50)
    uint16_t head_index;                    // Next write position (0-49)
    uint32_t total_entries_written;         // Total entries ever written (wraps)
} error_log_stats_t;

// ============================================================================
// Public Functions
// ============================================================================

/**
 * @brief Initialize the error log manager
 *
 * Creates NVS namespace if needed, loads metadata from NVS.
 * Must be called before any other error_log functions.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_init(void);

/**
 * @brief Write an error entry to the log
 *
 * Writes error to NVS circular buffer. Thread-safe.
 * Oldest entry automatically overwritten when buffer full.
 *
 * @param entry Pointer to error log entry
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_write(const error_log_entry_t *entry);

/**
 * @brief Read recent error entries from the log
 *
 * Retrieves the most recent N error entries.
 * Entries returned in reverse chronological order (newest first).
 *
 * @param entries Array to store retrieved entries
 * @param max_count Maximum number of entries to retrieve
 * @param actual_count Output parameter for actual entries retrieved
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_read_recent(error_log_entry_t *entries,
                                uint16_t max_count,
                                uint16_t *actual_count);

/**
 * @brief Read a specific error entry by index
 *
 * Index 0 = oldest entry, index (count-1) = newest entry
 *
 * @param index Entry index (0 to current_entry_count-1)
 * @param entry Output parameter for error entry
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index out of range
 */
esp_err_t error_log_read_entry(uint16_t index, error_log_entry_t *entry);

/**
 * @brief Get error log statistics
 *
 * Returns current statistics including entry count, error counts by
 * source/severity, and last error timestamps.
 *
 * @param stats Output parameter for statistics
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_get_stats(error_log_stats_t *stats);

/**
 * @brief Clear all error log entries
 *
 * Erases all error entries from NVS, resets head index.
 * Statistics counters are preserved.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_clear(void);

/**
 * @brief Reset all error statistics
 *
 * Resets all error counters and timestamps.
 * Does NOT clear error log entries.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t error_log_reset_stats(void);

/**
 * @brief Get human-readable error source name
 *
 * @param source Error source enum value
 * @return String name of error source
 */
const char* error_log_get_source_name(error_source_t source);

/**
 * @brief Get human-readable error severity name
 *
 * @param severity Error severity enum value
 * @return String name of error severity
 */
const char* error_log_get_severity_name(error_severity_t severity);

// ============================================================================
// Helper Macros for Logging Errors
// ============================================================================

/**
 * @brief Log an error with minimal context
 *
 * Usage: ERROR_LOG(ERROR_SOURCE_I2C_BUS, ERROR_SEVERITY_ERROR, 0x1234, "I2C timeout");
 */
#define ERROR_LOG(source, severity, code, msg) do { \
    error_log_entry_t entry = { \
        .timestamp = time(NULL), \
        .uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL), \
        .error_source = source, \
        .error_severity = severity, \
        .error_code = code, \
        .context = {0}, \
        .message = "" \
    }; \
    snprintf(entry.message, ERROR_LOG_MESSAGE_SIZE, "%s", msg); \
    error_log_write(&entry); \
} while(0)

/**
 * @brief Log an error with custom context
 *
 * Usage:
 *   validation_error_context_t ctx = {...};
 *   ERROR_LOG_CONTEXT(ERROR_SOURCE_LED_VALIDATION, ERROR_SEVERITY_WARNING,
 *                     0x01, "Validation failed", &ctx, sizeof(ctx));
 */
#define ERROR_LOG_CONTEXT(source, severity, code, msg, ctx_ptr, ctx_size) do { \
    error_log_entry_t entry = { \
        .timestamp = time(NULL), \
        .uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL), \
        .error_source = source, \
        .error_severity = severity, \
        .error_code = code, \
        .context = {0}, \
        .message = "" \
    }; \
    if ((ctx_size) > 0 && (ctx_size) <= ERROR_LOG_CONTEXT_SIZE) { \
        memcpy(entry.context, (ctx_ptr), (ctx_size)); \
    } \
    snprintf(entry.message, ERROR_LOG_MESSAGE_SIZE, "%s", msg); \
    error_log_write(&entry); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // ERROR_LOG_MANAGER_H
