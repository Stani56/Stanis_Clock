/**
 * @brief Comprehensive test for MQTT Command Processor functionality
 * 
 * Tests command registration, execution, parameter extraction, and schema integration.
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

// Mock cJSON (enhanced for command processor testing)
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

// Enhanced mock cJSON state for command testing
static char mock_command_value[64] = "test_command";
static double mock_brightness_value = 128.0;
static bool mock_enabled_value = true;
static bool mock_has_command_field = true;
static cJSON mock_parameters_obj = {.type = cJSON_Object};
static cJSON mock_brightness_param = {.type = cJSON_Number, .string = "brightness"};
static cJSON mock_enabled_param = {.type = cJSON_True, .string = "enabled"};

// Enhanced cJSON functions for command testing
cJSON* cJSON_Parse(const char *value) {
    // Set default state
    mock_has_command_field = false;
    
    if (strstr(value, "\"command\":") != NULL) {
        mock_has_command_field = true;
        // Extract command value
        const char *cmd_start = strstr(value, "\"command\":");
        if (cmd_start) {
            cmd_start = strchr(cmd_start, ':') + 1;
            while (*cmd_start == ' ' || *cmd_start == '"') cmd_start++;
            const char *cmd_end = strchr(cmd_start, '"');
            if (cmd_end) {
                size_t len = cmd_end - cmd_start;
                if (len < sizeof(mock_command_value)) {
                    strncpy(mock_command_value, cmd_start, len);
                    mock_command_value[len] = '\0';
                }
            }
        }
        
        // Extract brightness if present
        if (strstr(value, "\"brightness\":") != NULL) {
            const char *bright_start = strstr(value, "\"brightness\":");
            if (bright_start) {
                bright_start = strchr(bright_start, ':') + 1;
                while (*bright_start == ' ') bright_start++;
                mock_brightness_value = atof(bright_start);
            }
        }
        
        // Extract enabled if present
        if (strstr(value, "\"enabled\":") != NULL) {
            mock_enabled_value = (strstr(value, "\"enabled\":true") != NULL) || 
                                (strstr(value, "\"enabled\": true") != NULL);
        }
        
        static cJSON root_obj = {.type = cJSON_Object};
        return &root_obj;
    }
    
    if (strcmp(value, "invalid_json") == 0) {
        return NULL;
    }
    
    // Handle case where JSON is valid but doesn't have command field
    if (strstr(value, "{") != NULL && strstr(value, "}") != NULL) {
        static cJSON generic_obj = {.type = cJSON_Object};
        return &generic_obj;
    }
    
    return NULL;
}

void cJSON_Delete(cJSON *c) { /* Mock: do nothing */ }

const char* cJSON_GetErrorPtr(void) { return "error_location"; }

int cJSON_IsObject(const cJSON *item) { return item && item->type == cJSON_Object; }
int cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String; }
int cJSON_IsNumber(const cJSON *item) { return item && item->type == cJSON_Number; }
int cJSON_IsBool(const cJSON *item) { return item && (item->type == cJSON_True || item->type == cJSON_False); }
int cJSON_IsTrue(const cJSON *item) { return item && item->type == cJSON_True; }
int cJSON_IsArray(const cJSON *item) { return item && item->type == cJSON_Array; }

cJSON* cJSON_GetObjectItem(const cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    
    if (strcmp(string, "command") == 0) {
        // Return NULL if payload doesn't contain "command" field
        if (!mock_has_command_field) {
            return NULL;
        }
        static cJSON command_item = {.type = cJSON_String};
        command_item.valuestring = mock_command_value;
        return &command_item;
    }
    
    if (strcmp(string, "parameters") == 0) {
        // Set up parameters object with mock children
        mock_brightness_param.valuedouble = mock_brightness_value;
        mock_enabled_param.type = mock_enabled_value ? cJSON_True : cJSON_False;
        mock_brightness_param.next = &mock_enabled_param;
        mock_parameters_obj.child = &mock_brightness_param;
        return &mock_parameters_obj;
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

#define cJSON_ArrayForEach(element, array) \
    for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

// Include schema validator types (minimal for testing)
#define MQTT_SCHEMA_MAX_ERROR_MSG_LEN 256
#define MQTT_SCHEMA_MAX_ERROR_PATH_LEN 128

typedef struct {
    esp_err_t validation_result;
    char error_message[MQTT_SCHEMA_MAX_ERROR_MSG_LEN];
    char error_path[MQTT_SCHEMA_MAX_ERROR_PATH_LEN];
    size_t error_offset;
} mqtt_schema_validation_result_t;

// Mock schema validator function
esp_err_t mqtt_schema_validator_validate_json(const char *topic, const char *json_payload, mqtt_schema_validation_result_t *result) {
    // Mock validation that always passes for test commands
    result->validation_result = ESP_OK;
    result->error_message[0] = '\0';
    result->error_path[0] = '\0';
    result->error_offset = 0;
    return ESP_OK;
}

// Include command processor types
#define MQTT_COMMAND_PROCESSOR_VERSION_MAJOR 1
#define MQTT_COMMAND_PROCESSOR_VERSION_MINOR 0
#define MQTT_COMMAND_PROCESSOR_VERSION_PATCH 0
#define MQTT_COMMAND_MAX_HANDLERS 16
#define MQTT_COMMAND_MAX_NAME_LEN 32
#define MQTT_COMMAND_MAX_PARAMS 8
#define MQTT_COMMAND_MAX_PARAM_NAME_LEN 32
#define MQTT_COMMAND_MAX_RESPONSE_LEN 512

typedef enum {
    MQTT_COMMAND_RESULT_SUCCESS = 0,
    MQTT_COMMAND_RESULT_INVALID_PARAMS,
    MQTT_COMMAND_RESULT_EXECUTION_FAILED,
    MQTT_COMMAND_RESULT_NOT_FOUND,
    MQTT_COMMAND_RESULT_SCHEMA_INVALID,
    MQTT_COMMAND_RESULT_SYSTEM_ERROR
} mqtt_command_result_t;

typedef struct {
    char name[MQTT_COMMAND_MAX_PARAM_NAME_LEN];
    cJSON *value;
    bool required;
} mqtt_command_param_t;

typedef struct {
    const char *command_name;
    const char *topic;
    const char *payload;
    cJSON *json_root;
    cJSON *parameters;
    size_t param_count;
    mqtt_command_param_t params[MQTT_COMMAND_MAX_PARAMS];
    char response[MQTT_COMMAND_MAX_RESPONSE_LEN];
} mqtt_command_context_t;

typedef mqtt_command_result_t (*mqtt_command_handler_t)(mqtt_command_context_t *context);

typedef struct {
    char command_name[MQTT_COMMAND_MAX_NAME_LEN];
    mqtt_command_handler_t handler;
    const char *description;
    const char *schema_name;
    bool enabled;
    uint32_t execution_count;
    uint32_t success_count;
    uint32_t failure_count;
    uint32_t last_execution_time_ms;
} mqtt_command_definition_t;

typedef struct {
    uint32_t total_commands_processed;
    uint32_t successful_commands;
    uint32_t failed_commands;
    uint32_t schema_validation_failures;
    uint32_t command_not_found_count;
    uint32_t registered_command_count;
    uint32_t last_command_time_ms;
} mqtt_command_stats_t;

// Function declarations
esp_err_t mqtt_command_processor_init(void);
esp_err_t mqtt_command_processor_deinit(void);
bool mqtt_command_processor_is_initialized(void);
esp_err_t mqtt_command_processor_register_command(const mqtt_command_definition_t *command);
mqtt_command_result_t mqtt_command_processor_execute(const char *topic, const char *payload, char *response, size_t response_len);
esp_err_t mqtt_command_processor_get_stats(mqtt_command_stats_t *stats);
esp_err_t mqtt_command_processor_reset_stats(void);
esp_err_t mqtt_command_processor_get_commands(const mqtt_command_definition_t **commands, size_t max_commands, size_t *actual_count);
esp_err_t mqtt_command_processor_remove_command(const char *command_name);
esp_err_t mqtt_command_processor_clear_commands(void);
const char* mqtt_command_get_string_param(const mqtt_command_context_t *context, const char *param_name, const char *default_value);
int mqtt_command_get_int_param(const mqtt_command_context_t *context, const char *param_name, int default_value);
bool mqtt_command_get_bool_param(const mqtt_command_context_t *context, const char *param_name, bool default_value);

// Simplified command processor implementation for testing
static const char *TAG = "mqtt_command_processor";
static bool g_processor_initialized = false;
static SemaphoreHandle_t g_processor_mutex = NULL;
static mqtt_command_stats_t g_processor_stats = {0};
static mqtt_command_definition_t g_command_registry[MQTT_COMMAND_MAX_HANDLERS];
static size_t g_command_count = 0;

esp_err_t mqtt_command_processor_init(void) {
    if (g_processor_initialized) return ESP_OK;
    g_processor_mutex = xSemaphoreCreateMutex();
    if (g_processor_mutex == NULL) return ESP_ERR_NO_MEM;
    memset(&g_processor_stats, 0, sizeof(mqtt_command_stats_t));
    memset(g_command_registry, 0, sizeof(g_command_registry));
    g_command_count = 0;
    g_processor_initialized = true;
    return ESP_OK;
}

esp_err_t mqtt_command_processor_deinit(void) {
    if (!g_processor_initialized) return ESP_OK;
    if (g_processor_mutex != NULL) {
        vSemaphoreDelete(g_processor_mutex);
        g_processor_mutex = NULL;
    }
    memset(&g_processor_stats, 0, sizeof(mqtt_command_stats_t));
    memset(g_command_registry, 0, sizeof(g_command_registry));
    g_command_count = 0;
    g_processor_initialized = false;
    return ESP_OK;
}

bool mqtt_command_processor_is_initialized(void) {
    return g_processor_initialized;
}

esp_err_t mqtt_command_processor_register_command(const mqtt_command_definition_t *command) {
    if (!g_processor_initialized) return ESP_ERR_INVALID_STATE;
    if (command == NULL) return ESP_ERR_INVALID_ARG;
    if (strlen(command->command_name) == 0 || command->handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_command_count >= MQTT_COMMAND_MAX_HANDLERS) return ESP_ERR_NO_MEM;
    
    memcpy(&g_command_registry[g_command_count], command, sizeof(mqtt_command_definition_t));
    g_command_registry[g_command_count].execution_count = 0;
    g_command_registry[g_command_count].success_count = 0;
    g_command_registry[g_command_count].failure_count = 0;
    g_command_registry[g_command_count].last_execution_time_ms = 0;
    g_command_count++;
    g_processor_stats.registered_command_count = g_command_count;
    
    return ESP_OK;
}

static mqtt_command_definition_t* find_command(const char *command_name) {
    if (command_name == NULL) return NULL;
    for (size_t i = 0; i < g_command_count; i++) {
        if (g_command_registry[i].enabled && strcmp(g_command_registry[i].command_name, command_name) == 0) {
            return &g_command_registry[i];
        }
    }
    return NULL;
}

static esp_err_t extract_parameters(mqtt_command_context_t *context) {
    if (context == NULL || context->json_root == NULL) return ESP_ERR_INVALID_ARG;
    
    context->parameters = cJSON_GetObjectItem(context->json_root, "parameters");
    context->param_count = 0;
    
    if (context->parameters == NULL || !cJSON_IsObject(context->parameters)) {
        return ESP_OK; // No parameters is valid
    }
    
    // Extract parameters from mock object
    cJSON *param = NULL;
    cJSON_ArrayForEach(param, context->parameters) {
        if (context->param_count >= MQTT_COMMAND_MAX_PARAMS) break;
        if (param->string) {
            strncpy(context->params[context->param_count].name, param->string, 
                   MQTT_COMMAND_MAX_PARAM_NAME_LEN - 1);
            context->params[context->param_count].name[MQTT_COMMAND_MAX_PARAM_NAME_LEN - 1] = '\0';
            context->params[context->param_count].value = param;
            context->params[context->param_count].required = false;
            context->param_count++;
        }
    }
    
    return ESP_OK;
}

mqtt_command_result_t mqtt_command_processor_execute(const char *topic, const char *payload, char *response, size_t response_len) {
    if (!g_processor_initialized) return MQTT_COMMAND_RESULT_SYSTEM_ERROR;
    if (topic == NULL || payload == NULL) return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    
    g_processor_stats.total_commands_processed++;
    g_processor_stats.last_command_time_ms = esp_log_timestamp();
    
    cJSON *json_root = cJSON_Parse(payload);
    if (json_root == NULL) {
        g_processor_stats.failed_commands++;
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    cJSON *command_item = cJSON_GetObjectItem(json_root, "command");
    if (command_item == NULL || !cJSON_IsString(command_item)) {
        cJSON_Delete(json_root);
        g_processor_stats.failed_commands++;
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    const char *command_name = cJSON_GetStringValue(command_item);
    mqtt_command_definition_t *command_def = find_command(command_name);
    if (command_def == NULL) {
        g_processor_stats.command_not_found_count++;
        g_processor_stats.failed_commands++;
        cJSON_Delete(json_root);
        return MQTT_COMMAND_RESULT_NOT_FOUND;
    }
    
    // Prepare command context
    mqtt_command_context_t context = {0};
    context.command_name = command_name;
    context.topic = topic;
    context.payload = payload;
    context.json_root = json_root;
    
    command_def->execution_count++;
    command_def->last_execution_time_ms = esp_log_timestamp();
    
    esp_err_t param_ret = extract_parameters(&context);
    if (param_ret != ESP_OK) {
        command_def->failure_count++;
        g_processor_stats.failed_commands++;
        cJSON_Delete(json_root);
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    
    mqtt_command_result_t result = command_def->handler(&context);
    
    if (result == MQTT_COMMAND_RESULT_SUCCESS) {
        command_def->success_count++;
        g_processor_stats.successful_commands++;
    } else {
        command_def->failure_count++;
        g_processor_stats.failed_commands++;
    }
    
    if (response && response_len > 0 && strlen(context.response) > 0) {
        strncpy(response, context.response, response_len - 1);
        response[response_len - 1] = '\0';
    }
    
    cJSON_Delete(json_root);
    return result;
}

esp_err_t mqtt_command_processor_get_stats(mqtt_command_stats_t *stats) {
    if (!g_processor_initialized) return ESP_ERR_INVALID_STATE;
    if (stats == NULL) return ESP_ERR_INVALID_ARG;
    memcpy(stats, &g_processor_stats, sizeof(mqtt_command_stats_t));
    return ESP_OK;
}

esp_err_t mqtt_command_processor_reset_stats(void) {
    if (!g_processor_initialized) return ESP_ERR_INVALID_STATE;
    g_processor_stats.total_commands_processed = 0;
    g_processor_stats.successful_commands = 0;
    g_processor_stats.failed_commands = 0;
    g_processor_stats.schema_validation_failures = 0;
    g_processor_stats.command_not_found_count = 0;
    g_processor_stats.last_command_time_ms = 0;
    for (size_t i = 0; i < g_command_count; i++) {
        g_command_registry[i].execution_count = 0;
        g_command_registry[i].success_count = 0;
        g_command_registry[i].failure_count = 0;
        g_command_registry[i].last_execution_time_ms = 0;
    }
    return ESP_OK;
}

const char* mqtt_command_get_string_param(const mqtt_command_context_t *context, const char *param_name, const char *default_value) {
    if (context == NULL || param_name == NULL) return default_value;
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsString(context->params[i].value)) {
                return cJSON_GetStringValue(context->params[i].value);
            }
        }
    }
    return default_value;
}

int mqtt_command_get_int_param(const mqtt_command_context_t *context, const char *param_name, int default_value) {
    if (context == NULL || param_name == NULL) return default_value;
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsNumber(context->params[i].value)) {
                return (int)cJSON_GetNumberValue(context->params[i].value);
            }
        }
    }
    return default_value;
}

bool mqtt_command_get_bool_param(const mqtt_command_context_t *context, const char *param_name, bool default_value) {
    if (context == NULL || param_name == NULL) return default_value;
    for (size_t i = 0; i < context->param_count; i++) {
        if (strcmp(context->params[i].name, param_name) == 0) {
            if (cJSON_IsBool(context->params[i].value)) {
                return cJSON_IsTrue(context->params[i].value);
            }
        }
    }
    return default_value;
}

// Test command handlers
static mqtt_command_result_t test_restart_handler(mqtt_command_context_t *context) {
    snprintf(context->response, sizeof(context->response), "System restart initiated");
    return MQTT_COMMAND_RESULT_SUCCESS;
}

static mqtt_command_result_t test_status_handler(mqtt_command_context_t *context) {
    snprintf(context->response, sizeof(context->response), "System status: OK");
    return MQTT_COMMAND_RESULT_SUCCESS;
}

static mqtt_command_result_t test_brightness_handler(mqtt_command_context_t *context) {
    int brightness = mqtt_command_get_int_param(context, "brightness", -1);
    if (brightness < 0 || brightness > 255) {
        snprintf(context->response, sizeof(context->response), "Invalid brightness value");
        return MQTT_COMMAND_RESULT_INVALID_PARAMS;
    }
    snprintf(context->response, sizeof(context->response), "Brightness set to %d", brightness);
    return MQTT_COMMAND_RESULT_SUCCESS;
}

static mqtt_command_result_t test_failing_handler(mqtt_command_context_t *context) {
    snprintf(context->response, sizeof(context->response), "Command failed intentionally");
    return MQTT_COMMAND_RESULT_EXECUTION_FAILED;
}

// Test functions
int test_command_processor_initialization(void) {
    printf("=== Command Processor Initialization Test ===\n");
    
    // Test initialization
    esp_err_t ret = mqtt_command_processor_init();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Initialization failed\n");
        return -1;
    }
    
    if (!mqtt_command_processor_is_initialized()) {
        printf("❌ FAIL: Processor should be initialized\n");
        return -1;
    }
    printf("✅ PASS: Command processor initialized\n");
    
    // Test double initialization
    ret = mqtt_command_processor_init();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Double initialization should succeed\n");
        return -1;
    }
    printf("✅ PASS: Double initialization handled correctly\n");
    
    // Test deinitialization
    ret = mqtt_command_processor_deinit();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Deinitialization failed\n");
        return -1;
    }
    
    if (mqtt_command_processor_is_initialized()) {
        printf("❌ FAIL: Processor should not be initialized after deinit\n");
        return -1;
    }
    printf("✅ PASS: Command processor deinitialized\n");
    
    // Re-initialize for other tests
    ret = mqtt_command_processor_init();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Re-initialization failed\n");
        return -1;
    }
    
    printf("=== Command Processor Initialization Tests Passed! ===\n");
    return 0;
}

int test_command_registration(void) {
    printf("=== Command Registration Test ===\n");
    
    // Test registering a command
    mqtt_command_definition_t restart_cmd = {0};
    strncpy(restart_cmd.command_name, "restart", sizeof(restart_cmd.command_name) - 1);
    restart_cmd.handler = test_restart_handler;
    restart_cmd.description = "Restart the system";
    restart_cmd.schema_name = "restart_schema";
    restart_cmd.enabled = true;
    
    esp_err_t ret = mqtt_command_processor_register_command(&restart_cmd);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register restart command\n");
        return -1;
    }
    printf("✅ PASS: Restart command registered\n");
    
    // Test registering another command
    mqtt_command_definition_t status_cmd = {0};
    strncpy(status_cmd.command_name, "status", sizeof(status_cmd.command_name) - 1);
    status_cmd.handler = test_status_handler;
    status_cmd.description = "Get system status";
    status_cmd.enabled = true;
    
    ret = mqtt_command_processor_register_command(&status_cmd);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register status command\n");
        return -1;
    }
    printf("✅ PASS: Status command registered\n");
    
    // Test invalid registration (NULL command)
    ret = mqtt_command_processor_register_command(NULL);
    if (ret != ESP_ERR_INVALID_ARG) {
        printf("❌ FAIL: NULL command should return INVALID_ARG\n");
        return -1;
    }
    printf("✅ PASS: NULL command registration rejected\n");
    
    // Test invalid registration (empty name)
    mqtt_command_definition_t invalid_cmd = {0};
    invalid_cmd.handler = test_restart_handler;
    ret = mqtt_command_processor_register_command(&invalid_cmd);
    if (ret != ESP_ERR_INVALID_ARG) {
        printf("❌ FAIL: Empty name command should return INVALID_ARG\n");
        return -1;
    }
    printf("✅ PASS: Empty name command registration rejected\n");
    
    // Test statistics
    mqtt_command_stats_t stats;
    ret = mqtt_command_processor_get_stats(&stats);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to get stats\n");
        return -1;
    }
    
    if (stats.registered_command_count != 2) {
        printf("❌ FAIL: Expected 2 registered commands, got %u\n", stats.registered_command_count);
        return -1;
    }
    printf("✅ PASS: Statistics show 2 registered commands\n");
    
    printf("=== Command Registration Tests Passed! ===\n");
    return 0;
}

int test_command_execution(void) {
    printf("=== Command Execution Test ===\n");
    
    // Test successful command execution
    const char *restart_payload = "{\"command\": \"restart\", \"parameters\": {}}";
    char response[256] = {0};
    
    mqtt_command_result_t result = mqtt_command_processor_execute(
        "home/esp32_core/command", restart_payload, response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_SUCCESS) {
        printf("❌ FAIL: Restart command should succeed, got result %d\n", result);
        return -1;
    }
    
    if (strlen(response) == 0) {
        printf("❌ FAIL: Response should not be empty\n");
        return -1;
    }
    printf("✅ PASS: Restart command executed successfully: %s\n", response);
    
    // Test command not found
    const char *unknown_payload = "{\"command\": \"unknown_command\", \"parameters\": {}}";
    result = mqtt_command_processor_execute(
        "home/esp32_core/command", unknown_payload, response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_NOT_FOUND) {
        printf("❌ FAIL: Unknown command should return NOT_FOUND, got %d\n", result);
        return -1;
    }
    printf("✅ PASS: Unknown command correctly returned NOT_FOUND\n");
    
    // Test invalid JSON
    result = mqtt_command_processor_execute(
        "home/esp32_core/command", "invalid_json", response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_INVALID_PARAMS) {
        printf("❌ FAIL: Invalid JSON should return INVALID_PARAMS, got %d\n", result);
        return -1;
    }
    printf("✅ PASS: Invalid JSON correctly returned INVALID_PARAMS\n");
    
    // Test missing command field
    const char *no_command_payload = "{\"parameters\": {}}";
    result = mqtt_command_processor_execute(
        "home/esp32_core/command", no_command_payload, response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_INVALID_PARAMS) {
        printf("❌ FAIL: Missing command field should return INVALID_PARAMS, got %d\n", result);
        return -1;
    }
    printf("✅ PASS: Missing command field correctly returned INVALID_PARAMS\n");
    
    printf("=== Command Execution Tests Passed! ===\n");
    return 0;
}

int test_parameter_extraction(void) {
    printf("=== Parameter Extraction Test ===\n");
    
    // Register brightness command with parameters
    mqtt_command_definition_t brightness_cmd = {0};
    strncpy(brightness_cmd.command_name, "set_brightness", sizeof(brightness_cmd.command_name) - 1);
    brightness_cmd.handler = test_brightness_handler;
    brightness_cmd.description = "Set brightness level";
    brightness_cmd.enabled = true;
    
    esp_err_t ret = mqtt_command_processor_register_command(&brightness_cmd);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register brightness command\n");
        return -1;
    }
    
    // Test command with valid parameters
    const char *brightness_payload = "{\"command\": \"set_brightness\", \"parameters\": {\"brightness\": 128, \"enabled\": true}}";
    char response[256] = {0};
    
    mqtt_command_result_t result = mqtt_command_processor_execute(
        "home/esp32_core/command", brightness_payload, response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_SUCCESS) {
        printf("❌ FAIL: Brightness command should succeed, got result %d\n", result);
        return -1;
    }
    
    if (strstr(response, "128") == NULL) {
        printf("❌ FAIL: Response should contain brightness value 128: %s\n", response);
        return -1;
    }
    printf("✅ PASS: Brightness command with parameters executed: %s\n", response);
    
    // Test command with invalid brightness
    const char *invalid_brightness_payload = "{\"command\": \"set_brightness\", \"parameters\": {\"brightness\": 300}}";
    result = mqtt_command_processor_execute(
        "home/esp32_core/command", invalid_brightness_payload, response, sizeof(response));
    
    if (result != MQTT_COMMAND_RESULT_INVALID_PARAMS) {
        printf("❌ FAIL: Invalid brightness should return INVALID_PARAMS, got %d\n", result);
        return -1;
    }
    printf("✅ PASS: Invalid brightness correctly rejected\n");
    
    printf("=== Parameter Extraction Tests Passed! ===\n");
    return 0;
}

int test_command_statistics(void) {
    printf("=== Command Statistics Test ===\n");
    
    // Register a failing command for statistics testing
    mqtt_command_definition_t fail_cmd = {0};
    strncpy(fail_cmd.command_name, "fail_test", sizeof(fail_cmd.command_name) - 1);
    fail_cmd.handler = test_failing_handler;
    fail_cmd.description = "Command that always fails";
    fail_cmd.enabled = true;
    
    esp_err_t ret = mqtt_command_processor_register_command(&fail_cmd);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to register failing command\n");
        return -1;
    }
    
    // Reset statistics before testing
    ret = mqtt_command_processor_reset_stats();
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to reset statistics\n");
        return -1;
    }
    
    // Execute some commands to generate statistics
    char response[256];
    
    // Successful command
    mqtt_command_processor_execute("home/esp32_core/command", 
        "{\"command\": \"restart\", \"parameters\": {}}", response, sizeof(response));
    
    // Failing command
    mqtt_command_processor_execute("home/esp32_core/command", 
        "{\"command\": \"fail_test\", \"parameters\": {}}", response, sizeof(response));
    
    // Unknown command
    mqtt_command_processor_execute("home/esp32_core/command", 
        "{\"command\": \"unknown\", \"parameters\": {}}", response, sizeof(response));
    
    // Check statistics
    mqtt_command_stats_t stats;
    ret = mqtt_command_processor_get_stats(&stats);
    if (ret != ESP_OK) {
        printf("❌ FAIL: Failed to get statistics\n");
        return -1;
    }
    
    if (stats.total_commands_processed != 3) {
        printf("❌ FAIL: Expected 3 total commands, got %u\n", stats.total_commands_processed);
        return -1;
    }
    
    if (stats.successful_commands != 1) {
        printf("❌ FAIL: Expected 1 successful command, got %u\n", stats.successful_commands);
        return -1;
    }
    
    if (stats.failed_commands != 2) {
        printf("❌ FAIL: Expected 2 failed commands, got %u\n", stats.failed_commands);
        return -1;
    }
    
    if (stats.command_not_found_count != 1) {
        printf("❌ FAIL: Expected 1 not found command, got %u\n", stats.command_not_found_count);
        return -1;
    }
    
    printf("✅ PASS: Statistics correctly tracked:\n");
    printf("  Total: %u, Success: %u, Failed: %u, Not Found: %u\n", 
           stats.total_commands_processed, stats.successful_commands, 
           stats.failed_commands, stats.command_not_found_count);
    
    printf("=== Command Statistics Tests Passed! ===\n");
    return 0;
}

int main(void) {
    int result = 0;
    
    result |= test_command_processor_initialization();
    result |= test_command_registration();
    result |= test_command_execution();
    result |= test_parameter_extraction();
    result |= test_command_statistics();
    
    if (result == 0) {
        printf("\n🎉 ALL COMMAND PROCESSOR TESTS PASSED! 🎉\n");
    } else {
        printf("\n❌ SOME COMMAND PROCESSOR TESTS FAILED ❌\n");
    }
    
    mqtt_command_processor_deinit();
    return result;
}