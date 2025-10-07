#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

// Button Configuration
#define RESET_BUTTON_PIN            GPIO_NUM_5      // Reset button on GPIO 5
#define BUTTON_LONG_PRESS_MS        5000            // 5 seconds for long press
#define BUTTON_CHECK_INTERVAL_MS    50              // Check button every 50ms

// Function Prototypes
esp_err_t button_manager_init(void);
void button_manager_task(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_MANAGER_H