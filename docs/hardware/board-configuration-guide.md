# Board Configuration Guide

**Date:** 2025-11-09
**Phase:** YB-AMP Migration - Phase 2
**Component:** board_config

---

## Overview

The board_config component provides a Hardware Abstraction Layer (HAL) for supporting multiple ESP32-S3 hardware variants with minimal code changes. This system allows the same firmware to run on different boards by selecting the appropriate board configuration at compile time.

---

## Supported Boards

### 1. ESP32-S3-DevKitC-1 N16R8 (Default)

**Module:** ESP32-S3-WROOM-1-N16R8
**Flash:** 16MB
**PSRAM:** 8MB Octal

**Peripherals:**
- **Audio:** External MAX98357A amplifier (mono) on GPIO 5/6/7
- **MicroSD:** External SPI module on GPIO 10/11/12/13
- **I2C LEDs:** TLC59116 controllers on GPIO 8/9
- **I2C Sensors:** DS3231 RTC + BH1750 on GPIO 1/18

**Status:** Current production hardware

---

### 2. YB-ESP32-S3-AMP N8R2 (Migration Target)

**Module:** ESP32-S3-WROOM-1-N8R2
**Flash:** 8MB
**PSRAM:** 2MB Octal

**Peripherals:**
- **Audio:** Integrated dual MAX98357A amplifiers (stereo) on GPIO 5/6/7
- **MicroSD:** Integrated SD card slot on GPIO 10/11/12/13
- **I2C LEDs:** TLC59116 controllers on GPIO 8/9
- **I2C Sensors:** DS3231 RTC + BH1750 on GPIO 1/42

**Status:** Migration in progress (Phase 2)

**Key Differences:**
- ✅ I2S pins identical (5/6/7)
- ✅ SD card pins identical (10/11/12/13)
- ⚠️ I2C Sensors SCL moved from GPIO 18 → GPIO 42 (WiFi-safe)
- ✅ Onboard status LED on GPIO 47
- ✅ Stereo audio (2 channels vs 1)

---

## Board Selection

### Compile-Time Selection

Edit [components/board_config/include/board_config.h](../../components/board_config/include/board_config.h) and uncomment the desired board:

```c
// Option 1: ESP32-S3-DevKitC-1 (current hardware)
#define CONFIG_BOARD_DEVKITC 1

// Option 2: YB-ESP32-S3-AMP (migration target)
//#define CONFIG_BOARD_YB_AMP 1
```

**Default Behavior:** If no board is selected, the system defaults to DevKitC-1 with a compile warning.

---

## GPIO Pin Mappings

### Identical Pins (No Changes Required)

| Peripheral | GPIOs | DevKitC | YB-AMP | Notes |
|------------|-------|---------|--------|-------|
| **I2S Audio** | 5, 6, 7 | ✅ | ✅ | **USER VERIFIED** - Identical pins |
| **MicroSD SPI** | 10, 11, 12, 13 | ✅ | ✅ | Identical pins |
| **I2C LEDs** | 8 (SDA), 9 (SCL) | ✅ | ✅ | Identical pins |
| **I2C Sensors SDA** | 1 | ✅ | ✅ | Identical pin |
| **Potentiometer** | 3 (ADC1_CH2) | ✅ | ✅ | WiFi-safe on both boards |
| **Status LEDs** | 21, 38 | ✅ | ✅ | External LEDs |
| **Reset Button** | 0 | ✅ | ✅ | Boot button |

### Board-Specific Pins

| Peripheral | DevKitC GPIO | YB-AMP GPIO | Reason |
|------------|--------------|-------------|--------|
| **I2C Sensors SCL** | 18 (ADC2_CH7) | 42 | GPIO 18 conflicts with WiFi (ADC2) |
| **Board Status LED** | N/A | 47 | YB-AMP has onboard LED |

---

## Configuration Flags

### Board Capability Flags

| Flag | DevKitC | YB-AMP | Description |
|------|---------|--------|-------------|
| `BOARD_NAME` | "ESP32-S3-DevKitC-1" | "YB-ESP32-S3-AMP" | Human-readable name |
| `BOARD_FLASH_SIZE_MB` | 16 | 8 | Flash capacity in MB |
| `BOARD_PSRAM_SIZE_MB` | 8 | 2 | PSRAM capacity in MB |
| `BOARD_AUDIO_INTEGRATED` | 0 | 1 | Audio is integrated (1) or external (0) |
| `BOARD_SD_INTEGRATED` | 0 | 1 | SD card is integrated (1) or external (0) |
| `BOARD_AUDIO_CHANNELS` | 1 | 2 | Audio channels (1=mono, 2=stereo) |
| `BOARD_HAS_STATUS_LED` | 0 | 1 | Board has onboard status LED |
| `BOARD_STATUS_LED_GPIO` | -1 | 47 | Onboard status LED GPIO (-1 if N/A) |

### GPIO Pin Defines

All GPIO pins are exposed via `BOARD_*_GPIO` defines:

```c
// I2S Audio (identical on both boards)
BOARD_I2S_BCLK_GPIO          // GPIO 5
BOARD_I2S_LRCLK_GPIO         // GPIO 6
BOARD_I2S_DOUT_GPIO          // GPIO 7

// MicroSD SPI (identical on both boards)
BOARD_SD_CS_GPIO             // GPIO 10
BOARD_SD_MOSI_GPIO           // GPIO 11
BOARD_SD_CLK_GPIO            // GPIO 12
BOARD_SD_MISO_GPIO           // GPIO 13

// I2C LEDs (identical on both boards)
BOARD_I2C_LEDS_SDA_GPIO      // GPIO 8
BOARD_I2C_LEDS_SCL_GPIO      // GPIO 9

// I2C Sensors (board-specific SCL)
BOARD_I2C_SENSORS_SDA_GPIO   // GPIO 1 (both boards)
BOARD_I2C_SENSORS_SCL_GPIO   // GPIO 18 (DevKitC) or GPIO 42 (YB-AMP)

// ADC (identical on both boards)
BOARD_ADC_POTENTIOMETER_GPIO // GPIO 3

// Status LEDs (identical on both boards)
BOARD_WIFI_STATUS_LED_GPIO   // GPIO 21
BOARD_NTP_STATUS_LED_GPIO    // GPIO 38

// Button (identical on both boards)
BOARD_RESET_BUTTON_GPIO      // GPIO 0
```

---

## Runtime API Functions

The board_config header provides inline functions for runtime board information:

### Board Information

```c
#include "board_config.h"

// Get board name
const char* name = board_get_name();
// Returns: "ESP32-S3-DevKitC-1" or "YB-ESP32-S3-AMP"

// Get flash size
uint8_t flash_mb = board_get_flash_size_mb();
// Returns: 16 (DevKitC) or 8 (YB-AMP)

// Get PSRAM size
uint8_t psram_mb = board_get_psram_size_mb();
// Returns: 8 (DevKitC) or 2 (YB-AMP)
```

### Audio Configuration

```c
// Check if audio is integrated
bool integrated = board_is_audio_integrated();
// Returns: false (DevKitC) or true (YB-AMP)

// Get audio channels
uint8_t channels = board_get_audio_channels();
// Returns: 1 (DevKitC mono) or 2 (YB-AMP stereo)
```

### SD Card Configuration

```c
// Check if SD card is integrated
bool integrated = board_is_sd_integrated();
// Returns: false (DevKitC) or true (YB-AMP)
```

### Status LED

```c
// Check if board has onboard status LED
bool has_led = board_has_status_led();
// Returns: false (DevKitC) or true (YB-AMP)

// Get status LED GPIO
int8_t gpio = board_get_status_led_gpio();
// Returns: -1 (DevKitC) or 47 (YB-AMP)
```

---

## Migration Example: Audio Manager

### Before (Hardcoded for DevKitC)

```c
// audio_manager.c
#define I2S_GPIO_BCLK   5
#define I2S_GPIO_LRCLK  6
#define I2S_GPIO_DOUT   7
#define AUDIO_CHANNELS  1  // Mono

void audio_init(void) {
    // Configure for external MAX98357A...
}
```

### After (Board-Agnostic)

```c
// audio_manager.c
#include "board_config.h"

void audio_init(void) {
    ESP_LOGI(TAG, "Initializing %s audio (%d channels)",
             board_is_audio_integrated() ? "integrated" : "external",
             board_get_audio_channels());

    i2s_config.channel_format = (board_get_audio_channels() == 2) ?
                                 I2S_CHANNEL_FMT_RIGHT_LEFT :
                                 I2S_CHANNEL_FMT_ONLY_LEFT;

    // Use BOARD_I2S_* defines for GPIO configuration
    i2s_pin_config.bck_io_num = BOARD_I2S_BCLK_GPIO;
    i2s_pin_config.ws_io_num = BOARD_I2S_LRCLK_GPIO;
    i2s_pin_config.data_out_num = BOARD_I2S_DOUT_GPIO;

    // ... rest of initialization
}
```

---

## Migration Checklist

### Phase 1: Documentation (✅ Complete)

- [x] Research YB-AMP specifications
- [x] Document GPIO pin mappings
- [x] Analyze GPIO conflicts
- [x] User verification of I2S pins
- [x] Create migration summary

### Phase 2: Pre-Migration Testing (✅ In Progress)

- [x] Add PSRAM usage logging
- [x] Create board_config component
- [ ] Test PSRAM logging (USER ACTION REQUIRED)
- [ ] Verify PSRAM usage < 2MB over 24 hours

### Phase 3: Code Migration (When YB-AMP Arrives)

- [ ] Update audio_manager to use BOARD_I2S_* defines
- [ ] Update sdcard_manager to use BOARD_SD_* defines
- [ ] Update i2c_devices to use BOARD_I2C_* defines
- [ ] Add YB-AMP board selection to board_config.h
- [ ] Build and flash test firmware
- [ ] Verify all peripherals working

### Phase 4: Production Deployment

- [ ] 24-hour stability test
- [ ] Update CLAUDE.md with YB-AMP configuration
- [ ] Create GitHub release for YB-AMP support
- [ ] Update README.md with board selection instructions

---

## Testing PSRAM Usage (User Action Required)

The firmware now includes PSRAM monitoring. When you power on the device, you'll see:

**Boot Logs:**
```
I (1234) wordclock: 📊 PSRAM Total: 8388608 bytes (8192 KB, 8.0 MB)
I (1235) wordclock: 📊 PSRAM Used: 524288 bytes (512 KB, 0.5 MB)
I (1236) wordclock: 📊 PSRAM Free: 7864320 bytes (7680 KB, 7.5 MB)
I (1237) wordclock: 📊 PSRAM Usage: 6.3%
I (1238) wordclock: ✅ PSRAM usage compatible with YB-AMP (< 2MB)
```

**Periodic Logs (every ~100 seconds):**
```
I (123456) wordclock: 📊 PSRAM: Used 0.52 MB / 8.00 MB (6.5%), Peak 0.75 MB
```

**Action Required:**
1. Power on the device and let it run for 24 hours
2. Check serial logs for peak PSRAM usage
3. If peak usage < 2MB: ✅ Safe to migrate to YB-AMP
4. If peak usage > 2MB: ⚠️ Optimization required before migration

---

## Related Documentation

- [YB-ESP32-S3-AMP-Hardware-Specifications.md](YB-ESP32-S3-AMP-Hardware-Specifications.md)
- [YB-ESP32-S3-AMP-GPIO-Conflict-Analysis.md](YB-ESP32-S3-AMP-GPIO-Conflict-Analysis.md)
- [YB-ESP32-S3-AMP-Migration-Summary.md](YB-ESP32-S3-AMP-Migration-Summary.md)
- [CLAUDE.md](../../CLAUDE.md) - Current hardware configuration

---

**Document Version:** 1.0
**Last Updated:** 2025-11-09
**Status:** Phase 2 - Pre-migration testing
