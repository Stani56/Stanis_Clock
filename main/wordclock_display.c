/**
 * @file wordclock_display.c
 * @brief German word clock display logic and LED control
 * 
 * This module handles the conversion of time to German words and controls
 * the LED matrix display. It includes the word database, LED state management,
 * and display update functions.
 */

#include "wordclock_display.h"
#include "i2c_devices.h"
#include "wordclock_time.h"
#include "thread_safety.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wordclock_display";

// LED state tracking for efficient updates
uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS] = {0};

// Track failed device rows to reduce logging spam
static bool device_row_failed[WORDCLOCK_ROWS] = {false};

// Complete German word layout for the word clock
static const word_definition_t word_database[] = {
    // Row 0: E S • I S T • F Ü N F • [●][●][●][●]
    {"ES",        0, 0, 2},   {"IST",       0, 3, 3},   {"FÜNF_M",    0, 7, 4},
    
    // Row 1: Z E H N Z W A N Z I G • • • • •
    {"ZEHN_M",    1, 0, 4},   {"ZWANZIG",   1, 4, 7},
    
    // Row 2: D R E I V I E R T E L • • • • •
    {"DREIVIERTEL", 2, 0, 11}, {"VIERTEL", 2, 4, 7},
    
    // Row 3: V O R • • • • N A C H • • • • •
    {"VOR",       3, 0, 3},   {"NACH",      3, 7, 4},
    
    // Row 4: H A L B • E L F Ü N F • • • • •
    {"HALB",      4, 0, 4},   {"ELF",       4, 5, 3},   {"FÜNF_H",    4, 7, 4},
    
    // Row 5: E I N S • • • Z W E I • • • • •
    {"EIN",       5, 0, 3},   {"EINS",      5, 0, 4},   {"ZWEI",      5, 7, 4},
    
    // Row 6: D R E I • • • V I E R • • • • •
    {"DREI",      6, 0, 4},   {"VIER",      6, 7, 4},
    
    // Row 7: S E C H S • • A C H T • • • • •
    {"SECHS",     7, 0, 5},   {"ACHT",      7, 7, 4},
    
    // Row 8: S I E B E N Z W Ö L F • • • • •
    {"SIEBEN",    8, 0, 6},   {"ZWÖLF",     8, 6, 5},
    
    // Row 9: Z E H N E U N • U H R • [●][●][●][●]
    {"ZEHN_H",    9, 0, 4},   {"NEUN",      9, 3, 4},   {"UHR",       9, 8, 3}
};

// Hour word arrays with proper German grammar
static const char* hour_words[] = {
    "ZWÖLF", "EINS", "ZWEI", "DREI", "VIER", "FÜNF_H",
    "SECHS", "SIEBEN", "ACHT", "NEUN", "ZEHN_H", "ELF", "ZWÖLF"
};

static const char* hour_words_with_uhr[] = {
    "ZWÖLF", "EIN", "ZWEI", "DREI", "VIER", "FÜNF_H",
    "SECHS", "SIEBEN", "ACHT", "NEUN", "ZEHN_H", "ELF", "ZWÖLF"
};

// Public functions to access word database
const word_definition_t* get_word_database(void) {
    return word_database;
}

size_t get_word_database_size(void) {
    return sizeof(word_database) / sizeof(word_database[0]);
}

/**
 * @brief Initialize the display system and clear all LEDs
 */
esp_err_t wordclock_display_init(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING WORD CLOCK DISPLAY ===");
    
    // Check TLC device status before clearing
    for (int i = 0; i < 3; i++) {  // Check first 3 devices
        esp_err_t test_ret = tlc_set_pwm(i, 0, 0);
        ESP_LOGI(TAG, "TLC device %d test: %s", i, esp_err_to_name(test_ret));
    }
    
    // Clear all LEDs on hardware to ensure clean state
    esp_err_t ret = tlc_turn_off_all_leds();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to clear LEDs during init: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ All LEDs cleared successfully");
    }
    
    // Reset software LED state tracking
    memset(led_state, 0, sizeof(led_state));
    
    // Reset device failure tracking
    memset(device_row_failed, false, sizeof(device_row_failed));
    
    ESP_LOGI(TAG, "✅ Display initialization complete - all LEDs cleared");
    return ESP_OK;
}

/**
 * @brief Set LEDs for a specific word in the brightness matrix
 */
void set_word_leds(uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS], 
                   const char* word, uint8_t brightness)
{
    for (size_t i = 0; i < sizeof(word_database) / sizeof(word_database[0]); i++) {
        if (strcmp(word_database[i].word, word) == 0) {
            for (uint8_t col = 0; col < word_database[i].length; col++) {
                led_state[word_database[i].row][word_database[i].start_col + col] = brightness;
            }
            return;
        }
    }
    ESP_LOGW(TAG, "Word '%s' not found in database", word);
}

/**
 * @brief Set minute indicator LEDs
 */
void set_minute_indicators(uint8_t led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS], 
                          uint8_t count, uint8_t brightness)
{
    // Minute indicators at Row 9, Columns 11-14
    for (uint8_t i = 0; i < 4; i++) {
        if (i < count) {
            led_state[9][11 + i] = brightness;
        } else {
            led_state[9][11 + i] = 0;
        }
    }
}

/**
 * @brief Build LED state matrix from time
 */
esp_err_t build_led_state_matrix(const wordclock_time_t *time, 
                                uint8_t new_led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS])
{
    if (!time) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear the LED state matrix
    memset(new_led_state, 0, WORDCLOCK_ROWS * WORDCLOCK_COLS);

    // Get individual brightness setting
    uint8_t brightness = tlc_get_individual_brightness();

    // Calculate base minutes and indicators
    uint8_t base_minutes = wordclock_calculate_base_minutes(time->minutes);
    uint8_t indicator_count = wordclock_calculate_indicators(time->minutes);
    uint8_t display_hour = wordclock_get_display_hour(time->hours, base_minutes);

    // Always show "ES IST"
    set_word_leds(new_led_state, "ES", brightness);
    set_word_leds(new_led_state, "IST", brightness);

    // Show minute words based on base_minutes
    switch (base_minutes) {
        case 0:
            // No minute words, just hour + UHR
            break;
        case 5:
            set_word_leds(new_led_state, "FÜNF_M", brightness);
            set_word_leds(new_led_state, "NACH", brightness);
            break;
        case 10:
            set_word_leds(new_led_state, "ZEHN_M", brightness);
            set_word_leds(new_led_state, "NACH", brightness);
            break;
        case 15:
            set_word_leds(new_led_state, "VIERTEL", brightness);
            set_word_leds(new_led_state, "NACH", brightness);
            break;
        case 20:
            set_word_leds(new_led_state, "ZWANZIG", brightness);
            set_word_leds(new_led_state, "NACH", brightness);
            break;
        case 25:
            set_word_leds(new_led_state, "FÜNF_M", brightness);
            set_word_leds(new_led_state, "VOR", brightness);
            set_word_leds(new_led_state, "HALB", brightness);
            break;
        case 30:
            set_word_leds(new_led_state, "HALB", brightness);
            break;
        case 35:
            set_word_leds(new_led_state, "FÜNF_M", brightness);
            set_word_leds(new_led_state, "NACH", brightness);
            set_word_leds(new_led_state, "HALB", brightness);
            break;
        case 40:
            set_word_leds(new_led_state, "ZWANZIG", brightness);
            set_word_leds(new_led_state, "VOR", brightness);
            break;
        case 45:
            set_word_leds(new_led_state, "VIERTEL", brightness);
            set_word_leds(new_led_state, "VOR", brightness);
            break;
        case 50:
            set_word_leds(new_led_state, "ZEHN_M", brightness);
            set_word_leds(new_led_state, "VOR", brightness);
            break;
        case 55:
            set_word_leds(new_led_state, "FÜNF_M", brightness);
            set_word_leds(new_led_state, "VOR", brightness);
            break;
    }

    // Show hour word with proper German grammar
    const char* hour_word;
    if (base_minutes == 0) {
        // Use "EIN UHR" form for exact hour
        hour_word = hour_words_with_uhr[display_hour];
        set_word_leds(new_led_state, hour_word, brightness);
        set_word_leds(new_led_state, "UHR", brightness);
    } else {
        // Use "EINS" form for other times
        hour_word = hour_words[display_hour];
        set_word_leds(new_led_state, hour_word, brightness);
    }

    // Set minute indicators
    set_minute_indicators(new_led_state, indicator_count, brightness);

    return ESP_OK;
}

/**
 * @brief Update LED hardware based on differential state changes
 */
static esp_err_t update_led_if_changed(uint8_t row, uint8_t col, uint8_t new_brightness)
{
    if (led_state[row][col] != new_brightness) {
        esp_err_t ret = tlc_set_matrix_led(row, col, new_brightness);
        if (ret == ESP_OK) {
            led_state[row][col] = new_brightness;
        }
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief Display German time with efficient differential updates
 */
esp_err_t display_german_time(const wordclock_time_t *time)
{
    if (!time) {
        ESP_LOGE(TAG, "Invalid time pointer");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Displaying time: %02d:%02d:%02d", 
             time->hours, time->minutes, time->seconds);

    // Build new LED state
    uint8_t new_led_state[WORDCLOCK_ROWS][WORDCLOCK_COLS] = {0};
    esp_err_t ret = build_led_state_matrix(time, new_led_state);
    if (ret != ESP_OK) {
        return ret;
    }

    // Apply differential updates - only change LEDs that need changing
    uint8_t changes_made = 0;
    for (uint8_t row = 0; row < WORDCLOCK_ROWS; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            if (led_state[row][col] != new_led_state[row][col]) {
                ret = update_led_if_changed(row, col, new_led_state[row][col]);
                if (ret == ESP_OK) {
                    changes_made++;
                } else if (!device_row_failed[row]) {
                    ESP_LOGW(TAG, "Failed to update LED at (%d,%d): %s", 
                             row, col, esp_err_to_name(ret));
                    device_row_failed[row] = true;
                }
            }
        }
    }

    ESP_LOGI(TAG, "Display update complete: %d LED changes", changes_made);
    return ESP_OK;
}

/**
 * @brief Refresh current display with updated brightness
 */
void refresh_current_display(void)
{
    // Skip refresh if transition is active
    extern bool transition_manager_is_active(void);
    if (transition_manager_is_active()) {
        ESP_LOGD(TAG, "Skipping refresh - transition active");
        return;
    }

    uint8_t current_brightness = tlc_get_individual_brightness();
    uint8_t updates = 0;

    // Only update LEDs that are currently on
    for (uint8_t row = 0; row < WORDCLOCK_ROWS; row++) {
        for (uint8_t col = 0; col < WORDCLOCK_COLS; col++) {
            if (led_state[row][col] > 0) {
                esp_err_t ret = tlc_set_matrix_led(row, col, current_brightness);
                if (ret == ESP_OK) {
                    led_state[row][col] = current_brightness;
                    updates++;
                }
            }
        }
    }
    
    ESP_LOGD(TAG, "Display refreshed: %d LEDs updated to brightness %d", 
             updates, current_brightness);
}

/**
 * @brief Test German time display with various times
 */
void test_german_time_display(void)
{
    ESP_LOGI(TAG, "=== TESTING GERMAN TIME DISPLAY ===");
    
    wordclock_time_t test_times[] = {
        {1, 0, 0, 1, 1, 24},    // 01:00 - "ES IST EIN UHR"
        {12, 55, 0, 1, 1, 24},  // 12:55 - "ES IST FÜNF VOR EINS"
        {14, 30, 0, 1, 1, 24},  // 14:30 - "ES IST HALB DREI"
        {9, 0, 0, 1, 1, 24},    // 09:00 - "ES IST NEUN UHR"
        {18, 15, 0, 1, 1, 24},  // 18:15 - "ES IST VIERTEL NACH SECHS"
        {22, 37, 0, 1, 1, 24},  // 22:37 - "ES IST FÜNF NACH HALB ELF" + 2 indicators
    };
    
    for (size_t i = 0; i < sizeof(test_times) / sizeof(test_times[0]); i++) {
        ESP_LOGI(TAG, "\nTest %zu: %02d:%02d", i+1, 
                 test_times[i].hours, test_times[i].minutes);
        
        display_german_time(&test_times[i]);
        vTaskDelay(pdMS_TO_TICKS(3000));  // 3 second delay between tests
    }
    
    ESP_LOGI(TAG, "=== GERMAN TIME DISPLAY TEST COMPLETE ===");
}

/**
 * @brief Display a test time (14:30 - HALB DREI)
 */
void display_test_time(void)
{
    wordclock_time_t test_time = {
        .hours = 14,
        .minutes = 30,
        .seconds = 0,
        .day = 1,
        .month = 1,
        .year = 24
    };
    
    ESP_LOGI(TAG, "Displaying test time: 14:30 (HALB DREI)");
    display_german_time(&test_time);
}