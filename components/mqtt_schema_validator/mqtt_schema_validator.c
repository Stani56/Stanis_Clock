#include "mqtt_schema_validator.h"
#include <string.h>

static const char *TAG = "mqtt_schema_validator";

// Global state
static bool g_validator_initialized = false;
static SemaphoreHandle_t g_validator_mutex = NULL;
static mqtt_schema_stats_t g_validator_stats = {0};

// Schema registry storage
static mqtt_schema_definition_t g_schema_registry[MQTT_SCHEMA_MAX_SCHEMAS];
static size_t g_schema_count = 0;

/**
 * @brief Initialize the MQTT schema validator
 */
esp_err_t mqtt_schema_validator_init(void)
{
    ESP_LOGI(TAG, "=== MQTT SCHEMA VALIDATOR INIT ===");
    
    if (g_validator_initialized) {
        ESP_LOGW(TAG, "Schema validator already initialized");
        return ESP_OK;
    }
    
    // Create mutex for thread safety
    g_validator_mutex = xSemaphoreCreateMutex();
    if (g_validator_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create validator mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&g_validator_stats, 0, sizeof(mqtt_schema_stats_t));
    
    // Initialize schema registry
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    
    g_validator_initialized = true;
    
    ESP_LOGI(TAG, "✅ MQTT Schema Validator v%d.%d.%d initialized successfully", 
             MQTT_SCHEMA_VALIDATOR_VERSION_MAJOR,
             MQTT_SCHEMA_VALIDATOR_VERSION_MINOR, 
             MQTT_SCHEMA_VALIDATOR_VERSION_PATCH);
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the MQTT schema validator
 */
esp_err_t mqtt_schema_validator_deinit(void)
{
    ESP_LOGI(TAG, "=== MQTT SCHEMA VALIDATOR DEINIT ===");
    
    if (!g_validator_initialized) {
        ESP_LOGW(TAG, "Schema validator not initialized");
        return ESP_OK;
    }
    
    // Clean up mutex
    if (g_validator_mutex != NULL) {
        vSemaphoreDelete(g_validator_mutex);
        g_validator_mutex = NULL;
    }
    
    // Reset statistics
    memset(&g_validator_stats, 0, sizeof(mqtt_schema_stats_t));
    
    // Clear schema registry
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    
    g_validator_initialized = false;
    
    ESP_LOGI(TAG, "✅ MQTT Schema Validator deinitialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Check if validator is initialized
 */
bool mqtt_schema_validator_is_initialized(void)
{
    return g_validator_initialized;
}

/**
 * @brief Get validator statistics
 */
esp_err_t mqtt_schema_validator_get_stats(mqtt_schema_stats_t *stats)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &g_validator_stats, sizeof(mqtt_schema_stats_t));
    
    xSemaphoreGive(g_validator_mutex);
    
    return ESP_OK;
}

/**
 * @brief Reset validator statistics
 */
esp_err_t mqtt_schema_validator_reset_stats(void)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memset(&g_validator_stats, 0, sizeof(mqtt_schema_stats_t));
    
    xSemaphoreGive(g_validator_mutex);
    
    ESP_LOGI(TAG, "Schema validator statistics reset");
    
    return ESP_OK;
}

/**
 * @brief Simple topic pattern matcher for MQTT wildcards
 * 
 * Supports basic MQTT wildcard patterns:
 * - + matches single level
 * - # matches multiple levels (must be at end)
 */
static bool topic_matches_pattern(const char *topic, const char *pattern)
{
    if (topic == NULL || pattern == NULL) {
        return false;
    }
    
    // Simple implementation - exact match for now
    // TODO: Implement full wildcard support in future step
    return (strcmp(topic, pattern) == 0);
}

/**
 * @brief Calculate simple hash for schema integrity
 */
static uint32_t calculate_schema_hash(const mqtt_schema_definition_t *schema)
{
    if (schema == NULL || schema->json_schema == NULL) {
        return 0;
    }
    
    // Simple hash based on schema name and JSON length
    uint32_t hash = 0;
    for (size_t i = 0; schema->schema_name[i] != '\0'; i++) {
        hash = hash * 31 + schema->schema_name[i];
    }
    hash += strlen(schema->json_schema);
    return hash;
}

/**
 * @brief Register a schema for topic validation
 */
esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (schema == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(schema->schema_name) == 0 || 
        strlen(schema->topic_pattern) == 0 || 
        schema->json_schema == NULL) {
        ESP_LOGE(TAG, "Invalid schema: missing required fields");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Check if schema already exists
    for (size_t i = 0; i < g_schema_count; i++) {
        if (strcmp(g_schema_registry[i].schema_name, schema->schema_name) == 0) {
            ESP_LOGW(TAG, "Schema '%s' already exists, replacing", schema->schema_name);
            // Copy new schema over existing one
            memcpy(&g_schema_registry[i], schema, sizeof(mqtt_schema_definition_t));
            g_schema_registry[i].schema_hash = calculate_schema_hash(schema);
            xSemaphoreGive(g_validator_mutex);
            ESP_LOGI(TAG, "Schema '%s' updated successfully", schema->schema_name);
            return ESP_OK;
        }
    }
    
    // Check if we have space for new schema
    if (g_schema_count >= MQTT_SCHEMA_MAX_SCHEMAS) {
        xSemaphoreGive(g_validator_mutex);
        ESP_LOGE(TAG, "Schema registry full (max %d schemas)", MQTT_SCHEMA_MAX_SCHEMAS);
        return ESP_ERR_NO_MEM;
    }
    
    // Add new schema
    memcpy(&g_schema_registry[g_schema_count], schema, sizeof(mqtt_schema_definition_t));
    g_schema_registry[g_schema_count].schema_hash = calculate_schema_hash(schema);
    g_schema_count++;
    
    // Update statistics
    g_validator_stats.schema_count = g_schema_count;
    
    xSemaphoreGive(g_validator_mutex);
    
    ESP_LOGI(TAG, "Schema '%s' registered successfully (pattern: '%s')", 
             schema->schema_name, schema->topic_pattern);
    
    return ESP_OK;
}

/**
 * @brief Find schema for a given MQTT topic
 */
esp_err_t mqtt_schema_validator_find_schema(const char *topic, const mqtt_schema_definition_t **schema)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL || schema == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *schema = NULL;
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Search for matching schema
    for (size_t i = 0; i < g_schema_count; i++) {
        if (g_schema_registry[i].enabled && 
            topic_matches_pattern(topic, g_schema_registry[i].topic_pattern)) {
            *schema = &g_schema_registry[i];
            xSemaphoreGive(g_validator_mutex);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(g_validator_mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Get count of registered schemas
 */
uint32_t mqtt_schema_validator_get_schema_count(void)
{
    if (!g_validator_initialized) {
        return 0;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return 0;
    }
    
    uint32_t count = g_schema_count;
    xSemaphoreGive(g_validator_mutex);
    return count;
}

/**
 * @brief Get list of all registered schemas
 */
esp_err_t mqtt_schema_validator_get_schemas(const mqtt_schema_definition_t **schemas, 
                                          size_t max_schemas, 
                                          size_t *actual_count)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (schemas == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    size_t copy_count = (g_schema_count < max_schemas) ? g_schema_count : max_schemas;
    
    for (size_t i = 0; i < copy_count; i++) {
        schemas[i] = &g_schema_registry[i];
    }
    
    *actual_count = copy_count;
    
    xSemaphoreGive(g_validator_mutex);
    return ESP_OK;
}

/**
 * @brief Remove a schema by name
 */
esp_err_t mqtt_schema_validator_remove_schema(const char *schema_name)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (schema_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find schema to remove
    for (size_t i = 0; i < g_schema_count; i++) {
        if (strcmp(g_schema_registry[i].schema_name, schema_name) == 0) {
            // Move remaining schemas down
            for (size_t j = i; j < g_schema_count - 1; j++) {
                memcpy(&g_schema_registry[j], &g_schema_registry[j + 1], 
                       sizeof(mqtt_schema_definition_t));
            }
            
            // Clear last entry
            memset(&g_schema_registry[g_schema_count - 1], 0, 
                   sizeof(mqtt_schema_definition_t));
            g_schema_count--;
            
            // Update statistics
            g_validator_stats.schema_count = g_schema_count;
            
            xSemaphoreGive(g_validator_mutex);
            ESP_LOGI(TAG, "Schema '%s' removed successfully", schema_name);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(g_validator_mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Clear all registered schemas
 */
esp_err_t mqtt_schema_validator_clear_schemas(void)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Clear all schemas
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    
    // Update statistics
    g_validator_stats.schema_count = 0;
    
    xSemaphoreGive(g_validator_mutex);
    
    ESP_LOGI(TAG, "All schemas cleared");
    return ESP_OK;
}

/**
 * @brief Basic JSON schema type validation engine
 * 
 * This implements a minimal JSON schema validator that supports:
 * - Type validation (object, string, number, boolean, array)
 * - Required properties
 * - Minimum/maximum values for numbers
 * - Enum validation for strings
 */
static esp_err_t validate_json_against_schema(cJSON *json_obj, 
                                             cJSON *schema_obj, 
                                             mqtt_schema_validation_result_t *result)
{
    if (json_obj == NULL || schema_obj == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize result
    result->validation_result = ESP_OK;
    result->error_message[0] = '\0';
    result->error_path[0] = '\0';
    result->error_offset = 0;
    
    // Get type from schema
    cJSON *type_item = cJSON_GetObjectItem(schema_obj, "type");
    if (type_item == NULL || !cJSON_IsString(type_item)) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "Schema missing or invalid 'type' field");
        result->validation_result = ESP_ERR_INVALID_ARG;
        return ESP_OK; // Function succeeded, but validation failed
    }
    
    const char *expected_type = cJSON_GetStringValue(type_item);
    
    // Type validation
    if (strcmp(expected_type, "object") == 0) {
        if (!cJSON_IsObject(json_obj)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Expected object, got %s", cJSON_IsArray(json_obj) ? "array" : 
                    cJSON_IsString(json_obj) ? "string" : 
                    cJSON_IsNumber(json_obj) ? "number" : 
                    cJSON_IsBool(json_obj) ? "boolean" : "unknown");
            result->validation_result = ESP_ERR_INVALID_ARG;
            return ESP_OK;
        }
    } else if (strcmp(expected_type, "string") == 0) {
        if (!cJSON_IsString(json_obj)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Expected string, got %s", cJSON_IsObject(json_obj) ? "object" : 
                    cJSON_IsArray(json_obj) ? "array" : 
                    cJSON_IsNumber(json_obj) ? "number" : 
                    cJSON_IsBool(json_obj) ? "boolean" : "unknown");
            result->validation_result = ESP_ERR_INVALID_ARG;
            return ESP_OK;
        }
    } else if (strcmp(expected_type, "number") == 0) {
        if (!cJSON_IsNumber(json_obj)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Expected number, got %s", cJSON_IsObject(json_obj) ? "object" : 
                    cJSON_IsArray(json_obj) ? "array" : 
                    cJSON_IsString(json_obj) ? "string" : 
                    cJSON_IsBool(json_obj) ? "boolean" : "unknown");
            result->validation_result = ESP_ERR_INVALID_ARG;
            return ESP_OK;
        }
        
        // Validate number range if specified
        cJSON *minimum = cJSON_GetObjectItem(schema_obj, "minimum");
        cJSON *maximum = cJSON_GetObjectItem(schema_obj, "maximum");
        double value = cJSON_GetNumberValue(json_obj);
        
        if (minimum && cJSON_IsNumber(minimum)) {
            if (value < cJSON_GetNumberValue(minimum)) {
                snprintf(result->error_message, sizeof(result->error_message), 
                        "Number %.2f is below minimum %.2f", value, cJSON_GetNumberValue(minimum));
                result->validation_result = ESP_ERR_INVALID_ARG;
                return ESP_OK;
            }
        }
        
        if (maximum && cJSON_IsNumber(maximum)) {
            if (value > cJSON_GetNumberValue(maximum)) {
                snprintf(result->error_message, sizeof(result->error_message), 
                        "Number %.2f is above maximum %.2f", value, cJSON_GetNumberValue(maximum));
                result->validation_result = ESP_ERR_INVALID_ARG;
                return ESP_OK;
            }
        }
    } else if (strcmp(expected_type, "boolean") == 0) {
        if (!cJSON_IsBool(json_obj)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Expected boolean, got %s", cJSON_IsObject(json_obj) ? "object" : 
                    cJSON_IsArray(json_obj) ? "array" : 
                    cJSON_IsString(json_obj) ? "string" : 
                    cJSON_IsNumber(json_obj) ? "number" : "unknown");
            result->validation_result = ESP_ERR_INVALID_ARG;
            return ESP_OK;
        }
    } else if (strcmp(expected_type, "array") == 0) {
        if (!cJSON_IsArray(json_obj)) {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Expected array, got %s", cJSON_IsObject(json_obj) ? "object" : 
                    cJSON_IsString(json_obj) ? "string" : 
                    cJSON_IsNumber(json_obj) ? "number" : 
                    cJSON_IsBool(json_obj) ? "boolean" : "unknown");
            result->validation_result = ESP_ERR_INVALID_ARG;
            return ESP_OK;
        }
    }
    
    // For objects, validate properties and required fields
    if (strcmp(expected_type, "object") == 0) {
        cJSON *properties = cJSON_GetObjectItem(schema_obj, "properties");
        cJSON *required = cJSON_GetObjectItem(schema_obj, "required");
        
        // Validate required properties
        if (required && cJSON_IsArray(required)) {
            cJSON *required_item = NULL;
            cJSON_ArrayForEach(required_item, required) {
                if (cJSON_IsString(required_item)) {
                    const char *required_prop = cJSON_GetStringValue(required_item);
                    if (!cJSON_HasObjectItem(json_obj, required_prop)) {
                        snprintf(result->error_message, sizeof(result->error_message), 
                                "Missing required property: %s", required_prop);
                        snprintf(result->error_path, sizeof(result->error_path), 
                                "/%s", required_prop);
                        result->validation_result = ESP_ERR_INVALID_ARG;
                        return ESP_OK;
                    }
                }
            }
        }
        
        // Validate each property if properties schema is defined
        if (properties && cJSON_IsObject(properties)) {
            cJSON *property = NULL;
            cJSON_ArrayForEach(property, json_obj) {
                const char *prop_name = property->string;
                if (prop_name) {
                    cJSON *prop_schema = cJSON_GetObjectItem(properties, prop_name);
                    if (prop_schema) {
                        // Recursively validate property
                        mqtt_schema_validation_result_t prop_result = {0};
                        esp_err_t prop_ret = validate_json_against_schema(property, prop_schema, &prop_result);
                        if (prop_ret != ESP_OK) {
                            return prop_ret; // Function error
                        }
                        if (prop_result.validation_result != ESP_OK) {
                            // Propagate validation error with path context
                            *result = prop_result;
                            if (strlen(result->error_path) > 0) {
                                char temp_path[MQTT_SCHEMA_MAX_ERROR_PATH_LEN * 2]; // Double size to avoid truncation
                                int written = snprintf(temp_path, sizeof(temp_path), "/%s%s", prop_name, result->error_path);
                                if (written >= 0 && written < (int)sizeof(temp_path)) {
                                    strncpy(result->error_path, temp_path, sizeof(result->error_path) - 1);
                                    result->error_path[sizeof(result->error_path) - 1] = '\0';
                                }
                            } else {
                                snprintf(result->error_path, sizeof(result->error_path), "/%s", prop_name);
                            }
                            return ESP_OK;
                        }
                    }
                }
            }
        }
    }
    
    // For strings, validate enum if specified
    if (strcmp(expected_type, "string") == 0) {
        cJSON *enum_array = cJSON_GetObjectItem(schema_obj, "enum");
        if (enum_array && cJSON_IsArray(enum_array)) {
            const char *string_value = cJSON_GetStringValue(json_obj);
            bool found_in_enum = false;
            
            cJSON *enum_item = NULL;
            cJSON_ArrayForEach(enum_item, enum_array) {
                if (cJSON_IsString(enum_item)) {
                    if (strcmp(string_value, cJSON_GetStringValue(enum_item)) == 0) {
                        found_in_enum = true;
                        break;
                    }
                }
            }
            
            if (!found_in_enum) {
                snprintf(result->error_message, sizeof(result->error_message), 
                        "String '%s' is not in allowed enum values", string_value);
                result->validation_result = ESP_ERR_INVALID_ARG;
                return ESP_OK;
            }
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Validate JSON payload against registered schema for topic
 */
esp_err_t mqtt_schema_validator_validate_json(const char *topic, 
                                             const char *json_payload, 
                                             mqtt_schema_validation_result_t *result)
{
    if (!g_validator_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL || json_payload == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize result
    result->validation_result = ESP_ERR_NOT_FOUND;
    result->error_message[0] = '\0';
    result->error_path[0] = '\0';
    result->error_offset = 0;
    
    // Find schema for topic
    const mqtt_schema_definition_t *schema = NULL;
    esp_err_t find_ret = mqtt_schema_validator_find_schema(topic, &schema);
    if (find_ret != ESP_OK) {
        snprintf(result->error_message, sizeof(result->error_message), 
                "No schema found for topic: %s", topic);
        result->validation_result = ESP_ERR_NOT_FOUND;
        return ESP_OK; // Function succeeded, validation indicates no schema found
    }
    
    // Parse JSON payload
    cJSON *json_obj = cJSON_Parse(json_payload);
    if (json_obj == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        snprintf(result->error_message, sizeof(result->error_message), 
                "Invalid JSON payload");
        if (error_ptr) {
            result->error_offset = error_ptr - json_payload;
        }
        result->validation_result = ESP_ERR_INVALID_ARG;
        return ESP_OK;
    }
    
    // Parse schema JSON
    cJSON *schema_obj = cJSON_Parse(schema->json_schema);
    if (schema_obj == NULL) {
        cJSON_Delete(json_obj);
        snprintf(result->error_message, sizeof(result->error_message), 
                "Invalid schema JSON for topic: %s", topic);
        result->validation_result = ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    
    // Update statistics
    if (xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_validator_stats.total_validations++;
        g_validator_stats.last_validation_time_ms = esp_log_timestamp();
        xSemaphoreGive(g_validator_mutex);
    }
    
    // Perform validation
    esp_err_t validation_ret = validate_json_against_schema(json_obj, schema_obj, result);
    
    // Update statistics based on result
    if (validation_ret == ESP_OK && xSemaphoreTake(g_validator_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (result->validation_result == ESP_OK) {
            g_validator_stats.successful_validations++;
            ESP_LOGI(TAG, "✅ JSON validation passed for topic: %s", topic);
        } else {
            g_validator_stats.failed_validations++;
            ESP_LOGW(TAG, "❌ JSON validation failed for topic: %s - %s", topic, result->error_message);
        }
        xSemaphoreGive(g_validator_mutex);
    }
    
    // Clean up
    cJSON_Delete(json_obj);
    cJSON_Delete(schema_obj);
    
    return validation_ret;
}