# 📖 API Reference

Comprehensive API documentation for ESP32-S3 German Word Clock components.

**Last Updated:** 2025-11-12
**Firmware Version:** v2.8+
**Components Documented:** 28

---

## Table of Contents

- [Hardware Components](#hardware-components)
- [Sensor and Control Components](#sensor-and-control-components)
- [Network Components](#network-components)
- [Smart Home Integration](#smart-home-integration)
- [System Components](#system-components)
- [Audio and Media Components](#audio-and-media-components)
- [Storage Components](#storage-components)
- [Validation and Error Handling](#validation-and-error-handling)
- [Common Patterns](#common-patterns)

---

## Hardware Components

### I2C Devices (`components/i2c_devices/`)

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

### Word Clock Display (`components/wordclock_display/`)

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

### Word Clock Time (`components/wordclock_time/`)

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

### Board Config (`components/board_config/`)

**Primary Interface**: Multi-board hardware abstraction layer

**Board Selection**:
```c
// Compile-time defines (only one should be set)
CONFIG_BOARD_YB_AMP      // YB-ESP32-S3-AMP (default - integrated peripherals)
CONFIG_BOARD_DEVKITC     // ESP32-S3-DevKitC-1 (legacy - external peripherals)
```

**Runtime Information**:
```c
const char* board_get_name(void);                    // "YB-ESP32-S3-AMP" or "ESP32-S3-DevKitC-1"
uint8_t board_get_flash_size_mb(void);              // 8 or 16 MB
uint8_t board_get_psram_size_mb(void);              // 2 or 8 MB
bool board_is_audio_integrated(void);               // Integrated vs external MAX98357A
bool board_is_sd_integrated(void);                  // Integrated vs external SD card
uint8_t board_get_audio_channels(void);             // 2 (stereo YB) or 1 (mono DevKitC)
bool board_has_status_led(void);                    // GPIO 47 (YB) or none (DevKitC)
int8_t board_get_status_led_gpio(void);             // 47 or -1
```

**Board Capability Macros**:
```c
#define BOARD_NAME                        // Board identifier string
#define BOARD_FLASH_SIZE_MB               // 8 or 16
#define BOARD_PSRAM_SIZE_MB               // 2 or 8
#define BOARD_AUDIO_INTEGRATED            // 1 or 0
#define BOARD_SD_INTEGRATED               // 1 or 0
#define BOARD_AUDIO_CHANNELS              // 2 or 1
#define BOARD_HAS_STATUS_LED              // 1 or 0

// Helper macros
BOARD_HAS_SUFFICIENT_PSRAM(required_mb)
BOARD_HAS_SUFFICIENT_FLASH(required_mb)
```

**GPIO Pin Mappings**: (Mostly identical across boards)
```c
// I2C LEDs (TLC59116) - Same on both boards
#define BOARD_I2C_LEDS_SDA_GPIO           8
#define BOARD_I2C_LEDS_SCL_GPIO           9

// I2C Sensors (DS3231, BH1750) - Different SCL!
#define BOARD_I2C_SENSORS_SDA_GPIO        1
#define BOARD_I2C_SENSORS_SCL_GPIO        42 (YB) / 18 (DevKitC)
// Note: GPIO 42 is WiFi-safe, GPIO 18 has ADC2 conflict

// I2S Audio - Same on both boards
#define BOARD_I2S_BCLK_GPIO               5
#define BOARD_I2S_LRCLK_GPIO              6
#define BOARD_I2S_DOUT_GPIO               7

// MicroSD SPI - Same on both boards
#define BOARD_SD_CS_GPIO                  10
#define BOARD_SD_MOSI_GPIO                11
#define BOARD_SD_CLK_GPIO                 12
#define BOARD_SD_MISO_GPIO                13

// Other - Same on both boards
#define BOARD_ADC_POTENTIOMETER_GPIO      3
#define BOARD_WIFI_STATUS_LED_GPIO        21
#define BOARD_NTP_STATUS_LED_GPIO         38
#define BOARD_RESET_BUTTON_GPIO           0
```

**Critical Notes**:
- YB-AMP: 2MB Quad SPI PSRAM (`CONFIG_SPIRAM_MODE_QUAD`)
- DevKitC: 8MB Octal SPI PSRAM (`CONFIG_SPIRAM_MODE_OCT`)
- GPIO 42 (YB) is WiFi-safe vs GPIO 18 (DevKitC) has ADC2 conflict

---

## Sensor and Control Components

### ADC Manager (`components/adc_manager/`)

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

### Light Sensor (`components/light_sensor/`)

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
    float lux;                   // Light level in lux
    uint8_t global_brightness;   // Mapped global brightness (20-255)
    uint32_t timestamp;          // Reading timestamp
} light_reading_t;
```

### Button Manager (`components/button_manager/`)

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

### Status LED Manager (`components/status_led_manager/`)

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

---

## Network Components

### WiFi Manager (`components/wifi_manager/`)

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

### NTP Manager (`components/ntp_manager/`)

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

### MQTT Manager (`components/mqtt_manager/`)

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
esp_err_t mqtt_publish_ota_source_status(const char *json_payload);

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
#define MQTT_TOPIC_BASE                "home/esp32_core"
#define MQTT_TOPIC_STATUS              MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_COMMAND             MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_NTP_LAST_SYNC       MQTT_TOPIC_BASE "/ntp/last_sync"
#define MQTT_TOPIC_OTA_SOURCE_SET      MQTT_TOPIC_BASE "/ota/source/set"
#define MQTT_TOPIC_OTA_SOURCE_STATUS   MQTT_TOPIC_BASE "/ota/source/status"
// ... (see mqtt_manager.h for complete list)
```

**Global Variables**:
```c
extern bool mqtt_connected;  // Global MQTT connection status
```

### Web Server (`components/web_server/`)

**Primary Interface**: WiFi configuration captive portal

```c
// Server Management
esp_err_t web_server_prescan_networks(void);  // Scan networks before starting AP
httpd_handle_t web_server_start(void);
void web_server_stop(void);
void web_server_cleanup(void);
void web_server_debug_credentials(void);
```

**Usage Flow**:
1. Connect to WiFi AP "ESP32-LED-Config" (password: 12345678)
2. Visit http://192.168.4.1
3. Select network and enter password
4. System saves credentials and reboots to connect

---

## Smart Home Integration

### MQTT Discovery (`components/mqtt_discovery/`)

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

**Entities Created**: 36 total
- 1 Light (main display control)
- 7 Sensors (WiFi, NTP, brightness levels, light sensor, potentiometer)
- 24 Config Controls (zone brightness, curves, safety limits)
- 4 Buttons (restart, test transitions, refresh sensors, set time)

### MQTT Command Processor (`components/mqtt_command_processor/`)

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

### MQTT Schema Validator (`components/mqtt_schema_validator/`)

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

### MQTT Message Persistence (`components/mqtt_message_persistence/`)

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

---

## System Components

### NVS Manager (`components/nvs_manager/`)

**Primary Interface**: Non-Volatile Storage wrapper for WiFi credentials

```c
// NVS Management
esp_err_t nvs_manager_init(void);
esp_err_t nvs_manager_save_wifi_credentials(const char* ssid, const char* password);
esp_err_t nvs_manager_load_wifi_credentials(char* ssid, char* password, size_t ssid_len, size_t pass_len);
void nvs_manager_clear_wifi_credentials(void);
```

**Key Constants**:
```c
#define NVS_WIFI_NAMESPACE          "wifi_config"
#define NVS_SSID_KEY                "ssid"
#define NVS_PASSWORD_KEY            "password"
```

### OTA Manager (`components/ota_manager/`) ⭐ **RECENTLY UPDATED**

**Primary Interface**: Secure OTA firmware updates with SHA-256 verification

**Recent Changes** (2025-11-12):
- ✅ Added SHA-256 firmware verification
- ✅ Added `sha256[65]` field to `firmware_version_t`
- ✅ Integrated mbedtls SHA-256 calculation
- ✅ Automatic checksum verification after download
- ✅ Abort on mismatch before flashing

#### Data Structures

**OTA Sources**:
```c
typedef enum {
    OTA_SOURCE_GITHUB = 0,        // GitHub raw URLs (primary, auto-updated on push)
    OTA_SOURCE_CLOUDFLARE = 1     // Cloudflare R2 CDN (backup, manual upload)
} ota_source_t;
```

**OTA States**:
```c
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,          // ⭐ NEW: SHA-256 verification stage
    OTA_STATE_FLASHING,
    OTA_STATE_COMPLETE,
    OTA_STATE_FAILED
} ota_state_t;
```

**OTA Errors**:
```c
typedef enum {
    OTA_ERROR_NONE,
    OTA_ERROR_NO_INTERNET,
    OTA_ERROR_DOWNLOAD_FAILED,
    OTA_ERROR_CHECKSUM_MISMATCH,  // ⭐ NEW: SHA-256 mismatch
    OTA_ERROR_FLASH_FAILED,
    OTA_ERROR_NO_UPDATE_PARTITION,
    OTA_ERROR_INVALID_VERSION,
    OTA_ERROR_LOW_MEMORY,
    OTA_ERROR_ALREADY_RUNNING
} ota_error_t;
```

**Firmware Version** ⭐ **UPDATED**:
```c
typedef struct {
    char version[32];             // e.g., "v2.3.1"
    char build_date[16];          // e.g., "2025-11-07"
    char idf_version[16];         // e.g., "5.4.2"
    uint32_t size_bytes;          // Firmware size in bytes
    char sha256[65];              // ⭐ NEW: SHA-256 checksum (64 hex chars + null)
} firmware_version_t;
```

**OTA Progress**:
```c
typedef struct {
    ota_state_t state;
    ota_error_t error;
    uint8_t progress_percent;     // 0-100%
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    uint32_t time_elapsed_ms;
    uint32_t time_remaining_ms;
} ota_progress_t;
```

**OTA Configuration**:
```c
typedef struct {
    const char *firmware_url;
    const char *version_url;
    bool auto_reboot;
    uint32_t timeout_ms;
    bool skip_version_check;
} ota_config_t;
```

#### Public Functions

**Initialization**:
```c
esp_err_t ota_manager_init(void);
bool ota_manager_is_available(void);
```

**Version Management**:
```c
esp_err_t ota_get_current_version(firmware_version_t *version);
esp_err_t ota_check_for_updates(firmware_version_t *available_version);
```

**Update Execution**:
```c
esp_err_t ota_start_update(const ota_config_t *config);
esp_err_t ota_start_update_default(void);
esp_err_t ota_cancel_update(void);
```

**Progress Monitoring**:
```c
esp_err_t ota_get_progress(ota_progress_t *progress);
ota_state_t ota_get_state(void);
```

**Post-Update Validation**:
```c
bool ota_is_first_boot_after_update(void);
esp_err_t ota_mark_app_valid(void);
esp_err_t ota_trigger_rollback(void);
esp_err_t ota_perform_health_checks(void);
```

**Diagnostics**:
```c
const char* ota_get_running_partition_name(void);
uint32_t ota_get_boot_count(void);
const char* ota_error_to_string(ota_error_t error);
```

**Source Management** ⭐ **NEW**:
```c
esp_err_t ota_set_source(ota_source_t source);
ota_source_t ota_get_source(void);
const char* ota_source_to_string(ota_source_t source);
esp_err_t ota_source_from_string(const char *source_str, ota_source_t *source);
```

#### SHA-256 Verification Details ⭐

**Verification Flow**:
1. **Parse version.json** → Extract `sha256` field
2. **Store expected checksum** → `ota_context.expected_sha256[65]`
3. **Download firmware** → HTTPS OTA to partition
4. **Calculate actual SHA-256** → Read partition in 4KB chunks, hash with mbedtls
5. **Compare checksums** → Case-insensitive `strcasecmp()`
6. **Abort if mismatch** → `OTA_ERROR_CHECKSUM_MISMATCH`, abort before flashing
7. **Flash if match** → Proceed with `esp_https_ota_finish()`

**Version JSON Format**:
```json
{
  "version": "v2.8.1",
  "build_date": "2025-11-12",
  "idf_version": "5.4.2",
  "size_bytes": 1332336,
  "sha256": "e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1"
}
```

**Security Benefits**:
- ✅ Detects network corruption during download
- ✅ Prevents malicious firmware injection
- ✅ Validates data integrity across network transfer
- ✅ Works with both OTA sources (GitHub + Cloudflare)
- ✅ 2^256 collision resistance (SHA-256 cryptographic strength)
- ✅ Defense in depth (works even if TLS is compromised)

**Performance**:
- Calculation time: ~3-5 seconds for 1.3 MB firmware
- Memory usage: +4KB heap during verification
- Chunk size: 4KB for efficient partition reading

**Backwards Compatibility**:
- If `sha256` field missing in version.json → Verification skipped with warning
- No breaking changes to existing deployments

#### Key Constants

```c
// GitHub URLs (primary source, auto-updated on git push)
#define OTA_GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin"
#define OTA_GITHUB_VERSION_URL  "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json"

// Cloudflare R2 URLs (backup source, manual upload required)
#define OTA_CLOUDFLARE_FIRMWARE_URL "https://pub-3ef23ec5bae14629b323217785443321.r2.dev/wordclock.bin"
#define OTA_CLOUDFLARE_VERSION_URL  "https://pub-3ef23ec5bae14629b323217785443321.r2.dev/version.json"

#define OTA_DEFAULT_SOURCE       OTA_SOURCE_GITHUB
#define OTA_DEFAULT_TIMEOUT_MS   120000  // 2 minutes
```

---

## Audio and Media Components

### Audio Manager (`components/audio_manager/`)

**Primary Interface**: I2S audio output for MAX98357A amplifiers

```c
// Initialization
esp_err_t audio_manager_init(void);
esp_err_t audio_manager_deinit(void);

// Playback Functions
esp_err_t audio_play_pcm(const int16_t *pcm_data, size_t sample_count);
esp_err_t audio_play_from_flash(uint32_t flash_addr, size_t sample_count);
esp_err_t audio_play_from_file(const char *filepath);
esp_err_t audio_play_test_tone(void);  // 440Hz test tone (2 seconds)

// Playback Control
esp_err_t audio_stop(void);
esp_err_t audio_wait_until_done(uint32_t timeout_ms);
bool audio_is_playing(void);

// Volume Control
esp_err_t audio_set_volume(uint8_t volume_percent);  // 0-100%
uint8_t audio_get_volume(void);

// Statistics
esp_err_t audio_get_stats(uint32_t *samples_played, uint32_t *buffer_underruns);
```

**Audio Specifications**:
```c
#define AUDIO_SAMPLE_RATE       16000  // 16 kHz
#define AUDIO_BITS_PER_SAMPLE   16     // 16-bit
#define AUDIO_CHANNELS          1      // Mono (YB-AMP can do stereo)
#define AUDIO_DMA_BUF_COUNT     8      // Number of DMA buffers
#define AUDIO_DMA_BUF_LEN       256    // Samples per buffer

// GPIO Pins (same on both boards)
#define I2S_GPIO_BCLK           5      // Bit Clock
#define I2S_GPIO_LRCLK          6      // Left/Right Clock
#define I2S_GPIO_DOUT           7      // Data Out
```

### Chime Manager (`components/chime_manager/`)

**Primary Interface**: Westminster quarter-hour chime scheduler

```c
// Initialization
esp_err_t chime_manager_init(void);
esp_err_t chime_manager_deinit(void);

// Configuration
esp_err_t chime_manager_enable(bool enabled);
bool chime_manager_is_enabled(void);
esp_err_t chime_manager_set_quiet_hours(uint8_t start_hour, uint8_t end_hour);
esp_err_t chime_manager_get_quiet_hours(uint8_t *start_hour, uint8_t *end_hour);
esp_err_t chime_manager_set_chime_set(const char *chime_set);
const char* chime_manager_get_chime_set(void);
esp_err_t chime_manager_set_volume(uint8_t volume);
uint8_t chime_manager_get_volume(void);

// Runtime
esp_err_t chime_manager_check_time(void);  // Called every minute
esp_err_t chime_manager_play_test(chime_type_t type);

// Storage
esp_err_t chime_manager_get_config(chime_config_t *config);
esp_err_t chime_manager_reset_config(void);
esp_err_t chime_manager_save_config(void);
```

**Key Data Structures**:
```c
typedef enum {
    CHIME_QUARTER_PAST,   // :15 - 4 notes
    CHIME_HALF_PAST,      // :30 - 8 notes
    CHIME_QUARTER_TO,     // :45 - 12 notes
    CHIME_HOUR,           // :00 - 16 notes + hour strikes
    CHIME_TEST_SINGLE,    // Test: single bell strike
    CHIME_TYPE_MAX
} chime_type_t;

typedef struct {
    bool enabled;
    uint8_t quiet_start_hour;  // 0-23 (default: 22 = 10 PM)
    uint8_t quiet_end_hour;    // 0-23 (default: 7 = 7 AM)
    uint8_t volume;            // 0-100%
    char chime_set[32];        // "WESTMINSTER", "BIGBEN", etc.
} chime_config_t;
```

**SD Card File Paths** (FAT32 with LFN support):
```c
#define CHIME_BASE_PATH             "/sdcard/CHIMES"
#define CHIME_DIR_NAME              "WESTMINSTER"
#define CHIME_QUARTER_PAST_FILE     "QUARTER_PAST.PCM"
#define CHIME_HALF_PAST_FILE        "HALF_PAST.PCM"
#define CHIME_QUARTER_TO_FILE       "QUARTER_TO.PCM"
#define CHIME_HOUR_FILE             "HOUR.PCM"
#define CHIME_STRIKE_FILE           "STRIKE.PCM"
```

---

## Storage Components

### SD Card Manager (`components/sdcard_manager/`)

**Primary Interface**: SPI SD card interface for audio files

```c
// Initialization
esp_err_t sdcard_manager_init(void);
esp_err_t sdcard_manager_deinit(void);
bool sdcard_is_initialized(void);

// File Operations
bool sdcard_file_exists(const char *filepath);
esp_err_t sdcard_get_file_size(const char *filepath, size_t *size);
esp_err_t sdcard_list_files(const char *dirpath, char ***files, size_t *count);
FILE* sdcard_open_file(const char *filepath);
void sdcard_close_file(FILE *file);

// Information
esp_err_t sdcard_get_info(uint64_t *total_bytes, uint64_t *used_bytes);
```

**Key Constants**:
```c
#define SDCARD_MOUNT_POINT      "/sdcard"
#define SDCARD_MAX_FILES        20

// GPIO Pins (SPI, same on both boards)
#define SDCARD_GPIO_MOSI        11
#define SDCARD_GPIO_MISO        13
#define SDCARD_GPIO_CLK         12
#define SDCARD_GPIO_CS          10
```

### External Flash (`components/external_flash/`) **(OPTIONAL)**

**Primary Interface**: W25Q64 8MB SPI flash driver

**Note**: Optional hardware component - system works without W25Q64 installed.

```c
// Initialization
esp_err_t external_flash_init(void);
bool external_flash_available(void);

// Basic Operations
esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size);
esp_err_t external_flash_write(uint32_t address, const void *data, size_t size);
esp_err_t external_flash_erase_sector(uint32_t address);
esp_err_t external_flash_erase_range(uint32_t start_address, size_t size);

// Identification
esp_err_t external_flash_read_jedec_id(uint8_t *manufacturer, uint8_t *mem_type, uint8_t *capacity);
esp_err_t external_flash_verify_chip(void);
uint32_t external_flash_get_size(void);

// Diagnostics
esp_err_t external_flash_read_status(uint8_t *status);
esp_err_t external_flash_wait_ready(uint32_t timeout_ms);
void external_flash_get_stats(uint32_t *total_reads, uint32_t *total_writes, uint32_t *total_erases);
void external_flash_reset_stats(void);

// Filesystem Integration
esp_err_t external_flash_register_partition(const esp_partition_t **out_partition);

// Testing
int external_flash_run_all_tests(void);   // 9 comprehensive tests
bool external_flash_quick_test(void);     // Fast smoke test
```

**Key Constants**:
```c
#define EXTERNAL_FLASH_SIZE         (8 * 1024 * 1024)  // 8 MB
#define EXTERNAL_FLASH_SECTOR_SIZE  4096               // 4 KB
#define EXTERNAL_FLASH_PAGE_SIZE    256                // 256 bytes

// JEDEC ID for W25Q64
#define W25Q64_JEDEC_MFG            0xEF
#define W25Q64_JEDEC_MEM_TYPE       0x40
#define W25Q64_JEDEC_CAPACITY       0x17
```

### Filesystem Manager (`components/filesystem_manager/`)

**Primary Interface**: LittleFS filesystem on external flash

```c
// Initialization
esp_err_t filesystem_manager_init(void);
esp_err_t filesystem_manager_deinit(void);
bool filesystem_is_initialized(void);

// File Operations
esp_err_t filesystem_list_files(const char *path, filesystem_file_info_t *file_list, size_t max_files, size_t *count);
esp_err_t filesystem_delete_file(const char *filepath);
esp_err_t filesystem_rename_file(const char *old_path, const char *new_path);
bool filesystem_file_exists(const char *filepath);
esp_err_t filesystem_get_file_size(const char *filepath, size_t *size);

// Directory Operations
esp_err_t filesystem_create_directory(const char *path);

// Convenience Functions
esp_err_t filesystem_read_file(const char *filepath, uint8_t **buffer, size_t *size);
esp_err_t filesystem_write_file(const char *filepath, const uint8_t *buffer, size_t size);

// Filesystem Management
esp_err_t filesystem_get_usage(size_t *total_bytes, size_t *used_bytes);
esp_err_t filesystem_format(void);  // WARNING: Erases all files!
```

**Key Data Structures**:
```c
typedef struct {
    char name[256];
    char path[256];
    size_t size;
    bool is_directory;
} filesystem_file_info_t;
```

**Key Constants**:
```c
#define FILESYSTEM_MAX_FILES        32
#define FILESYSTEM_MAX_FILENAME     256
#define FILESYSTEM_MOUNT_POINT      "/storage"
#define FILESYSTEM_CHIMES_DIR       "/storage/chimes"
#define FILESYSTEM_CONFIG_DIR       "/storage/config"
```

---

## Validation and Error Handling

### LED Validation (`components/led_validation/`)

**Primary Interface**: Post-transition LED hardware validation

```c
// Initialization
esp_err_t led_validation_init(void);
esp_err_t tlc_diagnostic_test(void);  // Startup diagnostic

// Validation
validation_result_enhanced_t validate_display_with_hardware(uint8_t current_hour, uint8_t current_minutes);
failure_type_t classify_failure(const validation_result_enhanced_t *result);
uint8_t calculate_validation_health_score(const validation_statistics_t *stats);

// Recovery
bool attempt_recovery(const validation_result_enhanced_t *result, failure_type_t failure);
bool recover_hardware_mismatch(const validation_result_enhanced_t *result);
bool recover_hardware_fault(const validation_result_enhanced_t *result);
bool recover_i2c_bus_failure(void);
bool recover_grppwm_mismatch(const validation_result_enhanced_t *result);

// Configuration
esp_err_t load_validation_config(validation_config_t *config);
esp_err_t save_validation_config(const validation_config_t *config);
void get_default_validation_config(validation_config_t *config);

// Statistics
void update_statistics(validation_statistics_t *stats, failure_type_t failure);
void get_validation_statistics(validation_statistics_t *stats);

// Task Management
esp_err_t start_validation_task(void);
esp_err_t stop_validation_task(void);
esp_err_t trigger_validation_post_transition(void);  // Triggered after LED transitions

// Utilities
const char* get_failure_type_name(failure_type_t failure);
bool should_restart_on_failure(const validation_config_t *config, failure_type_t failure);
```

**Key Data Structures**:
```c
typedef enum {
    FAILURE_NONE,
    FAILURE_HARDWARE_FAULT,          // EFLAG shows open/short
    FAILURE_I2C_BUS_FAILURE,
    FAILURE_SYSTEMATIC_MISMATCH,     // >20% LEDs wrong
    FAILURE_PARTIAL_MISMATCH,        // 1-20% LEDs wrong
    FAILURE_GRPPWM_MISMATCH,
    FAILURE_SOFTWARE_ERROR
} failure_type_t;

typedef struct {
    bool restart_on_hardware_fault;
    bool restart_on_i2c_bus_failure;
    bool restart_on_systematic_mismatch;
    bool restart_on_partial_mismatch;
    bool restart_on_grppwm_mismatch;
    uint8_t max_restarts_per_session;
    uint16_t validation_interval_sec;
    bool validation_enabled;
} validation_config_t;
```

**Critical Implementation Notes**:
- **Validation timing**: Triggered immediately after LED transitions complete (~every 5 minutes when display changes)
- **Execution time**: ~130ms (16 reads × 10 TLC59116 devices)
- **TLC59116 fix**: Uses explicit no-auto-increment addressing (0x1F mask) to prevent register pointer corruption
- **Manual recovery**: Auto-recovery disabled, requires user approval via MQTT

### Error Log Manager (`components/error_log_manager/`)

**Primary Interface**: Persistent error logging with NVS storage

```c
// Initialization
esp_err_t error_log_init(void);

// Logging
esp_err_t error_log_write(const error_log_entry_t *entry);

// Reading
esp_err_t error_log_read_recent(error_log_entry_t *entries, uint16_t max_count, uint16_t *actual_count);
esp_err_t error_log_read_entry(uint16_t index, error_log_entry_t *entry);

// Statistics
esp_err_t error_log_get_stats(error_log_stats_t *stats);

// Maintenance
esp_err_t error_log_clear(void);
esp_err_t error_log_reset_stats(void);

// Utilities
const char* error_log_get_source_name(error_source_t source);
const char* error_log_get_severity_name(error_severity_t severity);
```

**Key Data Structures**:
```c
typedef enum {
    ERROR_SOURCE_UNKNOWN,
    ERROR_SOURCE_LED_VALIDATION,
    ERROR_SOURCE_I2C_BUS,
    ERROR_SOURCE_WIFI,
    ERROR_SOURCE_MQTT,
    ERROR_SOURCE_NTP,
    ERROR_SOURCE_SYSTEM,
    ERROR_SOURCE_POWER,
    ERROR_SOURCE_SENSOR,
    ERROR_SOURCE_DISPLAY,
    ERROR_SOURCE_TRANSITION,
    ERROR_SOURCE_BRIGHTNESS
} error_source_t;

typedef enum {
    ERROR_SEVERITY_INFO,
    ERROR_SEVERITY_WARNING,
    ERROR_SEVERITY_ERROR,
    ERROR_SEVERITY_CRITICAL
} error_severity_t;

typedef struct {
    time_t timestamp;
    uint32_t uptime_sec;
    uint8_t error_source;
    uint8_t error_severity;
    uint16_t error_code;
    uint8_t context[32];       // Error-specific context
    char message[64];          // Human-readable message
} error_log_entry_t;  // 112 bytes per entry
```

**Helper Macros**:
```c
// Simple logging
ERROR_LOG(source, severity, code, msg)

// With custom context
ERROR_LOG_CONTEXT(source, severity, code, msg, ctx_ptr, ctx_size)
```

**Storage Details**:
- 50-entry circular buffer
- ~5.6KB total storage (50 × 112 bytes)
- Persists across reboots in NVS
- Detailed statistics tracking

---

## Advanced Feature Components

### Transition Manager (`components/transition_manager/`)

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

### Transition Config (`components/transition_config/`)

**Primary Interface**: Transition animation configuration

```c
// Initialization
esp_err_t transition_config_init(void);

// Storage
esp_err_t transition_config_save_to_nvs(void);
esp_err_t transition_config_load_from_nvs(void);
esp_err_t transition_config_reset_to_defaults(void);

// Setters/Getters
esp_err_t transition_config_set_duration(uint16_t duration_ms);
esp_err_t transition_config_set_enabled(bool enabled);
esp_err_t transition_config_set_curves(transition_curve_type_t fadein, transition_curve_type_t fadeout);

uint16_t transition_config_get_duration(void);
bool transition_config_get_enabled(void);
transition_curve_type_t transition_config_get_fadein_curve(void);
transition_curve_type_t transition_config_get_fadeout_curve(void);

// Utilities
const char* transition_curve_type_to_string(transition_curve_type_t type);
transition_curve_type_t transition_curve_type_from_string(const char* str);
```

**Key Data Structures**:
```c
typedef enum {
    TRANSITION_CURVE_LINEAR,
    TRANSITION_CURVE_EASE_IN,
    TRANSITION_CURVE_EASE_OUT,
    TRANSITION_CURVE_EASE_IN_OUT,
    TRANSITION_CURVE_BOUNCE,
    TRANSITION_CURVE_MAX
} transition_curve_type_t;

typedef struct {
    uint16_t duration_ms;              // 200-5000ms
    bool enabled;
    transition_curve_type_t fadein_curve;
    transition_curve_type_t fadeout_curve;
    bool is_initialized;
} transition_config_t;
```

**Defaults**:
```c
#define DEFAULT_TRANSITION_DURATION     1500
#define DEFAULT_TRANSITION_ENABLED      true
#define DEFAULT_FADEIN_CURVE            TRANSITION_CURVE_EASE_IN
#define DEFAULT_FADEOUT_CURVE           TRANSITION_CURVE_EASE_OUT
```

### Brightness Config (`components/brightness_config/`)

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

---

## Common Patterns

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

### Thread-Safe Access Pattern

```c
// Never access globals directly
extern uint8_t global_brightness;  // ❌ Don't use directly

// Always use thread-safe accessors
uint8_t value = thread_safe_get_global_brightness();  // ✅ Correct
thread_safe_set_global_brightness(180);               // ✅ Correct
```

---

## Component Dependencies

```
wordclock (main)
├── board_config (no dependencies)
├── i2c_devices → board_config
├── audio_manager → board_config
├── sdcard_manager → board_config
├── chime_manager → audio_manager, sdcard_manager
├── led_validation → error_log_manager, i2c_devices
├── error_log_manager → nvs_manager
├── ota_manager → nvs_manager, wifi_manager, mbedtls (SHA-256)
├── web_server → wifi_manager, nvs_manager
├── mqtt_manager → wifi_manager
├── mqtt_discovery → mqtt_manager
└── external_flash, filesystem_manager (OPTIONAL)
```

---

## Quick Reference

### Initialization Order

1. **Hardware Foundation**
   - `board_config` (implicit - compile-time)
   - `nvs_manager_init()`
   - `i2c_devices_init()`

2. **Display System**
   - `wordclock_display_init()`
   - `transition_manager_init()`
   - `brightness_config_init()`

3. **Optional Storage** (if W25Q64 present)
   - `external_flash_init()`
   - `filesystem_manager_init()`

4. **Audio System**
   - `audio_manager_init()`
   - `sdcard_manager_init()`
   - `chime_manager_init()`

5. **Networking**
   - `wifi_manager_init()`
   - `ntp_manager_init()`
   - `mqtt_manager_init()`

6. **System Services**
   - `ota_manager_init()`
   - `led_validation_init()`
   - `error_log_init()`

### Critical Constants

```c
// LED Matrix
#define LED_ROWS                    10
#define LED_COLS                    16
#define TLC59116_DEVICE_COUNT       10
#define TLC59116_CHANNELS           16

// Brightness
#define BRIGHTNESS_MIN              5
#define BRIGHTNESS_MAX              80   // Safety limit
#define BRIGHTNESS_DEFAULT          60

// I2C
#define I2C_MASTER_FREQ_HZ          100000  // 100 kHz
#define I2C_MASTER_TIMEOUT_MS       1000

// Audio
#define AUDIO_SAMPLE_RATE           16000   // 16 kHz
#define AUDIO_BITS_PER_SAMPLE       16

// OTA
#define OTA_DEFAULT_SOURCE          OTA_SOURCE_GITHUB
#define OTA_DEFAULT_TIMEOUT_MS      120000  // 2 minutes
```

---

## See Also

- [Post-Build Automation Guide](post-build-automation-guide.md) - OTA firmware preparation with SHA-256
- [Dual OTA Source System](../technical/dual-ota-source-system.md) - Complete OTA architecture
- [MQTT User Guide](../../Mqtt_User_Guide.md) - MQTT commands and topics
- [CLAUDE.md](../../CLAUDE.md) - Developer reference and project overview

---

📖 **Scope**: Complete API reference for all 28 components
🎯 **Audience**: Developers integrating with or extending the system
📝 **Maintenance**: Keep synchronized with component headers
⭐ **Last Major Update**: SHA-256 firmware verification (2025-11-12)
