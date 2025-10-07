#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// NVS Configuration
#define NVS_WIFI_NAMESPACE          "wifi_config"
#define NVS_SSID_KEY                "ssid"
#define NVS_PASSWORD_KEY            "password"

// Function Prototypes
esp_err_t nvs_manager_init(void);
esp_err_t nvs_manager_save_wifi_credentials(const char* ssid, const char* password);
esp_err_t nvs_manager_load_wifi_credentials(char* ssid, char* password, size_t ssid_len, size_t pass_len);
void nvs_manager_clear_wifi_credentials(void);

#ifdef __cplusplus
}
#endif

#endif // NVS_MANAGER_H