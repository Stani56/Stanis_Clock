# ESP-IDF Menuconfig Board Configuration Guide

**Last Updated:** November 2025
**ESP-IDF Version:** 5.4.2
**Target:** ESP32-S3

This guide provides the complete menuconfig settings for different board variants of the ESP32-S3 German Word Clock project.

---

## 📋 Quick Board Selection

| Board | Flash | PSRAM | Audio | SD Card | Typical Use |
|-------|-------|-------|-------|---------|-------------|
| **YB-ESP32-S3-AMP** | 8MB | 2MB Quad SPI | Integrated stereo | Integrated | **Default production board** |
| **ESP32-S3-DevKitC-1** | 16MB | 8MB Octal SPI | External mono | External | Development/legacy |

---

## 🎯 YB-ESP32-S3-AMP Configuration (Default)

### Overview
- **Board:** YelloByte YB-ESP32-S3-AMP
- **Module:** ESP32-S3-WROOM-1-N8R2
- **Flash:** 8MB
- **PSRAM:** 2MB Quad SPI PSRAM
- **Peripherals:** Integrated MAX98357A audio + microSD + status LED
- **Status:** Production hardware (default configuration)

### Critical Settings Summary

```bash
# Target Platform
CONFIG_IDF_TARGET="esp32s3"
CONFIG_IDF_TARGET_ESP32S3=y

# Flash Configuration
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="8MB"

# PSRAM Configuration (CRITICAL!)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_QUAD=y          # 2MB Quad SPI PSRAM
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
```

### Step-by-Step Menuconfig

#### 1. Serial Flasher Configuration

```
→ Serial flasher config
  → Flash size (8 MB)
     [*] 8 MB
  → 'idf.py monitor' baud rate
     (115200) 115200 baud
```

**Settings:**
```
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="8MB"
CONFIG_ESPTOOLPY_BAUD_115200B=y
CONFIG_ESPTOOLPY_MONITOR_BAUD_115200B=y
```

#### 2. Partition Table

```
→ Partition Table
  → Partition Table (Custom partition table CSV)
     (*) Custom partition table CSV
  → Custom partition CSV file
     (partitions.csv) partitions.csv
```

**Settings:**
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

#### 3. Component Config → ESP PSRAM

**⚠️ CRITICAL SECTION - Most Important for Stability**

```
→ Component config
  → ESP PSRAM
    [*] Support for external, SPI-connected RAM
    → Mode (PSRAM) --->
       (*) Quad mode PSRAM
    → SPI RAM access method --->
       (*) Make RAM allocatable using malloc() as well
    → Set RAM clock speed
       (80MHz) 80 MHz clock speed
    [*] Run memory test on PSRAM initialization
    [*] Enable workaround for bug in SPI RAM cache for Rev1 ESP32s
```

**Settings:**
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_QUAD=y              # 2MB Quad SPI
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y             # Allow heap allocation
CONFIG_SPIRAM_MEMTEST=y                # Memory test on boot
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384  # <16KB always in DRAM
```

**Why This Matters:**
- Without PSRAM: Only 303KB free heap → MQTT disconnections (errno=119)
- With PSRAM: 2.3MB free heap → Stable MQTT/TLS connections
- Wrong mode (Octal instead of Quad): PSRAM not detected, low heap
- Result: 7.6× memory improvement, critical for Home Assistant discovery

#### 4. Component Config → FreeRTOS

```
→ Component config
  → FreeRTOS
    → Kernel
      (1000) configTICK_RATE_HZ
      [*] Enable FreeRTOS trace facility
      [*] Enable FreeRTOS stats formatting functions
```

**Settings:**
```
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
```

#### 5. Component Config → ESP System Settings

```
→ Component config
  → ESP System Settings
    → CPU frequency
       (*) 240 MHz
    → Main task stack size
       (8192) 8192
```

**Settings:**
```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

**Note:** See `set_240mhz.sh` script for automated CPU frequency switching.

#### 6. Component Config → Log Output

```
→ Component config
  → Log output
    → Default log verbosity
       (*) Info
    [*] Use ANSI terminal colors in log output
```

**Settings:**
```
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_LOG_COLORS=y
```

#### 7. Component Config → Wi-Fi

```
→ Component config
  → Wi-Fi
    → WiFi IRAM speed optimization
       [*] Enable
    → WiFi RX IRAM speed optimization
       [*] Enable
    → Maximum WiFi TX power (dBm)
       (20) 20 dBm
```

**Settings:**
```
CONFIG_ESP_WIFI_IRAM_OPT=y
CONFIG_ESP_WIFI_RX_IRAM_OPT=y
CONFIG_ESP_PHY_MAX_WIFI_TX_POWER=20
```

---

## 🎯 ESP32-S3-DevKitC-1 Configuration (Legacy)

### Overview
- **Board:** Espressif ESP32-S3-DevKitC-1
- **Module:** ESP32-S3-WROOM-1-N16R8
- **Flash:** 16MB
- **PSRAM:** 8MB Octal SPI PSRAM
- **Peripherals:** External MAX98357A + microSD
- **Status:** Legacy/development board

### Critical Differences from YB-AMP

| Setting | YB-AMP (Default) | DevKitC-1 (Legacy) |
|---------|------------------|---------------------|
| Flash Size | 8MB | 16MB |
| PSRAM Mode | **Quad SPI** | **Octal SPI** |
| PSRAM Size | 2MB | 8MB |
| GPIO I2C Sensors | GPIO 1/42 (WiFi-safe) | GPIO 1/18 (ADC2 conflict) |
| Status LED | GPIO 47 (onboard) | None |
| Audio | Integrated stereo | External mono |

### Step-by-Step Menuconfig Changes

#### 1. Serial Flasher Configuration

```
→ Serial flasher config
  → Flash size (16 MB)
     [*] 16 MB
```

**Settings:**
```
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
```

#### 2. Component Config → ESP PSRAM

**⚠️ CRITICAL CHANGE - Different PSRAM Mode**

```
→ Component config
  → ESP PSRAM
    [*] Support for external, SPI-connected RAM
    → Mode (PSRAM) --->
       (*) Octal mode PSRAM          # ← DIFFERENT FROM YB-AMP!
    → SPI RAM access method --->
       (*) Make RAM allocatable using malloc() as well
    → Set RAM clock speed
       (80MHz) 80 MHz clock speed
    [*] Run memory test on PSRAM initialization
```

**Settings:**
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y               # ← 8MB Octal SPI (NOT Quad!)
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MEMTEST=y
```

**⚠️ WARNING:** Using Quad mode on DevKitC-1 will result in PSRAM not being detected!

#### 3. Board Selection in Code

Edit `components/board_config/include/board_config.h`:

```c
// Uncomment this line for ESP32-S3-DevKitC-1
#define CONFIG_BOARD_DEVKITC

// Comment out for YB-ESP32-S3-AMP (default)
// #undef CONFIG_BOARD_DEVKITC
```

Or add to CMakeLists.txt:
```cmake
target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_BOARD_DEVKITC)
```

---

## 🔍 Verification and Troubleshooting

### Verify PSRAM Detection

After flashing, check boot logs for:

**YB-AMP (Correct):**
```
I (335) esp_psram: Found 2MB PSRAM device
I (335) esp_psram: Speed: 80MHz
I (339) esp_psram: PSRAM initialized, cache is in normal (1-core) mode.
I (711) wordclock: Free heap: 2361220 bytes (2.3MB)
```

**DevKitC-1 (Correct):**
```
I (335) esp_psram: Found 8MB PSRAM device
I (335) esp_psram: Speed: 80MHz
I (339) esp_psram: PSRAM initialized, cache is in normal (1-core) mode.
I (711) wordclock: Free heap: 8300000 bytes (8.3MB)
```

**YB-AMP with WRONG Octal Mode (ERROR):**
```
E (335) esp_psram: PSRAM ID read error: 0xffffffff
E (335) cpu_start: Failed to init external RAM!
I (711) wordclock: Free heap: 303456 bytes (303KB) ← LOW!
```

**DevKitC-1 with WRONG Quad Mode (ERROR):**
```
E (335) esp_psram: PSRAM ID read error: 0x00000000
E (335) cpu_start: Failed to init external RAM!
I (711) wordclock: Free heap: 303456 bytes (303KB) ← LOW!
```

### Common Problems and Fixes

#### Problem: "PSRAM not detected"

**Symptoms:**
- Boot log shows PSRAM ID read error
- Free heap is ~303KB instead of 2MB+
- MQTT disconnects after ~28 seconds with errno=119

**Solution:**
```bash
# Check current PSRAM mode
grep "CONFIG_SPIRAM_MODE" sdkconfig

# For YB-AMP (should show):
CONFIG_SPIRAM_MODE_QUAD=y

# For DevKitC-1 (should show):
CONFIG_SPIRAM_MODE_OCT=y

# If wrong, reconfigure:
idf.py menuconfig
# → Component config → ESP PSRAM → Mode → Select correct mode
idf.py fullclean && idf.py build
```

#### Problem: "Build fails - partition too small"

**Symptoms:**
```
Error: app partition is too small for binary
```

**Solution:**
- Check flash size matches board (8MB for YB-AMP, 16MB for DevKitC-1)
- Verify partitions.csv matches flash size
- Run `idf.py menuconfig` → Serial flasher → Flash size

#### Problem: "WiFi unstable on DevKitC-1"

**Symptoms:**
- WiFi connects but drops frequently
- I2C sensor reads fail during WiFi activity

**Root Cause:**
- GPIO 18 (I2C SCL for sensors) is on ADC2
- ADC2 cannot be used while WiFi is active

**Solution:**
- Use YB-AMP board (GPIO 42 is WiFi-safe)
- Or rewire sensors to different GPIO pins (requires code changes)

---

## 📝 Complete sdkconfig Checklist

### YB-ESP32-S3-AMP (Default)

```bash
# Verify these settings in sdkconfig:
grep -E "CONFIG_IDF_TARGET|CONFIG_ESPTOOLPY_FLASHSIZE|CONFIG_SPIRAM" sdkconfig

# Expected output:
CONFIG_IDF_TARGET="esp32s3"
CONFIG_IDF_TARGET_ESP32S3=y
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="8MB"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_QUAD=y          # ← CRITICAL!
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
```

### ESP32-S3-DevKitC-1 (Legacy)

```bash
# Verify these settings in sdkconfig:
grep -E "CONFIG_IDF_TARGET|CONFIG_ESPTOOLPY_FLASHSIZE|CONFIG_SPIRAM" sdkconfig

# Expected output:
CONFIG_IDF_TARGET="esp32s3"
CONFIG_IDF_TARGET_ESP32S3=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y           # ← CRITICAL! (Octal, not Quad)
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
```

---

## 🚀 Quick Setup Scripts

### Auto-configure for YB-AMP (Default)

```bash
#!/bin/bash
# setup_yb_amp.sh

echo "Configuring for YB-ESP32-S3-AMP..."

# Ensure correct PSRAM mode
idf.py menuconfig <<EOF
c
e
SPIRAM
m
q
q
q
q
y
EOF

# Verify settings
grep "CONFIG_SPIRAM_MODE_QUAD=y" sdkconfig || echo "❌ ERROR: PSRAM not set to Quad mode!"
grep "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y" sdkconfig || echo "❌ ERROR: Flash not set to 8MB!"

echo "✅ Configuration complete. Run: idf.py fullclean && idf.py build"
```

### Auto-configure for DevKitC-1

```bash
#!/bin/bash
# setup_devkitc.sh

echo "Configuring for ESP32-S3-DevKitC-1..."

# Update board selection
sed -i 's|// #define CONFIG_BOARD_DEVKITC|#define CONFIG_BOARD_DEVKITC|' \
    components/board_config/include/board_config.h

# Ensure correct PSRAM mode
idf.py menuconfig <<EOF
c
e
SPIRAM
m
o
q
q
q
q
y
EOF

# Verify settings
grep "CONFIG_SPIRAM_MODE_OCT=y" sdkconfig || echo "❌ ERROR: PSRAM not set to Octal mode!"
grep "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y" sdkconfig || echo "❌ ERROR: Flash not set to 16MB!"

echo "✅ Configuration complete. Run: idf.py fullclean && idf.py build"
```

---

## 📊 Feature Comparison

| Feature | YB-AMP | DevKitC-1 | Notes |
|---------|--------|-----------|-------|
| **Hardware** |
| CPU | ESP32-S3 @ 240MHz | ESP32-S3 @ 240MHz | Same |
| Flash | 8MB | 16MB | DevKitC has 2× flash |
| PSRAM | 2MB Quad SPI | 8MB Octal SPI | DevKitC has 4× PSRAM |
| PSRAM Speed | 80MHz | 80MHz | Same |
| **Peripherals** |
| Audio | Integrated stereo | External mono | YB-AMP better |
| SD Card | Integrated | External | YB-AMP better |
| Status LED | GPIO 47 onboard | None | YB-AMP only |
| **GPIO Pins** |
| I2C Sensors | GPIO 1/42 | GPIO 1/18 | YB-AMP WiFi-safe |
| I2C LEDs | GPIO 8/9 | GPIO 8/9 | Same |
| Audio I2S | GPIO 5/6/7 | GPIO 5/6/7 | Same |
| SD Card SPI | GPIO 10/11/12/13 | GPIO 10/11/12/13 | Same |
| **Software** |
| MQTT Stability | Excellent (WiFi-safe GPIO) | Good (GPIO 18 ADC2 conflict) | YB-AMP better |
| Memory | 2.3MB heap available | 8.3MB heap available | DevKitC better |
| Board Config | Default | Define CONFIG_BOARD_DEVKITC | Code change needed |

---

## 🎯 Recommendations

### Use YB-ESP32-S3-AMP if:
- ✅ You want integrated peripherals (audio + SD card)
- ✅ You need maximum WiFi stability (no ADC2 conflicts)
- ✅ You want the default/production configuration
- ✅ You prefer compact hardware design
- ✅ You need onboard status LED

### Use ESP32-S3-DevKitC-1 if:
- ✅ You need more flash (16MB vs 8MB)
- ✅ You need more PSRAM (8MB vs 2MB)
- ✅ You're doing development/testing
- ✅ You already have DevKitC-1 hardware
- ✅ You need external peripheral flexibility

---

## 🔗 Related Documentation

- [Project Structure](project-structure.md) - Component architecture
- [API Reference](api-reference.md) - Board config component details
- [CLAUDE.md](../../CLAUDE.md) - Quick hardware reference
- [Post-Build Automation](post-build-automation-guide.md) - OTA workflow

---

## 📌 Key Takeaways

1. **PSRAM mode is CRITICAL:**
   - YB-AMP: **Quad SPI** (2MB)
   - DevKitC-1: **Octal SPI** (8MB)
   - Wrong mode = PSRAM not detected = MQTT instability

2. **Flash size must match:**
   - YB-AMP: 8MB
   - DevKitC-1: 16MB

3. **Board selection in code:**
   - YB-AMP: Default (no changes)
   - DevKitC-1: Define `CONFIG_BOARD_DEVKITC`

4. **GPIO conflicts:**
   - YB-AMP: All GPIO WiFi-safe
   - DevKitC-1: GPIO 18 is ADC2 (WiFi conflict)

5. **Memory requirements:**
   - PSRAM is **mandatory** for MQTT/TLS stability
   - Without PSRAM: Only 303KB heap → Disconnections
   - With PSRAM: 2-8MB heap → Stable operation

---

**Last Updated:** 2025-11-13
**Maintained By:** Project maintainers
**Status:** Production-ready configuration guide
