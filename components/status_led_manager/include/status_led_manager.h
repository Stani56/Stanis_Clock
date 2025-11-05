#ifndef STATUS_LED_MANAGER_H
#define STATUS_LED_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// GPIO Pin Definitions
#define WIFI_STATUS_LED_PIN     GPIO_NUM_21  // ESP32-S3: No change
#define NTP_STATUS_LED_PIN      GPIO_NUM_38  // ESP32-S3: GPIO 38 (no ADC, WiFi safe)

// Status enumerations (simplified for standalone operation)
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,  
    WIFI_STATUS_CONNECTED
} wifi_status_t;

typedef enum {
    NTP_STATUS_NOT_SYNCED = 0,
    NTP_STATUS_SYNCING,
    NTP_STATUS_SYNCED
} ntp_status_t;

// Global status variables (for future network integration)
extern bool wifi_connected;
extern bool ntp_synced;

// Function Prototypes
esp_err_t status_led_init(void);
esp_err_t status_led_start_task(void);
void status_led_stop_task(void);

void status_led_set_wifi_status(wifi_status_t status);
void status_led_set_ntp_status(ntp_status_t status);
void status_led_update_from_globals(void);

void status_led_test_sequence(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_LED_MANAGER_H