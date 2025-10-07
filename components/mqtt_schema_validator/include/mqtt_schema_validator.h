#ifndef MQTT_SCHEMA_VALIDATOR_H
#define MQTT_SCHEMA_VALIDATOR_H

#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT Schema Validator - Core Data Structures
 * 
 * This component provides JSON schema validation for MQTT messages,
 * enabling robust configuration management and preventing data corruption.
 */

// Version information
#define MQTT_SCHEMA_VALIDATOR_VERSION_MAJOR 1
#define MQTT_SCHEMA_VALIDATOR_VERSION_MINOR 0
#define MQTT_SCHEMA_VALIDATOR_VERSION_PATCH 0

// Configuration limits
#define MQTT_SCHEMA_MAX_SCHEMAS 32
#define MQTT_SCHEMA_MAX_TOPIC_LEN 128
#define MQTT_SCHEMA_MAX_NAME_LEN 64
#define MQTT_SCHEMA_MAX_ERROR_MSG_LEN 256
#define MQTT_SCHEMA_MAX_ERROR_PATH_LEN 128

/**
 * @brief Schema validation result
 */
typedef struct {
    esp_err_t validation_result;                    ///< ESP_OK if valid, error code if invalid
    char error_message[MQTT_SCHEMA_MAX_ERROR_MSG_LEN];  ///< Human-readable error description
    char error_path[MQTT_SCHEMA_MAX_ERROR_PATH_LEN];    ///< JSON path where error occurred
    size_t error_offset;                            ///< Character offset in JSON where error found
} mqtt_schema_validation_result_t;

/**
 * @brief Schema definition structure
 */
typedef struct {
    char schema_name[MQTT_SCHEMA_MAX_NAME_LEN];     ///< Unique schema identifier
    char topic_pattern[MQTT_SCHEMA_MAX_TOPIC_LEN];  ///< MQTT topic pattern (supports + and #)
    const char *json_schema;                        ///< JSON schema definition string
    uint32_t schema_hash;                           ///< Hash for integrity checking
    bool enabled;                                   ///< Whether this schema is active
} mqtt_schema_definition_t;

/**
 * @brief Schema registry statistics
 */
typedef struct {
    uint32_t total_validations;                     ///< Total validation attempts
    uint32_t successful_validations;               ///< Successful validations
    uint32_t failed_validations;                   ///< Failed validations
    uint32_t schema_count;                          ///< Number of registered schemas
    uint32_t last_validation_time_ms;              ///< Timestamp of last validation
} mqtt_schema_stats_t;

/**
 * @brief Initialize the MQTT schema validator
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_schema_validator_init(void);

/**
 * @brief Deinitialize the MQTT schema validator
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_schema_validator_deinit(void);

/**
 * @brief Check if validator is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool mqtt_schema_validator_is_initialized(void);

/**
 * @brief Get validator statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_schema_validator_get_stats(mqtt_schema_stats_t *stats);

/**
 * @brief Reset validator statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_schema_validator_reset_stats(void);

/**
 * @brief Register a schema for topic validation
 * 
 * @param schema Pointer to schema definition structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema);

/**
 * @brief Find schema for a given MQTT topic
 * 
 * @param topic MQTT topic to find schema for
 * @param schema Output pointer to store found schema (set to NULL if not found)
 * @return ESP_OK if schema found, ESP_ERR_NOT_FOUND if no matching schema
 */
esp_err_t mqtt_schema_validator_find_schema(const char *topic, const mqtt_schema_definition_t **schema);

/**
 * @brief Get count of registered schemas
 * 
 * @return Number of registered schemas
 */
uint32_t mqtt_schema_validator_get_schema_count(void);

/**
 * @brief Get list of all registered schemas
 * 
 * @param schemas Array to store schema pointers (must be large enough)
 * @param max_schemas Maximum number of schemas to return
 * @param actual_count Output parameter for actual number of schemas returned
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_schema_validator_get_schemas(const mqtt_schema_definition_t **schemas, 
                                          size_t max_schemas, 
                                          size_t *actual_count);

/**
 * @brief Remove a schema by name
 * 
 * @param schema_name Name of schema to remove
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if schema not found
 */
esp_err_t mqtt_schema_validator_remove_schema(const char *schema_name);

/**
 * @brief Clear all registered schemas
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_schema_validator_clear_schemas(void);

/**
 * @brief Validate JSON payload against registered schema for topic
 * 
 * @param topic MQTT topic to find schema for
 * @param json_payload JSON payload to validate
 * @param result Pointer to validation result structure
 * @return ESP_OK if validation completed (check result->validation_result for actual validation outcome)
 */
esp_err_t mqtt_schema_validator_validate_json(const char *topic, 
                                             const char *json_payload, 
                                             mqtt_schema_validation_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_SCHEMA_VALIDATOR_H */