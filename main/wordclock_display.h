#ifndef WORDCLOCK_DISPLAY_H
#define WORDCLOCK_DISPLAY_H

#include "esp_err.h"
#include "wordclock_time.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Display dimensions
#define WORDCLOCK_ROWS 10
#define WORDCLOCK_COLS 16

// German Word Layout Database Structure
typedef struct {
    const char* word;
    uint8_t row;
    uint8_t start_col;
    uint8_t length;
} word_definition_t;

// External LED state array (defined in wordclock_display.c)
extern uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS];

// Word database access
const word_definition_t* get_word_database(void);
size_t get_word_database_size(void);

// Display functions
esp_err_t wordclock_display_init(void);
esp_err_t display_german_time(const wordclock_time_t *time);
esp_err_t build_led_state_matrix(const wordclock_time_t *time, uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS]);
void set_word_leds(uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS], const char* word, uint8_t brightness);
void refresh_current_display(void);

// Minute indicator functions
void set_minute_indicators(uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS], uint8_t count, uint8_t brightness);

// Test functions
void test_german_time_display(void);
void display_test_time(void);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_DISPLAY_H