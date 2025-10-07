# 📖 API Reference

Comprehensive API documentation for ESP32 German Word Clock components.

## 🧩 Component APIs

### Core Hardware Components

#### I2C Devices (`components/i2c_devices/`)

**Primary Interface**: TLC59116 LED controllers and DS3231 RTC communication

```c
// Initialization
esp_err_t i2c_devices_init(void);

// TLC59116 LED Control
esp_err_t tlc_set_pwm(uint8_t tlc_index, uint8_t channel, uint8_t pwm_value);
esp_err_t tlc_set_global_brightness(uint8_t brightness);
esp_err_t tlc_set_led_brightness_list(const uint8_t *led_list, uint8_t brightness);
esp_err_t tlc_turn_off_all_leds(void);
esp_err_t tlc_read_all_status(uint8_t *status_array);

// DS3231 RTC
esp_err_t ds3231_read_time(uint8_t *time_data);
esp_err_t ds3231_write_time(const uint8_t *time_data);
esp_err_t ds3231_set_time_struct(const wordclock_time_t *time);
esp_err_t ds3231_get_time_struct(wordclock_time_t *time);
```

**Key Data Structures**:
```c
typedef struct {
    uint8_t hours;      // 0-23
    uint8_t minutes;    // 0-59
    uint8_t seconds;    // 0-59
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint8_t year;       // 0-99 (20xx)
} wordclock_time_t;
```

#### Word Clock Display (`components/wordclock_display/`)

**Primary Interface**: German word display logic and LED matrix control

```c
// Display Management
esp_err_t wordclock_display_init(void);
esp_err_t wordclock_display_update(const wordclock_time_t *time);
esp_err_t wordclock_display_set_brightness(uint8_t brightness);
esp_err_t wordclock_display_clear(void);

// German Time Processing
void display_german_time(uint8_t hours, uint8_t minutes);
esp_err_t update_led_if_changed(uint8_t row, uint8_t col, uint8_t new_brightness);
esp_err_t refresh_current_display(void);
```

#### Word Clock Time (`components/wordclock_time/`)

**Primary Interface**: Time calculation and German grammar logic

```c
// Time Processing
esp_err_t wordclock_time_init(void);
esp_err_t wordclock_time_get(wordclock_time_t *time);
esp_err_t wordclock_time_set(const wordclock_time_t *time);

// German Time Calculations
uint8_t wordclock_calculate_base_minutes(uint8_t minutes);      // (MM/5)*5
uint8_t wordclock_calculate_indicators(uint8_t minutes);        // MM - base_minutes
uint8_t wordclock_get_display_hour(uint8_t hour, uint8_t base_minutes);

// Word Position Management
word_position_t get_word_position(const char* word);
esp_err_t set_word_leds(const char* word, uint8_t brightness);
```

**Key Data Structures**:
```c
typedef struct {
    uint8_t row;
    uint8_t start_col;
    uint8_t length;
} word_position_t;

typedef struct {
    const char* word;
    uint8_t row;
    uint8_t start_col;
    uint8_t length;
} word_definition_t;
```

### Sensor and Control Components

#### ADC Manager (`components/adc_manager/`)

**Primary Interface**: Potentiometer brightness control

```c
// ADC Management
esp_err_t adc_manager_init(void);
esp_err_t adc_read_potentiometer_averaged(adc_reading_t *reading);

// Brightness Control
uint8_t adc_voltage_to_brightness(uint32_t voltage_mv);
esp_err_t update_brightness_from_potentiometer(void);
esp_err_t set_global_brightness(uint8_t brightness);
```

**Key Data Structures**:
```c
typedef struct {
    uint32_t raw_reading;    // Raw ADC value (0-4095)
    uint32_t voltage_mv;     // Voltage in millivolts
    uint8_t brightness;      // Mapped brightness (5-80)
} adc_reading_t;
```

#### Light Sensor (`components/light_sensor/`)

**Primary Interface**: BH1750 ambient light sensor integration

```c
// Sensor Management
esp_err_t light_sensor_init(void);
esp_err_t light_sensor_read(light_reading_t *reading);

// Brightness Mapping
uint8_t light_lux_to_brightness(float lux);
esp_err_t light_update_global_brightness(void);
void light_sensor_task(void *pvParameters);
```

**Key Data Structures**:
```c
typedef struct {
    float lux;               // Light level in lux
    uint8_t global_brightness; // Mapped global brightness (20-255)
    uint32_t timestamp;      // Reading timestamp
} light_reading_t;
```

### Network Components

#### WiFi Manager (`components/wifi_manager/`)

**Primary Interface**: WiFi connectivity and configuration

```c
// WiFi Management
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_sta(const char* ssid, const char* password);
esp_err_t wifi_manager_start_ap(void);
esp_err_t wifi_manager_stop(void);

// Status and Monitoring
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_info(wifi_info_t *info);
void wifi_manager_monitor_task(void *pvParameters);
```

**Global Variables**:
```c
extern bool wifi_connected;  // Global WiFi connection status
```

#### NTP Manager (`components/ntp_manager/`)

**Primary Interface**: Network time synchronization

```c
// NTP Management
esp_err_t ntp_manager_init(void);
esp_err_t ntp_start_sync(void);
bool ntp_is_synced(void);

// Time Synchronization
esp_err_t ntp_sync_to_rtc(void);
time_t ntp_get_last_sync_time(void);
esp_err_t ntp_format_last_sync_iso8601(char *buffer, size_t buffer_size);

// Callback
void ntp_time_sync_notification_cb(struct timeval *tv);
```

**Configuration**:
```c
#define NTP_SERVER_PRIMARY    "pool.ntp.org"
#define NTP_SERVER_SECONDARY  "time.google.com"
#define NTP_TIMEZONE         "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SYNC_TIMEOUT_MS   30000
```

**Global Variables**:
```c
extern bool ntp_synced;  // Global NTP synchronization status
```

#### MQTT Manager (`components/mqtt_manager/`)

**Primary Interface**: Secure MQTT communication with HiveMQ Cloud

```c
// MQTT Management
esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start(void);
void mqtt_manager_stop(void);
void mqtt_manager_deinit(void);

// Publishing Functions
esp_err_t mqtt_publish_status(const char* status);
esp_err_t mqtt_publish_wifi_status(bool connected);
esp_err_t mqtt_publish_ntp_status(bool synced);
esp_err_t mqtt_publish_ntp_last_sync(void);
esp_err_t mqtt_publish_availability(bool online);
esp_err_t mqtt_publish_heartbeat_with_ntp(void);

// Configuration Management
esp_err_t mqtt_load_config(mqtt_config_t *config);
esp_err_t mqtt_save_config(const mqtt_config_t *config);
bool mqtt_is_connected(void);
```

**Key Data Structures**:
```c
typedef struct {
    char broker_uri[128];
    char client_id[32];
    char username[32];
    char password[64];
    bool use_ssl;
    uint16_t port;
} mqtt_config_t;

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_connection_state_t;
```

**MQTT Topics**:
```c
#define MQTT_TOPIC_BASE             "home/esp32_core"
#define MQTT_TOPIC_STATUS           MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_COMMAND          MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_NTP_LAST_SYNC    MQTT_TOPIC_BASE "/ntp/last_sync"
// ... (see mqtt_manager.h for complete list)
```

**Global Variables**:
```c
extern bool mqtt_connected;  // Global MQTT connection status
```

### Smart Home Integration Components

#### MQTT Discovery (`components/mqtt_discovery/`)

**Primary Interface**: Home Assistant auto-discovery

```c
// Discovery Management
esp_err_t mqtt_discovery_init(void);
esp_err_t mqtt_discovery_start(esp_mqtt_client_handle_t client);
esp_err_t mqtt_discovery_stop(void);

// Entity Publishing
esp_err_t mqtt_discovery_publish_light(void);
esp_err_t mqtt_discovery_publish_sensors(void);
esp_err_t mqtt_discovery_publish_controls(void);
esp_err_t mqtt_discovery_publish_brightness_config(void);

// Device Management
esp_err_t mqtt_discovery_generate_device_id(char* device_id, size_t len);
esp_err_t mqtt_discovery_get_device_info(mqtt_discovery_device_t* device);
```

**Key Data Structures**:
```c
typedef struct {
    bool enabled;
    const char* discovery_prefix;
    const char* node_id;
    char device_id[32];
    uint32_t publish_delay_ms;
} mqtt_discovery_config_t;

typedef struct {
    char identifiers[64];
    char connections[32];
    char name[64];
    char model[32];
    char manufacturer[32];
    char sw_version[16];
    char hw_version[16];
    char configuration_url[128];
} mqtt_discovery_device_t;
```

#### Button Manager (`components/button_manager/`)

**Primary Interface**: Reset button functionality

```c
// Button Management
esp_err_t button_manager_init(void);
esp_err_t button_manager_start(void);
void button_manager_stop(void);

// Button State
bool button_manager_is_pressed(void);
uint32_t button_manager_get_press_duration(void);
void button_manager_task(void *pvParameters);
```

#### Status LED Manager (`components/status_led_manager/`)

**Primary Interface**: Network status LED indicators

```c
// LED Management
esp_err_t status_led_manager_init(void);
esp_err_t status_led_manager_start(void);
void status_led_manager_stop(void);

// LED Control
esp_err_t status_led_set_wifi(status_led_state_t state);
esp_err_t status_led_set_ntp(status_led_state_t state);
void status_led_manager_task(void *pvParameters);
```

**Key Data Structures**:
```c
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK_FAST,
    LED_STATE_BLINK_SLOW
} status_led_state_t;
```

### Advanced Feature Components

#### Transition Manager (`components/transition_manager/`)

**Primary Interface**: Smooth LED animation system

```c
// Transition Management
esp_err_t transition_manager_init(void);
esp_err_t transition_start_batch(const transition_request_t *requests, uint8_t count);
bool transition_manager_is_active(void);

// Animation Control
esp_err_t start_transition_test_mode(void);
esp_err_t stop_transition_test_mode(void);
bool is_transition_test_mode(void);

// Curve Functions
uint8_t apply_transition_curve(uint8_t progress, transition_curve_t curve);
```

**Key Data Structures**:
```c
typedef enum {
    CURVE_LINEAR = 0,
    CURVE_EASE_IN,
    CURVE_EASE_OUT,
    CURVE_EASE_IN_OUT,
    CURVE_BOUNCE
} transition_curve_t;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t start_brightness;
    uint8_t target_brightness;
    uint16_t duration_ms;
    transition_curve_t curve;
} transition_request_t;
```

#### Brightness Config (`components/brightness_config/`)

**Primary Interface**: Advanced brightness calibration

```c
// Configuration Management
esp_err_t brightness_config_init(void);
esp_err_t brightness_config_load_from_nvs(void);
esp_err_t brightness_config_save_to_nvs(void);
esp_err_t brightness_config_reset_to_defaults(void);

// Light Sensor Configuration
esp_err_t brightness_config_set_light_sensor_zone(brightness_zone_t zone, 
                                                  const light_sensor_zone_config_t *config);
esp_err_t brightness_config_get_light_sensor_zone(brightness_zone_t zone,
                                                  light_sensor_zone_config_t *config);

// Potentiometer Configuration
esp_err_t brightness_config_set_potentiometer_config(const potentiometer_config_t *config);
esp_err_t brightness_config_get_potentiometer_config(potentiometer_config_t *config);
```

### Tier 1 MQTT Components (Professional)

#### MQTT Command Processor (`components/mqtt_command_processor/`)

**Primary Interface**: Structured command handling

```c
// Command Processing
esp_err_t mqtt_command_processor_init(void);
esp_err_t mqtt_command_processor_register(const mqtt_command_definition_t *cmd_def);
esp_err_t mqtt_command_processor_execute(const char *command, const char *payload);

// Command Validation
bool mqtt_command_processor_validate(const char *command, const char *payload);
esp_err_t mqtt_command_processor_get_schema(const char *command, char *schema_buffer, size_t buffer_size);
```

#### MQTT Schema Validator (`components/mqtt_schema_validator/`)

**Primary Interface**: JSON validation framework

```c
// Schema Management
esp_err_t mqtt_schema_validator_init(void);
esp_err_t mqtt_schema_validator_register_schema(const char *schema_name, const char *schema_json);
esp_err_t mqtt_schema_validator_validate(const char *schema_name, const char *json_data);

// Validation Results
esp_err_t mqtt_schema_validator_get_last_error(char *error_buffer, size_t buffer_size);
bool mqtt_schema_validator_is_valid_json(const char *json_data);
```

#### MQTT Message Persistence (`components/mqtt_message_persistence/`)

**Primary Interface**: Reliable message delivery

```c
// Persistence Management
esp_err_t mqtt_message_persistence_init(void);
esp_err_t mqtt_message_persistence_store(const char *topic, const char *payload, uint8_t qos);
esp_err_t mqtt_message_persistence_retrieve_pending(mqtt_pending_message_t *messages, uint8_t max_count);

// Message Management
esp_err_t mqtt_message_persistence_mark_delivered(uint32_t message_id);
esp_err_t mqtt_message_persistence_cleanup_old_messages(uint32_t max_age_seconds);
uint32_t mqtt_message_persistence_get_pending_count(void);
```

## 🔧 Common Patterns

### Error Handling Pattern
```c
esp_err_t function_example(void) {
    esp_err_t ret = ESP_OK;
    
    // Validate parameters
    if (parameter == NULL) {
        ESP_LOGE(TAG, "Invalid parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Perform operation with error checking
    ret = some_operation();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Operation completed successfully");
    return ESP_OK;
}
```

### Task Creation Pattern
```c
void component_task(void *pvParameters) {
    while (1) {
        // Perform task work
        
        // Proper delay
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t component_start_task(void) {
    BaseType_t ret = xTaskCreate(
        component_task,
        "component_task",
        4096,           // Stack size
        NULL,           // Parameters
        5,              // Priority
        &task_handle    // Task handle
    );
    
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
```

### Configuration Pattern
```c
typedef struct {
    bool enabled;
    uint32_t interval_ms;
    char name[32];
} component_config_t;

static component_config_t config = {
    .enabled = true,
    .interval_ms = 1000,
    .name = "default"
};

esp_err_t component_set_config(const component_config_t *new_config) {
    if (new_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&config, new_config, sizeof(component_config_t));
    return ESP_OK;
}
```

---
📖 **Scope**: Complete API reference for all components  
🎯 **Audience**: Developers integrating with or extending the system  
📝 **Maintenance**: Keep synchronized with component headers