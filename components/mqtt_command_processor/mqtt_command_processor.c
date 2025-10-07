#include "mqtt_command_processor.h"
#include <string.h>

static const char *TAG = "mqtt_command_processor";

// Global state
static bool g_processor_initialized = false;
static SemaphoreHandle_t g_processor_mutex = NULL;
static mqtt_command_stats_t g_processor_stats = {0};

// Command registry storage
static mqtt_command_definition_t g_command_registry[MQTT_COMMAND_MAX_HANDLERS];
static size_t g_command_count = 0;

/**
 * @brief Initialize the MQTT command processor
 */
esp_err_t mqtt_command_processor_init(void)
{
    ESP_LOGI(TAG, "=== MQTT COMMAND PROCESSOR INIT ===");
    
    if (g_processor_initialized) {
        ESP_LOGW(TAG, "Command processor already initialized");
        return ESP_OK;
    }
    
    // Create mutex for thread safety
    g_processor_mutex = xSemaphoreCreateMutex();
    if (g_processor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create processor mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&g_processor_stats, 0, sizeof(mqtt_command_stats_t));
    
    // Initialize command registry
    memset(g_command_registry, 0, sizeof(g_command_registry));
    g_command_count = 0;
    
    g_processor_initialized = true;
    
    ESP_LOGI(TAG, "✅ MQTT Command Processor v%d.%d.%d initialized successfully", 
             MQTT_COMMAND_PROCESSOR_VERSION_MAJOR,
             MQTT_COMMAND_PROCESSOR_VERSION_MINOR, 
             MQTT_COMMAND_PROCESSOR_VERSION_PATCH);
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the MQTT command processor
 */
esp_err_t mqtt_command_processor_deinit(void)
{
    ESP_LOGI(TAG, "=== MQTT COMMAND PROCESSOR DEINIT ===");
    
    if (!g_processor_initialized) {
        ESP_LOGW(TAG, "Command processor not initialized");
        return ESP_OK;
    }
    
    // Clean up mutex
    if (g_processor_mutex != NULL) {
        vSemaphoreDelete(g_processor_mutex);
        g_processor_mutex = NULL;
    }
    
    // Reset statistics
    memset(&g_processor_stats, 0, sizeof(mqtt_command_stats_t));
    
    // Clear command registry
    memset(g_command_registry, 0, sizeof(g_command_registry));
    g_command_count = 0;
    
    g_processor_initialized = false;
    
    ESP_LOGI(TAG, "✅ MQTT Command Processor deinitialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Check if command processor is initialized
 */
bool mqtt_command_processor_is_initialized(void)
{
    return g_processor_initialized;
}

/**
 * @brief Register a command handler
 */
esp_err_t mqtt_command_processor_register_command(const mqtt_command_definition_t *command)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (command == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(command->command_name) == 0 || command->handler == NULL) {
        ESP_LOGE(TAG, "Invalid command: missing name or handler");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Check if command already exists
    for (size_t i = 0; i < g_command_count; i++) {
        if (strcmp(g_command_registry[i].command_name, command->command_name) == 0) {
            ESP_LOGW(TAG, "Command '%s' already exists, replacing", command->command_name);
            // Copy new command over existing one
            memcpy(&g_command_registry[i], command, sizeof(mqtt_command_definition_t));
            // Reset statistics for replaced command
            g_command_registry[i].execution_count = 0;
            g_command_registry[i].success_count = 0;
            g_command_registry[i].failure_count = 0;
            g_command_registry[i].last_execution_time_ms = 0;
            xSemaphoreGive(g_processor_mutex);
            ESP_LOGI(TAG, "Command '%s' updated successfully", command->command_name);
            return ESP_OK;
        }
    }
    
    // Check if we have space for new command
    if (g_command_count >= MQTT_COMMAND_MAX_HANDLERS) {
        xSemaphoreGive(g_processor_mutex);
        ESP_LOGE(TAG, "Command registry full (max %d commands)", MQTT_COMMAND_MAX_HANDLERS);
        return ESP_ERR_NO_MEM;
    }
    
    // Add new command
    memcpy(&g_command_registry[g_command_count], command, sizeof(mqtt_command_definition_t));
    // Initialize statistics for new command
    g_command_registry[g_command_count].execution_count = 0;
    g_command_registry[g_command_count].success_count = 0;
    g_command_registry[g_command_count].failure_count = 0;
    g_command_registry[g_command_count].last_execution_time_ms = 0;
    g_command_count++;
    
    // Update statistics
    g_processor_stats.registered_command_count = g_command_count;
    
    xSemaphoreGive(g_processor_mutex);
    
    ESP_LOGI(TAG, "Command '%s' registered successfully (handler: %p)", 
             command->command_name, command->handler);
    if (command->description) {
        ESP_LOGI(TAG, "  Description: %s", command->description);
    }
    if (command->schema_name) {
        ESP_LOGI(TAG, "  Schema: %s", command->schema_name);
    }
    
    return ESP_OK;
}

/**
 * @brief Find command by name
 */
static mqtt_command_definition_t* find_command(const char *command_name)
{
    if (command_name == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < g_command_count; i++) {
        if (g_command_registry[i].enabled && 
            strcmp(g_command_registry[i].command_name, command_name) == 0) {
            return &g_command_registry[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Extract parameters from JSON into command context
 */
static esp_err_t extract_parameters(mqtt_command_context_t *context)
{
    if (context == NULL || context->json_root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get parameters object
    context->parameters = cJSON_GetObjectItem(context->json_root, "parameters");
    context->param_count = 0;
    
    if (context->parameters == NULL || !cJSON_IsObject(context->parameters)) {
        // No parameters object - this is valid for commands without parameters
        return ESP_OK;
    }
    
    // Extract each parameter
    cJSON *param = NULL;
    cJSON_ArrayForEach(param, context->parameters) {
        if (context->param_count >= MQTT_COMMAND_MAX_PARAMS) {
            ESP_LOGW(TAG, "Maximum parameter count reached (%d)", MQTT_COMMAND_MAX_PARAMS);
            break;
        }
        
        if (param->string) {
            strncpy(context->params[context->param_count].name, param->string, 
                   MQTT_COMMAND_MAX_PARAM_NAME_LEN - 1);
            context->params[context->param_count].name[MQTT_COMMAND_MAX_PARAM_NAME_LEN - 1] = '\0';
            context->params[context->param_count].value = param;
            context->params[context->param_count].required = false; // Default, could be enhanced with schema
            context->param_count++;
        }
    }
    
    ESP_LOGI(TAG, "Extracted %zu parameters from command", context->param_count);
    return ESP_OK;
}

/**
 * @brief Process an MQTT command with schema validation
 */
mqtt_command_result_t mqtt_command_processor_execute(const char *topic, 
                                                    const char *payload,
                                                    char *response,
                                                    size_t response_len)
{
    if (!g_processor_initialized) {
        return MQTT_COMMAND_RESULT_SYSTEM_ERROR;
    }
    
    if (topic == NULL || payload == NULL) {
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    // Update statistics
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_processor_stats.total_commands_processed++;
        g_processor_stats.last_command_time_ms = esp_log_timestamp();
        xSemaphoreGive(g_processor_mutex);
    }
    
    // Parse JSON payload
    cJSON *json_root = cJSON_Parse(payload);
    if (json_root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON payload: %s", payload);
        if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            g_processor_stats.failed_commands++;
            xSemaphoreGive(g_processor_mutex);
        }
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    // Extract command name
    cJSON *command_item = cJSON_GetObjectItem(json_root, "command");
    if (command_item == NULL || !cJSON_IsString(command_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'command' field in payload");
        cJSON_Delete(json_root);
        if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            g_processor_stats.failed_commands++;
            xSemaphoreGive(g_processor_mutex);
        }
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    const char *command_name = cJSON_GetStringValue(command_item);
    ESP_LOGI(TAG, "Processing command: %s", command_name);
    
    // Find command handler
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        cJSON_Delete(json_root);
        return MQTT_COMMAND_RESULT_SYSTEM_ERROR;
    }
    
    mqtt_command_definition_t *command_def = find_command(command_name);
    if (command_def == NULL) {
        ESP_LOGW(TAG, "Command not found: %s", command_name);
        g_processor_stats.command_not_found_count++;
        g_processor_stats.failed_commands++;
        xSemaphoreGive(g_processor_mutex);
        cJSON_Delete(json_root);
        return MQTT_COMMAND_RESULT_NOT_FOUND;
    }
    
    // Schema validation if schema name is specified
    if (command_def->schema_name && strlen(command_def->schema_name) > 0) {
        xSemaphoreGive(g_processor_mutex);
        
        mqtt_schema_validation_result_t validation_result = {0};
        esp_err_t validation_ret = mqtt_schema_validator_validate_json(topic, payload, &validation_result);
        
        if (validation_ret != ESP_OK || validation_result.validation_result != ESP_OK) {
            ESP_LOGW(TAG, "Schema validation failed for command '%s': %s", 
                     command_name, validation_result.error_message);
            cJSON_Delete(json_root);
            if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                g_processor_stats.schema_validation_failures++;
                g_processor_stats.failed_commands++;
                xSemaphoreGive(g_processor_mutex);
            }
            return MQTT_COMMAND_RESULT_SCHEMA_INVALID;
        }
        
        ESP_LOGI(TAG, "Schema validation passed for command '%s'", command_name);
        
        // Re-acquire mutex for command execution
        if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            cJSON_Delete(json_root);
            return MQTT_COMMAND_RESULT_SYSTEM_ERROR;
        }
        
        // Re-find command (in case registry changed)
        command_def = find_command(command_name);
        if (command_def == NULL) {
            ESP_LOGW(TAG, "Command disappeared during validation: %s", command_name);
            g_processor_stats.failed_commands++;
            xSemaphoreGive(g_processor_mutex);
            cJSON_Delete(json_root);
            return MQTT_COMMAND_RESULT_NOT_FOUND;
        }
    }
    
    // Prepare command context
    mqtt_command_context_t context = {0};
    context.command_name = command_name;
    context.topic = topic;
    context.payload = payload;
    context.json_root = json_root;
    
    // Update command statistics
    command_def->execution_count++;
    command_def->last_execution_time_ms = esp_log_timestamp();
    
    xSemaphoreGive(g_processor_mutex);
    
    // Extract parameters
    esp_err_t param_ret = extract_parameters(&context);
    if (param_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to extract parameters for command '%s'", command_name);
        cJSON_Delete(json_root);
        if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            command_def->failure_count++;
            g_processor_stats.failed_commands++;
            xSemaphoreGive(g_processor_mutex);
        }
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    // Execute command
    ESP_LOGI(TAG, "Executing command '%s' with %zu parameters", command_name, context.param_count);
    mqtt_command_result_t result = command_def->handler(&context);
    
    // Update statistics based on result
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (result == MQTT_COMMAND_RESULT_SUCCESS) {
            command_def->success_count++;
            g_processor_stats.successful_commands++;
            ESP_LOGI(TAG, "✅ Command '%s' executed successfully", command_name);
        } else {
            command_def->failure_count++;
            g_processor_stats.failed_commands++;
            ESP_LOGW(TAG, "❌ Command '%s' execution failed: %d", command_name, result);
        }
        xSemaphoreGive(g_processor_mutex);
    }
    
    // Copy response if provided
    if (response && response_len > 0 && strlen(context.response) > 0) {
        strncpy(response, context.response, response_len - 1);
        response[response_len - 1] = '\0';
    }
    
    // Clean up
    cJSON_Delete(json_root);
    
    return result;
}

/**
 * @brief Get command processor statistics
 */
esp_err_t mqtt_command_processor_get_stats(mqtt_command_stats_t *stats)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &g_processor_stats, sizeof(mqtt_command_stats_t));
    
    xSemaphoreGive(g_processor_mutex);
    
    return ESP_OK;
}

/**
 * @brief Reset command processor statistics
 */
esp_err_t mqtt_command_processor_reset_stats(void)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Reset global statistics
    g_processor_stats.total_commands_processed = 0;
    g_processor_stats.successful_commands = 0;
    g_processor_stats.failed_commands = 0;
    g_processor_stats.schema_validation_failures = 0;
    g_processor_stats.command_not_found_count = 0;
    g_processor_stats.last_command_time_ms = 0;
    // Keep registered_command_count
    
    // Reset individual command statistics
    for (size_t i = 0; i < g_command_count; i++) {
        g_command_registry[i].execution_count = 0;
        g_command_registry[i].success_count = 0;
        g_command_registry[i].failure_count = 0;
        g_command_registry[i].last_execution_time_ms = 0;
    }
    
    xSemaphoreGive(g_processor_mutex);
    
    ESP_LOGI(TAG, "Command processor statistics reset");
    
    return ESP_OK;
}

/**
 * @brief Get list of registered commands
 */
esp_err_t mqtt_command_processor_get_commands(const mqtt_command_definition_t **commands, 
                                            size_t max_commands, 
                                            size_t *actual_count)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (commands == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    size_t copy_count = (g_command_count < max_commands) ? g_command_count : max_commands;
    
    for (size_t i = 0; i < copy_count; i++) {
        commands[i] = &g_command_registry[i];
    }
    
    *actual_count = copy_count;
    
    xSemaphoreGive(g_processor_mutex);
    return ESP_OK;
}

/**
 * @brief Remove a command by name
 */
esp_err_t mqtt_command_processor_remove_command(const char *command_name)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (command_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find command to remove
    for (size_t i = 0; i < g_command_count; i++) {
        if (strcmp(g_command_registry[i].command_name, command_name) == 0) {
            // Move remaining commands down
            for (size_t j = i; j < g_command_count - 1; j++) {
                memcpy(&g_command_registry[j], &g_command_registry[j + 1], 
                       sizeof(mqtt_command_definition_t));
            }
            
            // Clear last entry
            memset(&g_command_registry[g_command_count - 1], 0, 
                   sizeof(mqtt_command_definition_t));
            g_command_count--;
            
            // Update statistics
            g_processor_stats.registered_command_count = g_command_count;
            
            xSemaphoreGive(g_processor_mutex);
            ESP_LOGI(TAG, "Command '%s' removed successfully", command_name);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(g_processor_mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Clear all registered commands
 */
esp_err_t mqtt_command_processor_clear_commands(void)
{
    if (!g_processor_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_processor_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Clear all commands
    memset(g_command_registry, 0, sizeof(g_command_registry));
    g_command_count = 0;
    
    // Update statistics
    g_processor_stats.registered_command_count = 0;
    
    xSemaphoreGive(g_processor_mutex);
    
    ESP_LOGI(TAG, "All commands cleared");
    return ESP_OK;
}

/**
 * @brief Helper function to extract string parameter from command context
 */
const char* mqtt_command_get_string_param(const mqtt_command_context_t *context, 
                                         const char *param_name, 
                                         const char *default_value)
{
    if (context == NULL || param_name == NULL) {
        return default_value;
    }
    
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsString(context->params[i].value)) {
                return cJSON_GetStringValue(context->params[i].value);
            }
        }
    }
    
    return default_value;
}

/**
 * @brief Helper function to extract integer parameter from command context
 */
int mqtt_command_get_int_param(const mqtt_command_context_t *context, 
                              const char *param_name, 
                              int default_value)
{
    if (context == NULL || param_name == NULL) {
        return default_value;
    }
    
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsNumber(context->params[i].value)) {
                return (int)cJSON_GetNumberValue(context->params[i].value);
            }
        }
    }
    
    return default_value;
}

/**
 * @brief Helper function to extract boolean parameter from command context
 */
bool mqtt_command_get_bool_param(const mqtt_command_context_t *context, 
                                const char *param_name, 
                                bool default_value)
{
    if (context == NULL || param_name == NULL) {
        return default_value;
    }
    
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsBool(context->params[i].value)) {
                return cJSON_IsTrue(context->params[i].value);
            }
        }
    }
    
    return default_value;
}