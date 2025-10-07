// Test if includes work properly
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"

// Test Tier 1 includes
#include "mqtt_schema_validator.h"
#include "mqtt_command_processor.h" 
#include "mqtt_message_persistence.h"

// Test if types are defined
void test_types() {
    mqtt_schema_definition_t schema_def;
    mqtt_command_result_t cmd_result;
    mqtt_command_context_t cmd_context;
    mqtt_schema_validation_result_t validation_result;
    mqtt_persistence_config_t persist_config;
    mqtt_message_priority_t priority = MQTT_MESSAGE_PRIORITY_NORMAL;
    
    (void)schema_def;
    (void)cmd_result;
    (void)cmd_context;
    (void)validation_result;
    (void)persist_config;
    (void)priority;
}

int main() {
    return 0;
}