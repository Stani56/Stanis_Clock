#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi Configuration
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1
#define ESP_MAXIMUM_RETRY           5
#define WIFI_MONITOR_INTERVAL_MS    30000

// Global WiFi State (for compatibility with existing code and status LEDs)
extern EventGroupHandle_t s_wifi_event_group;
extern int s_retry_num;
extern bool wifi_connected;
extern TaskHandle_t wifi_monitor_task_handle;
extern TaskHandle_t ntp_sync_task_handle;
extern bool ntp_synced;

// Function Prototypes - Matching your working implementation
void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_manager_init_sta(const char* ssid, const char* password);
void wifi_manager_init_ap(void);
void wifi_manager_monitor_task(void *pvParameter);

// Additional functions for integration
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_with_credentials(void);
esp_err_t wifi_manager_start_ap_mode(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H