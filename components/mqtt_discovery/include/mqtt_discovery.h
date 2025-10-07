#ifndef MQTT_DISCOVERY_H
#define MQTT_DISCOVERY_H

#include "esp_err.h"
#include "mqtt_client.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Discovery configuration
#define DISCOVERY_PREFIX_DEFAULT    "homeassistant"
#define DISCOVERY_NODE_ID           "esp32_wordclock"
#define DISCOVERY_PUBLISH_DELAY_MS  100
#define DEVICE_SW_VERSION          "2.0.0"
#define DEVICE_HW_VERSION          "1.0"
#define DEVICE_MODEL               "ESP32 LED Matrix Clock"
#define DEVICE_MANUFACTURER        "Custom Build"

// Discovery topics
#define DISCOVERY_LIGHT_TOPIC      "%s/light/%s/%s/config"
#define DISCOVERY_SENSOR_TOPIC     "%s/sensor/%s/%s/config"
#define DISCOVERY_SWITCH_TOPIC     "%s/switch/%s/%s/config"
#define DISCOVERY_NUMBER_TOPIC     "%s/number/%s/%s/config"
#define DISCOVERY_SELECT_TOPIC     "%s/select/%s/%s/config"
#define DISCOVERY_BUTTON_TOPIC     "%s/button/%s/%s/config"

// Discovery configuration structure
typedef struct {
    bool enabled;
    char discovery_prefix[32];
    char node_id[32];
    char device_id[32];  // Unique ID based on MAC
    uint32_t publish_delay_ms;
} mqtt_discovery_config_t;

// Device information structure
typedef struct {
    char identifiers[64];
    char connections[64];
    char name[64];
    char model[64];
    char manufacturer[32];
    char sw_version[16];
    char hw_version[16];
    char configuration_url[128];
} mqtt_discovery_device_t;

// Function prototypes
esp_err_t mqtt_discovery_init(void);
esp_err_t mqtt_discovery_start(esp_mqtt_client_handle_t client);
esp_err_t mqtt_discovery_stop(void);
esp_err_t mqtt_discovery_publish_all(void);
esp_err_t mqtt_discovery_remove_all(void);

// Individual discovery publish functions
esp_err_t mqtt_discovery_publish_light(void);
esp_err_t mqtt_discovery_publish_sensors(void);
esp_err_t mqtt_discovery_publish_switches(void);
esp_err_t mqtt_discovery_publish_numbers(void);
esp_err_t mqtt_discovery_publish_selects(void);
esp_err_t mqtt_discovery_publish_buttons(void);
esp_err_t mqtt_discovery_publish_brightness_config(void);

// Helper functions
esp_err_t mqtt_discovery_generate_device_id(char* device_id, size_t len);
esp_err_t mqtt_discovery_get_device_info(mqtt_discovery_device_t* device);
const char* mqtt_discovery_get_device_id(void);

// Configuration functions
esp_err_t mqtt_discovery_set_enabled(bool enabled);
bool mqtt_discovery_is_enabled(void);
esp_err_t mqtt_discovery_set_prefix(const char* prefix);

#ifdef __cplusplus
}
#endif

#endif // MQTT_DISCOVERY_H