// ntp_manager.c - ESP-IDF 5.3.1 Compatible NTP Time Synchronization
#include "ntp_manager.h"
#include "../../main/thread_safety.h"  // Thread-safe network status flags

static const char *TAG = "NTP_MANAGER";

// NTP sync state
static bool ntp_initialized = false;
static bool sntp_configured = false;
static time_t last_ntp_sync_time = 0;  // Track last successful sync timestamp

esp_err_t ntp_manager_init(void) {
    ESP_LOGI(TAG, "=== NTP MANAGER EARLY INIT ===");
    
    if (ntp_initialized) {
        ESP_LOGI(TAG, "✅ NTP manager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🕐 Early NTP manager initialization (timezone only)");
    
    // Set timezone to Vienna (CET/CEST) - this is safe to do early
    ESP_LOGI(TAG, "🕐 Setting timezone...");
    setenv("TZ", NTP_TIMEZONE, 1);
    tzset();
    ESP_LOGI(TAG, "✅ Timezone set to Vienna: %s", NTP_TIMEZONE);
    
    // Mark as initialized (but SNTP not configured yet)
    ntp_initialized = true;
    ESP_LOGI(TAG, "✅ NTP MANAGER EARLY INIT COMPLETE (SNTP will be configured when WiFi connects)");
    return ESP_OK;
}

esp_err_t ntp_start_sync(void) {
    ESP_LOGI(TAG, "=== NTP START SYNC CALLED ===");
    ESP_LOGI(TAG, "NTP initialized: %s", ntp_initialized ? "YES" : "NO");
    ESP_LOGI(TAG, "SNTP configured: %s", sntp_configured ? "YES" : "NO");
    ESP_LOGI(TAG, "WiFi connected: %s", thread_safe_get_wifi_connected() ? "YES" : "NO");
    ESP_LOGI(TAG, "NTP synced: %s", thread_safe_get_ntp_synced() ? "YES" : "NO");
    
    if (!ntp_initialized) {
        ESP_LOGE(TAG, "❌ NTP manager not initialized - cannot start sync");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!thread_safe_get_wifi_connected()) {
        ESP_LOGW(TAG, "❌ Cannot start NTP sync - WiFi not connected");
        return ESP_FAIL;
    }
    
    // Configure SNTP now that WiFi/TCP stack is ready
    if (!sntp_configured) {
        ESP_LOGI(TAG, "🕐 Configuring SNTP (TCP/IP stack now ready)...");
        
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, NTP_SERVER_PRIMARY);
        esp_sntp_setservername(1, NTP_SERVER_SECONDARY);
        esp_sntp_set_time_sync_notification_cb(ntp_time_sync_notification_cb);
        
        // CRITICAL FIX: Enable periodic sync - without this, NTP only syncs once!
        esp_sntp_set_sync_interval(3600000);  // 1 hour (3600000 ms) - production setting
        
        ESP_LOGI(TAG, "✅ SNTP configured with servers: %s, %s", NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
        ESP_LOGI(TAG, "✅ SNTP sync interval set to 1 hour (3600000 ms)");
        sntp_configured = true;
    }
    
    ESP_LOGI(TAG, "✅ Starting SNTP synchronization...");
    
    // Start SNTP service
    esp_sntp_init();
    
    ESP_LOGI(TAG, "✅ SNTP service started, waiting for synchronization...");
    return ESP_OK;
}

void ntp_time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "NTP time synchronization complete!");
    
    if (tv != NULL) {
        thread_safe_set_ntp_synced(true);
        
        // Get current time and display it
        time_t now = tv->tv_sec;
        last_ntp_sync_time = now;  // Store last sync timestamp
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        // Sync to DS3231 RTC
        esp_err_t ret = ntp_sync_to_rtc();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Time synchronized to DS3231 RTC successfully");
        } else {
            ESP_LOGW(TAG, "Failed to sync to DS3231 RTC: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "NTP sync callback with null timeval");
    }
}

bool ntp_is_synced(void) {
    return thread_safe_get_ntp_synced();
}

time_t ntp_get_last_sync_time(void) {
    return last_ntp_sync_time;
}

esp_err_t ntp_format_last_sync_iso8601(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size < 24) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (last_ntp_sync_time == 0) {
        strncpy(buffer, "", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return ESP_ERR_INVALID_STATE;
    }
    
    struct tm timeinfo;
    gmtime_r(&last_ntp_sync_time, &timeinfo);
    
    // Format as ISO 8601 UTC: YYYY-MM-DDTHH:MM:SS+00:00
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    
    return ESP_OK;
}

esp_err_t ntp_sync_to_rtc(void) {
    if (!thread_safe_get_ntp_synced()) {
        ESP_LOGW(TAG, "NTP not synced yet");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get current system time
    time_t now = 0;
    struct tm timeinfo = {0};
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Validate time is reasonable (after 2024)
    if (timeinfo.tm_year < (2024 - 1900)) {
        ESP_LOGW(TAG, "Time not set correctly (year %d)", timeinfo.tm_year + 1900);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Syncing NTP time to DS3231: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Convert tm structure to wordclock_time_t format
    wordclock_time_t wc_time;
    wc_time.year = (timeinfo.tm_year + 1900) % 100;  // Last 2 digits (e.g., 2024 -> 24)
    wc_time.month = timeinfo.tm_mon + 1;             // tm_mon is 0-11, we need 1-12
    wc_time.day = timeinfo.tm_mday;                  // Day of month (1-31)
    wc_time.hours = timeinfo.tm_hour;                // Hours (0-23)
    wc_time.minutes = timeinfo.tm_min;               // Minutes (0-59)
    wc_time.seconds = timeinfo.tm_sec;               // Seconds (0-59)
    
    ESP_LOGI(TAG, "DS3231 format: %02d/%02d/%02d %02d:%02d:%02d",
             wc_time.day, wc_time.month, wc_time.year,
             wc_time.hours, wc_time.minutes, wc_time.seconds);
    
    // Write to DS3231 RTC using the correct function name
    esp_err_t ret = ds3231_set_time_struct(&wc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set DS3231 time: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Successfully synchronized NTP time to DS3231 RTC");
    return ESP_OK;
}