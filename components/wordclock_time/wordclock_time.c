#include "wordclock_time.h"
#include "i2c_devices.h"
#include "esp_log.h"

static const char *TAG = "wordclock_time";

esp_err_t wordclock_time_init(void)
{
    ESP_LOGI(TAG, "Wordclock time module init");
    return i2c_devices_init();
}

uint8_t wordclock_calculate_base_minutes(uint8_t minutes)
{
    return (minutes / 5) * 5;
}

uint8_t wordclock_calculate_indicators(uint8_t minutes)
{
    return minutes - wordclock_calculate_base_minutes(minutes);
}

uint8_t wordclock_get_display_hour(uint8_t hour, uint8_t base_minutes)
{
    uint8_t display_hour = hour;
    
    // If base_minutes >= 25, use next hour for display
    if (base_minutes >= 25) {
        display_hour = (display_hour + 1) % 24;
    }
    
    // Convert to 12-hour format for word display
    if (display_hour == 0) {
        return 12;
    } else if (display_hour > 12) {
        return display_hour - 12;
    } else {
        return display_hour;
    }
}