#ifndef WORDCLOCK_DISPLAY_H
#define WORDCLOCK_DISPLAY_H

#include "wordclock_time.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wordclock_display_init(void);
esp_err_t wordclock_display_show_time(const wordclock_time_t *time);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_DISPLAY_H