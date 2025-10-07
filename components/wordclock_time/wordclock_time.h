#ifndef WORDCLOCK_TIME_H
#define WORDCLOCK_TIME_H

#include "i2c_devices.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wordclock_time_init(void);
uint8_t wordclock_calculate_base_minutes(uint8_t minutes);
uint8_t wordclock_calculate_indicators(uint8_t minutes);
uint8_t wordclock_get_display_hour(uint8_t hour, uint8_t base_minutes);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_TIME_H