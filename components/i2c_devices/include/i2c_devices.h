#ifndef I2C_DEVICES_H
#define I2C_DEVICES_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C Configuration
#define I2C_LEDS_MASTER_PORT           0
#define I2C_SENSORS_MASTER_PORT        1
#define I2C_LEDS_MASTER_SDA_IO         8      // ESP32-S3: GPIO 8 (default I2C0 SDA on YB board)
#define I2C_LEDS_MASTER_SCL_IO         9      // ESP32-S3: GPIO 9 (default I2C0 SCL on YB board)
#define I2C_SENSORS_MASTER_SDA_IO      1      // ESP32-S3: GPIO 1 (ADC1_CH0, WiFi safe)
#define I2C_SENSORS_MASTER_SCL_IO      18     // ESP32-S3: GPIO 18 (no ADC, WiFi safe)
#define I2C_LEDS_MASTER_FREQ_HZ        100000
#define I2C_SENSORS_MASTER_FREQ_HZ     100000

// TLC59116 Configuration
#define TLC59116_COUNT                 10
#define TLC59116_CHANNELS              16
#define TLC59116_ADDR_BASE             0x60

// TLC59116 Register Addresses
#define TLC59116_MODE1                 0x00
#define TLC59116_MODE2                 0x01
#define TLC59116_PWM0                  0x02
#define TLC59116_GRPPWM                0x12
#define TLC59116_GRPFREQ               0x13
#define TLC59116_LEDOUT0               0x14
#define TLC59116_SUBADR1               0x17
#define TLC59116_SUBADR2               0x18
#define TLC59116_SUBADR3               0x19
#define TLC59116_ALLCALLADR            0x1A
#define TLC59116_IREF                  0x1C
#define TLC59116_EFLAG1                0x1D
#define TLC59116_EFLAG2                0x1E

// TLC59116 Bit Definitions
#define TLC59116_MODE1_AI0             0x20
#define TLC59116_MODE1_AI1             0x40
#define TLC59116_MODE1_AI2             0x80
#define TLC59116_MODE1_SLEEP           0x10
#define TLC59116_MODE2_DMBLNK          0x20
#define TLC59116_LED_OFF               0x00
#define TLC59116_LED_ON                0x01
#define TLC59116_LED_PWM               0x02
#define TLC59116_LED_GRPPWM            0x03

// DS3231 Configuration
#define DS3231_ADDR                    0x68

// BH1750 Light Sensor Configuration
#define BH1750_ADDR                    0x23

// DS3231 Register Addresses
#define DS3231_REG_SECONDS             0x00
#define DS3231_REG_MINUTES             0x01
#define DS3231_REG_HOURS               0x02
#define DS3231_REG_DAY                 0x03
#define DS3231_REG_DATE                0x04
#define DS3231_REG_MONTH               0x05
#define DS3231_REG_YEAR                0x06
#define DS3231_REG_CONTROL             0x0E
#define DS3231_REG_STATUS              0x0F
#define DS3231_REG_TEMP_MSB            0x11
#define DS3231_REG_TEMP_LSB            0x12

// Word Clock Layout
#define WORDCLOCK_ROWS                 10
#define WORDCLOCK_COLS                 16
#define WORDCLOCK_WORD_COLS            11
#define WORDCLOCK_MINUTE_LEDS          4

// Data Structures
typedef struct {
    uint8_t hours;      // 0-23
    uint8_t minutes;    // 0-59
    uint8_t seconds;    // 0-59
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint8_t year;       // 0-99 (20xx)
} wordclock_time_t;

typedef struct {
    float temperature;
    bool valid;
} ds3231_temp_t;

typedef struct {
    bool initialized;
    uint8_t pwm_values[TLC59116_CHANNELS];
    uint8_t led_states[TLC59116_CHANNELS];
} tlc59116_device_t;

// Global Arrays
extern const uint8_t tlc_addresses[TLC59116_COUNT];

// Core Functions
esp_err_t i2c_devices_init(void);
esp_err_t i2c_scan_bus(i2c_port_t port);

// TLC59116 Functions
esp_err_t tlc59116_init_all(void);
esp_err_t tlc59116_init_device(uint8_t tlc_index);
esp_err_t tlc59116_reset_device(uint8_t tlc_index);
esp_err_t tlc59116_sleep_device(uint8_t tlc_index, bool sleep);
esp_err_t tlc_set_pwm(uint8_t tlc_index, uint8_t channel, uint8_t pwm_value);
esp_err_t tlc_set_global_brightness(uint8_t brightness);
esp_err_t tlc_turn_off_all_leds(void);
esp_err_t tlc_read_all_status(uint8_t *status_array);
esp_err_t tlc_set_matrix_led(uint8_t row, uint8_t col, uint8_t brightness);
esp_err_t tlc_set_matrix_led_auto(uint8_t row, uint8_t col);
esp_err_t tlc_set_minute_leds(uint8_t indicator_count);
esp_err_t tlc_set_individual_brightness(uint8_t brightness);
uint8_t tlc_get_individual_brightness(void);
uint8_t tlc_get_global_brightness(void);

// TLC59116 Register Read Functions (for LED validation)
esp_err_t tlc_read_pwm_values(uint8_t tlc_index, uint8_t pwm_values[16]);
esp_err_t tlc_read_all_pwm_values(uint8_t hardware_state[10][16]);
esp_err_t tlc_read_global_brightness(uint8_t grppwm_values[10]);
esp_err_t tlc_read_error_flags(uint8_t eflag_values[10][2]);

// TLC59116 Hardware Reset Functions (GPIO 4 - Shared Reset Line)
/**
 * @brief Initialize TLC59116 hardware reset GPIO
 *
 * Configures GPIO 4 as open-drain output with external pull-up.
 * Only functional if BOARD_TLC_HW_RESET_AVAILABLE is enabled.
 *
 * Hardware Requirements:
 * - External 10kΩ pull-up resistor from GPIO 4 to 3.3V
 * - GPIO 4 connected to Pin 2 (RESET) on all 10 TLC59116 devices
 *
 * Must be called during system initialization before tlc59116_init_all().
 *
 * @return ESP_OK on success
 *         ESP_ERR_NOT_SUPPORTED if hardware reset feature disabled
 *         ESP_FAIL if GPIO configuration failed
 */
esp_err_t tlc59116_reset_gpio_init(void);

/**
 * @brief Perform hardware reset of all TLC59116 devices
 *
 * Asserts shared RESET line (active LOW) for 10µs, then releases and waits 5ms
 * for all devices to stabilize.
 *
 * ⚠️ WARNING: This resets ALL 10 TLC59116 devices simultaneously.
 * All LEDs will turn off. Re-initialization required after reset.
 *
 * Reset Timing:
 * - 10µs LOW pulse on GPIO 4 (datasheet minimum: 50ns)
 * - 5ms stabilization delay (datasheet minimum: 1ms)
 * - Total hardware reset time: ~5.01ms
 *
 * Use Cases:
 * - Recovery from frozen TLC state after ESP32 watchdog reset
 * - Recovery from I2C bus lockup
 * - Post-power-surge TLC state restoration
 * - Manual reset via MQTT command
 * - Preventive reset during post-reset validation
 *
 * Typical Recovery Sequence:
 * 1. Call tlc59116_hardware_reset_all() - 5ms
 * 2. Call tlc59116_init_all() - 50ms
 * 3. Restore LED display state - automatic
 * Total recovery time: ~55ms
 *
 * @return ESP_OK on success
 *         ESP_ERR_NOT_SUPPORTED if hardware reset not available
 *         ESP_FAIL if GPIO not initialized
 */
esp_err_t tlc59116_hardware_reset_all(void);

/**
 * @brief Check if hardware reset capability is available
 *
 * Returns true only if:
 * 1. BOARD_TLC_HW_RESET_AVAILABLE is enabled (compile-time)
 * 2. GPIO has been successfully initialized (runtime)
 *
 * Use this function to conditionally enable hardware reset features:
 * - MQTT command handler
 * - Home Assistant button entity
 * - LED validation enhanced recovery
 *
 * @return true if hardware reset is available and initialized
 *         false if feature disabled or GPIO not initialized
 */
bool tlc59116_has_hardware_reset(void);

// DS3231 Functions
esp_err_t ds3231_init(void);
esp_err_t ds3231_get_time_struct(wordclock_time_t *time);
esp_err_t ds3231_set_time_struct(const wordclock_time_t *time);
esp_err_t ds3231_get_temperature(ds3231_temp_t *temperature);

// BH1750 Light Sensor Functions
esp_err_t bh1750_init(void);
esp_err_t bh1750_write_command(uint8_t command);
esp_err_t bh1750_read_data(uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // I2C_DEVICES_H