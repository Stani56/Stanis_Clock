# ESP32 to ESP32-S3 Migration Analysis
## YelloByte YB-ESP32-S3-AMP Board - Complete Change Document

**Date:** 2025-11-02
**Target Hardware:** YelloByte YB-ESP32-S3-AMP (ESP32-S3-WROOM-1-N8R2)
**Current Hardware:** ESP32-PICO-KIT v4.1 (ESP32-PICO-D4)
**Purpose:** Comprehensive analysis of ALL required changes (no code modifications yet)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Hardware Changes](#hardware-changes)
3. [Component-by-Component Analysis](#component-by-component-analysis)
4. [Configuration Changes](#configuration-changes)
5. [External Storage Decision](#external-storage-decision)
6. [Build System Changes](#build-system-changes)
7. [Testing Strategy](#testing-strategy)
8. [Risk Assessment](#risk-assessment)
9. [Migration Timeline](#migration-timeline)
10. [Rollback Plan](#rollback-plan)

---

## Executive Summary

### Migration Scope

**GPIO Pin Changes:** 8 files (~15 lines of code)
**Filesystem Migration:** 3 components (external_flash → sdcard_manager, filesystem_manager → FatFS API)
**Total Estimated Migration Time:** 4-6 hours (1-2h GPIO + 2-4h filesystem)
**Risk Level:** MEDIUM (GPIO changes are low risk, filesystem migration requires careful testing)

### Key Benefits

✅ **Solves WiFi+MQTT+I2S crash** (primary goal)
✅ **50% faster CPU** (240 MHz vs 160 MHz)
✅ **PSRAM support** (2MB for audio buffering)
✅ **Built-in audio hardware** (2× MAX98357A amplifiers)
✅ **Built-in microSD card** (better than W25Q64)
✅ **Native USB** (easier debugging)

### Critical Requirements

⚠️ **PSRAM configuration must be enabled** (board has 2MB)
⚠️ **Target must be changed from esp32 → esp32s3**
⚠️ **Flash size must be increased to 8MB** (board has 8MB vs current 4MB)
✅ **External storage: microSD card (FINALIZED)** - Migrate from W25Q64 + LittleFS to microSD + FatFS

---

## Hardware Changes

### Current Hardware: ESP32-PICO-KIT v4.1

| Component | GPIO Pins | Notes |
|-----------|-----------|-------|
| I2C Bus 0 (LEDs) | SDA=25, SCL=26 | 10× TLC59116 LED controllers |
| I2C Bus 1 (Sensors) | SDA=18, SCL=19 | DS3231 RTC + BH1750 light sensor |
| ADC Potentiometer | GPIO 34 (ADC1_CH6) | Brightness control |
| Status LED (WiFi) | GPIO 21 | WiFi connection indicator |
| Status LED (NTP) | GPIO 22 | NTP sync indicator |
| Reset Button | GPIO 5 | WiFi credentials clear |
| Audio I2S (External MAX98357A) | DOUT=32, BCLK=33, LRCLK=27 | External amplifier wiring |
| External Flash W25Q64 (Optional) | MISO=12, MOSI=13, CLK=14, CS=15 | 8MB SPI flash |
| Flash Size | 4MB | ESP32-PICO-D4 module |
| PSRAM | None | Not available on ESP32-PICO-D4 |

### New Hardware: YelloByte YB-ESP32-S3-AMP

| Component | GPIO Pins | Notes |
|-----------|-----------|-------|
| I2C Bus 0 (LEDs) | **SDA=8, SCL=9** | ✅ Default I2C on YB board |
| I2C Bus 1 (Sensors) | **SDA=2, SCL=4** | ✅ Available GPIOs |
| ADC Potentiometer | **GPIO 1 (ADC1_CH0)** | ✅ ADC1 works with WiFi |
| Status LED (WiFi) | **GPIO 21** | ✅ No change needed |
| Status LED (NTP) | **GPIO 18** | ⚠️ GPIO 22 conflicts with audio |
| Reset Button | **GPIO 0** | ✅ Boot button (common on ESP32-S3) |
| Audio I2S (**Built-in** 2× MAX98357A) | **BCLK=5, LRCLK=6, DIN=7** | ✅ **Pre-wired, not exposed on headers** |
| External Flash W25Q64 | **⚠️ CONFLICT** | ❌ GPIO 10/11/12/13 used by microSD |
| microSD Card Slot (**Built-in**) | MISO=13, MOSI=11, CLK=12, CS=10 | ✅ **Pre-wired, not exposed on headers** |
| Flash Size | **8MB** | ESP32-S3-WROOM-1-N8R2 module |
| PSRAM | **2MB** | Octal PSRAM (critical for audio) |
| Status LED (Onboard) | GPIO 47 | Can be used but has LED |
| USB | GPIO 19/20 (Rev.3 only) | Native USB, do not use for GPIO |

### GPIO Pin Mapping Summary

| Function | ESP32 GPIO | ESP32-S3 GPIO | Change Required |
|----------|------------|---------------|-----------------|
| **I2C Bus 0 SDA (LEDs)** | 25 | **8** | YES |
| **I2C Bus 0 SCL (LEDs)** | 26 | **9** | YES |
| **I2C Bus 1 SDA (Sensors)** | 18 | **2** | YES |
| **I2C Bus 1 SCL (Sensors)** | 19 | **4** | YES |
| **Potentiometer (ADC)** | 34 (ADC1_CH6) | **1 (ADC1_CH0)** | YES |
| **WiFi Status LED** | 21 | **21** | NO |
| **NTP Status LED** | 22 | **18** | YES |
| **Reset Button** | 5 | **0** | YES |
| **I2S BCLK (Audio)** | 33 | **5** | YES (built-in) |
| **I2S LRCLK (Audio)** | 27 | **6** | YES (built-in) |
| **I2S DOUT/DIN (Audio)** | 32 | **7** | YES (built-in) |

**Total GPIO Changes:** 10 pin numbers

---

## Component-by-Component Analysis

### 1. I2C Devices Component

**File:** `components/i2c_devices/include/i2c_devices.h`

**Current Configuration:**
```c
// Lines 14-21
#define I2C_LEDS_MASTER_PORT           0
#define I2C_SENSORS_MASTER_PORT        1
#define I2C_LEDS_MASTER_SDA_IO         25
#define I2C_LEDS_MASTER_SCL_IO         26
#define I2C_SENSORS_MASTER_SDA_IO      18
#define I2C_SENSORS_MASTER_SCL_IO      19
#define I2C_LEDS_MASTER_FREQ_HZ        100000
#define I2C_SENSORS_MASTER_FREQ_HZ     100000
```

**Required Changes:**
```c
// ESP32-S3 YB-ESP32-S3-AMP Configuration
#define I2C_LEDS_MASTER_PORT           0
#define I2C_SENSORS_MASTER_PORT        1
#define I2C_LEDS_MASTER_SDA_IO         8      // Changed: 25 → 8 (default I2C on YB board)
#define I2C_LEDS_MASTER_SCL_IO         9      // Changed: 26 → 9 (default I2C on YB board)
#define I2C_SENSORS_MASTER_SDA_IO      2      // Changed: 18 → 2 (available GPIO)
#define I2C_SENSORS_MASTER_SCL_IO      4      // Changed: 19 → 4 (available GPIO)
#define I2C_LEDS_MASTER_FREQ_HZ        100000 // No change
#define I2C_SENSORS_MASTER_FREQ_HZ     100000 // No change
```

**Lines to Change:** 4 (lines 16-19)

**Rationale:**
- GPIO 8/9 are the default I2C pins on YB-ESP32-S3-AMP board
- GPIO 2/4 are available and have no conflicts
- 100kHz I2C clock is conservative and works well on ESP32-S3
- Two separate I2C buses maintain isolation between heavy LED traffic and sensors

**No Code Logic Changes:** I2C API is identical on ESP32-S3

---

### 2. ADC Manager Component

**File:** `components/adc_manager/include/adc_manager.h`

**Current Configuration:**
```c
// Lines 16-20
#define ADC_POTENTIOMETER_UNIT ADC_UNIT_1
#define ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_6     // GPIO 34 (ADC1_CH6)
#define ADC_POTENTIOMETER_ATTEN ADC_ATTEN_DB_12     // 3.3V range (0-3300mV)
#define ADC_POTENTIOMETER_BITWIDTH ADC_BITWIDTH_12  // 12-bit resolution (0-4095)
```

**Required Changes:**
```c
// ESP32-S3 YB-ESP32-S3-AMP Configuration
#define ADC_POTENTIOMETER_UNIT ADC_UNIT_1           // No change (ADC1 required for WiFi)
#define ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_0     // Changed: ADC_CHANNEL_6 → ADC_CHANNEL_0 (GPIO 1)
#define ADC_POTENTIOMETER_ATTEN ADC_ATTEN_DB_12     // No change
#define ADC_POTENTIOMETER_BITWIDTH ADC_BITWIDTH_12  // No change
```

**Lines to Change:** 1 (line 18)

**Rationale:**
- GPIO 1 = ADC1_CHANNEL_0 on ESP32-S3
- ADC1 required because ADC2 cannot be used with WiFi active (same as ESP32)
- Same attenuation and bit width work on ESP32-S3
- ESP32-S3 has improved ADC linearity (better performance)

**ADC Channel Mapping:**
| ESP32 | ESP32-S3 |
|-------|----------|
| GPIO 34 = ADC1_CH6 | GPIO 1 = ADC1_CH0 |

**No Code Logic Changes:** ADC API is identical on ESP32-S3

---

### 3. Button Manager Component

**File:** `components/button_manager/include/button_manager.h`

**Current Configuration:**
```c
// Lines 15-18
#define RESET_BUTTON_PIN            GPIO_NUM_5      // Reset button on GPIO 5
#define BUTTON_LONG_PRESS_MS        5000            // 5 seconds for long press
#define BUTTON_CHECK_INTERVAL_MS    50              // Check button every 50ms
```

**Required Changes:**
```c
// ESP32-S3 YB-ESP32-S3-AMP Configuration
#define RESET_BUTTON_PIN            GPIO_NUM_0      // Changed: GPIO_NUM_5 → GPIO_NUM_0 (Boot button)
#define BUTTON_LONG_PRESS_MS        5000            // No change
#define BUTTON_CHECK_INTERVAL_MS    50              // No change
```

**Lines to Change:** 1 (line 16)

**Rationale:**
- GPIO 0 is the standard boot button on ESP32-S3 boards
- GPIO 0 is a strapping pin but safe to use as input with pull-up
- Same button handling logic works on ESP32-S3

**Note:** GPIO 0 has special boot behavior:
- If held LOW during boot → enters download mode
- Normal operation: Can be used as regular button with internal pull-up

**No Code Logic Changes:** GPIO API is identical on ESP32-S3

---

### 4. Status LED Manager Component

**File:** `components/status_led_manager/include/status_led_manager.h`

**Current Configuration:**
```c
// Lines 14-16
#define WIFI_STATUS_LED_PIN     GPIO_NUM_21
#define NTP_STATUS_LED_PIN      GPIO_NUM_22
```

**Required Changes:**
```c
// ESP32-S3 YB-ESP32-S3-AMP Configuration
#define WIFI_STATUS_LED_PIN     GPIO_NUM_21  // No change (GPIO 21 available)
#define NTP_STATUS_LED_PIN      GPIO_NUM_18  // Changed: GPIO_NUM_22 → GPIO_NUM_18
```

**Lines to Change:** 1 (line 16)

**Rationale:**
- GPIO 21 is available and has no conflicts on YB board
- GPIO 22 would conflict with audio on current ESP32 design, but GPIO 18 is free
- GPIO 18 is available on ESP32-S3 YB board

**Alternative Options:**
- Could use GPIO 47 (has onboard status LED) as additional indicator
- Could use GPIO 14-17 range if needed

**No Code Logic Changes:** GPIO API is identical on ESP32-S3

---

### 5. Audio Manager Component

**File:** `components/audio_manager/include/audio_manager.h`

**Current Configuration:**
```c
// Lines 29-43
#define AUDIO_SAMPLE_RATE       16000  // 16 kHz (matches Chimes_System)
#define AUDIO_BITS_PER_SAMPLE   16     // 16-bit
#define AUDIO_CHANNELS          1      // Mono
#define AUDIO_DMA_BUF_COUNT     8      // Number of DMA buffers
#define AUDIO_DMA_BUF_LEN       256    // Samples per DMA buffer (matches Chimes_System)

// GPIO Pin Definitions for I2S
#define I2S_GPIO_DOUT           32     // I2S Data Out (DIN on MAX98357A)
#define I2S_GPIO_BCLK           33     // I2S Bit Clock
#define I2S_GPIO_LRCLK          27     // I2S Left/Right Clock (Word Select)
#define I2S_GPIO_SD             -1     // SD pin: -1=hardwired to VIN, or GPIO number
```

**Required Changes:**
```c
// ESP32-S3 YB-ESP32-S3-AMP Configuration
#define AUDIO_SAMPLE_RATE       16000  // No change
#define AUDIO_BITS_PER_SAMPLE   16     // No change
#define AUDIO_CHANNELS          1      // No change
#define AUDIO_DMA_BUF_COUNT     8      // No change (may increase with PSRAM available)
#define AUDIO_DMA_BUF_LEN       256    // No change (may increase with PSRAM available)

// GPIO Pin Definitions for I2S (Built-in MAX98357A on YB board)
#define I2S_GPIO_DOUT           7      // Changed: 32 → 7 (DIN on built-in MAX98357A)
#define I2S_GPIO_BCLK           5      // Changed: 33 → 5 (Bit Clock)
#define I2S_GPIO_LRCLK          6      // Changed: 27 → 6 (Left/Right Clock)
#define I2S_GPIO_SD             -1     // No change (hardwired on YB board)
```

**Lines to Change:** 3 (lines 40-42)

**Rationale:**
- YB-ESP32-S3-AMP board has **2× MAX98357A amplifiers built-in**
- GPIO 5/6/7 are pre-wired to amplifiers and **NOT exposed on board headers**
- No external wiring needed - just connect speaker to onboard terminal blocks
- Same audio parameters (16kHz, 16-bit mono) work on ESP32-S3

**Important Notes:**
- These GPIO pins (5/6/7) are **NOT available for other uses** on YB board
- Stereo capability available (2× MAX98357A) - future expansion possible
- Current project uses mono, so only one amplifier will be active

**No Code Logic Changes:** I2S API is identical on ESP32-S3

---

### 6. External Flash Component

**File:** `components/external_flash/include/external_flash.h`

**Current Configuration:**
```c
// Lines 6-10 (from file header comment)
// GPIO 13: MOSI (Master Out, Slave In)
// GPIO 12: MISO (Master In, Slave Out)
// GPIO 14: SCK  (Serial Clock)
// GPIO 15: CS   (Chip Select - has internal pull-up)
```

**⚠️ CRITICAL CONFLICT:** GPIO 10/11/12/13 are used by **built-in microSD card** on YB board!

**Three Options:**

#### **Option A: Use Built-in microSD Card Instead of W25Q64 (RECOMMENDED)**

**Changes Required:**
- Create new component: `components/sdcard_manager/`
- Migrate from LittleFS → FatFS (ESP-IDF has built-in support)
- Update `filesystem_manager` to use FatFS API
- Mount point: `/sdcard` or keep `/storage`

**Benefits:**
- ✅ No GPIO conflicts (uses GPIO 10/11/12/13 already on board)
- ✅ Much larger capacity (8GB+ vs 8MB = 1000× more!)
- ✅ Easy file management (remove SD card, copy files on PC)
- ✅ No external wiring needed
- ✅ Standard filesystem (FatFS) with excellent ESP-IDF support

**Drawbacks:**
- ⚠️ Code changes needed (LittleFS → FatFS API migration)
- ⚠️ Filesystem API differences (similar but not identical)
- ⚠️ SD card can be removed (handle gracefully)

**Estimated Work:** 2-4 hours (filesystem API migration)

---

#### **Option B: Use Alternative SPI Pins for W25Q64**

**Changes Required:**
```c
// ESP32-S3 YB-ESP32-S3-AMP - Alternative HSPI pins
// Use second SPI bus (HSPI/SPI3_HOST) instead of FSPI (which is used by microSD)
#define EXTERNAL_FLASH_MISO    GPIO_NUM_14  // Changed: 12 → 14
#define EXTERNAL_FLASH_MOSI    GPIO_NUM_15  // Changed: 13 → 15
#define EXTERNAL_FLASH_CLK     GPIO_NUM_16  // Changed: 14 → 16
#define EXTERNAL_FLASH_CS      GPIO_NUM_17  // Changed: 15 → 17
```

**Benefits:**
- ✅ Keep existing LittleFS code unchanged
- ✅ Keep W25Q64 hardware (if already installed)
- ✅ No filesystem API changes

**Drawbacks:**
- ⚠️ Requires external W25Q64 chip wiring
- ⚠️ Limited to 8MB storage
- ⚠️ Uses 4 additional GPIOs (14/15/16/17)

**Estimated Work:** 30 minutes (GPIO pin changes only)

---

#### **Option C: Hybrid Approach (microSD + W25Q64)**

Use both storage solutions:
- microSD for large chime files (GB of audio)
- W25Q64 for configuration/logs (8MB, fast access)

**Estimated Work:** 4-6 hours (implement both drivers)

---

**Recommendation:** **Option A** - Use built-in microSD card

**Rationale:**
- Better capacity (1000× more storage)
- Easier file management
- No GPIO conflicts
- Standard filesystem (FatFS)
- Worth the 2-4 hours of filesystem API migration

---

### ✅ DECISION FINALIZED: microSD Storage (Option A)

**Date:** 2025-11-03
**Decision:** Use built-in microSD card storage, migrate from W25Q64 + LittleFS to microSD + FatFS

**Key Changes Required:**
1. **Replace external_flash component** with sdcard_manager component
2. **Migrate filesystem_manager** from LittleFS to FatFS API
3. **Update CMakeLists.txt** to include FatFS components
4. **Mount point:** Keep `/storage` for API compatibility
5. **File operations:** Migrate from LittleFS API to POSIX/FatFS API

**Benefits Confirmed:**
- ✅ 1000× more storage capacity (8GB+ vs 8MB)
- ✅ No GPIO conflicts with YB board design
- ✅ Easy file management (removable SD card)
- ✅ Standard ESP-IDF FatFS support (well-tested)
- ✅ Future-proof for audio expansion

**Components to Update:**
- `components/external_flash/` → Archive (not used on ESP32-S3)
- `components/filesystem_manager/` → Migrate to FatFS
- `main/wordclock.c` → Update initialization calls
- New: `components/sdcard_manager/` → Handle SD card init

**Estimated Migration Time:** 2-4 hours for filesystem API changes

**Testing Priority:** High - Must verify SD card detection, mount, read/write before audio

---

### 7. Filesystem Manager Component

**File:** `components/filesystem_manager/`

**Current Implementation:**
- Uses LittleFS on W25Q64 external flash
- Mount point: `/storage`
- Directories: `/storage/chimes`, `/storage/config`

**Required Changes (if using Option A - microSD):**

**Step 1: Replace LittleFS with FatFS**
```c
// Old: LittleFS
#include "esp_littlefs.h"

// New: FatFS
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
```

**Step 2: Update filesystem_manager.c initialization**
```c
// Old: LittleFS mount
esp_vfs_littlefs_conf_t conf = {
    .base_path = "/storage",
    .partition = partition,
    .format_if_mount_failed = true
};
esp_vfs_littlefs_register(&conf);

// New: FatFS mount
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
esp_vfs_fat_sdmmc_mount("/storage", &host, &slot_config, &mount_config, &card);
```

**Step 3: Update file operations**
- `filesystem_write_file()` - FatFS uses same fopen/fwrite/fclose
- `filesystem_read_file()` - FatFS uses same fopen/fread/fclose
- `filesystem_list_files()` - FatFS uses opendir/readdir/closedir (compatible)

**API Compatibility:** ~90% compatible (both use POSIX-like APIs)

**Estimated Changes:** ~100 lines across 3 functions

---

### 8. WiFi Manager, MQTT Manager, NTP Manager

**Files:**
- `components/wifi_manager/`
- `components/mqtt_manager/`
- `components/ntp_manager/`

**Required Changes:** **NONE**

**Rationale:**
- WiFi API is identical on ESP32-S3
- MQTT library is platform-independent
- NTP client is platform-independent
- No GPIO pins involved

**Verification Needed:**
- Test WiFi + MQTT + I2S concurrent operation
- Verify WiFi power save mode works (CONFIG_WIFI_PS_NONE already set)
- Test MQTT keepalive with I2S audio active

---

### 9. All Other Components

**Components with NO changes needed:**
- `wordclock_display` - Display logic (no hardware dependency)
- `wordclock_time` - Time calculation (no hardware dependency)
- `transition_manager` - Animation logic (no hardware dependency)
- `brightness_config` - Configuration storage (no hardware dependency)
- `nvs_manager` - Non-volatile storage (ESP32-S3 compatible)
- `thread_safety` - Mutex/synchronization (ESP32-S3 compatible)
- `error_log_manager` - Error logging (ESP32-S3 compatible)
- `led_validation` - LED validation (uses I2C, already covered)
- `mqtt_discovery` - Home Assistant integration (no hardware dependency)
- `web_server` - WiFi config portal (ESP32-S3 compatible)
- `light_sensor` - BH1750 sensor (uses I2C, already covered)

**Total:** 11 components with ZERO changes needed

---

## Configuration Changes

### 1. sdkconfig Changes

**File:** `sdkconfig`

**Current Configuration:**
```
CONFIG_IDF_TARGET="esp32"
CONFIG_IDF_TARGET_ESP32=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
CONFIG_I2S_ISR_IRAM_SAFE=y
```

**Required Changes:**
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_IDF_TARGET_ESP32S3=y
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="8MB"
CONFIG_I2S_ISR_IRAM_SAFE=y

# NEW: PSRAM Configuration (CRITICAL for WiFi + I2S)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_SPIRAM_MODE_OCT=y

# NEW: Memory Configuration
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_SIZE=2097152
```

**How to Change:**
```bash
# Option 1: Interactive menuconfig
idf.py menuconfig
# → Component config → ESP-IDF Target → esp32s3
# → Serial flasher config → Flash size → 8MB
# → Component config → ESP PSRAM → Enable

# Option 2: Delete sdkconfig and regenerate
rm sdkconfig
idf.py set-target esp32s3
idf.py menuconfig  # Configure PSRAM and flash size
```

**Critical Settings:**

| Setting | Current (ESP32) | New (ESP32-S3) | Purpose |
|---------|-----------------|----------------|---------|
| Target | esp32 | esp32s3 | MCU selection |
| Flash Size | 4MB | 8MB | YB board has 8MB |
| PSRAM | Not available | 2MB (enabled) | **Required for WiFi+I2S** |
| I2S ISR IRAM Safe | Enabled | Enabled | Keep enabled |
| FreeRTOS Hz | 1000 | 1000 | No change |

---

### 2. CMakeLists.txt Changes

**File:** Root `CMakeLists.txt`

**Current:**
```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wordclock)
```

**Required Changes:** **NONE**

**Note:** Target is set via `idf.py set-target esp32s3` (not in CMakeLists.txt)

---

### 3. partitions.csv Changes

**File:** `partitions.csv`

**Current:**
```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x280000,  # 2.5MB app
storage,  data, fat,     0x290000, 0x160000,  # 1.5MB storage
```

**Option A: Keep Same Partition Layout (4MB total)**
```csv
# No changes needed - same layout works on 8MB flash
# Uses only first 4MB of 8MB flash (conservative approach)
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x280000,  # 2.5MB app
storage,  data, fat,     0x290000, 0x160000,  # 1.5MB storage (unused if using microSD)
```

**Option B: Expand Storage Partition (use full 8MB)**
```csv
# Expand storage partition to use remaining 4MB
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x280000,  # 2.5MB app
storage,  data, fat,     0x290000, 0x560000,  # 5.5MB storage (expanded)
```

**Option C: Add OTA Partition for Future Updates**
```csv
# Two app partitions for OTA (Over-The-Air) updates
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,
phy_init, data, phy,     0x11000, 0x1000,
factory,  app,  ota_0,   0x20000, 0x280000,  # 2.5MB app slot 0
ota_1,    app,  ota_1,   0x2A0000, 0x280000, # 2.5MB app slot 1
storage,  data, fat,     0x520000, 0x2E0000, # 3MB storage
```

**Recommendation:** **Option A** initially (conservative), then **Option C** for production (OTA support)

**Note:** If using microSD card, `storage` partition is unused (can be removed or repurposed)

---

## External Storage Decision ✅ FINALIZED: microSD Card

**Decision Date:** 2025-11-03
**Selected Option:** Built-in microSD card with FatFS filesystem

### Comparison Matrix (For Reference)

| Feature | W25Q64 SPI Flash | microSD Card (Built-in) |
|---------|------------------|-------------------------|
| **Capacity** | 8 MB | 8 GB - 128 GB (1000-16000× more) |
| **Cost** | ~$1-2 (external chip) | ~$5-15 (SD card) |
| **Wiring** | Requires 4 GPIOs + wiring | ✅ Built-in, no wiring |
| **GPIO Conflict** | ⚠️ YES (needs alt pins) | ✅ NO (already on board) |
| **File Management** | Difficult (requires flash tool) | ✅ Easy (pop out, copy files) |
| **Filesystem** | LittleFS (current code) | FatFS (code changes needed) |
| **Reliability** | High (no moving parts) | Medium (SD card can fail) |
| **Speed** | 10 MHz SPI (~1 MB/s) | SDMMC 4-bit (~20 MB/s) |
| **Code Changes** | Minimal (4 GPIO pins) | Moderate (~100 lines) |
| **Future Expansion** | Limited (8 MB max) | ✅ Excellent (add more files) |
| **Removable** | No | Yes (feature or risk?) |

### Recommendation: **Use Built-in microSD Card**

**Rationale:**
1. **1000× more storage** - Room for hundreds of chime audio files
2. **No GPIO conflicts** - Uses pins already allocated on YB board
3. **Easy file management** - Remove SD card, copy MP3/WAV files, reinsert
4. **Built-in hardware** - No external wiring or soldering
5. **Standard filesystem** - FatFS is widely used and well-supported
6. **Worth the code migration** - 100 lines of code change vs huge benefits

**Migration Effort:** 2-4 hours (filesystem API changes)

---

## Build System Changes

### Step-by-Step Build Migration

#### Step 1: Set Target to ESP32-S3
```bash
cd /home/tanihp/esp_projects/Stanis_Clock
idf.py set-target esp32s3
```

**Effect:**
- Regenerates sdkconfig for ESP32-S3
- Updates build system files
- May lose custom configurations (backup sdkconfig first!)

#### Step 2: Configure PSRAM and Flash
```bash
idf.py menuconfig
```

**Settings to Configure:**
1. **Serial flasher config → Flash size → 8MB**
2. **Component config → ESP PSRAM:**
   - ☑ Support for external SPI-connected RAM
   - Mode: Octal Mode PSRAM
   - Size: 2MB
   - ☑ Initialize SPI RAM when booting
   - ☑ Try to allocate memories of WiFi and LWIP in SPIRAM firstly

3. **Component config → ESP32S3-Specific:**
   - ☑ Support for external, SPI-connected RAM

4. **Component config → FreeRTOS:**
   - Tick rate (Hz): 1000 (keep current)

5. **Component config → I2S Configuration:**
   - ☑ Place I2S ISR function into IRAM (already set)

#### Step 3: Backup Old Configuration (Optional)
```bash
cp sdkconfig sdkconfig.esp32.backup
```

#### Step 4: Clean Build
```bash
idf.py fullclean
idf.py build
```

**Expected Build Time:** 2-5 minutes (first build on ESP32-S3)

#### Step 5: Flash and Test
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Testing Strategy

### Phase 1: Basic Hardware Verification (30 minutes)

**Test Order:**
1. ✅ **Power-on and boot** - Verify ESP32-S3 boots successfully
2. ✅ **Serial console** - Check boot logs, no errors
3. ✅ **WiFi connection** - Connect to AP, verify IP address
4. ✅ **NTP sync** - Time synchronization works
5. ✅ **MQTT connection** - HiveMQ Cloud connection established

**Expected Results:**
- Boot time: <5 seconds
- WiFi connects within 10 seconds
- NTP syncs within 5 seconds
- MQTT connects within 10 seconds

**Success Criteria:**
- No crashes or reboots
- All network services online
- Serial logs clean (no errors)

---

### Phase 2: I2C Bus Verification (45 minutes)

**Test Order:**
1. ✅ **I2C Bus 0 scan** - Detect 10× TLC59116 controllers (0x60-0x6A)
2. ✅ **I2C Bus 1 scan** - Detect DS3231 (0x68) + BH1750 (0x23)
3. ✅ **LED initialization** - All TLC59116 devices initialize
4. ✅ **RTC read** - Read time from DS3231
5. ✅ **Light sensor read** - Read lux value from BH1750

**Expected Results:**
- All 10 TLC59116 controllers detected
- DS3231 responds with valid time
- BH1750 responds with lux value (0-65535)

**Success Criteria:**
- No I2C timeouts
- All device addresses respond
- Read values are reasonable

---

### Phase 3: Display Verification (30 minutes)

**Test Order:**
1. ✅ **LED matrix test** - Light up all 160 LEDs individually
2. ✅ **German word display** - Display current time in German
3. ✅ **Indicator LEDs** - Test 4 minute indicator LEDs
4. ✅ **Brightness control** - Test potentiometer (ADC)
5. ✅ **Transitions** - Test smooth LED animations

**Expected Results:**
- All LEDs light up correctly
- German time display matches RTC time
- Potentiometer changes brightness (5-80 PWM)
- Transitions animate smoothly (20Hz update rate)

**Success Criteria:**
- No stuck LEDs
- No incorrect word patterns
- Smooth transitions, no flicker

---

### Phase 4: Audio Verification (30 minutes)

**Test Order:**
1. ✅ **I2S initialization** - Audio manager initializes without errors
2. ✅ **Test tone generation** - Generate 440Hz sine wave
3. ✅ **Audio playback (WiFi OFF)** - Play test tone with WiFi disconnected
4. ✅ **Audio playback (WiFi ON)** - Play test tone with WiFi connected
5. ✅ **Audio playback (MQTT ACTIVE)** - **CRITICAL TEST** - Play with MQTT connected

**Expected Results:**
- I2S initializes successfully
- Test tone audible from speaker (440Hz for 2 seconds)
- Audio plays smoothly with WiFi active
- **Audio plays without crashes when MQTT is active** ← **KEY TEST**

**Success Criteria:**
- ✅ **NO CRASHES** during WiFi + MQTT + I2S concurrent operation
- Clear audio, no distortion
- MQTT stays connected during audio playback
- WiFi remains stable

---

### Phase 5: Integration Testing (1 hour)

**Test Order:**
1. ✅ **Home Assistant discovery** - All 36 entities discovered
2. ✅ **MQTT commands** - Test status, restart, test_audio commands
3. ✅ **Brightness controls** - Test all HA brightness controls
4. ✅ **LED validation** - Trigger post-transition validation
5. ✅ **Error logging** - Query error log via MQTT
6. ✅ **Long-term stability** - Run for 30 minutes, check for crashes

**Expected Results:**
- All HA entities available and functional
- MQTT commands execute correctly
- LED validation passes (no mismatches)
- No errors in error log
- System runs stably for 30+ minutes

**Success Criteria:**
- Zero crashes or reboots
- All Home Assistant controls work
- LED validation health score >95%
- No memory leaks

---

### Phase 6: External Storage Testing (if using microSD)

**Test Order:**
1. ✅ **SD card detection** - Verify SD card detected at boot
2. ✅ **Filesystem mount** - Mount FatFS on /storage
3. ✅ **File write** - Write test file to /storage/chimes/
4. ✅ **File read** - Read test file back
5. ✅ **Audio from SD** - Play audio file from SD card

**Expected Results:**
- SD card detected (prints capacity in boot logs)
- Filesystem mounts successfully
- Files read/write correctly
- Audio playback from SD card works

**Success Criteria:**
- No SD card errors
- File operations complete successfully
- Audio playback quality same as RAM

---

## Risk Assessment

### Risk Matrix

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **GPIO pin errors** | Medium | High | Triple-check pin mappings before build |
| **PSRAM configuration wrong** | Low | Critical | Follow exact sdkconfig settings above |
| **I2C bus conflicts** | Low | Medium | Test I2C scan before full init |
| **Audio still crashes** | Low | Critical | Verify PSRAM enabled, test incrementally |
| **SD card not detected** | Low | Medium | Verify JEDEC ID, check SPI pins |
| **Filesystem migration errors** | Medium | Medium | Test file operations before audio |
| **WiFi performance degraded** | Low | Low | Monitor WiFi RSSI and throughput |
| **LED validation fails** | Low | Medium | Re-test TLC59116 readback on new I2C pins |
| **MQTT disconnects** | Low | High | Monitor keepalive, test concurrent I2S |
| **Build system errors** | Low | Low | Clean build, regenerate sdkconfig |

### Critical Risks

**Risk 1: Audio Still Crashes with WiFi + MQTT + I2S**
- **Mitigation:** Verify PSRAM enabled in sdkconfig, test with/without MQTT active
- **Fallback:** Increase DMA buffers (AUDIO_DMA_BUF_COUNT), reduce sample rate

**Risk 2: PSRAM Not Initialized**
- **Mitigation:** Check boot logs for "Found 2MB PSRAM" message
- **Fallback:** Re-run menuconfig, verify all PSRAM settings

**Risk 3: I2C Devices Not Responding**
- **Mitigation:** Run I2C bus scan, check GPIO pin connections
- **Fallback:** Verify ESP32-S3 I2C pull-ups enabled (same as ESP32)

---

## Migration Timeline

### Estimated Timeline: 4-6 Hours Total

**Phase 1: Preparation (30 minutes)**
- ✅ Backup current code (`git commit`)
- ✅ Read this document thoroughly
- ✅ Prepare YB-ESP32-S3-AMP board (connect USB-C, verify power)
- ✅ Backup sdkconfig: `cp sdkconfig sdkconfig.esp32.backup`

**Phase 2: Configuration Changes (30 minutes)**
- ✅ Change target: `idf.py set-target esp32s3`
- ✅ Configure PSRAM and flash: `idf.py menuconfig`
- ✅ Verify sdkconfig settings (see Section 4.1)
- ✅ Clean build: `idf.py fullclean`

**Phase 3: GPIO Pin Changes (45 minutes)**
- ✅ Update `i2c_devices.h` (4 lines)
- ✅ Update `adc_manager.h` (1 line)
- ✅ Update `button_manager.h` (1 line)
- ✅ Update `status_led_manager.h` (1 line)
- ✅ Update `audio_manager.h` (3 lines)

**Phase 4: Storage Decision (1-4 hours)**
- **Option A (microSD):** 2-4 hours filesystem migration
- **Option B (W25Q64 alt pins):** 30 minutes GPIO pin changes

**Phase 5: Build and Flash (30 minutes)**
- ✅ Build: `idf.py build`
- ✅ Resolve any build errors
- ✅ Flash: `idf.py -p /dev/ttyUSB0 flash`
- ✅ Monitor: `idf.py monitor`

**Phase 6: Testing (2.5 hours)**
- ✅ Phase 1: Basic hardware (30 min)
- ✅ Phase 2: I2C buses (45 min)
- ✅ Phase 3: Display (30 min)
- ✅ Phase 4: Audio (30 min) ← **CRITICAL**
- ✅ Phase 5: Integration (1 hour)

**Phase 7: Documentation (30 minutes)**
- ✅ Update CLAUDE.md with new GPIO pins
- ✅ Update README.md with ESP32-S3 info
- ✅ Git commit: "Migrate to ESP32-S3 YB-ESP32-S3-AMP board"

---

## Rollback Plan

### If Migration Fails

**Step 1: Revert to ESP32**
```bash
git checkout main  # Or previous commit
idf.py set-target esp32
cp sdkconfig.esp32.backup sdkconfig
idf.py fullclean
idf.py build flash
```

**Step 2: Verify Old Hardware Works**
- Flash ESP32-PICO-KIT v4.1
- Test all functionality
- Investigate ESP32-S3 issue

**Step 3: Debug ESP32-S3 Issues**
- Check serial logs for errors
- Verify PSRAM initialization
- Test GPIO pins with multimeter
- Check power supply (5V/1.5A minimum)

---

## Files Summary

### Files Requiring Changes

| File | Lines Changed | Type | Priority |
|------|---------------|------|----------|
| `sdkconfig` | ~10 | Configuration | Critical |
| `components/i2c_devices/include/i2c_devices.h` | 4 | GPIO pins | Critical |
| `components/adc_manager/include/adc_manager.h` | 1 | GPIO pins | Critical |
| `components/button_manager/include/button_manager.h` | 1 | GPIO pins | Critical |
| `components/status_led_manager/include/status_led_manager.h` | 1 | GPIO pins | Critical |
| `components/audio_manager/include/audio_manager.h` | 3 | GPIO pins | Critical |
| `components/external_flash/include/external_flash.h` | 4 | GPIO pins | Optional |
| `components/filesystem_manager/filesystem_manager.c` | ~100 | Storage API | Optional |
| `partitions.csv` | 0-5 | Partition table | Optional |
| `CLAUDE.md` | ~20 | Documentation | Low |
| `README.md` | ~10 | Documentation | Low |

**Total:** 8 critical files, 3 optional files

---

## Checklist

### Pre-Migration Checklist

- [ ] Read this document completely
- [ ] Backup current code: `git commit -m "Pre-ESP32-S3 migration backup"`
- [ ] Backup sdkconfig: `cp sdkconfig sdkconfig.esp32.backup`
- [ ] YB-ESP32-S3-AMP board received and powered on
- [ ] USB-C cable connected and recognized by PC
- [ ] Decision made: microSD or W25Q64? (Recommend: microSD)
- [ ] Speaker ready to connect to onboard terminal blocks
- [ ] I2C devices (TLC59116, DS3231, BH1750) ready to connect

### Migration Checklist

- [ ] Set target: `idf.py set-target esp32s3`
- [ ] Configure PSRAM: `idf.py menuconfig` (8MB flash, 2MB PSRAM)
- [ ] Update `i2c_devices.h` GPIO pins (SDA/SCL for both buses)
- [ ] Update `adc_manager.h` GPIO pin (potentiometer)
- [ ] Update `button_manager.h` GPIO pin (reset button)
- [ ] Update `status_led_manager.h` GPIO pin (NTP LED)
- [ ] Update `audio_manager.h` GPIO pins (I2S)
- [ ] **(Optional)** Update `external_flash.h` GPIO pins (if using W25Q64 alt pins)
- [ ] **(Optional)** Migrate filesystem to FatFS (if using microSD)
- [ ] Clean build: `idf.py fullclean && idf.py build`
- [ ] Resolve any build errors
- [ ] Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

### Post-Migration Testing Checklist

- [ ] **Phase 1:** Basic hardware (boot, WiFi, NTP, MQTT)
- [ ] **Phase 2:** I2C buses (detect all devices)
- [ ] **Phase 3:** Display (LEDs, German time, brightness)
- [ ] **Phase 4:** Audio (test tone, WiFi+MQTT+I2S concurrent) ← **CRITICAL**
- [ ] **Phase 5:** Integration (Home Assistant, MQTT, validation)
- [ ] **Phase 6:** External storage (if using microSD)
- [ ] All tests pass with no crashes
- [ ] Update documentation (CLAUDE.md, README.md)
- [ ] Git commit: `git commit -m "Migrate to ESP32-S3 YB-ESP32-S3-AMP board"`

---

## Conclusion

### Migration Complexity: **LOW to MEDIUM**

**Total Code Changes:** ~15 lines (GPIO pins only)
**Configuration Changes:** sdkconfig (PSRAM + target)
**Optional Work:** Filesystem migration (2-4 hours if using microSD)

### Key Success Factors

1. ✅ **PSRAM must be enabled** - Verify in sdkconfig
2. ✅ **GPIO pins triple-checked** - Use exact pins from this document
3. ✅ **Test incrementally** - Don't skip testing phases
4. ✅ **Monitor serial logs** - Watch for initialization errors
5. ✅ **Test WiFi+MQTT+I2S** - This is the primary goal!

### Expected Outcome

After successful migration:
- ✅ **Audio plays without crashes** (WiFi + MQTT + I2S concurrent)
- ✅ **System runs 50% faster** (240 MHz CPU)
- ✅ **More stable** (better DMA architecture)
- ✅ **Future-proof** (PSRAM for expansion)
- ✅ **Better audio** (larger buffers, smoother playback)

### Final Recommendation

**Proceed with migration.** The changes are minimal, the risks are low, and the benefits are substantial. The primary goal (fixing WiFi+MQTT+I2S crashes) has strong evidence of success on ESP32-S3.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-02
**Status:** Ready for Implementation
**Next Step:** Begin Phase 1 (Preparation) when YB-ESP32-S3-AMP board arrives
