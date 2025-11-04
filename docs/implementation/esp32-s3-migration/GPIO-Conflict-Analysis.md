# ESP32-S3 GPIO Conflict Analysis - Complete Verification

**Date:** November 2025
**Purpose:** Verify NO GPIO conflicts exist for ESP32-S3-N16R8 migration
**Status:** ✅ VERIFIED - No conflicts found

---

## Executive Summary

**Result:** ✅ **NO CONFLICTS** - All GPIO assignments are safe and do not overlap.

**User Decision:** Potentiometer will use **GPIO 3 (ADC1_CH2)** on ESP32-S3-N16R8.

---

## Complete GPIO Assignment Map (ESP32-S3-N16R8)

### External Connections (11 pins - User Must Wire)

| GPIO | Function | Component | Type | Conflicts? |
|------|----------|-----------|------|------------|
| **0** | Reset Button | button_manager | Input | ✅ None - Boot button safe |
| **1** | **RESERVED** | - | - | ⚠️ **Was planned for potentiometer, NOW AVAILABLE** |
| **2** | I2C1 SDA (Sensors) | i2c_devices | I/O | ✅ None - ADC2_CH1 but not used for ADC |
| **3** | **Potentiometer** | adc_manager | Input | ✅ None - ADC1_CH2, WiFi safe |
| **4** | I2C1 SCL (Sensors) | i2c_devices | Output | ✅ None - ADC2_CH0 but not used for ADC |
| **8** | I2C0 SDA (LEDs) | i2c_devices | I/O | ✅ None - YB board default I2C |
| **9** | I2C0 SCL (LEDs) | i2c_devices | Output | ✅ None - YB board default I2C |
| **18** | NTP Status LED | status_led_manager | Output | ✅ None - GPIO safe |
| **21** | WiFi Status LED | status_led_manager | Output | ✅ None - Same as ESP32 |

**Total External Pins:** 9 (vs ESP32: 11)
**Reason for Reduction:** No external audio (built-in), GPIO 1 now available

### Internal Connections (Pre-wired on YB Board)

| GPIO | Function | Component | Type | Conflicts? |
|------|----------|-----------|------|------------|
| **5** | I2S BCLK | audio_manager | Output | ✅ None - Pre-wired to MAX98357A |
| **6** | I2S LRCLK | audio_manager | Output | ✅ None - Pre-wired to MAX98357A |
| **7** | I2S DIN | audio_manager | Output | ✅ None - Pre-wired to MAX98357A |
| **10** | microSD CS | sdcard_manager | Output | ✅ None - Pre-wired, not on headers |
| **11** | microSD MOSI | sdcard_manager | Output | ✅ None - Pre-wired, not on headers |
| **12** | microSD CLK | sdcard_manager | Output | ✅ None - Pre-wired, not on headers |
| **13** | microSD MISO | sdcard_manager | Input | ✅ None - Pre-wired, not on headers |
| **19** | USB D- | USB | I/O | ✅ None - Native USB, do not use |
| **20** | USB D+ | USB | I/O | ✅ None - Native USB, do not use |
| **43** | UART TX | Debug | Output | ✅ None - Serial console |
| **44** | UART RX | Debug | Input | ✅ None - Serial console |
| **47** | Onboard LED | YB Board | Output | ℹ️ Can be used but has LED |

**Total Internal Pins:** 12

### Reserved/Unavailable GPIOs

| GPIO Range | Reason | Available? |
|------------|--------|------------|
| **22-32** | Flash/PSRAM | ❌ NEVER USE |
| **33-37** | Reserved | ❌ NEVER USE |
| **39-42** | JTAG (MTCK, MTDO, MTDI, MTMS) | ❌ NEVER USE |

### Available for Future Expansion

| GPIO | ADC Channel | WiFi Safe? | Notes |
|------|-------------|------------|-------|
| **1** | ADC1_CH0 | ✅ Yes | Available (was planned for pot, now free) |
| **14** | ADC2_CH3 | ❌ No | ADC2 conflicts with WiFi |
| **15** | ADC2_CH4 | ❌ No | ADC2 conflicts with WiFi |
| **16** | ADC2_CH5 | ❌ No | ADC2 conflicts with WiFi |
| **17** | ADC2_CH6 | ❌ No | ADC2 conflicts with WiFi |
| **38** | - | ✅ Yes | General GPIO |
| **45** | - | ✅ Yes | General GPIO |
| **46** | - | ✅ Yes | General GPIO |
| **48** | - | ✅ Yes | General GPIO |

**Total Available:** 9 GPIOs (1, 14-17, 38, 45-46, 48)
**WiFi-Safe Available:** 5 GPIOs (1, 38, 45-46, 48)

---

## Conflict Analysis by Category

### 1. I2C Bus Conflicts ✅ NO CONFLICTS

**I2C Bus 0 (GPIO 8/9) - LED Controllers:**
- 10× TLC59116 controllers @ addresses 0x60-0x6A
- GPIO 8: SDA (I/O, no ADC function on this pin)
- GPIO 9: SCL (Output, no ADC function on this pin)
- **Conflict Check:** ✅ None - Dedicated I2C pins

**I2C Bus 1 (GPIO 2/4) - Sensors:**
- DS3231 RTC @ 0x68
- BH1750 Light Sensor @ 0x23
- GPIO 2: SDA (I/O, ADC2_CH1 but not used for ADC)
- GPIO 4: SCL (Output, ADC2_CH0 but not used for ADC)
- **Conflict Check:** ✅ None - Separate bus from LEDs to avoid address conflict

**Why Two Buses?**
- TLC59116 #9 uses address 0x68 (conflicts with DS3231 @ 0x68)
- Solution: DS3231 on separate I2C bus 1

### 2. ADC Conflicts ✅ NO CONFLICTS

**Potentiometer: GPIO 3 (ADC1_CH2)**
- **ADC Unit:** ADC1
- **WiFi Compatibility:** ✅ Safe (ADC1 works with WiFi)
- **Other Functions:** None
- **Conflict Check:** ✅ No conflicts

**ADC Channel Mapping (ESP32-S3):**
| GPIO | ADC Channel | WiFi Safe? | Used By |
|------|-------------|------------|---------|
| 1 | ADC1_CH0 | ✅ Yes | **AVAILABLE** (was planned for pot) |
| 2 | ADC2_CH1 | ❌ No | I2C1 SDA (not using ADC function) |
| 3 | ADC1_CH2 | ✅ Yes | **POTENTIOMETER** ✅ |
| 4 | ADC2_CH0 | ❌ No | I2C1 SCL (not using ADC function) |
| 5 | ADC1_CH4 | ✅ Yes | I2S BCLK (not using ADC function) |
| 6 | ADC1_CH5 | ✅ Yes | I2S LRCLK (not using ADC function) |
| 7 | ADC1_CH6 | ✅ Yes | I2S DIN (not using ADC function) |
| 8 | ADC1_CH7 | ✅ Yes | I2C0 SDA (not using ADC function) |
| 9 | ADC1_CH8 | ✅ Yes | I2C0 SCL (not using ADC function) |
| 10 | ADC1_CH9 | ✅ Yes | microSD CS (not using ADC function) |

**Conflict Check:** ✅ GPIO 3 is ONLY used for potentiometer ADC, no other function conflicts.

### 3. Audio (I2S) Conflicts ✅ NO CONFLICTS

**I2S Pins (GPIO 5/6/7) - Built-in MAX98357A:**
- GPIO 5: I2S BCLK (Output, pre-wired)
- GPIO 6: I2S LRCLK (Output, pre-wired)
- GPIO 7: I2S DIN (Output, pre-wired)
- **Conflict Check:** ✅ None - Not exposed on headers, only used internally

### 4. SPI/microSD Conflicts ✅ NO CONFLICTS

**microSD Pins (GPIO 10/11/12/13) - Built-in Slot:**
- GPIO 10: microSD CS (Output, pre-wired)
- GPIO 11: microSD MOSI (Output, pre-wired)
- GPIO 12: microSD CLK (Output, pre-wired)
- GPIO 13: microSD MISO (Input, pre-wired)
- **Conflict Check:** ✅ None - Not exposed on headers, only used internally

### 5. Status LED Conflicts ✅ NO CONFLICTS

**WiFi Status LED: GPIO 21**
- **Conflict Check:** ✅ None - Same as ESP32, dedicated GPIO

**NTP Status LED: GPIO 18**
- **Conflict Check:** ✅ None - No other functions on GPIO 18

### 6. Button Conflicts ✅ NO CONFLICTS

**Reset Button: GPIO 0**
- **Function:** Boot button (pull-up required)
- **Conflict Check:** ✅ None - Standard boot button, safe to use

### 7. UART/USB Conflicts ✅ NO CONFLICTS

**UART (GPIO 43/44):**
- GPIO 43: TX (Output, debug console)
- GPIO 44: RX (Input, debug console)
- **Conflict Check:** ✅ None - Dedicated UART pins

**Native USB (GPIO 19/20):**
- GPIO 19: USB D- (do not use for GPIO)
- GPIO 20: USB D+ (do not use for GPIO)
- **Conflict Check:** ✅ None - Reserved for USB, not available for other functions

### 8. Flash/PSRAM Conflicts ✅ NO CONFLICTS

**Reserved GPIOs (22-32, 33-37):**
- Used internally by ESP32-S3 module for flash and PSRAM
- **Conflict Check:** ✅ None - Never exposed, cannot be used

---

## Critical Comparison: ESP32 vs ESP32-S3

### GPIO Changes Summary

| Function | ESP32 GPIO | ESP32-S3 GPIO | Conflict Risk? |
|----------|-----------|---------------|----------------|
| **I2C0 SDA** | 25 | **8** | ✅ Safe |
| **I2C0 SCL** | 26 | **9** | ✅ Safe |
| **I2C1 SDA** | 18 | **2** | ✅ Safe (not using ADC) |
| **I2C1 SCL** | 19 | **4** | ✅ Safe (not using ADC) |
| **Potentiometer** | 34 (ADC1_CH6) | **3 (ADC1_CH2)** | ✅ Safe (WiFi compatible) |
| **WiFi LED** | 21 | **21** | ✅ Safe (no change) |
| **NTP LED** | 22 | **18** | ✅ Safe |
| **Reset Button** | 5 | **0** | ✅ Safe |
| **I2S BCLK** | 33 (disabled) | **5 (internal)** | ✅ Safe |
| **I2S LRCLK** | 27 (disabled) | **6 (internal)** | ✅ Safe |
| **I2S DIN** | 32 (disabled) | **7 (internal)** | ✅ Safe |

**Total Changes:** 10 GPIO reassignments
**Conflicts Found:** 0

---

## Verification Checklist

### ✅ No Overlapping GPIOs
- Each GPIO is assigned to exactly one function
- No GPIO is used by multiple components

### ✅ No ADC2 + WiFi Conflicts
- Potentiometer uses ADC1 (GPIO 3) ✅
- I2C GPIO 2/4 have ADC2 channels but not used for ADC ✅
- All ADC2 GPIOs (14-17) are unused ✅

### ✅ No I2C Address Conflicts
- LED controllers (0x60-0x6A) on I2C0 (GPIO 8/9) ✅
- DS3231 RTC (0x68) on I2C1 (GPIO 2/4) ✅
- Separate buses prevent 0x68 conflict ✅

### ✅ No Flash/PSRAM GPIO Usage
- GPIO 22-32 reserved ✅
- GPIO 33-37 reserved ✅
- No user code touches these pins ✅

### ✅ No USB GPIO Usage
- GPIO 19/20 reserved for USB ✅
- No user code uses these pins ✅

### ✅ No UART Conflicts
- GPIO 43/44 dedicated to debug console ✅
- No user code uses these pins ✅

### ✅ Internal Pins Not Exposed
- Audio (GPIO 5/6/7) not on headers ✅
- microSD (GPIO 10/11/12/13) not on headers ✅
- No user wiring needed ✅

---

## Final GPIO Assignment List (ESP32-S3-N16R8)

### User Must Wire (9 External Pins)

```
GPIO 0  ← Reset Button (boot button)
GPIO 2  ← I2C1 SDA (DS3231 + BH1750)
GPIO 3  ← Potentiometer (ADC1_CH2) ★ USER CONFIRMED
GPIO 4  ← I2C1 SCL (DS3231 + BH1750)
GPIO 8  ← I2C0 SDA (10× TLC59116 LEDs)
GPIO 9  ← I2C0 SCL (10× TLC59116 LEDs)
GPIO 18 ← NTP Status LED
GPIO 21 ← WiFi Status LED
```

### Pre-Wired on Board (12 Internal Pins - No User Action)

```
GPIO 5/6/7   ← I2S Audio (2× MAX98357A)
GPIO 10/11/12/13 ← microSD Card
GPIO 19/20   ← Native USB
GPIO 43/44   ← UART Debug Console
GPIO 47      ← Onboard RGB LED
```

### Reserved (NEVER USE)

```
GPIO 22-32   ← Flash/PSRAM (internal)
GPIO 33-37   ← Reserved (internal)
GPIO 39-42   ← JTAG (internal)
```

### Available for Future (9 GPIOs)

```
GPIO 1       ← ADC1_CH0 (WiFi safe) ★ NOW AVAILABLE
GPIO 14-17   ← ADC2 (NOT WiFi safe)
GPIO 38, 45-46, 48 ← General GPIO (WiFi safe)
```

---

## Code Changes Required (8 Files)

**1. components/i2c_devices/include/i2c_devices.h (4 changes):**
```c
#define I2C_LEDS_MASTER_SDA_IO         8      // Was 25
#define I2C_LEDS_MASTER_SCL_IO         9      // Was 26
#define I2C_SENSORS_MASTER_SDA_IO      2      // Was 18
#define I2C_SENSORS_MASTER_SCL_IO      4      // Was 19
```

**2. components/adc_manager/include/adc_manager.h (1 change):**
```c
#define ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_2  // Was ADC_CHANNEL_6 (GPIO 34)
// GPIO 3 (ADC1_CH2) - WiFi safe
```

**3. components/button_manager/include/button_manager.h (1 change):**
```c
#define RESET_BUTTON_PIN GPIO_NUM_0  // Was GPIO_NUM_5
```

**4. components/status_led_manager/include/status_led_manager.h (1 change):**
```c
#define STATUS_LED_NTP_PIN GPIO_NUM_18  // Was GPIO_NUM_22
// WiFi LED stays GPIO_NUM_21 (no change)
```

**5-8. Audio/Filesystem (migration work):**
- components/audio_manager/ - Update I2S pins to GPIO 5/6/7
- components/filesystem_manager/ - Migrate LittleFS → FatFS
- Create components/sdcard_manager/ - New microSD driver
- main/wordclock.c - Update initialization sequence

**Total Lines Changed:** ~15 lines in headers + filesystem migration

---

## Conflict Resolution History

### Initial Plan (October 2025)
- Potentiometer: GPIO 1 (ADC1_CH0)

### User Decision (November 2025)
- Potentiometer: **GPIO 3 (ADC1_CH2)** ✅ CONFIRMED

**Reason for Change:**
- Both GPIO 1 and GPIO 3 are ADC1 (WiFi safe)
- User preference: GPIO 3 (ADC1_CH2)
- No conflicts with GPIO 3
- GPIO 1 now available for future expansion

---

## Summary

**Conflicts Found:** ✅ **ZERO**

**GPIO 3 (ADC1_CH2) Verification:**
- ✅ Not used by any other component
- ✅ ADC1 (WiFi safe)
- ✅ No I2C conflict
- ✅ No I2S conflict
- ✅ No SPI/microSD conflict
- ✅ Not reserved for flash/PSRAM
- ✅ Not reserved for USB
- ✅ Available and safe to use

**Migration Status:** ✅ **READY TO PROCEED**

All GPIO assignments are verified conflict-free and ready for ESP32-S3-N16R8 hardware implementation.

---

**Last Updated:** November 2025
**Verified By:** Complete cross-component GPIO analysis
**Status:** ✅ APPROVED - No conflicts, ready for migration
