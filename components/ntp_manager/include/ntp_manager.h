#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_devices.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// NTP Configuration
#define NTP_SERVER_PRIMARY    "pool.ntp.org"
#define NTP_SERVER_SECONDARY  "time.google.com"
#define NTP_TIMEZONE         "CET-1CEST,M3.5.0,M10.5.0/3"  // Vienna timezone
#define NTP_SYNC_TIMEOUT_MS   30000  // 30 seconds

// External global variables (defined in wifi_manager)
extern bool wifi_connected;
extern bool ntp_synced;

// Function prototypes
esp_err_t ntp_manager_init(void);
esp_err_t ntp_start_sync(void);
bool ntp_is_synced(void);
time_t ntp_get_last_sync_time(void);
esp_err_t ntp_format_last_sync_iso8601(char *buffer, size_t buffer_size);
esp_err_t ntp_sync_to_rtc(void);
void ntp_time_sync_notification_cb(struct timeval *tv);

#ifdef __cplusplus
}
#endif

#endif // NTP_MANAGER_H