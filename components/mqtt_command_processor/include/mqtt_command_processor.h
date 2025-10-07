#ifndef MQTT_COMMAND_PROCESSOR_H
#define MQTT_COMMAND_PROCESSOR_H

#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_schema_validator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT Command Processor - Structured Command Framework
 * 
 * This component provides a structured framework for processing MQTT commands
 * with schema validation, parameter extraction, and reliable execution.
 */

// Version information
#define MQTT_COMMAND_PROCESSOR_VERSION_MAJOR 1
#define MQTT_COMMAND_PROCESSOR_VERSION_MINOR 0
#define MQTT_COMMAND_PROCESSOR_VERSION_PATCH 0

// Configuration limits
#define MQTT_COMMAND_MAX_HANDLERS 16
#define MQTT_COMMAND_MAX_NAME_LEN 32
#define MQTT_COMMAND_MAX_PARAMS 8
#define MQTT_COMMAND_MAX_PARAM_NAME_LEN 32
#define MQTT_COMMAND_MAX_RESPONSE_LEN 512

/**
 * @brief Command execution result
 */
typedef enum {
    MQTT_COMMAND_RESULT_SUCCESS = 0,        ///< Command executed successfully
    MQTT_COMMAND_RESULT_INVALID_PARAMS,     ///< Invalid command parameters
    MQTT_COMMAND_RESULT_EXECUTION_FAILED,   ///< Command execution failed
    MQTT_COMMAND_RESULT_NOT_FOUND,          ///< Command not found
    MQTT_COMMAND_RESULT_SCHEMA_INVALID,     ///< Schema validation failed
    MQTT_COMMAND_RESULT_SYSTEM_ERROR        ///< System error (memory, mutex, etc.)
} mqtt_command_result_t;

/**
 * @brief Command parameter structure
 */
typedef struct {
    char name[MQTT_COMMAND_MAX_PARAM_NAME_LEN];     ///< Parameter name
    cJSON *value;                                   ///< Parameter value (JSON object)
    bool required;                                  ///< Whether parameter is required
} mqtt_command_param_t;

/**
 * @brief Command execution context
 */
typedef struct {
    const char *command_name;                       ///< Name of the command being executed
    const char *topic;                              ///< MQTT topic where command was received
    const char *payload;                            ///< Original JSON payload
    cJSON *json_root;                               ///< Parsed JSON root object
    cJSON *parameters;                              ///< Parameters object from JSON
    size_t param_count;                             ///< Number of parameters
    mqtt_command_param_t params[MQTT_COMMAND_MAX_PARAMS]; ///< Extracted parameters
    char response[MQTT_COMMAND_MAX_RESPONSE_LEN];   ///< Response message to send back
} mqtt_command_context_t;

/**
 * @brief Command handler function signature
 * 
 * @param context Command execution context with parameters and response buffer
 * @return Command execution result
 */
typedef mqtt_command_result_t (*mqtt_command_handler_t)(mqtt_command_context_t *context);

/**
 * @brief Command definition structure
 */
typedef struct {
    char command_name[MQTT_COMMAND_MAX_NAME_LEN];   ///< Unique command name
    mqtt_command_handler_t handler;                 ///< Function to execute command
    const char *description;                        ///< Human-readable description
    const char *schema_name;                        ///< Associated schema name for validation
    bool enabled;                                   ///< Whether command is active
    uint32_t execution_count;                       ///< Number of times executed
    uint32_t success_count;                         ///< Number of successful executions
    uint32_t failure_count;                         ///< Number of failed executions
    uint32_t last_execution_time_ms;               ///< Timestamp of last execution
} mqtt_command_definition_t;

/**
 * @brief Command processor statistics
 */
typedef struct {
    uint32_t total_commands_processed;              ///< Total commands processed
    uint32_t successful_commands;                   ///< Successfully executed commands
    uint32_t failed_commands;                       ///< Failed command executions
    uint32_t schema_validation_failures;           ///< Commands that failed schema validation
    uint32_t command_not_found_count;              ///< Commands not found in registry
    uint32_t registered_command_count;             ///< Number of registered commands
    uint32_t last_command_time_ms;                 ///< Timestamp of last command
} mqtt_command_stats_t;

/**
 * @brief Initialize the MQTT command processor
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_command_processor_init(void);

/**
 * @brief Deinitialize the MQTT command processor
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_command_processor_deinit(void);

/**
 * @brief Check if command processor is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool mqtt_command_processor_is_initialized(void);

/**
 * @brief Register a command handler
 * 
 * @param command Pointer to command definition structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_command_processor_register_command(const mqtt_command_definition_t *command);

/**
 * @brief Process an MQTT command with schema validation
 * 
 * @param topic MQTT topic where command was received
 * @param payload JSON command payload
 * @param response Buffer to store response message (optional, can be NULL)
 * @param response_len Size of response buffer
 * @return Command execution result
 */
mqtt_command_result_t mqtt_command_processor_execute(const char *topic, 
                                                    const char *payload,
                                                    char *response,
                                                    size_t response_len);

/**
 * @brief Get command processor statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_command_processor_get_stats(mqtt_command_stats_t *stats);

/**
 * @brief Reset command processor statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_command_processor_reset_stats(void);

/**
 * @brief Get list of registered commands
 * 
 * @param commands Array to store command pointers (must be large enough)
 * @param max_commands Maximum number of commands to return
 * @param actual_count Output parameter for actual number of commands returned
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_command_processor_get_commands(const mqtt_command_definition_t **commands, 
                                            size_t max_commands, 
                                            size_t *actual_count);

/**
 * @brief Remove a command by name
 * 
 * @param command_name Name of command to remove
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if command not found
 */
esp_err_t mqtt_command_processor_remove_command(const char *command_name);

/**
 * @brief Clear all registered commands
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_command_processor_clear_commands(void);

/**
 * @brief Helper function to extract string parameter from command context
 * 
 * @param context Command execution context
 * @param param_name Parameter name to extract
 * @param default_value Default value if parameter not found
 * @return Parameter value as string, or default_value if not found
 */
const char* mqtt_command_get_string_param(const mqtt_command_context_t *context, 
                                         const char *param_name, 
                                         const char *default_value);

/**
 * @brief Helper function to extract integer parameter from command context
 * 
 * @param context Command execution context
 * @param param_name Parameter name to extract
 * @param default_value Default value if parameter not found
 * @return Parameter value as integer, or default_value if not found
 */
int mqtt_command_get_int_param(const mqtt_command_context_t *context, 
                              const char *param_name, 
                              int default_value);

/**
 * @brief Helper function to extract boolean parameter from command context
 * 
 * @param context Command execution context
 * @param param_name Parameter name to extract
 * @param default_value Default value if parameter not found
 * @return Parameter value as boolean, or default_value if not found
 */
bool mqtt_command_get_bool_param(const mqtt_command_context_t *context, 
                                const char *param_name, 
                                bool default_value);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_COMMAND_PROCESSOR_H */