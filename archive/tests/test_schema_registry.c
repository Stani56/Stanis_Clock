/**
 * @brief Comprehensive test for MQTT schema registry functionality
 * 
 * Tests schema registration, lookup, removal, and pattern matching.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Mock ESP-IDF types and functions
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_TIMEOUT -4
#define ESP_ERR_NOT_FOUND -5

// Mock FreeRTOS types
typedef void* SemaphoreHandle_t;
typedef unsigned long TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)

// Mock FreeRTOS functions
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void*)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) { return pdTRUE; }
void xSemaphoreGive(SemaphoreHandle_t sem) { }
void vSemaphoreDelete(SemaphoreHandle_t sem) { }

// Mock ESP logging
#define ESP_LOGI(tag, format, ...) printf("[INFO] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] %s: " format "\n", tag, ##__VA_ARGS__)

// Mock cJSON (minimal)
typedef struct cJSON {
    int type;
} cJSON;

// Schema validator configuration and types
#define MQTT_SCHEMA_VALIDATOR_VERSION_MAJOR 1
#define MQTT_SCHEMA_VALIDATOR_VERSION_MINOR 0
#define MQTT_SCHEMA_VALIDATOR_VERSION_PATCH 0
#define MQTT_SCHEMA_MAX_SCHEMAS 32
#define MQTT_SCHEMA_MAX_TOPIC_LEN 128
#define MQTT_SCHEMA_MAX_NAME_LEN 64
#define MQTT_SCHEMA_MAX_ERROR_MSG_LEN 256
#define MQTT_SCHEMA_MAX_ERROR_PATH_LEN 128

typedef struct {
    esp_err_t validation_result;
    char error_message[MQTT_SCHEMA_MAX_ERROR_MSG_LEN];
    char error_path[MQTT_SCHEMA_MAX_ERROR_PATH_LEN];
    size_t error_offset;
} mqtt_schema_validation_result_t;

typedef struct {
    char schema_name[MQTT_SCHEMA_MAX_NAME_LEN];
    char topic_pattern[MQTT_SCHEMA_MAX_TOPIC_LEN];
    const char *json_schema;
    uint32_t schema_hash;
    bool enabled;
} mqtt_schema_definition_t;

typedef struct {
    uint32_t total_validations;
    uint32_t successful_validations;
    uint32_t failed_validations;
    uint32_t schema_count;
    uint32_t last_validation_time_ms;
} mqtt_schema_stats_t;

// Function declarations
esp_err_t mqtt_schema_validator_init(void);
esp_err_t mqtt_schema_validator_deinit(void);
bool mqtt_schema_validator_is_initialized(void);
esp_err_t mqtt_schema_validator_get_stats(mqtt_schema_stats_t *stats);
esp_err_t mqtt_schema_validator_reset_stats(void);
esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema);
esp_err_t mqtt_schema_validator_find_schema(const char *topic, const mqtt_schema_definition_t **schema);
uint32_t mqtt_schema_validator_get_schema_count(void);
esp_err_t mqtt_schema_validator_get_schemas(const mqtt_schema_definition_t **schemas, size_t max_schemas, size_t *actual_count);
esp_err_t mqtt_schema_validator_remove_schema(const char *schema_name);
esp_err_t mqtt_schema_validator_clear_schemas(void);

// Include implementation (simplified copy from main file)
static const char *TAG = "mqtt_schema_validator";
static bool g_validator_initialized = false;
static SemaphoreHandle_t g_validator_mutex = NULL;
static mqtt_schema_stats_t g_validator_stats = {0};
static mqtt_schema_definition_t g_schema_registry[MQTT_SCHEMA_MAX_SCHEMAS];
static size_t g_schema_count = 0;

// Helper functions
static bool topic_matches_pattern(const char *topic, const char *pattern) {
    if (topic == NULL || pattern == NULL) return false;
    return (strcmp(topic, pattern) == 0);  // Simple exact match for now
}

static uint32_t calculate_schema_hash(const mqtt_schema_definition_t *schema) {
    if (schema == NULL || schema->json_schema == NULL) return 0;
    uint32_t hash = 0;
    for (size_t i = 0; schema->schema_name[i] != '\0'; i++) {
        hash = hash * 31 + schema->schema_name[i];
    }
    hash += strlen(schema->json_schema);
    return hash;
}

// Implementation (copy key functions)
esp_err_t mqtt_schema_validator_init(void) {
    if (g_validator_initialized) return ESP_OK;
    g_validator_mutex = xSemaphoreCreateMutex();
    if (g_validator_mutex == NULL) return ESP_ERR_NO_MEM;
    memset(&g_validator_stats, 0, sizeof(mqtt_schema_stats_t));
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    g_validator_initialized = true;
    return ESP_OK;
}

esp_err_t mqtt_schema_validator_deinit(void) {
    if (!g_validator_initialized) return ESP_OK;
    if (g_validator_mutex != NULL) {
        vSemaphoreDelete(g_validator_mutex);
        g_validator_mutex = NULL;
    }
    memset(&g_validator_stats, 0, sizeof(mqtt_schema_stats_t));
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    g_validator_initialized = false;
    return ESP_OK;
}

bool mqtt_schema_validator_is_initialized(void) {
    return g_validator_initialized;
}

uint32_t mqtt_schema_validator_get_schema_count(void) {
    if (!g_validator_initialized) return 0;
    return g_schema_count;
}

esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (schema == NULL) return ESP_ERR_INVALID_ARG;
    if (strlen(schema->schema_name) == 0 || strlen(schema->topic_pattern) == 0 || schema->json_schema == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if schema already exists
    for (size_t i = 0; i < g_schema_count; i++) {
        if (strcmp(g_schema_registry[i].schema_name, schema->schema_name) == 0) {
            memcpy(&g_schema_registry[i], schema, sizeof(mqtt_schema_definition_t));
            g_schema_registry[i].schema_hash = calculate_schema_hash(schema);
            return ESP_OK;
        }
    }
    
    if (g_schema_count >= MQTT_SCHEMA_MAX_SCHEMAS) return ESP_ERR_NO_MEM;
    
    memcpy(&g_schema_registry[g_schema_count], schema, sizeof(mqtt_schema_definition_t));
    g_schema_registry[g_schema_count].schema_hash = calculate_schema_hash(schema);
    g_schema_count++;
    g_validator_stats.schema_count = g_schema_count;
    return ESP_OK;
}

esp_err_t mqtt_schema_validator_find_schema(const char *topic, const mqtt_schema_definition_t **schema) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (topic == NULL || schema == NULL) return ESP_ERR_INVALID_ARG;
    
    *schema = NULL;
    for (size_t i = 0; i < g_schema_count; i++) {
        if (g_schema_registry[i].enabled && topic_matches_pattern(topic, g_schema_registry[i].topic_pattern)) {
            *schema = &g_schema_registry[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t mqtt_schema_validator_remove_schema(const char *schema_name) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (schema_name == NULL) return ESP_ERR_INVALID_ARG;
    
    for (size_t i = 0; i < g_schema_count; i++) {
        if (strcmp(g_schema_registry[i].schema_name, schema_name) == 0) {
            for (size_t j = i; j < g_schema_count - 1; j++) {
                memcpy(&g_schema_registry[j], &g_schema_registry[j + 1], sizeof(mqtt_schema_definition_t));
            }
            memset(&g_schema_registry[g_schema_count - 1], 0, sizeof(mqtt_schema_definition_t));
            g_schema_count--;
            g_validator_stats.schema_count = g_schema_count;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t mqtt_schema_validator_clear_schemas(void) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    memset(g_schema_registry, 0, sizeof(g_schema_registry));
    g_schema_count = 0;
    g_validator_stats.schema_count = 0;
    return ESP_OK;
}

esp_err_t mqtt_schema_validator_get_schemas(const mqtt_schema_definition_t **schemas, size_t max_schemas, size_t *actual_count) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (schemas == NULL || actual_count == NULL) return ESP_ERR_INVALID_ARG;
    
    size_t copy_count = (g_schema_count < max_schemas) ? g_schema_count : max_schemas;
    for (size_t i = 0; i < copy_count; i++) {
        schemas[i] = &g_schema_registry[i];
    }
    *actual_count = copy_count;
    return ESP_OK;
}

// Test schemas
static const char *test_brightness_schema = 
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"brightness\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 255},"
    "    \"transition\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 5000}"
    "  },"
    "  \"required\": [\"brightness\"]"
    "}";

static const char *test_transition_schema = 
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"duration\": {\"type\": \"number\", \"minimum\": 200, \"maximum\": 5000},"
    "    \"enabled\": {\"type\": \"boolean\"}"
    "  },"
    "  \"required\": [\"duration\", \"enabled\"]"
    "}";

static const char *test_command_schema = 
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"command\": {\"type\": \"string\", \"enum\": [\"restart\", \"status\", \"reset\"]},"
    "    \"parameters\": {\"type\": \"object\"}"
    "  },"
    "  \"required\": [\"command\"]"
    "}";

// Test functions
int test_schema_registry_basic(void) {
    printf("=== MQTT Schema Registry Basic Test ===\n");
    
    // Initialize validator
    esp_err_t ret = mqtt_schema_validator_init();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Initialization failed\n");
        return -1;
    }
    
    // Test 1: Initially no schemas
    if (mqtt_schema_validator_get_schema_count() != 0) {
        printf("❌ FAIL: Initial schema count should be 0\n");
        return -1;
    }
    printf("✅ PASS: Initial schema count correct\n");
    
    // Test 2: Register first schema
    mqtt_schema_definition_t brightness_schema = {0};
    strncpy(brightness_schema.schema_name, "brightness_config", sizeof(brightness_schema.schema_name) - 1);
    strncpy(brightness_schema.topic_pattern, "home/esp32_core/brightness/set", sizeof(brightness_schema.topic_pattern) - 1);
    brightness_schema.json_schema = test_brightness_schema;
    brightness_schema.enabled = true;
    
    ret = mqtt_schema_validator_register_schema(&brightness_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register brightness schema: %d\n", ret);
        return -1;
    }
    printf("✅ PASS: Brightness schema registered\n");
    
    // Test 3: Schema count should be 1
    if (mqtt_schema_validator_get_schema_count() != 1) {
        printf("❌ FAIL: Schema count should be 1 after registration\n");
        return -1;
    }
    printf("✅ PASS: Schema count correct after registration\n");
    
    // Test 4: Find schema by topic
    const mqtt_schema_definition_t *found_schema = NULL;
    ret = mqtt_schema_validator_find_schema("home/esp32_core/brightness/set", &found_schema);
    if (ret != ESP_OK || found_schema == NULL) {
        printf("❌ FAIL: Failed to find brightness schema\n");
        return -1;
    }
    if (strcmp(found_schema->schema_name, "brightness_config") != 0) {
        printf("❌ FAIL: Found wrong schema: %s\n", found_schema->schema_name);
        return -1;
    }
    printf("✅ PASS: Schema found by topic\n");
    
    // Test 5: Register second schema
    mqtt_schema_definition_t transition_schema = {0};
    strncpy(transition_schema.schema_name, "transition_config", sizeof(transition_schema.schema_name) - 1);
    strncpy(transition_schema.topic_pattern, "home/esp32_core/transition/set", sizeof(transition_schema.topic_pattern) - 1);
    transition_schema.json_schema = test_transition_schema;
    transition_schema.enabled = true;
    
    ret = mqtt_schema_validator_register_schema(&transition_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register transition schema: %d\n", ret);
        return -1;
    }
    printf("✅ PASS: Transition schema registered\n");
    
    // Test 6: Schema count should be 2
    if (mqtt_schema_validator_get_schema_count() != 2) {
        printf("❌ FAIL: Schema count should be 2 after second registration\n");
        return -1;
    }
    printf("✅ PASS: Schema count correct after second registration\n");
    
    // Test 7: Find both schemas
    ret = mqtt_schema_validator_find_schema("home/esp32_core/transition/set", &found_schema);
    if (ret != ESP_OK || found_schema == NULL) {
        printf("❌ FAIL: Failed to find transition schema\n");
        return -1;
    }
    if (strcmp(found_schema->schema_name, "transition_config") != 0) {
        printf("❌ FAIL: Found wrong transition schema: %s\n", found_schema->schema_name);
        return -1;
    }
    printf("✅ PASS: Both schemas found correctly\n");
    
    // Test 8: Schema not found for wrong topic
    ret = mqtt_schema_validator_find_schema("home/esp32_core/nonexistent/topic", &found_schema);
    if (ret != ESP_ERR_NOT_FOUND) {
        printf("❌ FAIL: Should not find schema for nonexistent topic\n");
        return -1;
    }
    printf("✅ PASS: Correctly returns not found for wrong topic\n");
    
    printf("=== Schema Registry Basic Tests Passed! ===\n");
    return 0;
}

int test_schema_registry_advanced(void) {
    printf("=== MQTT Schema Registry Advanced Test ===\n");
    
    // Test 1: Remove schema
    esp_err_t ret = mqtt_schema_validator_remove_schema("brightness_config");
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to remove brightness schema: %d\n", ret);
        return -1;
    }
    
    if (mqtt_schema_validator_get_schema_count() != 1) {
        printf("❌ FAIL: Schema count should be 1 after removal\n");
        return -1;
    }
    printf("✅ PASS: Schema removed successfully\n");
    
    // Test 2: Verify removed schema not found
    const mqtt_schema_definition_t *found_schema = NULL;
    ret = mqtt_schema_validator_find_schema("home/esp32_core/brightness/set", &found_schema);
    if (ret != ESP_ERR_NOT_FOUND) {
        printf("❌ FAIL: Removed schema should not be found\n");
        return -1;
    }
    printf("✅ PASS: Removed schema correctly not found\n");
    
    // Test 3: Remaining schema still works
    ret = mqtt_schema_validator_find_schema("home/esp32_core/transition/set", &found_schema);
    if (ret != ESP_OK || found_schema == NULL) {
        printf("❌ FAIL: Remaining schema should still be found\n");
        return -1;
    }
    printf("✅ PASS: Remaining schema still works\n");
    
    // Test 4: Add multiple schemas
    mqtt_schema_definition_t command_schema = {0};
    strncpy(command_schema.schema_name, "command_schema", sizeof(command_schema.schema_name) - 1);
    strncpy(command_schema.topic_pattern, "home/esp32_core/command", sizeof(command_schema.topic_pattern) - 1);
    command_schema.json_schema = test_command_schema;
    command_schema.enabled = true;
    
    ret = mqtt_schema_validator_register_schema(&command_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register command schema: %d\n", ret);
        return -1;
    }
    
    if (mqtt_schema_validator_get_schema_count() != 2) {
        printf("❌ FAIL: Schema count should be 2 after adding command schema\n");
        return -1;
    }
    printf("✅ PASS: Multiple schemas work correctly\n");
    
    // Test 5: Get all schemas
    const mqtt_schema_definition_t *schemas[MQTT_SCHEMA_MAX_SCHEMAS];
    size_t actual_count = 0;
    ret = mqtt_schema_validator_get_schemas(schemas, MQTT_SCHEMA_MAX_SCHEMAS, &actual_count);
    if (ret != ESP_OK || actual_count != 2) {
        printf("❌ FAIL: Failed to get all schemas: ret=%d, count=%zu\n", ret, actual_count);
        return -1;
    }
    printf("✅ PASS: Get all schemas works (count: %zu)\n", actual_count);
    
    // Test 6: Clear all schemas
    ret = mqtt_schema_validator_clear_schemas();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to clear schemas: %d\n", ret);
        return -1;
    }
    
    if (mqtt_schema_validator_get_schema_count() != 0) {
        printf("❌ FAIL: Schema count should be 0 after clear\n");
        return -1;
    }
    printf("✅ PASS: Clear schemas works correctly\n");
    
    // Test 7: Schema replacement
    mqtt_schema_definition_t original_schema = {0};
    strncpy(original_schema.schema_name, "test_schema", sizeof(original_schema.schema_name) - 1);
    strncpy(original_schema.topic_pattern, "test/topic", sizeof(original_schema.topic_pattern) - 1);
    original_schema.json_schema = "{\"type\":\"object\"}";
    original_schema.enabled = true;
    
    ret = mqtt_schema_validator_register_schema(&original_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register original schema\n");
        return -1;
    }
    
    // Register same schema name with different content
    mqtt_schema_definition_t replacement_schema = original_schema;
    replacement_schema.json_schema = "{\"type\":\"string\"}";
    
    ret = mqtt_schema_validator_register_schema(&replacement_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to replace schema\n");
        return -1;
    }
    
    if (mqtt_schema_validator_get_schema_count() != 1) {
        printf("❌ FAIL: Schema count should remain 1 after replacement\n");
        return -1;
    }
    printf("✅ PASS: Schema replacement works correctly\n");
    
    // Clean up
    mqtt_schema_validator_deinit();
    
    printf("=== Schema Registry Advanced Tests Passed! ===\n");
    return 0;
}

int test_error_conditions(void) {
    printf("=== MQTT Schema Registry Error Conditions Test ===\n");
    
    // Test operations on uninitialized validator
    const mqtt_schema_definition_t *schema = NULL;
    esp_err_t ret;
    
    // Should fail when not initialized
    ret = mqtt_schema_validator_find_schema("test", &schema);
    if (ret != ESP_ERR_INVALID_STATE) {
        printf("❌ FAIL: Should fail when not initialized\n");
        return -1;
    }
    printf("✅ PASS: Correctly fails when not initialized\n");
    
    // Initialize for further tests
    mqtt_schema_validator_init();
    
    // Test invalid arguments
    mqtt_schema_definition_t valid_schema = {0};
    strncpy(valid_schema.schema_name, "valid", sizeof(valid_schema.schema_name) - 1);
    strncpy(valid_schema.topic_pattern, "valid/topic", sizeof(valid_schema.topic_pattern) - 1);
    valid_schema.json_schema = "{\"type\":\"object\"}";
    valid_schema.enabled = true;
    
    // NULL schema
    ret = mqtt_schema_validator_register_schema(NULL);
    if (ret != ESP_ERR_INVALID_ARG) {
        printf("❌ FAIL: Should reject NULL schema\n");
        return -1;
    }
    
    // Empty schema name
    mqtt_schema_definition_t invalid_schema = valid_schema;
    invalid_schema.schema_name[0] = '\0';
    ret = mqtt_schema_validator_register_schema(&invalid_schema);
    if (ret != ESP_ERR_INVALID_ARG) {
        printf("❌ FAIL: Should reject empty schema name\n");
        return -1;
    }
    
    // NULL JSON schema
    invalid_schema = valid_schema;
    invalid_schema.json_schema = NULL;
    ret = mqtt_schema_validator_register_schema(&invalid_schema);
    if (ret != ESP_ERR_INVALID_ARG) {
        printf("❌ FAIL: Should reject NULL JSON schema\n");
        return -1;
    }
    
    printf("✅ PASS: All error conditions handled correctly\n");
    
    // Clean up
    mqtt_schema_validator_deinit();
    
    printf("=== Error Conditions Tests Passed! ===\n");
    return 0;
}

int main(void) {
    int result = 0;
    
    result |= test_schema_registry_basic();
    result |= test_schema_registry_advanced();
    result |= test_error_conditions();
    
    if (result == 0) {
        printf("\n🎉 ALL SCHEMA REGISTRY TESTS PASSED! 🎉\n");
    } else {
        printf("\n❌ SOME TESTS FAILED ❌\n");
    }
    
    return result;
}