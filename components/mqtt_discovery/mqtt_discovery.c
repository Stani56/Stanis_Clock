#include "mqtt_discovery.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mqtt_discovery";

// Global discovery configuration
static mqtt_discovery_config_t discovery_config = {
    .enabled = true,
    .discovery_prefix = DISCOVERY_PREFIX_DEFAULT,
    .node_id = DISCOVERY_NODE_ID,
    .device_id = "",
    .publish_delay_ms = DISCOVERY_PUBLISH_DELAY_MS
};

// Global device information
static mqtt_discovery_device_t device_info;

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;

// Helper function to add base topic to config
static void add_base_topic(cJSON *config) {
    char base_topic[64];
    snprintf(base_topic, sizeof(base_topic), "home/%s", MQTT_DEVICE_NAME);
    cJSON_AddStringToObject(config, "~", base_topic);
}

// Initialize discovery module
esp_err_t mqtt_discovery_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT Discovery module");
    
    // Generate unique device ID from MAC address
    esp_err_t ret = mqtt_discovery_generate_device_id(discovery_config.device_id, 
                                                      sizeof(discovery_config.device_id));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate device ID");
        return ret;
    }
    
    ESP_LOGI(TAG, "Device ID: %s", discovery_config.device_id);
    
    // Initialize device information
    ret = mqtt_discovery_get_device_info(&device_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device info");
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT Discovery initialized successfully");
    return ESP_OK;
}

// Generate unique device ID from MAC address
esp_err_t mqtt_discovery_generate_device_id(char* device_id, size_t len)
{
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        return ret;
    }
    
    snprintf(device_id, len, "%s_%02X%02X%02X", 
             MQTT_DEVICE_NAME, mac[3], mac[4], mac[5]);
    
    return ESP_OK;
}

// Get device information for discovery
esp_err_t mqtt_discovery_get_device_info(mqtt_discovery_device_t* device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get MAC address for connections
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Set device identifiers
    snprintf(device->identifiers, sizeof(device->identifiers), 
             "%s", discovery_config.device_id);
    
    // Set MAC connection
    snprintf(device->connections, sizeof(device->connections),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Set device info - capitalize first letter of device name for display
    char display_name[64];
    snprintf(display_name, sizeof(display_name), "Word Clock %s", MQTT_DEVICE_NAME);
    // Replace underscores with spaces for better display
    for (char *p = display_name; *p; p++) {
        if (*p == '_') *p = ' ';
    }
    strncpy(device->name, display_name, sizeof(device->name) - 1);
    strncpy(device->model, DEVICE_MODEL, sizeof(device->model) - 1);
    strncpy(device->manufacturer, DEVICE_MANUFACTURER, sizeof(device->manufacturer) - 1);
    strncpy(device->sw_version, DEVICE_SW_VERSION, sizeof(device->sw_version) - 1);
    strncpy(device->hw_version, DEVICE_HW_VERSION, sizeof(device->hw_version) - 1);
    
    // TODO: Set actual IP address for configuration URL
    snprintf(device->configuration_url, sizeof(device->configuration_url),
             "http://esp32_wordclock.local");
    
    return ESP_OK;
}

// Helper function to create device JSON object
static cJSON* create_device_json(void)
{
    cJSON *device = cJSON_CreateObject();
    if (device == NULL) return NULL;
    
    // Create identifiers array
    cJSON *identifiers = cJSON_CreateArray();
    if (identifiers) {
        cJSON_AddItemToArray(identifiers, cJSON_CreateString(device_info.identifiers));
        cJSON_AddItemToObject(device, "identifiers", identifiers);
    }
    
    // Create connections array
    cJSON *connections = cJSON_CreateArray();
    if (connections) {
        cJSON *mac_connection = cJSON_CreateArray();
        if (mac_connection) {
            cJSON_AddItemToArray(mac_connection, cJSON_CreateString("mac"));
            cJSON_AddItemToArray(mac_connection, cJSON_CreateString(device_info.connections));
            cJSON_AddItemToArray(connections, mac_connection);
        }
        cJSON_AddItemToObject(device, "connections", connections);
    }
    
    // Add device properties
    cJSON_AddStringToObject(device, "name", device_info.name);
    cJSON_AddStringToObject(device, "model", device_info.model);
    cJSON_AddStringToObject(device, "manufacturer", device_info.manufacturer);
    cJSON_AddStringToObject(device, "sw_version", device_info.sw_version);
    cJSON_AddStringToObject(device, "hw_version", device_info.hw_version);
    cJSON_AddStringToObject(device, "configuration_url", device_info.configuration_url);
    
    return device;
}

// Publish discovery configuration
static esp_err_t publish_discovery_config(const char* component, const char* object_id, 
                                         cJSON* config, bool retain)
{
    if (mqtt_client == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build topic
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config",
             discovery_config.discovery_prefix,
             component,
             discovery_config.node_id,
             object_id);
    
    // Convert config to string
    char *json_string = cJSON_Print(config);
    if (json_string == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Publish discovery message
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_string, 0, 1, retain);
    
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Published discovery for %s/%s", component, object_id);
        ESP_LOGD(TAG, "Topic: %s", topic);
    } else {
        ESP_LOGE(TAG, "Failed to publish discovery for %s/%s", component, object_id);
    }
    
    free(json_string);
    
    // Delay between discovery messages
    vTaskDelay(pdMS_TO_TICKS(discovery_config.publish_delay_ms));
    
    return (msg_id != -1) ? ESP_OK : ESP_FAIL;
}

// Publish main light entity discovery
esp_err_t mqtt_discovery_publish_light(void)
{
    cJSON *config = cJSON_CreateObject();
    if (config == NULL) return ESP_ERR_NO_MEM;
    
    // Base topic abbreviation - use actual device name
    add_base_topic(config);
    
    // Basic configuration
    cJSON_AddStringToObject(config, "name", "Word Clock Display");
    char unique_id[64];
    snprintf(unique_id, sizeof(unique_id), "%s_display", discovery_config.device_id);
    cJSON_AddStringToObject(config, "unique_id", unique_id);
    
    // State configuration
    cJSON_AddStringToObject(config, "state_topic", "~/availability");
    cJSON_AddStringToObject(config, "command_topic", "~/brightness/set");
    cJSON_AddStringToObject(config, "brightness_state_topic", "~/brightness/status");
    cJSON_AddStringToObject(config, "brightness_command_topic", "~/brightness/set");
    cJSON_AddNumberToObject(config, "brightness_scale", 255);
    cJSON_AddStringToObject(config, "brightness_value_template", "{{ value_json.global }}");
    cJSON_AddStringToObject(config, "on_command_type", "brightness");
    cJSON_AddStringToObject(config, "payload_on", "online");
    cJSON_AddStringToObject(config, "payload_off", "offline");
    cJSON_AddStringToObject(config, "state_value_template", 
                           "{{ 'ON' if value == 'online' else 'OFF' }}");
    cJSON_AddStringToObject(config, "brightness_command_template", 
                           "{\"global\": {{ brightness }}}");
    cJSON_AddStringToObject(config, "schema", "template");
    cJSON_AddBoolToObject(config, "optimistic", false);
    cJSON_AddNumberToObject(config, "qos", 1);
    cJSON_AddBoolToObject(config, "retain", false);
    
    // Availability configuration
    cJSON *availability = cJSON_CreateObject();
    if (availability) {
        cJSON_AddStringToObject(availability, "topic", "~/availability");
        cJSON_AddStringToObject(availability, "payload_available", "online");
        cJSON_AddStringToObject(availability, "payload_not_available", "offline");
        cJSON_AddItemToObject(config, "availability", availability);
    }
    
    // Effect list
    cJSON *effect_list = cJSON_CreateArray();
    if (effect_list) {
        cJSON_AddItemToArray(effect_list, cJSON_CreateString("none"));
        cJSON_AddItemToArray(effect_list, cJSON_CreateString("smooth_transitions"));
        cJSON_AddItemToArray(effect_list, cJSON_CreateString("instant"));
        cJSON_AddItemToObject(config, "effect_list", effect_list);
    }
    
    cJSON_AddStringToObject(config, "effect_state_topic", "~/transition/status");
    cJSON_AddStringToObject(config, "effect_command_topic", "~/transition/set");
    cJSON_AddStringToObject(config, "effect_value_template", 
                           "{{ 'smooth_transitions' if value_json.enabled else 'instant' }}");
    
    // Add device information
    cJSON *device = create_device_json();
    if (device) {
        cJSON_AddItemToObject(config, "device", device);
    }
    
    // Publish the discovery configuration
    esp_err_t ret = publish_discovery_config("light", "display", config, true);
    
    cJSON_Delete(config);
    return ret;
}

// Publish sensor discoveries
esp_err_t mqtt_discovery_publish_sensors(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];
    
    // WiFi Status Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "WiFi Status");
            snprintf(unique_id, sizeof(unique_id), "%s_wifi", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/wifi");
            cJSON_AddStringToObject(config, "icon", "mdi:wifi");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "wifi_status", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // NTP Status Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Time Sync Status");
            snprintf(unique_id, sizeof(unique_id), "%s_ntp", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/ntp");
            cJSON_AddStringToObject(config, "icon", "mdi:clock-check");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "ntp_status", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // NTP Last Sync Timestamp Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Last NTP Sync");
            snprintf(unique_id, sizeof(unique_id), "%s_ntp_last_sync", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "object_id", "german_word_clock_ntp_last_sync");
            cJSON_AddStringToObject(config, "state_topic", "~/ntp/last_sync");
            cJSON_AddStringToObject(config, "device_class", "timestamp");
            cJSON_AddStringToObject(config, "icon", "mdi:clock-check-outline");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "ntp_last_sync", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Individual Brightness Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "LED Brightness");
            snprintf(unique_id, sizeof(unique_id), "%s_led_brightness", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "%");
            cJSON_AddStringToObject(config, "value_template", 
                                   "{{ ((value_json.current_pwm / 255) * 100) | round(0) }}");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-6");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "led_brightness", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Global Brightness Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Display Brightness");
            snprintf(unique_id, sizeof(unique_id), "%s_display_brightness", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "%");
            cJSON_AddStringToObject(config, "value_template", 
                                   "{{ ((value_json.current_grppwm / 255) * 100) | round(0) }}");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-7");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "display_brightness", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Current Light Level Sensor (Lux)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Current Light Level");
            snprintf(unique_id, sizeof(unique_id), "%s_current_lux", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "lx");
            cJSON_AddStringToObject(config, "device_class", "illuminance");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.light_level_lux | round(1) }}");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-5");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "current_lux", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Current Potentiometer Value Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Potentiometer Reading");
            snprintf(unique_id, sizeof(unique_id), "%s_pot_reading", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "%");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.potentiometer_percentage | round(1) }}");
            cJSON_AddStringToObject(config, "icon", "mdi:knob");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "pot_reading", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Potentiometer Voltage Sensor (for debugging)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Potentiometer Voltage");
            snprintf(unique_id, sizeof(unique_id), "%s_pot_voltage", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "V");
            cJSON_AddStringToObject(config, "device_class", "voltage");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "value_template", "{{ '%.1f' | format(value_json.potentiometer_voltage_mv / 1000) }}");
            cJSON_AddNumberToObject(config, "suggested_display_precision", 1);
            cJSON_AddStringToObject(config, "icon", "mdi:lightning-bolt");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "pot_voltage", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Current PWM Sensor (Individual Brightness)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Current PWM");
            snprintf(unique_id, sizeof(unique_id), "%s_current_pwm", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "PWM");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.current_pwm }}");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-4");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "current_pwm", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Current GRPPWM Sensor (Global Brightness)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Current GRPPWM");
            snprintf(unique_id, sizeof(unique_id), "%s_current_grppwm", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/sensors/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "PWM");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.current_grppwm }}");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-1");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("sensor", "current_grppwm", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    return ret;
}

// Publish LED validation sensor discoveries
esp_err_t mqtt_discovery_publish_validation_sensors(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];

    // Validation Status Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "LED Validation Status");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_status", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/status");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.result }}");
            cJSON_AddStringToObject(config, "icon", "mdi:check-circle");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_status", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Validation Health Score Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "LED Validation Health Score");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_health", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "unit_of_measurement", "%");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.health_score }}");
            cJSON_AddStringToObject(config, "icon", "mdi:heart-pulse");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_health", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Last Validation Time Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Last Validation Time");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_last_time", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/status");
            cJSON_AddStringToObject(config, "device_class", "timestamp");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.timestamp }}");
            cJSON_AddStringToObject(config, "icon", "mdi:clock-outline");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_last_time", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Total Validations Count Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Total Validations");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_total", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.total_validations }}");
            cJSON_AddStringToObject(config, "icon", "mdi:counter");
            cJSON_AddStringToObject(config, "state_class", "total_increasing");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_total", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Failed Validations Count Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Failed Validations");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_failed", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.validations_failed }}");
            cJSON_AddStringToObject(config, "icon", "mdi:alert-circle");
            cJSON_AddStringToObject(config, "state_class", "total_increasing");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_failed", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Hardware Faults Count Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Hardware Faults");
            snprintf(unique_id, sizeof(unique_id), "%s_hardware_faults", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.hardware_fault_count }}");
            cJSON_AddStringToObject(config, "icon", "mdi:chip");
            cJSON_AddStringToObject(config, "state_class", "total_increasing");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "hardware_faults", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // I2C Bus Failures Count Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "I2C Bus Failures");
            snprintf(unique_id, sizeof(unique_id), "%s_i2c_failures", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.i2c_bus_failure_count }}");
            cJSON_AddStringToObject(config, "icon", "mdi:swap-horizontal");
            cJSON_AddStringToObject(config, "state_class", "total_increasing");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "i2c_failures", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Consecutive Failures Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Consecutive Failures");
            snprintf(unique_id, sizeof(unique_id), "%s_consecutive_failures", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/statistics");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.consecutive_failures }}");
            cJSON_AddStringToObject(config, "icon", "mdi:alert-circle-outline");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "consecutive_failures", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // Validation Execution Time Sensor
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Validation Execution Time");
            snprintf(unique_id, sizeof(unique_id), "%s_validation_exec_time", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/validation/status");
            cJSON_AddStringToObject(config, "unit_of_measurement", "ms");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.execution_time_ms }}");
            cJSON_AddStringToObject(config, "icon", "mdi:timer");
            cJSON_AddStringToObject(config, "state_class", "measurement");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("sensor", "validation_exec_time", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    return ret;
}

// Publish switch discoveries
esp_err_t mqtt_discovery_publish_switches(void)
{
    char unique_id[64];
    esp_err_t ret = ESP_OK;

    // Smooth Transitions Switch
    {
        cJSON *config = cJSON_CreateObject();
        if (config == NULL) return ESP_ERR_NO_MEM;

        add_base_topic(config);
        cJSON_AddStringToObject(config, "name", "Smooth Transitions");
        snprintf(unique_id, sizeof(unique_id), "%s_transitions", discovery_config.device_id);
        cJSON_AddStringToObject(config, "unique_id", unique_id);
        cJSON_AddStringToObject(config, "state_topic", "~/transition/status");
        cJSON_AddStringToObject(config, "command_topic", "~/transition/set");
        cJSON_AddStringToObject(config, "value_template", "{{ value_json.enabled }}");
        cJSON_AddStringToObject(config, "payload_on", "{\"enabled\": true}");
        cJSON_AddStringToObject(config, "payload_off", "{\"enabled\": false}");
        cJSON_AddBoolToObject(config, "state_on", true);
        cJSON_AddBoolToObject(config, "state_off", false);
        cJSON_AddStringToObject(config, "icon", "mdi:transition");
        cJSON_AddStringToObject(config, "availability_topic", "~/availability");

        cJSON *device = create_device_json();
        if (device) cJSON_AddItemToObject(config, "device", device);

        ret = publish_discovery_config("switch", "transitions", config, true);
        cJSON_Delete(config);
        if (ret != ESP_OK) return ret;
    }

    // HALB-Centric Time Expression Style Switch
    {
        cJSON *config = cJSON_CreateObject();
        if (config == NULL) return ESP_ERR_NO_MEM;

        add_base_topic(config);
        cJSON_AddStringToObject(config, "name", "HALB-Centric Time Style");
        snprintf(unique_id, sizeof(unique_id), "%s_halb_centric_style", discovery_config.device_id);
        cJSON_AddStringToObject(config, "unique_id", unique_id);
        cJSON_AddStringToObject(config, "state_topic", "~/brightness/config/status");
        cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/set");
        cJSON_AddStringToObject(config, "value_template", "{{ value_json.halb_centric_style }}");
        cJSON_AddStringToObject(config, "payload_on", "{\"halb_centric_style\": true}");
        cJSON_AddStringToObject(config, "payload_off", "{\"halb_centric_style\": false}");
        cJSON_AddBoolToObject(config, "state_on", true);
        cJSON_AddBoolToObject(config, "state_off", false);
        cJSON_AddStringToObject(config, "icon", "mdi:clock-time-three");
        cJSON_AddStringToObject(config, "availability_topic", "~/availability");

        cJSON *device = create_device_json();
        if (device) cJSON_AddItemToObject(config, "device", device);

        ret = publish_discovery_config("switch", "halb_centric_style", config, true);
        cJSON_Delete(config);
        if (ret != ESP_OK) return ret;
    }

    return ret;
}

// Publish number discoveries
esp_err_t mqtt_discovery_publish_numbers(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];
    
    // Transition Duration
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "A0. Animation: Transition Duration");
            snprintf(unique_id, sizeof(unique_id), "%s_transition_duration", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/transition/status");
            cJSON_AddStringToObject(config, "command_topic", "~/transition/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.duration_ms }}");
            cJSON_AddStringToObject(config, "command_template", "{\"duration_ms\": {{ value }}}");
            
            // Range: 200ms to 5000ms (0.2 to 5 seconds)
            cJSON_AddNumberToObject(config, "min", 200);
            cJSON_AddNumberToObject(config, "max", 5000);
            cJSON_AddNumberToObject(config, "step", 100);
            cJSON_AddStringToObject(config, "unit_of_measurement", "ms");
            cJSON_AddStringToObject(config, "icon", "mdi:timer-outline");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("number", "transition_duration", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    return ESP_OK;
}

// Publish select discoveries
esp_err_t mqtt_discovery_publish_selects(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];
    
    // Fade In Curve
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "A1. Animation: Fade In Curve");
            snprintf(unique_id, sizeof(unique_id), "%s_fadein", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/transition/status");
            cJSON_AddStringToObject(config, "command_topic", "~/transition/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.fadein_curve }}");
            cJSON_AddStringToObject(config, "command_template", "{\"fadein_curve\": \"{{ value }}\"}");
            
            cJSON *options = cJSON_CreateArray();
            if (options) {
                cJSON_AddItemToArray(options, cJSON_CreateString("linear"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_in"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_out"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_in_out"));
                cJSON_AddItemToArray(options, cJSON_CreateString("bounce"));
                cJSON_AddItemToObject(config, "options", options);
            }
            
            cJSON_AddStringToObject(config, "icon", "mdi:chart-bell-curve");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("select", "fadein_curve", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Fade Out Curve
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "A2. Animation: Fade Out Curve");
            snprintf(unique_id, sizeof(unique_id), "%s_fadeout", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/transition/status");
            cJSON_AddStringToObject(config, "command_topic", "~/transition/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.fadeout_curve }}");
            cJSON_AddStringToObject(config, "command_template", "{\"fadeout_curve\": \"{{ value }}\"}");
            
            cJSON *options = cJSON_CreateArray();
            if (options) {
                cJSON_AddItemToArray(options, cJSON_CreateString("linear"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_in"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_out"));
                cJSON_AddItemToArray(options, cJSON_CreateString("ease_in_out"));
                cJSON_AddItemToArray(options, cJSON_CreateString("bounce"));
                cJSON_AddItemToObject(config, "options", options);
            }
            
            cJSON_AddStringToObject(config, "icon", "mdi:chart-bell-curve-cumulative");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("select", "fadeout_curve", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Potentiometer Response Curve (moved here to group with animation curves)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "A3. Animation: Potentiometer Curve");
            snprintf(unique_id, sizeof(unique_id), "%s_pot_curve", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/brightness/config/status");
            cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.potentiometer.curve_type }}");
            cJSON_AddStringToObject(config, "command_template", "{\"potentiometer\":{\"curve_type\":\"{{ value }}\"}}");
            
            cJSON *options = cJSON_CreateArray();
            if (options) {
                cJSON_AddItemToArray(options, cJSON_CreateString("linear"));
                cJSON_AddItemToArray(options, cJSON_CreateString("logarithmic"));
                cJSON_AddItemToArray(options, cJSON_CreateString("exponential"));
                cJSON_AddItemToObject(config, "options", options);
            }
            
            cJSON_AddStringToObject(config, "icon", "mdi:chart-curve");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("select", "pot_curve", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    return ret;
}

// Publish button discoveries
esp_err_t mqtt_discovery_publish_buttons(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];
    
    // Restart Button
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Z1. Action: Restart Device");
            snprintf(unique_id, sizeof(unique_id), "%s_restart", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/command");
            cJSON_AddStringToObject(config, "payload_press", "restart");
            cJSON_AddStringToObject(config, "icon", "mdi:restart");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("button", "restart", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Test Transitions Button
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Z2. Action: Test Transitions");
            snprintf(unique_id, sizeof(unique_id), "%s_test_transitions", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/command");
            cJSON_AddStringToObject(config, "payload_press", "test_transitions_start");
            cJSON_AddStringToObject(config, "icon", "mdi:animation-play");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("button", "test_transitions", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Refresh Sensor Values Button
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Z3. Action: Refresh Sensors");
            snprintf(unique_id, sizeof(unique_id), "%s_refresh_sensors", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/command");
            cJSON_AddStringToObject(config, "payload_press", "refresh_sensors");
            cJSON_AddStringToObject(config, "icon", "mdi:refresh");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("button", "refresh_sensors", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Set Time Button (for testing - requires text input in HA)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Z4. Action: Set Time (HH:MM)");
            snprintf(unique_id, sizeof(unique_id), "%s_set_time", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/command");
            cJSON_AddStringToObject(config, "icon", "mdi:clock-edit");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("button", "set_time", config, true);
            cJSON_Delete(config);
        }
    }
    
    return ret;
}

// Publish brightness configuration discoveries - CLEAN BRIGHTNESS-ONLY SYSTEM  
esp_err_t mqtt_discovery_publish_brightness_config(void)
{
    esp_err_t ret = ESP_OK;
    char unique_id[64];
    
    // BRIGHTNESS CONFIGURATION: 10 sliders total
    // Section 1: 5 Zone Brightness Values (brightness only, no units)
    // Section 2: 4 Zone Lux Thresholds (lux only, with lx units)
    // Section 3: 1 Safety PWM Limit (PWM units)
    // Potentiometer curve moved to selects section for better organization
    
    // === SECTION 1: ZONE BRIGHTNESS VALUES (5 sliders) ===
    const char* zones[] = {"very_dark", "dim", "normal", "bright", "very_bright"};
    const char* zone_labels[] = {"Very Dark", "Dim", "Normal", "Bright", "Very Bright"};
    
    // Zone-specific ranges for better user experience
    const int zone_min[] = {1, 10, 40, 100, 200};      // Minimum values for each zone
    const int zone_max[] = {50, 80, 150, 255, 255};    // Maximum values for each zone  
    const int zone_step[] = {1, 1, 1, 1, 1};           // Fine-grained control for all zones
    
    for (int i = 0; i < 5; i++) {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            char name[64];
            snprintf(name, sizeof(name), "%d. Brightness: %s Zone", i+1, zone_labels[i]);
            cJSON_AddStringToObject(config, "name", name);
            snprintf(unique_id, sizeof(unique_id), "%s_brightness_%s", discovery_config.device_id, zones[i]);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/brightness/config/status");
            cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/set");
            
            char value_template[64];
            snprintf(value_template, sizeof(value_template), "{{ value_json.light_sensor.%s.brightness }}", zones[i]);
            cJSON_AddStringToObject(config, "value_template", value_template);
            
            char command_template[64];
            snprintf(command_template, sizeof(command_template), "{\"light_sensor\":{\"%s\":{\"brightness\":{{ value | int }}}}}", zones[i]);
            cJSON_AddStringToObject(config, "command_template", command_template);
            
            cJSON_AddNumberToObject(config, "min", zone_min[i]);
            cJSON_AddNumberToObject(config, "max", zone_max[i]);
            cJSON_AddNumberToObject(config, "step", zone_step[i]);
            cJSON_AddStringToObject(config, "unit_of_measurement", "");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-6");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            char object_id[64];
            snprintf(object_id, sizeof(object_id), "brightness_%s", zones[i]);
            ret = publish_discovery_config("number", object_id, config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
            
            // Add delay between entity publications to prevent MQTT broker overload
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    
    // === SECTION 2: LUX THRESHOLD VALUES (4 sliders) ===  
    const char* threshold_labels[] = {"6. Threshold: Dark→Dim", "7. Threshold: Dim→Normal", "8. Threshold: Normal→Bright", "9. Threshold: Bright→Very Bright"};
    const float threshold_mins[] = {1.0, 31.0, 151.0, 401.0};
    const float threshold_maxs[] = {30.0, 150.0, 400.0, 800.0};
    
    for (int i = 0; i < 4; i++) {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", threshold_labels[i]);
            snprintf(unique_id, sizeof(unique_id), "%s_lux_threshold_%d", discovery_config.device_id, i);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/brightness/config/status");
            cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/set");
            
            char value_template[64];
            snprintf(value_template, sizeof(value_template), "{{ value_json.thresholds[%d] }}", i);
            cJSON_AddStringToObject(config, "value_template", value_template);
            
            char command_template[64];
            snprintf(command_template, sizeof(command_template), "{\"threshold_%d\":{{ value | float }}}", i);
            cJSON_AddStringToObject(config, "command_template", command_template);
            
            cJSON_AddNumberToObject(config, "min", threshold_mins[i]);
            cJSON_AddNumberToObject(config, "max", threshold_maxs[i]);
            cJSON_AddNumberToObject(config, "step", 1.0);
            cJSON_AddStringToObject(config, "unit_of_measurement", "lx");
            cJSON_AddStringToObject(config, "icon", "mdi:brightness-4");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            char object_id[64];
            snprintf(object_id, sizeof(object_id), "lux_threshold_%d", i);
            ret = publish_discovery_config("number", object_id, config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Current Safety PWM Limit (10th slider)
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Current Safety PWM Limit");
            snprintf(unique_id, sizeof(unique_id), "%s_safety_pwm_limit", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/brightness/config/status");
            cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/set");
            
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.potentiometer.safety_limit_max }}");
            cJSON_AddStringToObject(config, "command_template", "{\"potentiometer\":{\"safety_limit_max\":{{ value | int }}}}");
            
            cJSON_AddNumberToObject(config, "min", 20);  // Minimum safe PWM
            cJSON_AddNumberToObject(config, "max", 255); // Maximum possible PWM
            cJSON_AddNumberToObject(config, "step", 5);
            cJSON_AddStringToObject(config, "unit_of_measurement", "PWM");
            cJSON_AddStringToObject(config, "icon", "mdi:security");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("number", "safety_pwm_limit", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }
    
    // Reset to Defaults Button
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Z5. Action: Reset Brightness Config");
            snprintf(unique_id, sizeof(unique_id), "%s_reset_brightness", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/brightness/config/reset");
            cJSON_AddStringToObject(config, "payload_press", "reset");
            cJSON_AddStringToObject(config, "icon", "mdi:restore");
            cJSON_AddStringToObject(config, "availability_topic", "~/availability");
            
            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);
            
            ret = publish_discovery_config("button", "reset_brightness", config, true);
            cJSON_Delete(config);
        }
    }
    
    return ret;
}

// Start discovery with MQTT client
esp_err_t mqtt_discovery_start(esp_mqtt_client_handle_t client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mqtt_client = client;
    
    if (discovery_config.enabled) {
        ESP_LOGI(TAG, "Starting MQTT Discovery publishing...");
        
        // First, clear old entities to prevent duplicates
        ESP_LOGI(TAG, "Clearing old/removed entities...");
        char topic[256];
        
        // Clear removed brightness entities
        const char* zones[] = {"very_dark", "dim", "normal", "bright", "very_bright"};
        for (int i = 0; i < 5; i++) {
            // Clear old min/max lux and brightness entities
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_light_%s_min_lux/config",
                     discovery_config.device_id, zones[i]);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_light_%s_max_lux/config",
                     discovery_config.device_id, zones[i]);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_light_%s_min_brightness/config",
                     discovery_config.device_id, zones[i]);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_light_%s_max_brightness/config",
                     discovery_config.device_id, zones[i]);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        // Clear removed transition duration and individual brightness controls
        snprintf(topic, sizeof(topic), "homeassistant/number/%s/transition_duration/config", discovery_config.node_id);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        snprintf(topic, sizeof(topic), "homeassistant/number/%s/individual_brightness/config", discovery_config.node_id);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // Clear removed potentiometer min/max controls
        snprintf(topic, sizeof(topic), "homeassistant/number/%s/pot_min_brightness/config", discovery_config.node_id);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        snprintf(topic, sizeof(topic), "homeassistant/number/%s/pot_max_brightness/config", discovery_config.node_id);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // Clear old threshold and brightness naming schemes
        for (int i = 0; i < 5; i++) {
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_zone_brightness_%s/config",
                     discovery_config.device_id, zones[i]);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        for (int i = 0; i < 4; i++) {
            snprintf(topic, sizeof(topic), "homeassistant/number/%s_threshold_%d/config",
                     discovery_config.device_id, i);
            esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        // Clear old alphabetical naming schemes
        const char* old_names[] = {"Brightness", "Threshold", "Animation"};
        for (int i = 0; i < 3; i++) {
            for (int j = 1; j <= 5; j++) {
                snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s_%d/config",
                         discovery_config.device_id, old_names[i], j);
                esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        
        // Clear old potentiometer curve from brightness config section
        snprintf(topic, sizeof(topic), "homeassistant/select/%s_pot_curve_brightness/config", discovery_config.device_id);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // Now publish new entities
        return mqtt_discovery_publish_all();
    }
    
    return ESP_OK;
}

// Publish all discovery messages
esp_err_t mqtt_discovery_publish_all(void)
{
    if (!discovery_config.enabled) {
        ESP_LOGW(TAG, "MQTT Discovery is disabled");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Publishing all discovery configurations...");
    
    esp_err_t ret;
    
    // Publish light entity first (main entity)
    ret = mqtt_discovery_publish_light();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish light discovery");
        return ret;
    }
    
    // Publish all sensors
    ret = mqtt_discovery_publish_sensors();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish sensor discoveries");
        return ret;
    }

    // Publish LED validation sensors
    ret = mqtt_discovery_publish_validation_sensors();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish validation sensor discoveries");
        return ret;
    }

    // Publish switches
    ret = mqtt_discovery_publish_switches();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish switch discoveries");
        return ret;
    }
    
    // Publish numbers
    ret = mqtt_discovery_publish_numbers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish number discoveries");
        return ret;
    }
    
    // Publish selects
    ret = mqtt_discovery_publish_selects();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish select discoveries");
        return ret;
    }
    
    // Publish buttons
    ret = mqtt_discovery_publish_buttons();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish button discoveries");
        return ret;
    }
    
    // Publish brightness configuration entities
    ret = mqtt_discovery_publish_brightness_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish brightness config discoveries");
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ All discovery configurations published successfully");
    return ESP_OK;
}

// Remove all discovery configurations
esp_err_t mqtt_discovery_remove_all(void)
{
    if (mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Removing all discovery configurations...");
    
    // List of all discovery topics to clear (including old brightness entities)
    const char* components[] = {"light", "sensor", "switch", "number", "select", "button"};
    
    // Clear all old brightness zone entities first (20 old entities)
    const char* zones[] = {"very_dark", "dim", "normal", "bright", "very_bright"};
    for (int i = 0; i < 5; i++) {
        // Clear old min/max lux and brightness entities
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/number/%s_light_%s_min_lux/config",
                 discovery_config.discovery_prefix, discovery_config.device_id, zones[i]);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        
        snprintf(topic, sizeof(topic), "%s/number/%s_light_%s_max_lux/config",
                 discovery_config.discovery_prefix, discovery_config.device_id, zones[i]);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        
        snprintf(topic, sizeof(topic), "%s/number/%s_light_%s_min_brightness/config",
                 discovery_config.discovery_prefix, discovery_config.device_id, zones[i]);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        
        snprintf(topic, sizeof(topic), "%s/number/%s_light_%s_max_brightness/config",
                 discovery_config.discovery_prefix, discovery_config.device_id, zones[i]);
        esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // Clear each discovery topic
    for (int i = 0; i < sizeof(components)/sizeof(components[0]); i++) {
        // Build topic
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/%s/*/config",
                 discovery_config.discovery_prefix,
                 components[i],
                 discovery_config.node_id);
        
        // Publish empty message to remove discovery
        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, true);
        if (msg_id != -1) {
            ESP_LOGI(TAG, "Removed discovery for %s", components[i]);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    return ESP_OK;
}

// Configuration functions
esp_err_t mqtt_discovery_set_enabled(bool enabled)
{
    discovery_config.enabled = enabled;
    ESP_LOGI(TAG, "MQTT Discovery %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

bool mqtt_discovery_is_enabled(void)
{
    return discovery_config.enabled;
}

esp_err_t mqtt_discovery_set_prefix(const char* prefix)
{
    if (prefix == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(discovery_config.discovery_prefix, prefix, 
            sizeof(discovery_config.discovery_prefix) - 1);
    ESP_LOGI(TAG, "Discovery prefix set to: %s", discovery_config.discovery_prefix);
    return ESP_OK;
}

const char* mqtt_discovery_get_device_id(void)
{
    return discovery_config.device_id;
}

// Stop discovery
esp_err_t mqtt_discovery_stop(void)
{
    mqtt_client = NULL;
    return ESP_OK;
}