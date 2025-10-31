// wifi_manager.c - ESP-IDF 5.3.1 Compatible Implementation
#include "wifi_manager.h"
#include "nvs_manager.h"
#include "status_led_manager.h"
#include "ntp_manager.h"
#include "mqtt_manager.h"
#include "../../main/thread_safety.h"  // Thread safety for network status flags

static const char *TAG = "WIFI_MANAGER";

// Global Variables (matching your working implementation)
EventGroupHandle_t s_wifi_event_group = NULL;
int s_retry_num = 0;
bool wifi_connected = false;
bool ntp_synced = false;
bool mqtt_connected = false;
TaskHandle_t wifi_monitor_task_handle = NULL;
TaskHandle_t ntp_sync_task_handle = NULL;

void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, attempting connection...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected - Reason: %d (%s)", 
                 event->reason, 
                 (event->reason == WIFI_REASON_AUTH_FAIL) ? "Auth Failed" :
                 (event->reason == WIFI_REASON_NO_AP_FOUND) ? "AP Not Found" :
                 (event->reason == WIFI_REASON_ASSOC_FAIL) ? "Association Failed" :
                 (event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT) ? "Handshake Timeout" :
                 "Other");
        
        thread_safe_set_wifi_connected(false);
        status_led_set_wifi_status(WIFI_STATUS_DISCONNECTED);
        
        // CRITICAL FIX: Reset NTP sync flag when WiFi disconnects
        if (thread_safe_get_ntp_synced()) {
            ESP_LOGI(TAG, "WiFi disconnected - resetting NTP sync flag");
            thread_safe_set_ntp_synced(false);
            status_led_set_ntp_status(NTP_STATUS_NOT_SYNCED);
        }
        
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            ESP_LOGI(TAG, "Retry %d/%d to connect to the AP", s_retry_num + 1, ESP_MAXIMUM_RETRY);
            status_led_set_wifi_status(WIFI_STATUS_CONNECTING);
            esp_wifi_connect();
            s_retry_num++;
        } else {
            ESP_LOGE(TAG, "Failed to connect after %d attempts", ESP_MAXIMUM_RETRY);
            status_led_set_wifi_status(WIFI_STATUS_DISCONNECTED);
            // CRITICAL FIX: Check if event group exists before setting bits
            if (s_wifi_event_group != NULL) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            } else {
                ESP_LOGE(TAG, "Event group is NULL, cannot set WIFI_FAIL_BIT");
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        thread_safe_set_wifi_connected(true);
        status_led_set_wifi_status(WIFI_STATUS_CONNECTED);

        // Set WiFi to minimum modem power save (DTIM sleep already disabled via listen_interval=0)
        ESP_LOGI(TAG, "Setting WiFi power save to MIN_MODEM mode (DTIM sleep disabled)");
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        // Log the reconnection and NTP status
        ESP_LOGI(TAG, "WiFi connected successfully - NTP sync status: %s",
                 thread_safe_get_ntp_synced() ? "Synced" : "Not Synced");
        
        // Start NTP synchronization immediately when WiFi connects
        if (!thread_safe_get_ntp_synced()) {
            ESP_LOGI(TAG, "🕐 WiFi connected - triggering NTP synchronization");
            status_led_set_ntp_status(NTP_STATUS_SYNCING);
            ESP_LOGI(TAG, "🕐 Calling ntp_start_sync()...");
            esp_err_t ntp_ret = ntp_start_sync();
            if (ntp_ret != ESP_OK) {
                ESP_LOGW(TAG, "❌ Failed to start NTP sync: %s", esp_err_to_name(ntp_ret));
            } else {
                ESP_LOGI(TAG, "✅ NTP sync started successfully");
            }
        }
        
        // Start MQTT client automatically when WiFi connects
        if (!thread_safe_get_mqtt_connected()) {
            ESP_LOGI(TAG, "🔐 WiFi connected - starting MQTT client");
            // Small delay to let WiFi settle
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_err_t mqtt_ret = mqtt_manager_start();
            if (mqtt_ret != ESP_OK) {
                ESP_LOGW(TAG, "❌ Failed to start MQTT: %s", esp_err_to_name(mqtt_ret));
            } else {
                ESP_LOGI(TAG, "✅ MQTT client started from WiFi event");
            }
        }
        
        // CRITICAL FIX: Check if event group exists before setting bits
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        } else {
            ESP_LOGE(TAG, "Event group is NULL, cannot set WIFI_CONNECTED_BIT");
        }
    }
}

void wifi_manager_init_sta(const char* ssid, const char* password) {
    // CRITICAL FIX: Ensure event group is created first
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create WiFi event group");
            return;
        }
        ESP_LOGI(TAG, "WiFi event group created successfully");
    }
    
    // CRITICAL FIX: Check if already initialized to prevent ESP_ERR_INVALID_STATE
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init netif: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return;
    }
    
    // Only create if not already created
    if (esp_netif_get_default_netif() == NULL) {
        esp_netif_create_default_wifi_sta();
    }
    
    // CRITICAL FIX: Check if WiFi already initialized
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init WiFi: %s", esp_err_to_name(ret));
        return;
    }
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler, NULL, &instance_got_ip));
    
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.listen_interval = 0;  // Disable DTIM sleep (prevents WiFi sleep during I2S/audio)
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi init finished.");
    
    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, 
                                          pdFALSE, 
                                          pdFALSE, 
                                          portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", ssid);
    }
}

void wifi_manager_init_ap(void) {
    // Handle case where netif/event loop might already exist from pre-scan
    esp_err_t ret;
    
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32-LED-Config",
            .ssid_len = strlen("ESP32-LED-Config"),
            .channel = 1,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started. SSID: ESP32-LED-Config, Password: 12345678");
    ESP_LOGI(TAG, "Connect to WiFi and go to http://192.168.4.1 to configure");
}

// Improved WiFi monitoring with better reconnection logic
void wifi_manager_monitor_task(void *pvParameter) {
    bool was_connected = false;
    
    while (1) {
        if (!thread_safe_get_wifi_connected()) {
            if (was_connected) {
                ESP_LOGW(TAG, "WiFi connection lost - triggering cleanup");
                was_connected = false;
                status_led_set_wifi_status(WIFI_STATUS_DISCONNECTED);
                
                // Reset NTP sync flag when connection is lost
                if (thread_safe_get_ntp_synced()) {
                    ESP_LOGI(TAG, "Resetting NTP sync flag due to WiFi loss");
                    thread_safe_set_ntp_synced(false);
                    status_led_set_ntp_status(NTP_STATUS_NOT_SYNCED);
                }
            }
            
            // Check if WiFi is actually disconnected by trying to get WiFi status
            wifi_ap_record_t ap_info;
            esp_err_t ap_ret = esp_wifi_sta_get_ap_info(&ap_info);
            if (ap_ret == ESP_OK) {
                ESP_LOGI(TAG, "WiFi appears connected to %s (RSSI: %d), skipping reconnect", 
                         ap_info.ssid, ap_info.rssi);
                // WiFi is actually connected, update our state
                thread_safe_set_wifi_connected(true);
            } else {
                ESP_LOGW(TAG, "WiFi disconnected (ap_info failed: %s), attempting reconnect...", 
                         esp_err_to_name(ap_ret));
                status_led_set_wifi_status(WIFI_STATUS_CONNECTING);
                esp_err_t ret = esp_wifi_connect();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "WiFi reconnect failed: %s", esp_err_to_name(ret));
                    status_led_set_wifi_status(WIFI_STATUS_DISCONNECTED);
                }
                
                vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds before retry
            }
        } else {
            if (!was_connected) {
                ESP_LOGI(TAG, "WiFi connection established in monitor");
                was_connected = true;
                status_led_set_wifi_status(WIFI_STATUS_CONNECTED);
                
                // Start NTP sync if not synced
                if (!thread_safe_get_ntp_synced()) {
                    ESP_LOGI(TAG, "🕐 WiFi monitor starting NTP synchronization");
                    status_led_set_ntp_status(NTP_STATUS_SYNCING);
                    ESP_LOGI(TAG, "🕐 Monitor calling ntp_start_sync()...");
                    esp_err_t ntp_ret = ntp_start_sync();
                    if (ntp_ret != ESP_OK) {
                        ESP_LOGW(TAG, "❌ Failed to start NTP sync from monitor: %s", esp_err_to_name(ntp_ret));
                    } else {
                        ESP_LOGI(TAG, "✅ NTP sync started successfully from monitor");
                    }
                }
            }
            
            // WiFi is connected, check if we need NTP sync
            if (!thread_safe_get_ntp_synced()) {
                ESP_LOGI(TAG, "WiFi connected but NTP not synced, will trigger NTP sync");
            }
        }
        
        // Check WiFi status every 30 seconds
        vTaskDelay(pdMS_TO_TICKS(WIFI_MONITOR_INTERVAL_MS));
    }
}

// Initialization function for integration
esp_err_t wifi_manager_init(void) {
    ESP_LOGI(TAG, "WiFi Manager initializing...");
    
    // Initialize global variables using thread-safe setters
    thread_safe_set_wifi_connected(false);
    thread_safe_set_ntp_synced(false);
    s_retry_num = 0;
    
    ESP_LOGI(TAG, "WiFi Manager initialized successfully");
    return ESP_OK;
}

// Try to connect with stored credentials, fallback to AP mode
esp_err_t wifi_manager_start_with_credentials(void) {
    char ssid[64] = {0};
    char password[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);
    
    esp_err_t ret = nvs_manager_load_wifi_credentials(ssid, password, ssid_len, pass_len);
    
    if (ret == ESP_OK && strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Found stored WiFi credentials, attempting connection to: %s", ssid);
        status_led_set_wifi_status(WIFI_STATUS_CONNECTING);
        wifi_manager_init_sta(ssid, password);
        
        // Wait a moment for the connection event to be processed
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (thread_safe_get_wifi_connected()) {
            ESP_LOGI(TAG, "Successfully connected to stored WiFi network");
            
            // Start WiFi monitoring task
            BaseType_t task_ret = xTaskCreate(
                wifi_manager_monitor_task,
                "wifi_monitor",
                4096,
                NULL,
                5,
                &wifi_monitor_task_handle
            );
            
            if (task_ret == pdPASS) {
                ESP_LOGI(TAG, "WiFi monitor task started");
            }
            
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Failed to connect with stored credentials");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(TAG, "No valid stored WiFi credentials found");
        return ESP_ERR_NOT_FOUND;
    }
}

// Start AP mode for credential configuration
esp_err_t wifi_manager_start_ap_mode(void) {
    ESP_LOGI(TAG, "Starting WiFi AP mode for configuration...");
    status_led_set_wifi_status(WIFI_STATUS_CONNECTING);
    wifi_manager_init_ap();
    ESP_LOGI(TAG, "WiFi AP mode started successfully");
    return ESP_OK;
}