# Power Surge & Brownout Protection - ESP32-S3 Word Clock

**Last Updated:** November 2025
**ESP-IDF Version:** 5.4.2
**Platform:** ESP32-S3 (YB-ESP32-S3-AMP)

---

## Overview

This document describes the **hardware and software protection mechanisms** that detect and recover from power-related issues, including power surges, voltage drops, and undefined processor states.

---

## Hardware Protection Mechanisms

### 1. **Brownout Detector (BOD) - Hardware Feature**

**Purpose:** Detects when supply voltage drops below safe operating level and performs **immediate hardware reset** before processor enters undefined state.

**Current Configuration:**
```
CONFIG_ESP_BROWNOUT_DET=y                  # Brownout detection enabled
CONFIG_ESP_BROWNOUT_DET_LVL_SEL_7=y        # Level 7 selected
CONFIG_ESP_BROWNOUT_DET_LVL=7              # Voltage threshold
CONFIG_ESP_SYSTEM_BROWNOUT_INTR=y          # Generate interrupt before reset
CONFIG_SPI_FLASH_BROWNOUT_RESET=y          # Protect flash during brownout
```

**Brownout Voltage Thresholds (ESP32-S3):**

| Level | Voltage Threshold | Description | Risk Assessment |
|-------|------------------|-------------|-----------------|
| 1 | ~2.43V | Lowest (most permissive) | ⚠️ High risk - may allow undefined states |
| 2 | ~2.48V | Very Low | ⚠️ High risk |
| 3 | ~2.58V | Low | ⚠️ Medium risk |
| 4 | ~2.62V | Below Normal | ⚠️ Medium risk |
| 5 | ~2.67V | Normal | ⚠️ Low risk |
| 6 | ~2.70V | Above Normal | ✅ Good protection |
| **7** | **~2.80V** | **High (CURRENT)** | **✅ Best protection** |

**Our Setting: Level 7 (2.80V) - Maximum Protection** ✅

**How It Works:**
1. **Hardware monitoring:** Analog comparator continuously monitors VDD (3.3V rail)
2. **Voltage drops below 2.80V:** Brownout detector triggers
3. **Interrupt generated:** CPU receives brownout interrupt (if enabled)
4. **Immediate hardware reset:** Processor reset before corruption occurs
5. **System reboots:** Fresh start from known-good state

**Response Time:** <1 microsecond (hardware-level, faster than software can react)

---

### 2. **Watchdog Timers (WDT) - Software/Hardware Hybrid**

**Purpose:** Detect when processor is stuck/hung after power surge scrambles execution state.

**Three Watchdog Types:**

#### A. Interrupt Watchdog Timer (IWDT)
- **Timeout:** 300ms
- **Detects:** Interrupts disabled too long (processor stuck in ISR)
- **Response:** Hardware reset

#### B. Task Watchdog Timer (TWDT)
- **Timeout:** 5 seconds
- **Detects:** Tasks not running (scheduler corrupted)
- **Response:** Hardware reset or panic

#### C. RTC Watchdog Timer (RWDT)
- **Always active** during deep sleep and startup
- **Detects:** Boot process stuck
- **Response:** Hardware reset

**See:** [watchdog-configuration.md](watchdog-configuration.md) for details

---

### 3. **Flash Memory Protection**

**Purpose:** Prevent flash corruption during brownout events.

**Configuration:**
```
CONFIG_SPI_FLASH_BROWNOUT_RESET=y          # Reset during flash brownout
CONFIG_SPI_FLASH_BROWNOUT_RESET_XMC=y      # XMC flash brownout reset
```

**Protection Features:**
- Flash writes interrupted by brownout are **aborted**
- Prevents partial/corrupted flash writes
- Dual OTA partitions provide fallback (ota_0 / ota_1)

---

## Power Surge Scenarios & Protection

### Scenario 1: **Clean Power Loss (Voltage Drop)**

**Event:** Power supply voltage drops from 3.3V → 2.5V

**Protection Response:**
1. **Brownout detector triggers** at 2.80V threshold
2. **Interrupt generated** (if software wants to save state)
3. **Hardware reset issued** within <1µs
4. **Processor halts** before entering undefined state
5. **System reboots** when power restored

**Result:** ✅ Clean recovery, no undefined states

---

### Scenario 2: **Power Surge (Voltage Spike)**

**Event:** Voltage spike on 3.3V rail (e.g., 4.5V for 10ms)

**Hardware Protection:**
- **ESP32-S3 has NO overvoltage protection** (only brownout detector for undervoltage)
- Relies on **external power supply regulation**
- **Internal LDO** provides some tolerance (~3.6V max recommended)

**Software Protection:**
- **Watchdog timers** detect if surge corrupts processor state
- If processor enters undefined loop → TWDT/IWDT triggers reset (300ms - 5s)

**External Protection (Recommended):**
- ⚠️ **Add TVS diode** on 3.3V rail (Transient Voltage Suppressor)
- ⚠️ **Add ferrite beads** on power input
- ⚠️ **Use regulated power supply** with overvoltage protection

**Current Risk:** ⚠️ **Medium** - No hardware overvoltage protection on ESP32-S3

**Recommendation:** Add external protection if environment has frequent surges

---

### Scenario 3: **Electromagnetic Interference (EMI)**

**Event:** Strong electromagnetic pulse disrupts processor execution

**Protection:**
1. **Watchdog timers** detect execution going off-track
2. **Task watchdog** triggers if tasks stop running (5s)
3. **Interrupt watchdog** triggers if interrupts disabled (300ms)
4. **System resets** automatically

**Recovery Time:** 300ms - 5 seconds (depending on watchdog)

**Result:** ✅ Automatic recovery via watchdog reset

---

### Scenario 4: **Flash Corruption During Power Loss**

**Event:** Power lost while writing to flash memory

**Protection:**
1. **Brownout detector** aborts flash write at 2.80V
2. **Dual OTA partitions** provide fallback:
   - ota_0: Primary firmware
   - ota_1: Backup firmware
3. **OTA rollback** if new firmware fails to boot
4. **Bootloader watchdog** (9s) resets if boot hangs

**Result:** ✅ System boots from last-known-good partition

---

## Reset Cause Detection (Missing Feature!)

**Current Status:** ❌ **Not Implemented**

**What's Missing:**
The application **does NOT** currently log or analyze reset causes at startup. This means:
- ❌ Can't distinguish between normal reboot vs power surge recovery
- ❌ No telemetry on brownout events
- ❌ No tracking of watchdog resets

**ESP-IDF API Available:**
```c
#include "esp_system.h"

esp_reset_reason_t reason = esp_reset_reason();
```

**Possible Reset Reasons:**
```c
ESP_RST_UNKNOWN      // Unknown reset reason
ESP_RST_POWERON      // Normal power-on reset
ESP_RST_EXT          // External reset (reset button)
ESP_RST_SW           // Software reset (esp_restart())
ESP_RST_PANIC        // Exception/panic reset
ESP_RST_INT_WDT      // Interrupt watchdog timeout
ESP_RST_TASK_WDT     // Task watchdog timeout
ESP_RST_WDT          // Other watchdog reset
ESP_RST_DEEPSLEEP    // Wake from deep sleep
ESP_RST_BROWNOUT     // Brownout detector reset ← Important!
ESP_RST_SDIO         // SDIO reset
```

---

## Recommended Improvement: Add Reset Cause Logging

### Implementation

**Add to `main/wordclock.c` in `app_main()` after line 508:**

```c
void app_main(void) {
    // Log system information
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "ESP32 GERMAN WORD CLOCK - PRODUCTION VERSION");
    ESP_LOGI(TAG, "🚀 Firmware v2.11.3 - Version Consistency Fix");
    ESP_LOGI(TAG, "================================================");

    // ADD THIS: Reset cause detection
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char* reset_reason_str = get_reset_reason_string(reset_reason);

    ESP_LOGI(TAG, "📋 Reset Reason: %s", reset_reason_str);

    // Log to error log for telemetry
    if (reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGW(TAG, "⚠️ System recovered from brownout (voltage drop detected)");
        error_log_add(ERROR_SOURCE_POWER, ERROR_SEVERITY_WARNING,
                      "Brownout reset detected - check power supply");
    } else if (reset_reason == ESP_RST_PANIC) {
        ESP_LOGE(TAG, "❌ System recovered from panic/crash");
        error_log_add(ERROR_SOURCE_SYSTEM, ERROR_SEVERITY_CRITICAL,
                      "Panic reset - system crash detected");
    } else if (reset_reason == ESP_RST_INT_WDT || reset_reason == ESP_RST_TASK_WDT) {
        ESP_LOGE(TAG, "❌ System recovered from watchdog timeout");
        error_log_add(ERROR_SOURCE_SYSTEM, ERROR_SEVERITY_CRITICAL,
                      "Watchdog reset - system hung detected");
    }

    // Continue with existing code...
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    // ...
}

// Helper function to convert reset reason to string
static const char* get_reset_reason_string(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN:    return "Unknown";
        case ESP_RST_POWERON:    return "Power-on reset";
        case ESP_RST_EXT:        return "External reset (button)";
        case ESP_RST_SW:         return "Software reset";
        case ESP_RST_PANIC:      return "Exception/Panic";
        case ESP_RST_INT_WDT:    return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:   return "Task Watchdog";
        case ESP_RST_WDT:        return "Other Watchdog";
        case ESP_RST_DEEPSLEEP:  return "Wake from deep sleep";
        case ESP_RST_BROWNOUT:   return "Brownout (Voltage drop)";
        case ESP_RST_SDIO:       return "SDIO reset";
        default:                 return "Unknown";
    }
}
```

### Benefits

**With Reset Cause Logging:**
1. ✅ **Distinguish normal vs abnormal resets**
   - Normal: Power-on, software reset
   - Abnormal: Brownout, panic, watchdog

2. ✅ **Track power stability issues**
   - Count brownout resets over time
   - Identify power supply problems
   - MQTT publish reset statistics

3. ✅ **Debug crashes remotely**
   - Panic resets indicate bugs
   - Watchdog resets indicate hangs
   - Error log captures for analysis

4. ✅ **Home Assistant integration**
   - Publish reset reason via MQTT
   - Alert on abnormal resets
   - Track system stability metrics

---

## Current Protection Summary

| Protection Mechanism | Status | Effectiveness | Coverage |
|---------------------|--------|---------------|----------|
| **Brownout Detector (2.80V)** | ✅ Enabled | ⭐⭐⭐⭐⭐ Excellent | Voltage drops |
| **Interrupt Watchdog (300ms)** | ✅ Enabled | ⭐⭐⭐⭐⭐ Excellent | ISR hangs |
| **Task Watchdog (5s)** | ✅ Enabled | ⭐⭐⭐⭐⭐ Excellent | Task hangs |
| **RTC Watchdog (boot)** | ✅ Enabled | ⭐⭐⭐⭐⭐ Excellent | Boot hangs |
| **Flash Brownout Protection** | ✅ Enabled | ⭐⭐⭐⭐ Very Good | Flash corruption |
| **Dual OTA Partitions** | ✅ Enabled | ⭐⭐⭐⭐ Very Good | Firmware corruption |
| **Overvoltage Protection** | ❌ None | ⭐⭐ Fair | Voltage spikes |
| **Reset Cause Logging** | ❌ Not Implemented | N/A | Telemetry |

---

## Power Supply Recommendations

### Minimum Requirements (Current Setup)

**Input Voltage:** 5V DC (USB or barrel jack)
**Current Capacity:** Minimum 1A (1.5A recommended)

**Why:**
- ESP32-S3: ~300mA peak (WiFi transmit)
- TLC59116 LEDs (10 controllers): ~500mA @ 60 brightness
- MAX98357A audio: ~100mA peak
- SD card: ~100mA peak
- **Total peak:** ~1000mA (1A)

**Add margin:** 1.5A recommended for stability

---

### Recommended Improvements

#### 1. **External TVS Diode (Overvoltage Protection)**

**Part:** `SMAJ5.0A` or similar (5V TVS diode)
**Placement:** Across 3.3V rail near ESP32-S3
**Protection:** Clamps voltage spikes above ~6V

**Why:**
- ESP32-S3 brownout detector only protects against **undervoltage**
- No hardware protection for **overvoltage** spikes
- TVS diode clamps transients before reaching ESP32-S3

---

#### 2. **Ferrite Bead on Power Input**

**Part:** Ferrite bead (e.g., `BLM18PG471SN1D`)
**Placement:** Series with 5V input line
**Protection:** Filters high-frequency noise/spikes

**Why:**
- Reduces EMI coupling into power rail
- Protects against fast transients (<1µs)
- Complements brownout detector

---

#### 3. **Power Supply with Built-in Protection**

**Features to look for:**
- ✅ Overvoltage protection (OVP)
- ✅ Overcurrent protection (OCP)
- ✅ Short-circuit protection (SCP)
- ✅ EMI filtering

**Example:** Mean Well GS25A05-P1J (5V 5A, regulated, protected)

---

## Recovery Behavior Summary

### Power Surge → Undefined State

**Detection Path:**
1. **If voltage drops:** Brownout detector triggers reset (<1µs)
2. **If processor hung:** Watchdog timers trigger reset (300ms - 5s)
3. **If flash corrupted:** OTA rollback to previous firmware

**Result:** System always recovers to known-good state

**Recovery Time:**
- Brownout reset: <1 second (immediate)
- Watchdog reset: 0.3 - 5 seconds (depends on watchdog type)
- OTA rollback: ~10 seconds (bootloader fallback)

**Data Loss:**
- ✅ NVS storage preserved (flash survives reset)
- ✅ RTC time preserved (DS3231 has battery backup)
- ❌ MQTT connection lost (must reconnect)
- ❌ Current display state lost (rebuilds from RTC time)

---

## Testing Power Surge Protection

### Safe Test Methods

**1. Simulated Brownout (Software Test):**
```c
// Trigger software reset to test recovery
esp_restart();
```

**2. Voltage Drop Test (Hardware Test):**
- Use variable power supply
- Gradually lower voltage from 3.3V → 2.5V
- Observe brownout reset at ~2.8V
- **⚠️ Warning:** Do NOT exceed 3.6V on 3.3V rail!

**3. Watchdog Test (Software Test):**
```c
// Disable watchdog temporarily for testing
esp_task_wdt_delete(NULL);

// Infinite loop to trigger watchdog
while(1) {
    // No yield - watchdog will trigger after 5s
}
```

---

## Troubleshooting Power Issues

### Symptom: Frequent Brownout Resets

**Possible Causes:**
1. **Insufficient power supply** (current too low)
2. **Long power cables** (voltage drop)
3. **Weak 3.3V regulator** (can't handle peak current)

**Solution:**
- Use 1.5A+ power supply
- Shorten USB cable (<1 meter)
- Add 1000µF capacitor near ESP32-S3

---

### Symptom: Random Watchdog Resets

**Possible Causes:**
1. **EMI interference** (scrambles execution)
2. **Software bug** (task hang)
3. **Flash corruption** (code reads garbage)

**Solution:**
- Add ferrite bead on power input
- Check error log via MQTT: `home/Clock_Stani_1/error_log/query`
- Review watchdog panic logs

---

### Symptom: System Won't Boot After Power Loss

**Possible Causes:**
1. **Flash corruption** (partial write during power loss)
2. **Partition table corruption**

**Solution:**
- Bootloader will auto-rollback to previous partition
- If persistent, reflash firmware via USB

---

## Future Enhancements

### High Priority

1. **✅ Implement reset cause logging** (see code above)
   - Track brownout events
   - Publish statistics via MQTT
   - Alert on repeated resets

2. **⚠️ Add TVS diode for overvoltage protection**
   - Hardware modification
   - Protects against voltage spikes

### Medium Priority

3. **📊 Power supply monitoring**
   - Add voltage sensor on 3.3V rail
   - MQTT publish voltage level
   - Alert if voltage unstable

4. **🔔 Reset notification**
   - Send MQTT message on abnormal reset
   - Home Assistant automation for alerts

### Low Priority

5. **🕐 Graceful shutdown on brownout**
   - Use brownout interrupt (before reset)
   - Save critical state to flash
   - Requires fast NVS write (<500µs)

---

## Related Documentation

- [Watchdog Configuration](watchdog-configuration.md) - Detailed watchdog timer info
- [Error Logging Guide](../user/error-logging-guide.md) - Error log system
- [ESP32-S3 Technical Reference Manual - Brownout Detector](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf#brownout)
- [ESP-IDF Reset Reason API](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-reference/system/system.html#_CPPv418esp_reset_reason_tv)

---

## Summary

### ✅ Current Protection (Good)

**Hardware:**
- Brownout detector at 2.80V (highest protection level)
- Three watchdog timers (IWDT, TWDT, RWDT)
- Flash brownout protection
- Dual OTA partitions for firmware rollback

**Software:**
- All tasks yield properly (no long loops)
- Timeout on all blocking I/O
- Event-driven architecture

### ⚠️ Gaps (Medium Risk)

1. **No overvoltage protection** (ESP32-S3 limitation)
   - Recommendation: Add TVS diode externally

2. **No reset cause logging** (telemetry gap)
   - Recommendation: Implement reset cause detection (code provided above)

3. **No power supply monitoring** (can't predict brownouts)
   - Recommendation: Add voltage sensor (optional)

### 🎯 Answer to Original Question

**"What happens if processor enters undefined state?"**

**Answer:**
1. **Brownout detector** catches voltage drops and resets **before** undefined state (<1µs)
2. **Watchdog timers** detect hung/corrupted execution and force reset (0.3-5s)
3. **Dual OTA partitions** provide rollback if firmware corrupted
4. **System always recovers** to known-good state automatically

**Current protection: ⭐⭐⭐⭐ Very Good (4/5 stars)**
- Missing: Overvoltage protection, reset telemetry

---

**Document Version:** 1.0
**Status:** Complete ✅
**Recommended Action:** Add reset cause logging + consider TVS diode for production
