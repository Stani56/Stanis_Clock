/**
 * @file board_config.h
 * @brief Board-specific configuration for ESP32-S3 Word Clock
 *
 * This header provides board-specific configuration flags to support multiple
 * ESP32-S3 hardware variants. Use Kconfig or compile-time defines to select
 * the target board.
 *
 * Supported Boards:
 * - ESP32-S3-DevKitC-1 N16R8 (default) - External MAX98357A + microSD
 * - YB-ESP32-S3-AMP N8R2 - Integrated MAX98357A + microSD
 *
 * @date 2025-11-09
 * @author Claude Code (YB-AMP Migration - Phase 2)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Board Selection (Compile-time or Kconfig)
//=============================================================================

/**
 * Board selection defines
 *
 * Only ONE of these should be defined at compile time:
 * - CONFIG_BOARD_DEVKITC: ESP32-S3-DevKitC-1 with external peripherals
 * - CONFIG_BOARD_YB_AMP: YB-ESP32-S3-AMP with integrated peripherals
 *
 * If no board is defined, default to DevKitC-1 (current hardware)
 */

#if !defined(CONFIG_BOARD_DEVKITC) && !defined(CONFIG_BOARD_YB_AMP)
    #define CONFIG_BOARD_YB_AMP 1  // YB-ESP32-S3-AMP with integrated audio/SD
    #warning "No board selected, defaulting to YB-ESP32-S3-AMP"
#endif

//=============================================================================
// Board Capability Flags
//=============================================================================

#ifdef CONFIG_BOARD_DEVKITC
    // ESP32-S3-DevKitC-1 N16R8 with external peripherals
    #define BOARD_NAME                "ESP32-S3-DevKitC-1"
    #define BOARD_FLASH_SIZE_MB       16
    #define BOARD_PSRAM_SIZE_MB       8
    #define BOARD_AUDIO_INTEGRATED    0   // External MAX98357A modules
    #define BOARD_SD_INTEGRATED       0   // External microSD module
    #define BOARD_AUDIO_CHANNELS      1   // Mono (single MAX98357A)
    #define BOARD_HAS_STATUS_LED      0   // No onboard status LED
    #define BOARD_STATUS_LED_GPIO     -1  // N/A
#endif

#ifdef CONFIG_BOARD_YB_AMP
    // YB-ESP32-S3-AMP N8R2 with integrated audio and microSD
    #define BOARD_NAME                "YB-ESP32-S3-AMP"
    #define BOARD_FLASH_SIZE_MB       8
    #define BOARD_PSRAM_SIZE_MB       2
    #define BOARD_AUDIO_INTEGRATED    1   // Integrated MAX98357A amplifiers
    #define BOARD_SD_INTEGRATED       1   // Integrated microSD slot
    #define BOARD_AUDIO_CHANNELS      2   // Stereo (dual MAX98357A)
    #define BOARD_HAS_STATUS_LED      1   // Onboard status LED
    #define BOARD_STATUS_LED_GPIO     47  // GPIO 47
#endif

//=============================================================================
// GPIO Pin Assignments (Identical Between Boards)
//=============================================================================

/**
 * I2C Bus Configuration
 *
 * Both boards use identical I2C pin assignments:
 * - I2C_LEDS: Bus 0 for TLC59116 LED controllers (GPIO 8/9)
 * - I2C_SENSORS: Bus 1 for DS3231 RTC + BH1750 light sensor (GPIO 1/18)
 */

// I2C LEDs (TLC59116 controllers)
#define BOARD_I2C_LEDS_PORT           0
#define BOARD_I2C_LEDS_SDA_GPIO       8
#define BOARD_I2C_LEDS_SCL_GPIO       9

// I2C Sensors (DS3231 RTC, BH1750 light sensor)
#define BOARD_I2C_SENSORS_PORT        1
#define BOARD_I2C_SENSORS_SDA_GPIO    1

#ifdef CONFIG_BOARD_DEVKITC
    // DevKitC: GPIO 18 (ADC2_CH7, WiFi conflict - legacy configuration)
    #define BOARD_I2C_SENSORS_SCL_GPIO    18
#endif

#ifdef CONFIG_BOARD_YB_AMP
    // YB-AMP: GPIO 42 (WiFi-safe, no ADC conflict - recommended)
    #define BOARD_I2C_SENSORS_SCL_GPIO    42
#endif

/**
 * I2S Audio Configuration
 *
 * ✅ USER VERIFIED (2025-11-09): Pin assignments IDENTICAL between boards
 *
 * Both DevKitC and YB-AMP use GPIO 5/6/7 for I2S audio:
 * - GPIO 5: BCLK (Bit Clock)
 * - GPIO 6: LRCLK (Left/Right Clock / Word Select)
 * - GPIO 7: DIN/DOUT (Data Input/Output)
 *
 * DevKitC: Pins connect to external MAX98357A via jumper wires
 * YB-AMP: Pins hardwired to integrated MAX98357A amplifiers
 */

#define BOARD_I2S_BCLK_GPIO           5
#define BOARD_I2S_LRCLK_GPIO          6
#define BOARD_I2S_DOUT_GPIO           7
#define BOARD_I2S_SD_GPIO             -1  // Hardwired to VIN on both boards

/**
 * MicroSD Card Configuration (SPI)
 *
 * Both boards use identical SPI pin assignments:
 * - GPIO 10: CS (Chip Select)
 * - GPIO 11: MOSI (Data Out)
 * - GPIO 12: CLK (Clock)
 * - GPIO 13: MISO (Data In)
 *
 * DevKitC: Pins connect to external SD module via jumper wires
 * YB-AMP: Pins hardwired to integrated microSD slot
 */

#define BOARD_SD_CS_GPIO              10
#define BOARD_SD_MOSI_GPIO            11
#define BOARD_SD_CLK_GPIO             12
#define BOARD_SD_MISO_GPIO            13

/**
 * ADC Configuration
 *
 * Potentiometer for brightness control on GPIO 3 (ADC1_CH2, WiFi-safe)
 */

#define BOARD_ADC_POTENTIOMETER_GPIO  3
#define BOARD_ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_2  // ADC1_CH2

/**
 * Status LED Configuration
 *
 * External status LEDs for WiFi/NTP indicators (identical on both boards)
 */

#define BOARD_WIFI_STATUS_LED_GPIO    21
#define BOARD_NTP_STATUS_LED_GPIO     38

/**
 * Button Configuration
 *
 * Boot button on GPIO 0 (identical on both boards)
 */

#define BOARD_RESET_BUTTON_GPIO       0

//=============================================================================
// Board-Specific Configuration Macros
//=============================================================================

/**
 * Check if board has sufficient PSRAM for operation
 *
 * @return 1 if PSRAM capacity is sufficient, 0 otherwise
 */
#define BOARD_HAS_SUFFICIENT_PSRAM(required_mb) \
    (BOARD_PSRAM_SIZE_MB >= (required_mb))

/**
 * Check if board has sufficient Flash for operation
 *
 * @return 1 if Flash capacity is sufficient, 0 otherwise
 */
#define BOARD_HAS_SUFFICIENT_FLASH(required_mb) \
    (BOARD_FLASH_SIZE_MB >= (required_mb))

/**
 * Get audio configuration string for logging
 */
#define BOARD_AUDIO_CONFIG_STRING \
    (BOARD_AUDIO_INTEGRATED ? "Integrated" : "External")

/**
 * Get SD card configuration string for logging
 */
#define BOARD_SD_CONFIG_STRING \
    (BOARD_SD_INTEGRATED ? "Integrated" : "External")

//=============================================================================
// Runtime Board Information Functions
//=============================================================================

/**
 * @brief Get board name string
 * @return Constant string with board name
 */
static inline const char* board_get_name(void)
{
    return BOARD_NAME;
}

/**
 * @brief Get board flash size in MB
 * @return Flash size in megabytes
 */
static inline uint8_t board_get_flash_size_mb(void)
{
    return BOARD_FLASH_SIZE_MB;
}

/**
 * @brief Get board PSRAM size in MB
 * @return PSRAM size in megabytes
 */
static inline uint8_t board_get_psram_size_mb(void)
{
    return BOARD_PSRAM_SIZE_MB;
}

/**
 * @brief Check if audio is integrated
 * @return 1 if integrated, 0 if external
 */
static inline bool board_is_audio_integrated(void)
{
    return BOARD_AUDIO_INTEGRATED;
}

/**
 * @brief Check if microSD is integrated
 * @return 1 if integrated, 0 if external
 */
static inline bool board_is_sd_integrated(void)
{
    return BOARD_SD_INTEGRATED;
}

/**
 * @brief Get number of audio channels
 * @return Number of audio channels (1=mono, 2=stereo)
 */
static inline uint8_t board_get_audio_channels(void)
{
    return BOARD_AUDIO_CHANNELS;
}

/**
 * @brief Check if board has onboard status LED
 * @return 1 if available, 0 if not
 */
static inline bool board_has_status_led(void)
{
    return BOARD_HAS_STATUS_LED;
}

/**
 * @brief Get board status LED GPIO
 * @return GPIO number, or -1 if not available
 */
static inline int8_t board_get_status_led_gpio(void)
{
    return BOARD_STATUS_LED_GPIO;
}

#ifdef __cplusplus
}
#endif

#endif // BOARD_CONFIG_H
