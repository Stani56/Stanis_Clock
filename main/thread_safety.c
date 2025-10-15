#include "thread_safety.h"
#include "esp_log.h"

static const char *TAG = "thread_safety";

// Global mutexes
SemaphoreHandle_t g_network_status_mutex = NULL;
SemaphoreHandle_t g_brightness_mutex = NULL;
SemaphoreHandle_t g_led_state_mutex = NULL;
SemaphoreHandle_t g_transition_mutex = NULL;
SemaphoreHandle_t g_i2c_device_mutex = NULL;

// Thread-safe network status flags
volatile bool wifi_connected_safe = false;
volatile bool ntp_synced_safe = false;
volatile bool mqtt_connected_safe = false;

// Thread-safe brightness variables
volatile uint8_t global_brightness_safe = 120;
volatile uint8_t potentiometer_individual_safe = 32;

// Thread-safe transition test variables
volatile bool transition_test_mode_safe = false;
volatile bool transition_test_running_safe = false;

// Spinlocks for critical sections
static portMUX_TYPE network_status_spinlock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t thread_safety_init(void)
{
    ESP_LOGI(TAG, "Initializing thread safety mutexes");
    
    // Create mutexes
    g_network_status_mutex = xSemaphoreCreateMutex();
    if (g_network_status_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create network status mutex");
        return ESP_ERR_NO_MEM;
    }
    
    g_brightness_mutex = xSemaphoreCreateMutex();
    if (g_brightness_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create brightness mutex");
        thread_safety_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    g_led_state_mutex = xSemaphoreCreateMutex();
    if (g_led_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED state mutex");
        thread_safety_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    g_transition_mutex = xSemaphoreCreateMutex();
    if (g_transition_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create transition mutex");
        thread_safety_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    g_i2c_device_mutex = xSemaphoreCreateMutex();
    if (g_i2c_device_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C device mutex");
        thread_safety_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Thread safety initialization complete");
    return ESP_OK;
}

void thread_safety_deinit(void)
{
    if (g_network_status_mutex != NULL) {
        vSemaphoreDelete(g_network_status_mutex);
        g_network_status_mutex = NULL;
    }
    
    if (g_brightness_mutex != NULL) {
        vSemaphoreDelete(g_brightness_mutex);
        g_brightness_mutex = NULL;
    }
    
    if (g_led_state_mutex != NULL) {
        vSemaphoreDelete(g_led_state_mutex);
        g_led_state_mutex = NULL;
    }
    
    if (g_transition_mutex != NULL) {
        vSemaphoreDelete(g_transition_mutex);
        g_transition_mutex = NULL;
    }
    
    if (g_i2c_device_mutex != NULL) {
        vSemaphoreDelete(g_i2c_device_mutex);
        g_i2c_device_mutex = NULL;
    }
}

// Network status thread-safe accessors
bool thread_safe_get_wifi_connected(void)
{
    bool result = false;
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    result = wifi_connected_safe;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    return result;
}

void thread_safe_set_wifi_connected(bool connected)
{
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    wifi_connected_safe = connected;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    THREAD_SAFETY_LOG("WiFi connected: %s", connected ? "true" : "false");
}

bool thread_safe_get_ntp_synced(void)
{
    bool result = false;
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    result = ntp_synced_safe;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    return result;
}

void thread_safe_set_ntp_synced(bool synced)
{
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    ntp_synced_safe = synced;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    THREAD_SAFETY_LOG("NTP synced: %s", synced ? "true" : "false");
}

bool thread_safe_get_mqtt_connected(void)
{
    bool result = false;
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    result = mqtt_connected_safe;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    return result;
}

void thread_safe_set_mqtt_connected(bool connected)
{
    portENTER_CRITICAL_SAFE(&network_status_spinlock);
    mqtt_connected_safe = connected;
    portEXIT_CRITICAL_SAFE(&network_status_spinlock);
    THREAD_SAFETY_LOG("MQTT connected: %s", connected ? "true" : "false");
}

// Brightness thread-safe accessors
uint8_t thread_safe_get_global_brightness(void)
{
    uint8_t brightness = 120;
    if (g_brightness_mutex != NULL) {
        if (xSemaphoreTake(g_brightness_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            brightness = global_brightness_safe;
            xSemaphoreGive(g_brightness_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire brightness mutex for read");
        }
    }
    return brightness;
}

esp_err_t thread_safe_set_global_brightness(uint8_t brightness)
{
    if (g_brightness_mutex != NULL) {
        if (xSemaphoreTake(g_brightness_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            global_brightness_safe = brightness;
            xSemaphoreGive(g_brightness_mutex);
            THREAD_SAFETY_LOG("Global brightness set to %d", brightness);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to acquire brightness mutex for write");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

uint8_t thread_safe_get_potentiometer_brightness(void)
{
    uint8_t brightness = 32;
    if (g_brightness_mutex != NULL) {
        if (xSemaphoreTake(g_brightness_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            brightness = potentiometer_individual_safe;
            xSemaphoreGive(g_brightness_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire brightness mutex for read");
        }
    }
    return brightness;
}

esp_err_t thread_safe_set_potentiometer_brightness(uint8_t brightness)
{
    if (g_brightness_mutex != NULL) {
        if (xSemaphoreTake(g_brightness_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            potentiometer_individual_safe = brightness;
            xSemaphoreGive(g_brightness_mutex);
            THREAD_SAFETY_LOG("Potentiometer brightness set to %d", brightness);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to acquire brightness mutex for write");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

// Time expression style thread-safe accessors (uses brightness mutex)
bool thread_safe_get_halb_centric_style(void)
{
    // Calls brightness_config directly (already thread-safe internally)
    extern bool brightness_config_get_halb_centric_style(void);
    return brightness_config_get_halb_centric_style();
}

esp_err_t thread_safe_set_halb_centric_style(bool enabled)
{
    // Calls brightness_config directly (already thread-safe internally)
    extern esp_err_t brightness_config_set_halb_centric_style(bool enabled);
    return brightness_config_set_halb_centric_style(enabled);
}

// LED state synchronization helpers
esp_err_t thread_safe_led_state_lock(TickType_t timeout)
{
    if (g_led_state_mutex != NULL) {
        if (xSemaphoreTake(g_led_state_mutex, timeout) == pdTRUE) {
            THREAD_SAFETY_LOG("LED state mutex acquired");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to acquire LED state mutex");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

void thread_safe_led_state_unlock(void)
{
    if (g_led_state_mutex != NULL) {
        xSemaphoreGive(g_led_state_mutex);
        THREAD_SAFETY_LOG("LED state mutex released");
    }
}

// Transition system synchronization helpers
esp_err_t thread_safe_transition_lock(TickType_t timeout)
{
    if (g_transition_mutex != NULL) {
        if (xSemaphoreTake(g_transition_mutex, timeout) == pdTRUE) {
            THREAD_SAFETY_LOG("Transition mutex acquired");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to acquire transition mutex");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

void thread_safe_transition_unlock(void)
{
    if (g_transition_mutex != NULL) {
        xSemaphoreGive(g_transition_mutex);
        THREAD_SAFETY_LOG("Transition mutex released");
    }
}

// I2C device synchronization helpers
esp_err_t thread_safe_i2c_lock(TickType_t timeout)
{
    if (g_i2c_device_mutex != NULL) {
        if (xSemaphoreTake(g_i2c_device_mutex, timeout) == pdTRUE) {
            THREAD_SAFETY_LOG("I2C device mutex acquired");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to acquire I2C device mutex");
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

void thread_safe_i2c_unlock(void)
{
    if (g_i2c_device_mutex != NULL) {
        xSemaphoreGive(g_i2c_device_mutex);
        THREAD_SAFETY_LOG("I2C device mutex released");
    }
}

// Transition test thread-safe accessors
bool thread_safe_get_transition_test_mode(void)
{
    bool result = false;
    if (g_transition_mutex != NULL) {
        if (xSemaphoreTake(g_transition_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            result = transition_test_mode_safe;
            xSemaphoreGive(g_transition_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire transition mutex for test mode read");
        }
    }
    return result;
}

void thread_safe_set_transition_test_mode(bool enabled)
{
    if (g_transition_mutex != NULL) {
        if (xSemaphoreTake(g_transition_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            transition_test_mode_safe = enabled;
            xSemaphoreGive(g_transition_mutex);
            THREAD_SAFETY_LOG("Transition test mode set to %s", enabled ? "true" : "false");
        } else {
            ESP_LOGE(TAG, "Failed to acquire transition mutex for test mode write");
        }
    }
}

bool thread_safe_get_transition_test_running(void)
{
    bool result = false;
    if (g_transition_mutex != NULL) {
        if (xSemaphoreTake(g_transition_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            result = transition_test_running_safe;
            xSemaphoreGive(g_transition_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire transition mutex for test running read");
        }
    }
    return result;
}

void thread_safe_set_transition_test_running(bool running)
{
    if (g_transition_mutex != NULL) {
        if (xSemaphoreTake(g_transition_mutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            transition_test_running_safe = running;
            xSemaphoreGive(g_transition_mutex);
            THREAD_SAFETY_LOG("Transition test running set to %s", running ? "true" : "false");
        } else {
            ESP_LOGE(TAG, "Failed to acquire transition mutex for test running write");
        }
    }
}