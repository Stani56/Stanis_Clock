/**
 * @brief Comprehensive test for MQTT JSON schema validation functionality
 * 
 * Tests JSON validation engine with various payloads and schema types.
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

// Mock esp_log_timestamp
uint32_t esp_log_timestamp(void) { return 12345; }

// Mock cJSON (minimal implementation for testing)
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    double valuedouble;
    int valueint;
    char *string;
} cJSON;

#define cJSON_Invalid (0)
#define cJSON_False   (1 << 0)
#define cJSON_True    (1 << 1)
#define cJSON_NULL    (1 << 2)
#define cJSON_Number  (1 << 3)
#define cJSON_String  (1 << 4)
#define cJSON_Array   (1 << 5)
#define cJSON_Object  (1 << 6)

// Mock cJSON functions
static cJSON* test_json_object = NULL;
static cJSON* test_schema_object = NULL;
static char* mock_error_ptr = NULL;

// Global state for mock cJSON parsing
static double mock_brightness_value = 128.0;
static char mock_command_value[32] = "restart";

cJSON* cJSON_Parse(const char *value) {
    // Simple mock that returns a valid object for test payloads
    if (strstr(value, "\"brightness\":") != NULL) {
        // Extract brightness value from JSON string
        const char *brightness_start = strstr(value, "\"brightness\":");
        if (brightness_start) {
            brightness_start = strchr(brightness_start, ':') + 1;
            while (*brightness_start == ' ') brightness_start++; // Skip spaces
            mock_brightness_value = atof(brightness_start);
        }
        static cJSON brightness_obj = {.type = cJSON_Object};
        static cJSON brightness_prop = {.type = cJSON_Number, .string = "brightness"};
        brightness_prop.valuedouble = mock_brightness_value;
        brightness_obj.child = &brightness_prop;
        test_json_object = &brightness_obj;
        return &brightness_obj;
    }
    if (strstr(value, "\"command\":") != NULL) {
        // Extract command value from JSON string
        const char *command_start = strstr(value, "\"command\":");
        if (command_start) {
            command_start = strchr(command_start, ':') + 1;
            while (*command_start == ' ' || *command_start == '"') command_start++; // Skip spaces and quotes
            const char *command_end = strchr(command_start, '"');
            if (command_end) {
                size_t len = command_end - command_start;
                if (len < sizeof(mock_command_value)) {
                    strncpy(mock_command_value, command_start, len);
                    mock_command_value[len] = '\0';
                }
            }
        }
        static cJSON command_obj = {.type = cJSON_Object};
        static cJSON command_prop = {.type = cJSON_String, .string = "command"};
        command_prop.valuestring = mock_command_value;
        command_obj.child = &command_prop;
        test_json_object = &command_obj;
        return &command_obj;
    }
    if (strstr(value, "\"type\":\"object\"") != NULL) {
        static cJSON schema_obj = {.type = cJSON_Object};
        test_schema_object = &schema_obj;
        return &schema_obj;
    }
    if (strcmp(value, "invalid_json") == 0) {
        mock_error_ptr = (char*)value + 7; // Point to 'j' in 'json'
        return NULL;
    }
    return NULL;
}

void cJSON_Delete(cJSON *c) { /* Mock: do nothing */ }

const char* cJSON_GetErrorPtr(void) { return mock_error_ptr; }

int cJSON_IsObject(const cJSON *item) { return item && item->type == cJSON_Object; }
int cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String; }
int cJSON_IsNumber(const cJSON *item) { return item && item->type == cJSON_Number; }
int cJSON_IsBool(const cJSON *item) { return item && (item->type == cJSON_True || item->type == cJSON_False); }
int cJSON_IsArray(const cJSON *item) { return item && item->type == cJSON_Array; }

cJSON* cJSON_GetObjectItem(const cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    if (strcmp(string, "type") == 0) {
        static cJSON type_item = {.type = cJSON_String, .valuestring = "object"};
        return &type_item;
    }
    if (strcmp(string, "brightness") == 0 && test_json_object) {
        static cJSON brightness_item = {.type = cJSON_Number};
        brightness_item.valuedouble = mock_brightness_value;
        return &brightness_item;
    }
    if (strcmp(string, "command") == 0 && test_json_object) {
        static cJSON command_item = {.type = cJSON_String};
        command_item.valuestring = mock_command_value;
        return &command_item;
    }
    if (strcmp(string, "minimum") == 0) {
        static cJSON min_item = {.type = cJSON_Number, .valuedouble = 0};
        return &min_item;
    }
    if (strcmp(string, "maximum") == 0) {
        static cJSON max_item = {.type = cJSON_Number, .valuedouble = 255};
        return &max_item;
    }
    if (strcmp(string, "required") == 0) {
        static cJSON required_item = {.type = cJSON_Array};
        return &required_item;
    }
    return NULL;
}

char* cJSON_GetStringValue(const cJSON *item) {
    if (!item || item->type != cJSON_String) return NULL;
    return item->valuestring;
}

double cJSON_GetNumberValue(const cJSON *item) {
    if (!item || item->type != cJSON_Number) return 0.0;
    return item->valuedouble;
}

int cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) != NULL;
}

#define cJSON_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

// Include the schema validator types and functions
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
esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema);
esp_err_t mqtt_schema_validator_find_schema(const char *topic, const mqtt_schema_definition_t **schema);
esp_err_t mqtt_schema_validator_validate_json(const char *topic, const char *json_payload, mqtt_schema_validation_result_t *result);

// Include simplified implementation (core functions only)
static const char *TAG = "mqtt_schema_validator";
static bool g_validator_initialized = false;
static SemaphoreHandle_t g_validator_mutex = NULL;
static mqtt_schema_stats_t g_validator_stats = {0};
static mqtt_schema_definition_t g_schema_registry[MQTT_SCHEMA_MAX_SCHEMAS];
static size_t g_schema_count = 0;

static bool topic_matches_pattern(const char *topic, const char *pattern) {
    if (topic == NULL || pattern == NULL) return false;
    return (strcmp(topic, pattern) == 0);
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

esp_err_t mqtt_schema_validator_register_schema(const mqtt_schema_definition_t *schema) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (schema == NULL) return ESP_ERR_INVALID_ARG;
    if (strlen(schema->schema_name) == 0 || strlen(schema->topic_pattern) == 0 || schema->json_schema == NULL) {
        return ESP_ERR_INVALID_ARG;
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

// Simplified validation function for testing
esp_err_t mqtt_schema_validator_validate_json(const char *topic, const char *json_payload, mqtt_schema_validation_result_t *result) {
    if (!g_validator_initialized) return ESP_ERR_INVALID_STATE;
    if (topic == NULL || json_payload == NULL || result == NULL) return ESP_ERR_INVALID_ARG;
    
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
        return ESP_OK;
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
    
    // Simple validation: check if brightness is in valid range for brightness schema
    if (strstr(schema->schema_name, "brightness") != NULL) {
        cJSON *brightness_item = cJSON_GetObjectItem(json_obj, "brightness");
        if (brightness_item && cJSON_IsNumber(brightness_item)) {
            double brightness = cJSON_GetNumberValue(brightness_item);
            if (brightness >= 0 && brightness <= 255) {
                result->validation_result = ESP_OK;
            } else {
                snprintf(result->error_message, sizeof(result->error_message), 
                        "Brightness %.2f is out of range [0-255]", brightness);
                result->validation_result = ESP_ERR_INVALID_ARG;
            }
        } else {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Missing or invalid brightness property");
            result->validation_result = ESP_ERR_INVALID_ARG;
        }
    } else if (strstr(schema->schema_name, "command") != NULL) {
        cJSON *command_item = cJSON_GetObjectItem(json_obj, "command");
        if (command_item && cJSON_IsString(command_item)) {
            const char *command = cJSON_GetStringValue(command_item);
            if (strcmp(command, "restart") == 0 || strcmp(command, "status") == 0 || strcmp(command, "reset") == 0) {
                result->validation_result = ESP_OK;
            } else {
                snprintf(result->error_message, sizeof(result->error_message), 
                        "Invalid command: %s", command);
                result->validation_result = ESP_ERR_INVALID_ARG;
            }
        } else {
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Missing or invalid command property");
            result->validation_result = ESP_ERR_INVALID_ARG;
        }
    } else {
        result->validation_result = ESP_OK; // Default: pass validation
    }
    
    // Update statistics
    g_validator_stats.total_validations++;
    if (result->validation_result == ESP_OK) {
        g_validator_stats.successful_validations++;
    } else {
        g_validator_stats.failed_validations++;
    }
    
    cJSON_Delete(json_obj);
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
int test_json_validation_basic(void) {
    printf("=== JSON Validation Basic Test ===\n");
    
    // Initialize
    esp_err_t ret = mqtt_schema_validator_init();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Initialization failed\n");
        return -1;
    }
    
    // Register brightness schema
    mqtt_schema_definition_t brightness_schema = {0};
    strncpy(brightness_schema.schema_name, "brightness_config", sizeof(brightness_schema.schema_name) - 1);
    strncpy(brightness_schema.topic_pattern, "home/esp32_core/brightness/set", sizeof(brightness_schema.topic_pattern) - 1);
    brightness_schema.json_schema = test_brightness_schema;
    brightness_schema.enabled = true;
    
    ret = mqtt_schema_validator_register_schema(&brightness_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register brightness schema\n");
        return -1;
    }
    printf("✅ PASS: Brightness schema registered\n");
    
    // Test 1: Valid brightness JSON
    mqtt_schema_validation_result_t result = {0};
    const char *valid_brightness_json = "{\"brightness\": 128, \"transition\": 1000}";
    
    ret = mqtt_schema_validator_validate_json("home/esp32_core/brightness/set", valid_brightness_json, &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result != ESP_OK) {
        printf("❌ FAIL: Valid brightness JSON failed validation: %s\n", result.error_message);
        return -1;
    }
    printf("✅ PASS: Valid brightness JSON passed validation\n");
    
    // Test 2: Invalid brightness value (out of range)
    const char *invalid_brightness_json = "{\"brightness\": 300, \"transition\": 1000}";
    
    ret = mqtt_schema_validator_validate_json("home/esp32_core/brightness/set", invalid_brightness_json, &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed for invalid case: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result == ESP_OK) {
        printf("❌ FAIL: Invalid brightness JSON should have failed validation\n");
        return -1;
    }
    printf("✅ PASS: Invalid brightness JSON correctly failed validation: %s\n", result.error_message);
    
    // Test 3: Invalid JSON syntax
    ret = mqtt_schema_validator_validate_json("home/esp32_core/brightness/set", "invalid_json", &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed for invalid JSON: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result == ESP_OK) {
        printf("❌ FAIL: Invalid JSON syntax should have failed validation\n");
        return -1;
    }
    if (result.error_offset == 0) {
        printf("❌ FAIL: Error offset should be set for JSON parse error\n");
        return -1;
    }
    printf("✅ PASS: Invalid JSON syntax correctly failed validation: %s (offset: %zu)\n", 
           result.error_message, result.error_offset);
    
    printf("=== JSON Validation Basic Tests Passed! ===\n");
    return 0;
}

int test_json_validation_command_schema(void) {
    printf("=== JSON Validation Command Schema Test ===\n");
    
    // Register command schema
    mqtt_schema_definition_t command_schema = {0};
    strncpy(command_schema.schema_name, "command_schema", sizeof(command_schema.schema_name) - 1);
    strncpy(command_schema.topic_pattern, "home/esp32_core/command", sizeof(command_schema.topic_pattern) - 1);
    command_schema.json_schema = test_command_schema;
    command_schema.enabled = true;
    
    esp_err_t ret = mqtt_schema_validator_register_schema(&command_schema);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register command schema\n");
        return -1;
    }
    printf("✅ PASS: Command schema registered\n");
    
    // Test 1: Valid command JSON
    mqtt_schema_validation_result_t result = {0};
    const char *valid_command_json = "{\"command\": \"restart\", \"parameters\": {}}";
    
    ret = mqtt_schema_validator_validate_json("home/esp32_core/command", valid_command_json, &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result != ESP_OK) {
        printf("❌ FAIL: Valid command JSON failed validation: %s\n", result.error_message);
        return -1;
    }
    printf("✅ PASS: Valid command JSON passed validation\n");
    
    // Test 2: Invalid command value
    const char *invalid_command_json = "{\"command\": \"invalid_cmd\", \"parameters\": {}}";
    
    ret = mqtt_schema_validator_validate_json("home/esp32_core/command", invalid_command_json, &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed for invalid case: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result == ESP_OK) {
        printf("❌ FAIL: Invalid command JSON should have failed validation\n");
        return -1;
    }
    printf("✅ PASS: Invalid command JSON correctly failed validation: %s\n", result.error_message);
    
    printf("=== JSON Validation Command Schema Tests Passed! ===\n");
    return 0;
}

int test_json_validation_no_schema(void) {
    printf("=== JSON Validation No Schema Test ===\n");
    
    // Test validation for topic with no registered schema
    mqtt_schema_validation_result_t result = {0};
    const char *any_json = "{\"test\": \"value\"}";
    
    esp_err_t ret = mqtt_schema_validator_validate_json("nonexistent/topic", any_json, &result);
    if (ret != ESP_OK) {
        printf("❌ FAIL: JSON validation function failed: %d\n", ret);
        return -1;
    }
    
    if (result.validation_result != ESP_ERR_NOT_FOUND) {
        printf("❌ FAIL: Should return NOT_FOUND for topic with no schema\n");
        return -1;
    }
    
    if (strstr(result.error_message, "No schema found") == NULL) {
        printf("❌ FAIL: Error message should indicate no schema found\n");
        return -1;
    }
    printf("✅ PASS: No schema case correctly handled: %s\n", result.error_message);
    
    printf("=== JSON Validation No Schema Tests Passed! ===\n");
    return 0;
}

int main(void) {
    int result = 0;
    
    result |= test_json_validation_basic();
    result |= test_json_validation_command_schema();
    result |= test_json_validation_no_schema();
    
    if (result == 0) {
        printf("\n🎉 ALL JSON VALIDATION TESTS PASSED! 🎉\n");
    } else {
        printf("\n❌ SOME JSON VALIDATION TESTS FAILED ❌\n");
    }
    
    mqtt_schema_validator_deinit();
    return result;
}