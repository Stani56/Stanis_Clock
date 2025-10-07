// status_led_manager.c - WiFi and NTP Status LEDs
#include "status_led_manager.h"
#include "../../main/thread_safety.h"  // Thread-safe network status flags

static const char *TAG = "STATUS_LED";

// LED state tracking
static bool wifi_led_state = false;
static bool ntp_led_state = false;
static bool wifi_led_blink = false;
static bool ntp_led_blink = false;

// Task handle for LED blinking
static TaskHandle_t status_led_task_handle = NULL;

// External global status variables (defined in wifi_manager)
// These are declared as extern in the header file

esp_err_t status_led_init(void) {
    ESP_LOGI(TAG, "Initializing status LEDs on GPIO21 (WiFi) and GPIO22 (NTP)");
    
    // Configure GPIO21 for WiFi status LED
    gpio_config_t wifi_led_config = {
        .pin_bit_mask = (1ULL << WIFI_STATUS_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&wifi_led_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure WiFi status LED GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure GPIO22 for NTP status LED
    gpio_config_t ntp_led_config = {
        .pin_bit_mask = (1ULL << NTP_STATUS_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&ntp_led_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure NTP status LED GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize LEDs to OFF state
    gpio_set_level(WIFI_STATUS_LED_PIN, 0);
    gpio_set_level(NTP_STATUS_LED_PIN, 0);
    wifi_led_state = false;
    ntp_led_state = false;
    wifi_led_blink = false;
    ntp_led_blink = false;
    
    ESP_LOGI(TAG, "Status LEDs initialized successfully");
    return ESP_OK;
}

void status_led_set_wifi_status(wifi_status_t status) {
    switch (status) {
        case WIFI_STATUS_DISCONNECTED:
            wifi_led_blink = false;
            wifi_led_state = false;
            gpio_set_level(WIFI_STATUS_LED_PIN, 0);
            ESP_LOGI(TAG, "WiFi LED: OFF (disconnected)");
            break;
            
        case WIFI_STATUS_CONNECTING:
            wifi_led_blink = true;
            wifi_led_state = false;  // Will be controlled by blink task
            ESP_LOGI(TAG, "WiFi LED: BLINKING (connecting)");
            break;
            
        case WIFI_STATUS_CONNECTED:
            wifi_led_blink = false;
            wifi_led_state = true;
            gpio_set_level(WIFI_STATUS_LED_PIN, 1);
            ESP_LOGI(TAG, "WiFi LED: ON (connected)");
            break;
    }
}

void status_led_set_ntp_status(ntp_status_t status) {
    switch (status) {
        case NTP_STATUS_NOT_SYNCED:
            ntp_led_blink = false;
            ntp_led_state = false;
            gpio_set_level(NTP_STATUS_LED_PIN, 0);
            ESP_LOGI(TAG, "NTP LED: OFF (not synced)");
            break;
            
        case NTP_STATUS_SYNCING:
            ntp_led_blink = true;
            ntp_led_state = false;  // Will be controlled by blink task
            ESP_LOGI(TAG, "NTP LED: BLINKING (syncing)");
            break;
            
        case NTP_STATUS_SYNCED:
            ntp_led_blink = false;
            ntp_led_state = true;
            gpio_set_level(NTP_STATUS_LED_PIN, 1);
            ESP_LOGI(TAG, "NTP LED: ON (synced)");
            break;
    }
}

void status_led_update_from_globals(void) {
    // Update WiFi LED based on thread-safe wifi_connected state
    if (thread_safe_get_wifi_connected()) {
        status_led_set_wifi_status(WIFI_STATUS_CONNECTED);
    } else {
        status_led_set_wifi_status(WIFI_STATUS_DISCONNECTED);
    }
    
    // Update NTP LED based on thread-safe ntp_synced state
    if (thread_safe_get_ntp_synced()) {
        status_led_set_ntp_status(NTP_STATUS_SYNCED);
    } else {
        status_led_set_ntp_status(NTP_STATUS_NOT_SYNCED);
    }
}

void status_led_task(void *pvParameter) {
    ESP_LOGI(TAG, "Status LED blink task started");
    
    bool wifi_blink_state = false;
    bool ntp_blink_state = false;
    
    while (1) {
        // Handle WiFi LED blinking
        if (wifi_led_blink) {
            wifi_blink_state = !wifi_blink_state;
            gpio_set_level(WIFI_STATUS_LED_PIN, wifi_blink_state ? 1 : 0);
        }
        
        // Handle NTP LED blinking
        if (ntp_led_blink) {
            ntp_blink_state = !ntp_blink_state;
            gpio_set_level(NTP_STATUS_LED_PIN, ntp_blink_state ? 1 : 0);
        }
        
        // Update LEDs based on global states every few blinks
        static int update_counter = 0;
        if (++update_counter >= 10) {  // Update every 5 seconds
            status_led_update_from_globals();
            update_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));  // Blink every 500ms
    }
}

esp_err_t status_led_start_task(void) {
    if (status_led_task_handle != NULL) {
        ESP_LOGW(TAG, "Status LED task already running");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(
        status_led_task,
        "status_led_task",
        4096,
        NULL,
        4,  // Priority 4 (lower than critical tasks)
        &status_led_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status LED task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Status LED task created successfully");
    return ESP_OK;
}

void status_led_stop_task(void) {
    if (status_led_task_handle != NULL) {
        vTaskDelete(status_led_task_handle);
        status_led_task_handle = NULL;
        ESP_LOGI(TAG, "Status LED task stopped");
    }
}

void status_led_test_sequence(void) {
    ESP_LOGI(TAG, "Running LED test sequence...");
    
    // Test WiFi LED
    ESP_LOGI(TAG, "Testing WiFi LED (GPIO21)...");
    for (int i = 0; i < 3; i++) {
        gpio_set_level(WIFI_STATUS_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(WIFI_STATUS_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Test NTP LED
    ESP_LOGI(TAG, "Testing NTP LED (GPIO22)...");
    for (int i = 0; i < 3; i++) {
        gpio_set_level(NTP_STATUS_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(NTP_STATUS_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Test both LEDs together
    ESP_LOGI(TAG, "Testing both LEDs together...");
    for (int i = 0; i < 2; i++) {
        gpio_set_level(WIFI_STATUS_LED_PIN, 1);
        gpio_set_level(NTP_STATUS_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(WIFI_STATUS_LED_PIN, 0);
        gpio_set_level(NTP_STATUS_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    ESP_LOGI(TAG, "LED test sequence completed");
    
    // Restore current states
    status_led_update_from_globals();
}