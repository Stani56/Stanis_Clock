#include "i2c_devices.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "i2c_devices";

// Global Variables
static uint8_t individual_led_brightness = 32;
static uint8_t current_global_brightness = 255;
static i2c_master_bus_handle_t leds_bus_handle = NULL;
static i2c_master_bus_handle_t sensors_bus_handle = NULL;
static i2c_master_dev_handle_t tlc_dev_handles[TLC59116_COUNT] = {NULL};
static i2c_master_dev_handle_t ds3231_dev_handle = NULL;
static i2c_master_dev_handle_t bh1750_dev_handle = NULL;

// TLC59116 device states
static tlc59116_device_t tlc_devices[TLC59116_COUNT] = {0};

// TLC59116 addresses
const uint8_t tlc_addresses[TLC59116_COUNT] = {
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6A
};

// Helper Functions
static esp_err_t i2c_write_byte(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t value)
{
    i2c_master_dev_handle_t dev_handle = NULL;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Find TLC device handle
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_addresses[i] == device_addr) {
                dev_handle = tlc_dev_handles[i];
                break;
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == DS3231_ADDR) {
        dev_handle = ds3231_dev_handle;
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == BH1750_ADDR) {
        dev_handle = bh1750_dev_handle;
    }
    
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "Device handle not found for address 0x%02X", device_addr);
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t write_buffer[2] = {reg_addr, value};
    return i2c_master_transmit(dev_handle, write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(100));
}

static esp_err_t i2c_read_bytes(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev_handle = NULL;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Find TLC device handle
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_addresses[i] == device_addr) {
                dev_handle = tlc_dev_handles[i];
                break;
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == DS3231_ADDR) {
        dev_handle = ds3231_dev_handle;
    } else if (port == I2C_SENSORS_MASTER_PORT && device_addr == BH1750_ADDR) {
        dev_handle = bh1750_dev_handle;
    }
    
    if (dev_handle == NULL) {
        ESP_LOGE(TAG, "Device handle not found for address 0x%02X", device_addr);
        return ESP_ERR_NOT_FOUND;
    }
    
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, pdMS_TO_TICKS(100));
}

// Core I2C Initialization
esp_err_t i2c_devices_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C devices with ESP-IDF 5.3.1 driver");
    ESP_LOGI(TAG, "I2C Frequencies: LEDs=%dkHz, Sensors=%dkHz", 
             I2C_LEDS_MASTER_FREQ_HZ/1000, I2C_SENSORS_MASTER_FREQ_HZ/1000);
    
    // Initialize LEDs I2C bus
    i2c_master_bus_config_t leds_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_LEDS_MASTER_PORT,
        .scl_io_num = I2C_LEDS_MASTER_SCL_IO,
        .sda_io_num = I2C_LEDS_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&leds_bus_config, &leds_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LEDs I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LEDs I2C bus initialized on port %d", I2C_LEDS_MASTER_PORT);
    
    // Initialize Sensors I2C bus
    i2c_master_bus_config_t sensors_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_SENSORS_MASTER_PORT,
        .scl_io_num = I2C_SENSORS_MASTER_SCL_IO,
        .sda_io_num = I2C_SENSORS_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ret = i2c_new_master_bus(&sensors_bus_config, &sensors_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create Sensors I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Sensors I2C bus initialized on port %d", I2C_SENSORS_MASTER_PORT);
    
    // Add TLC59116 devices
    for (int i = 0; i < TLC59116_COUNT; i++) {
        i2c_device_config_t tlc_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = tlc_addresses[i],
            .scl_speed_hz = I2C_LEDS_MASTER_FREQ_HZ,
        };
        
        ret = i2c_master_bus_add_device(leds_bus_handle, &tlc_dev_cfg, &tlc_dev_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add TLC59116 device %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "TLC59116 device %d added (0x%02X)", i, tlc_addresses[i]);
    }
    
    // Add DS3231 device
    i2c_device_config_t ds3231_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDR,
        .scl_speed_hz = I2C_SENSORS_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(sensors_bus_handle, &ds3231_dev_cfg, &ds3231_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add DS3231 device: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "DS3231 device added (0x%02X)", DS3231_ADDR);
    
    // Add BH1750 light sensor device
    i2c_device_config_t bh1750_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_ADDR,
        .scl_speed_hz = I2C_SENSORS_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(sensors_bus_handle, &bh1750_dev_cfg, &bh1750_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BH1750 device: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BH1750 device added (0x%02X)", BH1750_ADDR);
    
    // Initialize all TLC59116 devices
    ret = tlc59116_init_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Some TLC59116 devices failed to initialize, continuing with available devices");
    }
    
    // Initialize DS3231 RTC
    ret = ds3231_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DS3231 initialization failed: %s", esp_err_to_name(ret));
    }
    
    // Initialize BH1750 light sensor
    ret = bh1750_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BH1750 initialization failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "I2C devices initialization completed");
    return ESP_OK;
}

// I2C Bus Scanner - Simplified to avoid timing conflicts
esp_err_t i2c_scan_bus(i2c_port_t port)
{
    ESP_LOGI(TAG, "Checking I2C bus %d device status...", port);
    int device_count = 0;
    
    if (port == I2C_LEDS_MASTER_PORT) {
        // Check known TLC59116 devices only
        for (int i = 0; i < TLC59116_COUNT; i++) {
            if (tlc_devices[i].initialized) {
                ESP_LOGI(TAG, "TLC59116 device %d (0x%02X) - OK", i, tlc_addresses[i]);
                device_count++;
            } else {
                ESP_LOGI(TAG, "TLC59116 device %d (0x%02X) - Failed", i, tlc_addresses[i]);
            }
        }
    } else if (port == I2C_SENSORS_MASTER_PORT) {
        // Test DS3231 with a simple read operation
        if (ds3231_dev_handle != NULL) {
            uint8_t test_data;
            esp_err_t ret = i2c_read_bytes(I2C_SENSORS_MASTER_PORT, DS3231_ADDR, DS3231_REG_SECONDS, &test_data, 1);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "DS3231 RTC (0x%02X) - OK", DS3231_ADDR);
                device_count++;
            } else {
                ESP_LOGI(TAG, "DS3231 RTC (0x%02X) - Failed", DS3231_ADDR);
            }
        }
    }
    
    ESP_LOGI(TAG, "I2C bus %d status check complete: %d devices working", port, device_count);
    return ESP_OK;
}

// TLC59116 LED Controller Functions
esp_err_t tlc59116_init_all(void)
{
    ESP_LOGI(TAG, "Initializing all TLC59116 devices");
    
    esp_err_t ret = ESP_OK;
    int success_count = 0;
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        esp_err_t device_ret = tlc59116_init_device(i);
        if (device_ret == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGW(TAG, "Failed to initialize TLC59116 device %d, continuing with others", i);
            ret = device_ret; // Keep last error
        }
    }
    
    ESP_LOGI(TAG, "TLC59116 initialization completed: %d/%d devices successful", success_count, TLC59116_COUNT);
    
    if (success_count == 0) {
        ESP_LOGE(TAG, "No TLC59116 devices initialized successfully");
        return ESP_FAIL;
    }
    
    return (success_count == TLC59116_COUNT) ? ESP_OK : ret;
}

esp_err_t tlc59116_init_device(uint8_t tlc_index)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (tlc_dev_handles[tlc_index] == NULL) {
        ESP_LOGE(TAG, "TLC59116 device handle %d not available", tlc_index);
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    uint8_t device_addr = tlc_addresses[tlc_index];
    
    ESP_LOGI(TAG, "Initializing TLC59116 device %d (0x%02X)", tlc_index, device_addr);
    
    // Reset device first
    ret = tlc59116_reset_device(tlc_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure MODE1 register: Normal mode, auto-increment enabled
    uint8_t mode1_val = TLC59116_MODE1_AI0 | TLC59116_MODE1_AI1; // Auto-increment for PWM registers
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, mode1_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure MODE1 for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure MODE2 register: Normal operation
    uint8_t mode2_val = 0x00; // Normal operation, no blinking
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE2, mode2_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure MODE2 for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    // Set all LEDs to GRPPWM mode for global brightness control (LEDOUT registers)
    uint8_t ledout_val = 0xFF; // 11111111 = GRPPWM mode for all 4 outputs per register
    for (int reg = 0; reg < 4; reg++) {
        ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_LEDOUT0 + reg, ledout_val);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure LEDOUT%d for TLC59116 device %d: %s", 
                     reg, tlc_index, esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Set all PWM channels to 0 (off)
    for (int ch = 0; ch < TLC59116_CHANNELS; ch++) {
        ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_PWM0 + ch, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set PWM%d for TLC59116 device %d: %s", 
                     ch, tlc_index, esp_err_to_name(ret));
            return ret;
        }
        tlc_devices[tlc_index].pwm_values[ch] = 0;
        tlc_devices[tlc_index].led_states[ch] = TLC59116_LED_GRPPWM;
    }
    
    // Set global brightness to match expected startup value (120) instead of 255
    // This prevents brightness jumps during startup when main sets GRPPWM to 120
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_GRPPWM, 120);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GRPPWM for TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    tlc_devices[tlc_index].initialized = true;
    ESP_LOGI(TAG, "TLC59116 device %d initialized successfully", tlc_index);
    
    return ESP_OK;
}

esp_err_t tlc59116_reset_device(uint8_t tlc_index)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t device_addr = tlc_addresses[tlc_index];
    
    // Set MODE1 to software reset (sleep mode first, then wake up)
    esp_err_t ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, TLC59116_MODE1_SLEEP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sleep TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Sleep delay
    
    // Wake up device
    ret = i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake TLC59116 device %d: %s", tlc_index, esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1)); // Wake-up delay
    
    return ESP_OK;
}

esp_err_t tlc_set_pwm(uint8_t tlc_index, uint8_t channel, uint8_t pwm_value)
{
    if (tlc_index >= TLC59116_COUNT) {
        ESP_LOGE(TAG, "Invalid TLC index: %d", tlc_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (channel >= TLC59116_CHANNELS) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!tlc_devices[tlc_index].initialized || tlc_dev_handles[tlc_index] == NULL) {
        // Silently skip uninitialized devices to avoid spam
        return ESP_ERR_INVALID_STATE;
    }
    
    // DEBUG: Log PWM writes when needed (can be commented out for production)
    // ESP_LOGD(TAG, "TLC PWM: device[%d] channel[%d] = %d", tlc_index, channel, pwm_value);
    
    // Use device handle directly - no lookup needed
    uint8_t write_buffer[2] = {TLC59116_PWM0 + channel, pwm_value};
    esp_err_t ret = i2c_master_transmit(tlc_dev_handles[tlc_index], write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));
    
    if (ret == ESP_OK) {
        tlc_devices[tlc_index].pwm_values[channel] = pwm_value;
        // Add small delay to ensure I2C timing stability and prevent brightness jumps
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return ret;
}

esp_err_t tlc_set_global_brightness(uint8_t brightness)
{
    // DEBUG: Track global brightness changes when needed
    ESP_LOGD(TAG, "Global brightness change: %d → %d", current_global_brightness, brightness);
    
    esp_err_t ret = ESP_OK;
    int success_count = 0;
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized || tlc_dev_handles[i] == NULL) {
            continue;
        }
        
        // Use device handle directly
        uint8_t write_buffer[2] = {TLC59116_GRPPWM, brightness};
        esp_err_t device_ret = i2c_master_transmit(tlc_dev_handles[i], write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));
        
        if (device_ret == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGW(TAG, "Failed to set global brightness for TLC59116 device %d: %s", 
                     i, esp_err_to_name(device_ret));
            ret = device_ret;
        }
    }
    
    ESP_LOGI(TAG, "Global brightness set to %d for %d devices", brightness, success_count);
    
    // Update global tracking variable only if at least one device succeeded
    if (success_count > 0) {
        current_global_brightness = brightness;
    }
    
    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_turn_off_all_leds(void)
{
    esp_err_t ret = ESP_OK;
    int success_count = 0;
    int error_count = 0;
    
    ESP_LOGI(TAG, "Turning off all LEDs");
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            continue;
        }
        
        // Set all PWM channels to 0 with error handling and timing
        for (int ch = 0; ch < TLC59116_CHANNELS; ch++) {
            esp_err_t ch_ret = tlc_set_pwm(i, ch, 0);
            if (ch_ret != ESP_OK) {
                error_count++;
                // Don't fail completely on individual channel errors
                if (error_count > 10) {
                    ESP_LOGW(TAG, "Too many I2C errors, stopping clear operation");
                    break;
                }
            } else {
                // Small delay between successful operations to prevent bus saturation
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        success_count++;
        
        // Break if too many errors
        if (error_count > 10) {
            break;
        }
    }
    
    if (error_count > 0) {
        ESP_LOGW(TAG, "LED clear completed with %d I2C errors for %d devices", error_count, success_count);
    } else {
        ESP_LOGI(TAG, "All LEDs turned off for %d devices", success_count);
    }
    
    return (success_count > 0) ? ESP_OK : ret;
}

esp_err_t tlc_read_all_status(uint8_t *status_array)
{
    if (status_array == NULL) {
        ESP_LOGE(TAG, "Status array is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    for (int i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            status_array[i] = 0xFF; // Mark as uninitialized
            continue;
        }
        
        uint8_t device_addr = tlc_addresses[i];
        esp_err_t device_ret = i2c_read_bytes(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_EFLAG1, &status_array[i], 1);
        
        if (device_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read status for TLC59116 device %d: %s", i, esp_err_to_name(device_ret));
            status_array[i] = 0xFF;
            ret = device_ret;
        }
    }
    
    return ret;
}

esp_err_t tlc_set_matrix_led(uint8_t row, uint8_t col, uint8_t brightness)
{
    if (row >= WORDCLOCK_ROWS) {
        ESP_LOGE(TAG, "Invalid row: %d", row);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (col >= WORDCLOCK_COLS) {
        ESP_LOGE(TAG, "Invalid column: %d", col);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate TLC device and channel from row/column
    uint8_t tlc_index = row;
    uint8_t channel = col;
    
    return tlc_set_pwm(tlc_index, channel, brightness);
}

esp_err_t tlc_set_matrix_led_auto(uint8_t row, uint8_t col)
{
    return tlc_set_matrix_led(row, col, individual_led_brightness);
}

esp_err_t tlc_set_minute_leds(uint8_t indicator_count)
{
    if (indicator_count > WORDCLOCK_MINUTE_LEDS) {
        ESP_LOGE(TAG, "Invalid indicator count: %d", indicator_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    // Minute indicator LEDs are at Row 9, Columns 11-14
    for (uint8_t i = 0; i < WORDCLOCK_MINUTE_LEDS; i++) {
        uint8_t brightness = (i < indicator_count) ? individual_led_brightness : 0;
        esp_err_t led_ret = tlc_set_matrix_led(9, 11 + i, brightness);
        if (led_ret != ESP_OK) {
            ret = led_ret;
        }
    }
    
    ESP_LOGI(TAG, "Minute indicator LEDs set: %d active", indicator_count);
    return ret;
}

esp_err_t tlc_set_individual_brightness(uint8_t brightness)
{
    individual_led_brightness = brightness;
    ESP_LOGI(TAG, "Individual LED brightness set to %d", brightness);
    return ESP_OK;
}

uint8_t tlc_get_individual_brightness(void)
{
    return individual_led_brightness;
}

uint8_t tlc_get_global_brightness(void)
{
    return current_global_brightness;
}

// DS3231 RTC Functions
esp_err_t ds3231_init(void)
{
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing DS3231 RTC");
    
    // Read control register to check device
    uint8_t control_reg;
    uint8_t reg_addr = DS3231_REG_CONTROL;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, &control_reg, 1, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read DS3231 control register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 control register: 0x%02X", control_reg);
    
    // Configure DS3231 for normal operation
    // Clear EOSC bit to enable oscillator, disable square wave output
    uint8_t control_val = 0x1C; // INTCN=1, RS2=1, RS1=1 (disable square wave)
    uint8_t control_write[2] = {DS3231_REG_CONTROL, control_val};
    ret = i2c_master_transmit(ds3231_dev_handle, control_write, sizeof(control_write), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure DS3231 control register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Clear status register
    uint8_t status_write[2] = {DS3231_REG_STATUS, 0x00};
    ret = i2c_master_transmit(ds3231_dev_handle, status_write, sizeof(status_write), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear DS3231 status register: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 RTC initialized successfully");
    return ESP_OK;
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

esp_err_t ds3231_get_time_struct(wordclock_time_t *time)
{
    if (time == NULL) {
        ESP_LOGE(TAG, "Time structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t time_data[7];
    uint8_t reg_addr = DS3231_REG_SECONDS;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, time_data, 7, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from DS3231: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Convert BCD to binary
    time->seconds = bcd_to_bin(time_data[0] & 0x7F);  // Mask out CH bit
    time->minutes = bcd_to_bin(time_data[1] & 0x7F);  // Mask out unused bits
    time->hours = bcd_to_bin(time_data[2] & 0x3F);    // Mask out 12/24h and AM/PM bits
    time->day = bcd_to_bin(time_data[4] & 0x3F);      // Mask out unused bits
    time->month = bcd_to_bin(time_data[5] & 0x1F);    // Mask out century bit
    time->year = bcd_to_bin(time_data[6]);
    
    return ESP_OK;
}

esp_err_t ds3231_set_time_struct(const wordclock_time_t *time)
{
    if (time == NULL) {
        ESP_LOGE(TAG, "Time structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate time values
    if (time->seconds > 59 || time->minutes > 59 || time->hours > 23 ||
        time->day < 1 || time->day > 31 || time->month < 1 || time->month > 12 ||
        time->year > 99) {
        ESP_LOGE(TAG, "Invalid time values");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert binary to BCD and write to DS3231
    uint8_t time_data[7];
    time_data[0] = bin_to_bcd(time->seconds);
    time_data[1] = bin_to_bcd(time->minutes);
    time_data[2] = bin_to_bcd(time->hours);    // 24-hour format
    time_data[3] = 1;                          // Day of week (not used)
    time_data[4] = bin_to_bcd(time->day);
    time_data[5] = bin_to_bcd(time->month);
    time_data[6] = bin_to_bcd(time->year);
    
    // Write all time data in one transaction
    uint8_t write_buffer[8];
    write_buffer[0] = DS3231_REG_SECONDS;  // Starting register
    memcpy(&write_buffer[1], time_data, 7); // Copy time data
    
    esp_err_t ret = i2c_master_transmit(ds3231_dev_handle, write_buffer, sizeof(write_buffer), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write time to DS3231: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "DS3231 time set: %02d:%02d:%02d %02d/%02d/20%02d",
             time->hours, time->minutes, time->seconds,
             time->day, time->month, time->year);
    
    return ESP_OK;
}

esp_err_t ds3231_get_temperature(ds3231_temp_t *temperature)
{
    if (temperature == NULL) {
        ESP_LOGE(TAG, "Temperature structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ds3231_dev_handle == NULL) {
        ESP_LOGE(TAG, "DS3231 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t temp_data[2];
    uint8_t reg_addr = DS3231_REG_TEMP_MSB;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_dev_handle, &reg_addr, 1, temp_data, 2, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature from DS3231: %s", esp_err_to_name(ret));
        temperature->valid = false;
        return ret;
    }
    
    // Convert temperature data
    int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
    temperature->temperature = (float)temp_raw / 256.0f;
    temperature->valid = true;
    
    return ESP_OK;
}

// BH1750 Light Sensor Functions
esp_err_t bh1750_init(void)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing BH1750 light sensor");
    return ESP_OK;
}

esp_err_t bh1750_write_command(uint8_t command)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    return i2c_master_transmit(bh1750_dev_handle, &command, 1, pdMS_TO_TICKS(1000));
}

esp_err_t bh1750_read_data(uint8_t *data, size_t len)
{
    if (bh1750_dev_handle == NULL) {
        ESP_LOGE(TAG, "BH1750 device handle not available");
        return ESP_ERR_INVALID_STATE;
    }
    
    return i2c_master_receive(bh1750_dev_handle, data, len, pdMS_TO_TICKS(1000));
}

