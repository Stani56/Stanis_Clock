#include "wordclock_display.h"
#include "i2c_devices.h"
#include "esp_log.h"

static const char *TAG = "wordclock_display";

esp_err_t wordclock_display_init(void)
{
    ESP_LOGI(TAG, "Wordclock display module init");
    return ESP_OK;
}

esp_err_t wordclock_display_show_time(const wordclock_time_t *time)
{
    if (time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t base = wordclock_calculate_base_minutes(time->minutes);
    ESP_LOGI(TAG, "Displaying time: %02d:%02d (base: %d)", 
             time->hours, time->minutes, base);
    
    return ESP_OK;
}