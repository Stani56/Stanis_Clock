#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ADC Configuration for potentiometer
#define ADC_POTENTIOMETER_UNIT ADC_UNIT_1
#define ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_2     // ESP32-S3: GPIO 3 (ADC1_CH2, WiFi safe)
#define ADC_POTENTIOMETER_ATTEN ADC_ATTEN_DB_12     // 3.3V range (0-3300mV)
#define ADC_POTENTIOMETER_BITWIDTH ADC_BITWIDTH_12  // 12-bit resolution (0-4095)

// Sampling and filtering configuration
#define ADC_SAMPLE_COUNT 8      // Number of samples for averaging
#define ADC_VOLTAGE_MIN 0       // Minimum voltage (mV)
#define ADC_VOLTAGE_MAX 3300    // Maximum voltage (mV)
#define ADC_BRIGHTNESS_MIN 5    // Minimum brightness value (further optimized)
#define ADC_BRIGHTNESS_MAX 255  // Maximum brightness value (increased range)

// ADC result structure
typedef struct {
    uint32_t raw_value;        // Raw ADC reading (0-4095)
    uint32_t voltage_mv;       // Voltage in millivolts
    uint8_t brightness_value;  // Mapped brightness (10-255)
    bool valid;                // Reading validity flag
} adc_reading_t;

// ADC manager state
typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;
    bool initialized;
    bool calibration_enabled;
    adc_reading_t last_reading;
} adc_manager_t;

// Core ADC functions
esp_err_t adc_manager_init(void);
esp_err_t adc_manager_deinit(void);
esp_err_t adc_read_potentiometer(adc_reading_t *reading);
esp_err_t adc_read_potentiometer_averaged(adc_reading_t *reading);

// Utility functions
uint8_t adc_voltage_to_brightness(uint32_t voltage_mv);
uint32_t adc_raw_to_voltage(uint32_t raw_value);
bool adc_manager_is_initialized(void);

// Debug functions
esp_err_t adc_test_gpio_connectivity(void);

// Calibration functions
esp_err_t adc_calibration_init(void);
void adc_calibration_deinit(void);
bool adc_calibration_available(void);

#ifdef __cplusplus
}
#endif

#endif  // ADC_MANAGER_H