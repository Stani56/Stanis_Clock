// button_manager.c
#include "button_manager.h"
#include "nvs_manager.h"

static const char *TAG = "BUTTON_MANAGER";

esp_err_t button_manager_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << RESET_BUTTON_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    return gpio_config(&io_conf);
}

void button_manager_task(void *pvParameter) {
    bool button_pressed = false;
    bool button_stable = true;
    TickType_t press_start_time = 0;
    TickType_t last_change_time = 0;
    int last_button_state = 1;
    const TickType_t debounce_delay = pdMS_TO_TICKS(50);
    
    while (1) {
        int button_state = gpio_get_level(RESET_BUTTON_PIN);
        TickType_t current_time = xTaskGetTickCount();
        
        // Debounce logic
        if (button_state != last_button_state) {
            last_change_time = current_time;
            button_stable = false;
        } else if (!button_stable && (current_time - last_change_time) > debounce_delay) {
            button_stable = true;
            
            // Process stable button state change
            if (button_state == 0 && !button_pressed) { // Button pressed (active low)
                button_pressed = true;
                press_start_time = current_time;
                ESP_LOGI(TAG, "Reset button pressed");
            } else if (button_state == 1 && button_pressed) { // Button released
                TickType_t press_duration = current_time - press_start_time;
                button_pressed = false;
                
                if (press_duration > pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS)) {
                    ESP_LOGI(TAG, "Reset button held for %ld ms, clearing WiFi credentials", 
                             pdTICKS_TO_MS(press_duration));
                    nvs_manager_clear_wifi_credentials();
                    ESP_LOGI(TAG, "WiFi credentials cleared, restarting in 2 seconds...");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    esp_restart();
                } else {
                    ESP_LOGI(TAG, "Reset button short press (%ld ms) - no action", 
                             pdTICKS_TO_MS(press_duration));
                }
            }
        }
        
        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_CHECK_INTERVAL_MS));
    }
}