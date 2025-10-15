#ifndef THREAD_SAFETY_H
#define THREAD_SAFETY_H

/**
 * @file thread_safety.h
 * @brief Global thread safety definitions and synchronization strategy
 * 
 * MUTEX HIERARCHY (to prevent deadlocks):
 * 1. Network Status Mutex (highest) - for wifi_connected, ntp_synced, mqtt_connected
 * 2. Brightness Mutex - for global_brightness, potentiometer_individual
 * 3. LED State Mutex - for led_state array
 * 4. Transition Mutex - for transition system state
 * 5. I2C Device Mutex (lowest) - for I2C shared variables
 * 
 * RULES:
 * - Always acquire mutexes in the order listed above
 * - Always use timeouts (1000ms default) to prevent deadlocks
 * - Use critical sections for simple flag updates
 * - Release mutexes in reverse order of acquisition
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default mutex timeout (1 second)
#define MUTEX_TIMEOUT_MS 1000
#define MUTEX_TIMEOUT_TICKS pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)

// Global mutexes (defined in thread_safety.c)
extern SemaphoreHandle_t g_network_status_mutex;
extern SemaphoreHandle_t g_brightness_mutex;
extern SemaphoreHandle_t g_led_state_mutex;
extern SemaphoreHandle_t g_transition_mutex;
extern SemaphoreHandle_t g_i2c_device_mutex;

// Network status flags with volatile qualifier
extern volatile bool wifi_connected_safe;
extern volatile bool ntp_synced_safe;
extern volatile bool mqtt_connected_safe;

// Brightness variables with volatile qualifier  
extern volatile uint8_t global_brightness_safe;
extern volatile uint8_t potentiometer_individual_safe;

// Transition test variables with volatile qualifier
extern volatile bool transition_test_mode_safe;
extern volatile bool transition_test_running_safe;

// Initialization function
esp_err_t thread_safety_init(void);
void thread_safety_deinit(void);

// Network status thread-safe accessors
bool thread_safe_get_wifi_connected(void);
void thread_safe_set_wifi_connected(bool connected);
bool thread_safe_get_ntp_synced(void);
void thread_safe_set_ntp_synced(bool synced);
bool thread_safe_get_mqtt_connected(void);
void thread_safe_set_mqtt_connected(bool connected);

// Brightness thread-safe accessors
uint8_t thread_safe_get_global_brightness(void);
esp_err_t thread_safe_set_global_brightness(uint8_t brightness);
uint8_t thread_safe_get_potentiometer_brightness(void);
esp_err_t thread_safe_set_potentiometer_brightness(uint8_t brightness);

// Time expression style thread-safe accessors (uses brightness mutex)
bool thread_safe_get_halb_centric_style(void);
esp_err_t thread_safe_set_halb_centric_style(bool enabled);

// LED state synchronization helpers
esp_err_t thread_safe_led_state_lock(TickType_t timeout);
void thread_safe_led_state_unlock(void);

// Transition system synchronization helpers
esp_err_t thread_safe_transition_lock(TickType_t timeout);
void thread_safe_transition_unlock(void);

// I2C device synchronization helpers
esp_err_t thread_safe_i2c_lock(TickType_t timeout);
void thread_safe_i2c_unlock(void);

// Transition test synchronization helpers
bool thread_safe_get_transition_test_mode(void);
void thread_safe_set_transition_test_mode(bool enabled);
bool thread_safe_get_transition_test_running(void);
void thread_safe_set_transition_test_running(bool running);

// Critical section macros for simple operations (removed - use direct portENTER/EXIT_CRITICAL_SAFE)

// Debug support (can be enabled via menuconfig or compile flag)
#ifdef CONFIG_THREAD_SAFETY_DEBUG
#define THREAD_SAFETY_LOG(fmt, ...) ESP_LOGI("THREAD_SAFETY", fmt, ##__VA_ARGS__)
#else
#define THREAD_SAFETY_LOG(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // THREAD_SAFETY_H