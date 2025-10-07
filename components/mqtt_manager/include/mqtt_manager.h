#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// MQTT Configuration - HiveMQ Cloud Secure Broker
#define MQTT_BROKER_URI_DEFAULT "mqtts://5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud:8883"
#define MQTT_CLIENT_ID_PREFIX "ESP32_LED_"
#define MQTT_USERNAME_DEFAULT "esp32_led_device"
#define MQTT_PASSWORD_DEFAULT "tufcux-3xuwda-Vomnys"
#define MQTT_KEEPALIVE 60
#define MQTT_RECONNECT_TIMEOUT_MS 5000
#define MQTT_TASK_STACK_SIZE 4096
#define MQTT_TASK_PRIORITY 5

// Device name configuration - EDIT THIS for multiple devices on same network
// Examples: "wordclock_bedroom", "wordclock_kitchen", "wordclock_living_room"
#define MQTT_DEVICE_NAME "Clock_Stani_1"

// Simplified Topic definitions
#define MQTT_TOPIC_BASE "home/" MQTT_DEVICE_NAME
#define MQTT_TOPIC_STATUS MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_WIFI_STATUS MQTT_TOPIC_BASE "/wifi"
#define MQTT_TOPIC_NTP_STATUS MQTT_TOPIC_BASE "/ntp"
#define MQTT_TOPIC_COMMAND MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_WILL MQTT_TOPIC_BASE "/availability"
#define MQTT_TOPIC_TRANSITION_SET MQTT_TOPIC_BASE "/transition/set"
#define MQTT_TOPIC_TRANSITION_STATUS MQTT_TOPIC_BASE "/transition/status"
#define MQTT_TOPIC_BRIGHTNESS_SET MQTT_TOPIC_BASE "/brightness/set"
#define MQTT_TOPIC_BRIGHTNESS_STATUS MQTT_TOPIC_BASE "/brightness/status"
#define MQTT_TOPIC_BRIGHTNESS_CONFIG_SET MQTT_TOPIC_BASE "/brightness/config/set"
#define MQTT_TOPIC_BRIGHTNESS_CONFIG_GET MQTT_TOPIC_BASE "/brightness/config/get"
#define MQTT_TOPIC_BRIGHTNESS_CONFIG_RESET MQTT_TOPIC_BASE "/brightness/config/reset"
#define MQTT_TOPIC_BRIGHTNESS_CONFIG_STATUS MQTT_TOPIC_BASE "/brightness/config/status"
#define MQTT_TOPIC_NTP_LAST_SYNC MQTT_TOPIC_BASE "/ntp/last_sync"

// MQTT Status Messages
#define MQTT_STATUS_ONLINE "online"
#define MQTT_STATUS_OFFLINE "offline"
#define MQTT_STATUS_CONNECTED "connected"
#define MQTT_STATUS_DISCONNECTED "disconnected"

// MQTT Connection States
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_connection_state_t;

// MQTT Configuration Structure
typedef struct {
    char broker_uri[128];
    char client_id[32];
    char username[32];
    char password[64];
    bool use_ssl;
    uint16_t port;
} mqtt_config_t;

// External global variables (defined in wifi_manager)
extern bool wifi_connected;
extern bool ntp_synced;
extern bool mqtt_connected;

// Core Function Prototypes
esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start(void);
void mqtt_manager_stop(void);
void mqtt_manager_deinit(void);

// Publishing Functions
esp_err_t mqtt_publish_status(const char *status);
esp_err_t mqtt_publish_wifi_status(bool connected);
esp_err_t mqtt_publish_ntp_status(bool synced);
esp_err_t mqtt_publish_availability(bool online);
esp_err_t mqtt_publish_transition_status(uint16_t duration_ms, bool enabled);
esp_err_t mqtt_publish_brightness_status(uint8_t individual, uint8_t global);
esp_err_t mqtt_publish_brightness_config_status(void);
esp_err_t mqtt_publish_sensor_status(void);
esp_err_t mqtt_publish_ntp_last_sync(void);
esp_err_t mqtt_publish_heartbeat_with_ntp(void);

// Configuration Functions
esp_err_t mqtt_load_config(mqtt_config_t *config);
esp_err_t mqtt_save_config(const mqtt_config_t *config);
void mqtt_set_default_config(mqtt_config_t *config);

// Task and Event Handlers
void mqtt_task_run(void *pvParameter);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Utility Functions
const char *mqtt_get_state_string(mqtt_connection_state_t state);
bool mqtt_is_connected(void);
esp_err_t mqtt_subscribe_to_topics(void);

// Discovery Functions
esp_err_t mqtt_manager_discovery_init(void);
esp_err_t mqtt_manager_discovery_publish(void);
esp_err_t mqtt_manager_discovery_remove(void);

#ifdef __cplusplus
}
#endif

#endif  // MQTT_MANAGER_H