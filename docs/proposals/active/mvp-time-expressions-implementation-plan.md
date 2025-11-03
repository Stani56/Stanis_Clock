# MVP Time Expressions - Micro-Step Implementation Plan

**Feature:** HALB-Centric Time Expression Style with DREIVIERTEL
**Strategy:** MVP (Minimal Viable Product) - Single Toggle
**Document Status:** Implementation Roadmap - Ready to Execute
**Created:** 2025-10-15
**Related:** [flexible-time-expressions-analysis.md](flexible-time-expressions-analysis.md)

---

## Executive Summary

This document provides a **detailed, testable, step-by-step implementation plan** for adding alternative German time expressions to the word clock. The implementation follows a **micro-step approach** where each change is:

1. **Small enough** to implement in <30 minutes
2. **Independently testable** before proceeding
3. **Reversible** with clear rollback points
4. **Verifiable** with specific success criteria

**Total Estimated Time:** 3-4 days (detailed breakdown below)

---

## Table of Contents

1. [MVP Scope Definition](#1-mvp-scope-definition)
2. [Implementation Strategy](#2-implementation-strategy)
3. [Detailed Step-by-Step Plan](#3-detailed-step-by-step-plan)
4. [Testing Strategy](#4-testing-strategy)
5. [Rollback Procedures](#5-rollback-procedures)
6. [Risk Mitigation](#6-risk-mitigation)
7. [Success Criteria](#7-success-criteria)

---

## 1. MVP Scope Definition

### 1.1 What We're Building

**Single User Control:** One MQTT-controlled boolean toggle
- **ON:** Use HALB-centric style + DREIVIERTEL
- **OFF:** Use current NACH/VOR style (default)

### 1.2 Affected Time Slots

| Time | Current (OFF) | HALB-Centric (ON) | DREIVIERTEL? |
|------|---------------|-------------------|--------------|
| :20 | ZWANZIG NACH (H) | ZEHN VOR HALB (H+1) | No |
| :40 | ZWANZIG VOR (H+1) | ZEHN NACH HALB (H+1) | No |
| :45 | VIERTEL VOR (H+1) | DREIVIERTEL (H+1) | **Yes** |

**All other time slots:** Unchanged

### 1.3 Storage Requirements

**Add to `brightness_config_t` struct:**
```c
bool use_halb_centric_style;  // 1 byte, default: false
```

**NVS Storage:** Extend existing "brightness" namespace, no new namespace needed

### 1.4 MQTT Integration

**Topics:**
```
home/[DEVICE_NAME]/brightness/config/set
  Payload: {"halb_centric": true}  or  {"halb_centric": false}

home/[DEVICE_NAME]/brightness/config/state
  Payload: {...existing fields..., "halb_centric": false}
```

**Home Assistant Entity:** 1 new SWITCH
```yaml
name: HALB-Centric Time Style
unique_id: esp32_wordclock_halb_style
state_topic: home/esp32_core/brightness/config/state
command_topic: home/esp32_core/brightness/config/set
value_template: "{{ value_json.halb_centric }}"
icon: mdi:clock-text-outline
```

### 1.5 What We're NOT Building

❌ Separate configuration component (reuse brightness_config)
❌ Independent DREIVIERTEL toggle (automatic with HALB style)
❌ Style enum (just boolean)
❌ SELECT entity (use SWITCH instead)
❌ Separate thread mutex (use brightness_config mutex)

---

## 2. Implementation Strategy

### 2.1 Guiding Principles

1. **One change at a time** - Commit after each working step
2. **Test before proceeding** - Verify each micro-step independently
3. **Keep it compiling** - Never break the build
4. **Document as you go** - Update comments immediately
5. **Rollback-friendly** - Each step is independently revertible

### 2.2 Development Phases

**Phase 1: Configuration (Day 1 Morning - 3-4 hours)**
- Add boolean field to brightness_config
- Update NVS save/load
- Test persistence

**Phase 2: Display Logic (Day 1 Afternoon - 2-3 hours)**
- Modify :20, :40, :45 cases
- Test all 12 time slots
- Verify grammar edge cases

**Phase 3: Thread Safety (Day 1 Evening - 1-2 hours)**
- Add accessor functions
- Test concurrent access

**Phase 4: MQTT Integration (Day 2 - 4-5 hours)**
- Add command handler
- Update discovery
- Test HA integration

**Phase 5: Testing & Documentation (Day 3-4 - variable)**
- Comprehensive testing
- Documentation updates
- Hardware validation

---

## 3. Detailed Step-by-Step Plan

### DAY 1 - PHASE 1: Configuration Foundation

---

#### STEP 1.1: Add Configuration Field (15 min)

**File:** `components/brightness_config/include/brightness_config.h`

**Location:** Line 46 (in `brightness_config_t` struct)

**Change:**
```c
// Combined brightness configuration
typedef struct {
    light_sensor_config_t light_sensor;
    potentiometer_config_t potentiometer;
    uint8_t global_minimum_brightness;
    bool is_initialized;
    bool use_halb_centric_style;  // NEW: HALB-centric time expression style
} brightness_config_t;
```

**Testing:**
```bash
# Verify compilation only
idf.py build
```

**Expected Result:**
- ✅ Build succeeds
- ✅ No warnings

**Rollback:** Delete added line

**Commit Message:**
```
Add use_halb_centric_style field to brightness_config_t

- Add boolean field for HALB-centric time expression style
- Default: false (current NACH/VOR style)
- No functional changes yet
```

---

#### STEP 1.2: Initialize Default Value (10 min)

**File:** `components/brightness_config/brightness_config.c`

**Location:** Line 11 (global initialization)

**Change:**
```c
// Global brightness configuration
brightness_config_t g_brightness_config = {
    .light_sensor = DEFAULT_LIGHT_SENSOR_CONFIG,
    .potentiometer = DEFAULT_POTENTIOMETER_CONFIG,
    .is_initialized = false,
    .use_halb_centric_style = false  // NEW: Default to NACH/VOR style
};
```

**Testing:**
```bash
# Build and check initialization
idf.py build

# Manual verification: Check struct initialization in build output
# No runtime test needed yet (read-only)
```

**Expected Result:**
- ✅ Build succeeds
- ✅ Default value is false

**Rollback:** Delete added line

**Commit Message:**
```
Initialize use_halb_centric_style to false (default)

- Default to current NACH/VOR time expression style
- No behavior change
```

---

#### STEP 1.3: Add NVS Save Support (20 min)

**File:** `components/brightness_config/brightness_config.c`

**Location:** After line 99 (in `brightness_config_save_to_nvs()`)

**Change:**
```c
// Save potentiometer configuration as blob
ret = nvs_set_blob(nvs_handle, NVS_KEY_POTENTIOMETER,
                   &g_brightness_config.potentiometer,
                   sizeof(potentiometer_config_t));
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save potentiometer config: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
}

// NEW: Save HALB-centric style preference
ret = nvs_set_u8(nvs_handle, "halb_style", g_brightness_config.use_halb_centric_style ? 1 : 0);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save halb_centric style: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
}

// Commit changes
ret = nvs_commit(nvs_handle);
```

**Testing:**
```bash
# Build
idf.py build

# Flash and test NVS save
idf.py flash monitor

# In console:
# 1. Manually trigger save (if test function exists)
# 2. OR wait for normal save to occur
# 3. Check logs for "Failed to save halb_centric style" (should NOT appear)
```

**Expected Result:**
- ✅ Build succeeds
- ✅ No error logs when saving configuration
- ✅ NVS commit succeeds

**Rollback:** Delete added block

**Commit Message:**
```
Add NVS save support for use_halb_centric_style

- Save boolean as uint8_t (0 or 1)
- Key: "halb_style"
- Part of existing "brightness" NVS namespace
```

---

#### STEP 1.4: Add NVS Load Support (20 min)

**File:** `components/brightness_config/brightness_config.c`

**Location:** After line 154 (in `brightness_config_load_from_nvs()`)

**Change:**
```c
// Load potentiometer configuration
length = sizeof(potentiometer_config_t);
ret = nvs_get_blob(nvs_handle, NVS_KEY_POTENTIOMETER,
                   &g_brightness_config.potentiometer, &length);
if (ret != ESP_OK || length != sizeof(potentiometer_config_t)) {
    ESP_LOGW(TAG, "Failed to load potentiometer config: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
}

// NEW: Load HALB-centric style preference
uint8_t halb_style_val = 0;
ret = nvs_get_u8(nvs_handle, "halb_style", &halb_style_val);
if (ret == ESP_OK) {
    g_brightness_config.use_halb_centric_style = (halb_style_val != 0);
    ESP_LOGI(TAG, "Loaded HALB-centric style: %s",
             g_brightness_config.use_halb_centric_style ? "enabled" : "disabled");
} else if (ret == ESP_ERR_NVS_NOT_FOUND) {
    // First boot or old config - use default (false)
    g_brightness_config.use_halb_centric_style = false;
    ESP_LOGI(TAG, "HALB-centric style not in NVS, using default: disabled");
} else {
    ESP_LOGW(TAG, "Failed to load HALB-centric style: %s (using default)", esp_err_to_name(ret));
    g_brightness_config.use_halb_centric_style = false;
}

ESP_LOGI(TAG, "Brightness configuration loaded from NVS successfully");
nvs_close(nvs_handle);
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# TEST 1: First boot (no saved value)
# Expected log: "HALB-centric style not in NVS, using default: disabled"

# TEST 2: After save/reboot cycle
# 1. Save configuration (trigger via MQTT or test function)
# 2. Reboot (esp_restart())
# 3. Expected log: "Loaded HALB-centric style: disabled"
```

**Expected Result:**
- ✅ Build succeeds
- ✅ First boot: Uses default (false)
- ✅ After save: Loads saved value
- ✅ Graceful handling of missing key

**Rollback:** Delete added block

**Commit Message:**
```
Add NVS load support for use_halb_centric_style

- Load boolean from NVS key "halb_style"
- Graceful handling if key not found (use default)
- Log loaded value for debugging
```

---

#### STEP 1.5: Update Reset Function (10 min)

**File:** `components/brightness_config/brightness_config.c`

**Location:** Line 176 (in `brightness_config_reset_to_defaults()`)

**Change:**
```c
g_brightness_config.light_sensor = default_light;
g_brightness_config.potentiometer = default_pot;

// NEW: Reset HALB-centric style to default
g_brightness_config.use_halb_centric_style = false;
ESP_LOGI(TAG, "HALB-centric style reset to: disabled");

// Save to NVS
return brightness_config_save_to_nvs();
```

**Testing:**
```bash
# Build
idf.py build flash monitor

# TEST: Reset functionality
# 1. Manually set use_halb_centric_style = true (via MQTT later, or code)
# 2. Trigger reset (MQTT: home/esp32_core/brightness/config/reset)
# 3. Expected log: "HALB-centric style reset to: disabled"
# 4. Verify value is false after reset
```

**Expected Result:**
- ✅ Reset sets use_halb_centric_style to false
- ✅ Log confirms reset
- ✅ Value persists after reboot

**Rollback:** Delete added lines

**Commit Message:**
```
Reset use_halb_centric_style to false in reset_to_defaults

- Reset HALB-centric style when resetting brightness config
- Default: disabled (NACH/VOR style)
```

---

#### STEP 1.6: Add Getter Function (15 min)

**File:** `components/brightness_config/include/brightness_config.h`

**Location:** After line 94 (end of function prototypes)

**Change:**
```c
const char* brightness_curve_type_to_string(brightness_curve_type_t type);
brightness_curve_type_t brightness_curve_type_from_string(const char* str);

// NEW: HALB-centric style accessors
bool brightness_config_get_halb_centric_style(void);
esp_err_t brightness_config_set_halb_centric_style(bool enabled);

#ifdef __cplusplus
```

**File:** `components/brightness_config/brightness_config.c`

**Location:** End of file (after line 411)

**Change:**
```c
uint8_t brightness_config_get_safety_limit_max(void)
{
    return g_brightness_config.potentiometer.safety_limit_max;
}

// NEW: Get HALB-centric style preference
bool brightness_config_get_halb_centric_style(void)
{
    return g_brightness_config.use_halb_centric_style;
}

// NEW: Set HALB-centric style preference
esp_err_t brightness_config_set_halb_centric_style(bool enabled)
{
    if (g_brightness_config.use_halb_centric_style != enabled) {
        g_brightness_config.use_halb_centric_style = enabled;
        ESP_LOGI(TAG, "HALB-centric style set to: %s", enabled ? "enabled" : "disabled");
    }
    return ESP_OK;
}
```

**Testing:**
```bash
# Build
idf.py build

# No runtime test needed yet (will test in display logic)
```

**Expected Result:**
- ✅ Build succeeds
- ✅ Functions compile without errors

**Rollback:** Delete added functions

**Commit Message:**
```
Add getter/setter for use_halb_centric_style

- brightness_config_get_halb_centric_style()
- brightness_config_set_halb_centric_style(bool)
- Logging for debug visibility
```

---

### PHASE 1 CHECKPOINT: Configuration Complete ✅

**Verification:**
```bash
# Full build
idf.py fullclean && idf.py build

# Flash and verify logs
idf.py flash monitor

# Check startup logs:
# ✅ "Brightness configuration loaded from NVS successfully"
# ✅ "HALB-centric style: disabled" (or "enabled" if previously saved)
```

**Git Commit:**
```bash
git add components/brightness_config/
git commit -m "Add HALB-centric style configuration to brightness_config

- Add use_halb_centric_style boolean field
- NVS save/load support
- Reset to defaults support
- Getter/setter functions
- Default: false (NACH/VOR style)
- No display logic changes yet

Part of MVP time expression feature
"
```

---

### DAY 1 - PHASE 2: Display Logic

---

#### STEP 2.1: Add Include for Config (5 min)

**File:** `main/wordclock_display.c`

**Location:** Line 15 (after existing includes)

**Change:**
```c
#include "wordclock_display.h"
#include "i2c_devices.h"
#include "wordclock_time.h"
#include "thread_safety.h"
#include "brightness_config.h"  // NEW: For HALB-centric style access
#include "esp_log.h"
```

**Testing:**
```bash
idf.py build
```

**Expected Result:**
- ✅ Build succeeds

**Rollback:** Delete added include

**Commit Message:**
```
Add brightness_config.h include to wordclock_display.c

- Preparation for HALB-centric style access
- No functional changes
```

---

#### STEP 2.2: Modify :20 Case (20 min)

**File:** `main/wordclock_display.c`

**Location:** Lines 189-192 (case 20)

**Change:**
```c
case 20:
    // NEW: Check if HALB-centric style is enabled
    if (brightness_config_get_halb_centric_style()) {
        // HALB-centric: "ZEHN VOR HALB"
        set_word_leds(new_led_state, "ZEHN_M", brightness);
        set_word_leds(new_led_state, "VOR", brightness);
        set_word_leds(new_led_state, "HALB", brightness);
        ESP_LOGD(TAG, "Time :20 - HALB-centric: ZEHN VOR HALB");
    } else {
        // Standard NACH/VOR: "ZWANZIG NACH"
        set_word_leds(new_led_state, "ZWANZIG", brightness);
        set_word_leds(new_led_state, "NACH", brightness);
        ESP_LOGD(TAG, "Time :20 - NACH/VOR: ZWANZIG NACH");
    }
    break;
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# TEST 1: Default style (NACH/VOR)
# Set time to 14:20 via MQTT: {"command": "set_time 14:20"}
# Expected: "ZWANZIG NACH ZWEI" displayed
# Expected log: "Time :20 - NACH/VOR: ZWANZIG NACH"

# TEST 2: HALB-centric style
# 1. Manually set use_halb_centric_style = true in code (temporary)
# 2. Rebuild and flash
# 3. Set time to 14:20
# Expected: "ZEHN VOR HALB DREI" displayed
# Expected log: "Time :20 - HALB-centric: ZEHN VOR HALB"

# TEST 3: Verify hour calculation (H+1 for >=25 min)
# Time 14:20 should display hour "DREI" (not "ZWEI")
# This is already handled by wordclock_get_display_hour()
```

**Expected Result:**
- ✅ NACH/VOR style: "ZWANZIG NACH" displayed
- ✅ HALB-centric style: "ZEHN VOR HALB" displayed
- ✅ Correct hour shown (H+1 already working)
- ✅ Debug logs appear

**Rollback:** Revert to original case 20 code

**Commit Message:**
```
Implement HALB-centric style for :20 time slot

- NACH/VOR (default): "ZWANZIG NACH"
- HALB-centric: "ZEHN VOR HALB"
- Add debug logging for visibility
- Hour calculation unchanged (already correct)
```

---

#### STEP 2.3: Modify :40 Case (20 min)

**File:** `main/wordclock_display.c`

**Location:** Lines 206-209 (case 40)

**Change:**
```c
case 40:
    // NEW: Check if HALB-centric style is enabled
    if (brightness_config_get_halb_centric_style()) {
        // HALB-centric: "ZEHN NACH HALB"
        set_word_leds(new_led_state, "ZEHN_M", brightness);
        set_word_leds(new_led_state, "NACH", brightness);
        set_word_leds(new_led_state, "HALB", brightness);
        ESP_LOGD(TAG, "Time :40 - HALB-centric: ZEHN NACH HALB");
    } else {
        // Standard NACH/VOR: "ZWANZIG VOR"
        set_word_leds(new_led_state, "ZWANZIG", brightness);
        set_word_leds(new_led_state, "VOR", brightness);
        ESP_LOGD(TAG, "Time :40 - NACH/VOR: ZWANZIG VOR");
    }
    break;
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# TEST 1: Default style (NACH/VOR)
# Set time to 14:40 via MQTT: {"command": "set_time 14:40"}
# Expected: "ZWANZIG VOR DREI" displayed
# Expected log: "Time :40 - NACH/VOR: ZWANZIG VOR"

# TEST 2: HALB-centric style
# 1. Manually set use_halb_centric_style = true
# 2. Rebuild and flash
# 3. Set time to 14:40
# Expected: "ZEHN NACH HALB DREI" displayed
# Expected log: "Time :40 - HALB-centric: ZEHN NACH HALB"
```

**Expected Result:**
- ✅ NACH/VOR style: "ZWANZIG VOR" displayed
- ✅ HALB-centric style: "ZEHN NACH HALB" displayed
- ✅ Correct hour shown (H+1)
- ✅ Debug logs appear

**Rollback:** Revert to original case 40 code

**Commit Message:**
```
Implement HALB-centric style for :40 time slot

- NACH/VOR (default): "ZWANZIG VOR"
- HALB-centric: "ZEHN NACH HALB"
- Add debug logging
```

---

#### STEP 2.4: Modify :45 Case (DREIVIERTEL) (20 min)

**File:** `main/wordclock_display.c`

**Location:** Lines 210-213 (case 45)

**Change:**
```c
case 45:
    // NEW: Check if HALB-centric style is enabled
    if (brightness_config_get_halb_centric_style()) {
        // HALB-centric with DREIVIERTEL
        set_word_leds(new_led_state, "DREIVIERTEL", brightness);
        ESP_LOGD(TAG, "Time :45 - HALB-centric: DREIVIERTEL");
    } else {
        // Standard NACH/VOR: "VIERTEL VOR"
        set_word_leds(new_led_state, "VIERTEL", brightness);
        set_word_leds(new_led_state, "VOR", brightness);
        ESP_LOGD(TAG, "Time :45 - NACH/VOR: VIERTEL VOR");
    }
    break;
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# TEST 1: Default style (NACH/VOR)
# Set time to 14:45 via MQTT: {"command": "set_time 14:45"}
# Expected: "VIERTEL VOR DREI" displayed
# Expected log: "Time :45 - NACH/VOR: VIERTEL VOR"

# TEST 2: HALB-centric style with DREIVIERTEL
# 1. Manually set use_halb_centric_style = true
# 2. Rebuild and flash
# 3. Set time to 14:45
# Expected: "DREIVIERTEL DREI" displayed
# Expected log: "Time :45 - HALB-centric: DREIVIERTEL"

# TEST 3: Verify DREIVIERTEL word exists in LED matrix
# Check word_database[] has "DREIVIERTEL" entry (row 2, cols 0-10)
# This should already exist (verified in analysis)
```

**Expected Result:**
- ✅ NACH/VOR style: "VIERTEL VOR" displayed
- ✅ HALB-centric style: "DREIVIERTEL" displayed
- ✅ Correct hour shown (H+1)
- ✅ DREIVIERTEL word displays correctly on hardware

**Rollback:** Revert to original case 45 code

**Commit Message:**
```
Implement DREIVIERTEL for :45 with HALB-centric style

- NACH/VOR (default): "VIERTEL VOR"
- HALB-centric: "DREIVIERTEL"
- Uses existing DREIVIERTEL word in LED matrix (row 2)
```

---

#### STEP 2.5: Test All 12 Time Slots (60 min)

**Purpose:** Verify nothing broke for unchanged time slots

**Test Script:**
```bash
# Create test_all_times.sh
#!/bin/bash
TIMES=("00" "05" "10" "15" "20" "25" "30" "35" "40" "45" "50" "55")

for MIN in "${TIMES[@]}"; do
    echo "Testing 14:$MIN"
    mosquitto_pub -h <broker> -t "home/esp32_core/command" -m "set_time 14:$MIN"
    sleep 5  # Allow time for display update
    # Manually verify display shows correct words
done
```

**Manual Verification Matrix:**

| Time | Expected (NACH/VOR OFF) | Expected (HALB ON) | Status |
|------|--------------------------|---------------------|--------|
| 14:00 | ES IST ZWEI UHR | (same) | ☐ |
| 14:05 | ES IST FÜNF NACH ZWEI | (same) | ☐ |
| 14:10 | ES IST ZEHN NACH ZWEI | (same) | ☐ |
| 14:15 | ES IST VIERTEL NACH ZWEI | (same) | ☐ |
| **14:20** | **ES IST ZWANZIG NACH ZWEI** | **ES IST ZEHN VOR HALB DREI** | ☐ |
| 14:25 | ES IST FÜNF VOR HALB DREI | (same) | ☐ |
| 14:30 | ES IST HALB DREI | (same) | ☐ |
| 14:35 | ES IST FÜNF NACH HALB DREI | (same) | ☐ |
| **14:40** | **ES IST ZWANZIG VOR DREI** | **ES IST ZEHN NACH HALB DREI** | ☐ |
| **14:45** | **ES IST VIERTEL VOR DREI** | **ES IST DREIVIERTEL DREI** | ☐ |
| 14:50 | ES IST ZEHN VOR DREI | (same) | ☐ |
| 14:55 | ES IST FÜNF VOR DREI | (same) | ☐ |

**Grammar Edge Cases:**
```bash
# Test "EIN UHR" vs "EINS"
set_time 01:00  # Expected: "ES IST EIN UHR" (both styles)
set_time 12:55  # Expected: "ES IST FÜNF VOR EINS" (both styles)

# Test hour transitions
set_time 23:59  # Verify no crash
set_time 00:00  # Expected: "ES IST ZWÖLF UHR"
set_time 11:59  # Verify transitions to 12:00
```

**Expected Result:**
- ✅ All 12 time slots display correctly (NACH/VOR mode)
- ✅ :20, :40, :45 change when HALB-centric enabled
- ✅ Grammar edge cases work (EIN UHR, EINS)
- ✅ Hour transitions work (23:59→00:00)
- ✅ No crashes or display corruption

**Rollback:** Revert all display logic changes

**Commit Message:**
```
Verify all 12 time slots work correctly

- Tested NACH/VOR mode: all time slots correct
- Tested HALB-centric mode: :20, :40, :45 changed
- Grammar edge cases verified (EIN UHR, EINS)
- Hour transitions working
- No regressions detected
```

---

### PHASE 2 CHECKPOINT: Display Logic Complete ✅

**Git Commit:**
```bash
git add main/wordclock_display.c
git commit -m "Implement HALB-centric time expressions in display logic

Changes:
- :20 → ZWANZIG NACH / ZEHN VOR HALB
- :40 → ZWANZIG VOR / ZEHN NACH HALB
- :45 → VIERTEL VOR / DREIVIERTEL

All other time slots unchanged
Grammar rules preserved (EIN UHR, EINS)
Debug logging added for visibility

Tested: All 12 time slots verified on hardware
"
```

---

### DAY 1 - PHASE 3: Thread Safety

---

#### STEP 3.1: Add Thread-Safe Getter (15 min)

**File:** `main/thread_safety.h`

**Location:** After existing brightness accessors (around line 60)

**Change:**
```c
// Brightness accessors
uint8_t thread_safe_get_global_brightness(void);
void thread_safe_set_global_brightness(uint8_t brightness);
uint8_t thread_safe_get_individual_brightness(void);
void thread_safe_set_individual_brightness(uint8_t brightness);

// NEW: Time expression style accessor
bool thread_safe_get_halb_centric_style(void);
void thread_safe_set_halb_centric_style(bool enabled);

#ifdef __cplusplus
```

**File:** `main/thread_safety.c`

**Location:** End of file (after other accessors)

**Change:**
```c
// NEW: Thread-safe getter for HALB-centric style
bool thread_safe_get_halb_centric_style(void)
{
    bool value;

    // Use brightness mutex (no need for new mutex, low contention)
    if (xSemaphoreTake(brightness_mutex, portMAX_DELAY) == pdTRUE) {
        value = brightness_config_get_halb_centric_style();
        xSemaphoreGive(brightness_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire brightness mutex for halb_centric_style get");
        value = false;  // Safe default
    }

    return value;
}

// NEW: Thread-safe setter for HALB-centric style
void thread_safe_set_halb_centric_style(bool enabled)
{
    if (xSemaphoreTake(brightness_mutex, portMAX_DELAY) == pdTRUE) {
        brightness_config_set_halb_centric_style(enabled);
        xSemaphoreGive(brightness_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire brightness mutex for halb_centric_style set");
    }
}
```

**Testing:**
```bash
# Build
idf.py build

# No specific runtime test needed yet
# Will test when integrated with MQTT
```

**Expected Result:**
- ✅ Build succeeds
- ✅ Functions compile without errors

**Rollback:** Delete added functions

**Commit Message:**
```
Add thread-safe accessors for HALB-centric style

- thread_safe_get_halb_centric_style()
- thread_safe_set_halb_centric_style(bool)
- Reuse existing brightness_mutex (appropriate scope)
- Safe defaults on mutex failure
```

---

#### STEP 3.2: Update Display to Use Thread-Safe Accessor (10 min)

**File:** `main/wordclock_display.c`

**Location:** Lines 189-221 (cases 20, 40, 45)

**Change:** Replace `brightness_config_get_halb_centric_style()` with `thread_safe_get_halb_centric_style()`

```c
case 20:
    // Use thread-safe accessor
    if (thread_safe_get_halb_centric_style()) {
        // HALB-centric: "ZEHN VOR HALB"
        set_word_leds(new_led_state, "ZEHN_M", brightness);
        set_word_leds(new_led_state, "VOR", brightness);
        set_word_leds(new_led_state, "HALB", brightness);
        ESP_LOGD(TAG, "Time :20 - HALB-centric: ZEHN VOR HALB");
    } else {
        // Standard NACH/VOR: "ZWANZIG NACH"
        set_word_leds(new_led_state, "ZWANZIG", brightness);
        set_word_leds(new_led_state, "NACH", brightness);
        ESP_LOGD(TAG, "Time :20 - NACH/VOR: ZWANZIG NACH");
    }
    break;

// Same change for case 40 and case 45
```

**Also update include:**
```c
#include "thread_safety.h"  // Change to use thread_safe_get_halb_centric_style()
#include "brightness_config.h"  // Can keep for direct access if needed elsewhere
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# Re-run all 12 time slots test from Step 2.5
# Verify behavior identical to before (thread-safety is transparent)
```

**Expected Result:**
- ✅ All time slots work identically to Step 2.5
- ✅ No deadlocks
- ✅ No mutex warnings in logs

**Rollback:** Revert to non-thread-safe accessor

**Commit Message:**
```
Use thread-safe accessor in display logic

- Replace brightness_config_get_halb_centric_style()
  with thread_safe_get_halb_centric_style()
- Thread-safe access from display update task
- No functional changes
```

---

### PHASE 3 CHECKPOINT: Thread Safety Complete ✅

**Git Commit:**
```bash
git add main/thread_safety.c main/thread_safety.h main/wordclock_display.c
git commit -m "Add thread-safe accessors for HALB-centric style

- New accessors use existing brightness_mutex
- Display logic updated to use thread-safe access
- Tested: All time slots work with thread-safe access
- No deadlocks detected
"
```

---

### DAY 2 - PHASE 4: MQTT Integration

---

#### STEP 4.1: Add MQTT Command Handler (30 min)

**File:** `components/mqtt_manager/mqtt_manager.c` or create new handler in `main/wordclock_mqtt_handlers.c`

**Strategy:** Extend existing brightness config handler

**Location:** Find existing brightness config handler (search for "brightness/config/set")

**Change:** Add parsing for "halb_centric" field

```c
// Example integration (exact location depends on current MQTT handler structure)
void handle_brightness_config_set(cJSON *json) {
    // ... existing code for light sensor, potentiometer ...

    // NEW: Handle halb_centric style toggle
    cJSON *halb_centric_json = cJSON_GetObjectItem(json, "halb_centric");
    if (halb_centric_json && cJSON_IsBool(halb_centric_json)) {
        bool enabled = cJSON_IsTrue(halb_centric_json);

        thread_safe_set_halb_centric_style(enabled);
        ESP_LOGI(TAG, "HALB-centric style set to: %s", enabled ? "enabled" : "disabled");

        // Save to NVS
        brightness_config_save_to_nvs();

        // Trigger display refresh
        wordclock_time_t current_time;
        if (ds3231_get_time_struct(&current_time) == ESP_OK) {
            display_german_time(&current_time);
        }

        // Publish updated state
        publish_brightness_config_state();
    }
}
```

**Testing:**
```bash
# Build and flash
idf.py build flash monitor

# TEST 1: Enable HALB-centric via MQTT
mosquitto_pub -h <broker> -t "home/esp32_core/brightness/config/set" \
  -m '{"halb_centric": true}'

# Expected logs:
# - "HALB-centric style set to: enabled"
# - Display updates immediately if showing :20, :40, or :45
# - Configuration saved to NVS

# TEST 2: Disable HALB-centric via MQTT
mosquitto_pub -h <broker> -t "home/esp32_core/brightness/config/set" \
  -m '{"halb_centric": false}'

# Expected logs:
# - "HALB-centric style set to: disabled"
# - Display updates immediately

# TEST 3: Verify persistence
# 1. Enable HALB-centric
# 2. Reboot ESP32 (esp_restart())
# 3. Check logs: "Loaded HALB-centric style: enabled"
# 4. Display should use HALB-centric style after reboot
```

**Expected Result:**
- ✅ MQTT command toggles style
- ✅ Display updates immediately
- ✅ Configuration persists across reboots
- ✅ Logs confirm changes

**Rollback:** Remove halb_centric parsing

**Commit Message:**
```
Add MQTT command handler for HALB-centric style toggle

- Extend brightness/config/set handler
- Parse "halb_centric" boolean field
- Immediate display refresh on change
- Auto-save to NVS
```

---

#### STEP 4.2: Update State Publishing (20 min)

**File:** MQTT manager (same as Step 4.1)

**Location:** Find `publish_brightness_config_state()` function

**Change:** Add "halb_centric" field to published JSON

```c
void publish_brightness_config_state(void) {
    char state_topic[128];
    char payload[512];  // May need larger buffer

    snprintf(state_topic, sizeof(state_topic),
             "home/%s/brightness/config/state", MQTT_DEVICE_NAME);

    bool halb_centric = thread_safe_get_halb_centric_style();

    // Build JSON with existing fields + new halb_centric field
    snprintf(payload, sizeof(payload),
             "{\"individual_brightness\":%d,"
             "\"global_brightness\":%d,"
             "\"halb_centric\":%s}",  // NEW field
             thread_safe_get_individual_brightness(),
             thread_safe_get_global_brightness(),
             halb_centric ? "true" : "false");

    mqtt_manager_publish(state_topic, payload, 1, true);  // QoS 1, retain
}
```

**Testing:**
```bash
# Subscribe to state topic
mosquitto_sub -h <broker> -t "home/esp32_core/brightness/config/state" -v

# Trigger state publish:
# 1. Toggle halb_centric via MQTT
# 2. Reboot ESP32 (auto-publishes on connect)
# 3. Reset brightness config (triggers state publish)

# Expected payload:
# {"individual_brightness":60,"global_brightness":180,"halb_centric":false}
```

**Expected Result:**
- ✅ State includes "halb_centric" field
- ✅ Value matches current setting
- ✅ Published on MQTT connect
- ✅ Published when changed

**Rollback:** Remove halb_centric from JSON

**Commit Message:**
```
Add halb_centric field to brightness config state publishing

- Include boolean in brightness/config/state topic
- Published on MQTT connect
- Published when changed
- QoS 1, retained
```

---

#### STEP 4.3: Add Home Assistant Discovery (45 min)

**File:** `components/mqtt_discovery/mqtt_discovery.c`

**Location:** Find `mqtt_discovery_publish_brightness_config()` or similar function

**Change:** Add SWITCH entity for HALB-centric toggle

```c
esp_err_t mqtt_discovery_publish_brightness_config(void) {
    // ... existing discovery messages ...

    // NEW: SWITCH for HALB-centric time style
    snprintf(topic, sizeof(topic),
             "%s/switch/%s/halb_style/config",
             discovery_config.discovery_prefix,
             discovery_config.node_id);

    snprintf(payload, sizeof(payload),
        "{"
            "\"name\":\"HALB-Centric Time Style\","
            "\"unique_id\":\"%s_halb_style\","
            "\"object_id\":\"halb_centric_style\","
            "\"state_topic\":\"home/%s/brightness/config/state\","
            "\"command_topic\":\"home/%s/brightness/config/set\","
            "\"value_template\":\"{{ value_json.halb_centric }}\","
            "\"payload_on\":\"{\\\"halb_centric\\\":true}\","
            "\"payload_off\":\"{\\\"halb_centric\\\":false}\","
            "\"state_on\":\"true\","
            "\"state_off\":\"false\","
            "\"icon\":\"mdi:clock-text-outline\","
            "\"device\":{"
                "\"identifiers\":[\"%s\"],"
                "\"name\":\"%s\","
                "\"model\":\"%s\","
                "\"manufacturer\":\"%s\","
                "\"sw_version\":\"%s\""
            "}"
        "}",
        device.identifiers,
        MQTT_DEVICE_NAME, MQTT_DEVICE_NAME,
        device.identifiers, device.name, device.model,
        device.manufacturer, device.sw_version
    );

    ret = mqtt_manager_publish(topic, payload, 1, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish HALB-centric style switch");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));  // Discovery delay

    ESP_LOGI(TAG, "✅ HALB-centric style switch published");
    return ESP_OK;
}
```

**Testing:**
```bash
# 1. Delete existing HA discovery (force re-discovery)
mosquitto_pub -h <broker> -t "homeassistant/switch/esp32_wordclock/halb_style/config" -n -r

# 2. Restart ESP32 or trigger discovery manually

# 3. Check Home Assistant:
# - Settings → Devices & Services → MQTT
# - Find "Word Clock" device
# - Should see new entity: "HALB-Centric Time Style" (switch)

# 4. Test toggle in Home Assistant:
# - Turn switch ON → Display changes to HALB-centric
# - Turn switch OFF → Display changes back to NACH/VOR
# - Verify immediate display update
```

**Expected Result:**
- ✅ New SWITCH entity appears in Home Assistant
- ✅ Entity has correct name and icon
- ✅ Toggle works bidirectionally (HA ↔ ESP32)
- ✅ State synchronization works
- ✅ Entity persists after HA restart

**Rollback:** Delete discovery message

**Commit Message:**
```
Add Home Assistant discovery for HALB-centric style switch

- New SWITCH entity: "HALB-Centric Time Style"
- Icon: mdi:clock-text-outline
- Bidirectional control (HA ↔ ESP32)
- State synchronization
```

---

### PHASE 4 CHECKPOINT: MQTT Integration Complete ✅

**Full End-to-End Test:**
```bash
# 1. Toggle via Home Assistant
# 2. Verify display changes immediately
# 3. Check state topic updates
# 4. Reboot ESP32
# 5. Verify style persists
# 6. Toggle via MQTT directly
# 7. Verify HA entity updates
# 8. Test at :20, :40, :45 times
```

**Git Commit:**
```bash
git add components/mqtt_manager/ components/mqtt_discovery/
git commit -m "Add MQTT integration for HALB-centric time style

MQTT Topics:
- home/esp32_core/brightness/config/set (command)
- home/esp32_core/brightness/config/state (state)

Home Assistant:
- New SWITCH entity: HALB-Centric Time Style
- Auto-discovery working
- Bidirectional control

Features:
- Immediate display refresh on change
- NVS persistence
- State publishing on connect
- Tested end-to-end
"
```

---

### DAY 3-4 - PHASE 5: Testing & Documentation

---

#### STEP 5.1: Comprehensive Hardware Testing (2-3 hours)

**Test Matrix:**

**Test 1: All Time Slots (Both Styles)**
```bash
# For each style (NACH/VOR and HALB-centric):
# Test all 12 time slots (00, 05, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55)
#
# Document with photos or video:
# - Display correctness
# - LED matrix appearance
# - Transition smoothness
```

**Test 2: Style Toggle at Different Times**
```bash
# Set time to 14:20
# Toggle HALB-centric ON → Verify display updates
# Toggle HALB-centric OFF → Verify display updates
#
# Repeat for 14:40 and 14:45
```

**Test 3: Persistence**
```bash
# 1. Enable HALB-centric
# 2. Reboot (power cycle)
# 3. Verify style enabled after boot
# 4. Disable HALB-centric
# 5. Reboot
# 6. Verify style disabled after boot
```

**Test 4: MQTT Integration**
```bash
# 1. Toggle via Home Assistant UI
# 2. Toggle via MQTT command line
# 3. Toggle via reset (brightness/config/reset)
# 4. Verify state topic updates correctly
# 5. Verify HA entity state matches ESP32
```

**Test 5: Edge Cases**
```bash
# Grammar:
# - 1:00 → "ES IST EIN UHR" (both styles)
# - 12:55 → "ES IST FÜNF VOR EINS" (both styles)
#
# Hour transitions:
# - 23:59 → 00:00
# - 11:59 → 12:00
# - 14:19 → 14:20 (style change visible)
# - 14:20 → 14:21 (return to normal)
```

**Test 6: Transitions**
```bash
# With transitions enabled:
# 1. Set time to 14:20
# 2. Toggle HALB-centric → Verify smooth transition
# 3. Verify ZWANZIG fades out, ZEHN/VOR/HALB fade in
# 4. No flicker or corruption
```

**Test 7: Brightness Control**
```bash
# With HALB-centric enabled:
# 1. Adjust potentiometer → Verify brightness changes
# 2. Change global brightness via MQTT → Verify update
# 3. Reset brightness config → Verify HALB-centric resets to OFF
```

**Test 8: Long-Running Stability**
```bash
# Leave device running for 24 hours
# Toggle style every hour (automation)
# Monitor logs for:
# - Memory leaks
# - Mutex deadlocks
# - Crashes
# - NVS errors
```

**Expected Result:**
- ✅ All tests pass
- ✅ No crashes or errors
- ✅ Smooth operation
- ✅ Persistent configuration

**Commit Message:**
```
Hardware testing complete - all tests pass

Tests performed:
- All 12 time slots (both styles)
- Style toggling at runtime
- NVS persistence across reboots
- MQTT integration
- Grammar edge cases
- Hour transitions
- Smooth transitions
- Brightness compatibility
- 24-hour stability test

Results: All pass, no issues detected
```

---

#### STEP 5.2: Update User Documentation (1-2 hours)

**File:** `README.md`

**Location:** Home Assistant Integration section

**Changes:**
1. Update entity count (36 → 37)
2. Add HALB-Centric style description
3. Add example dashboard card

**File:** `Mqtt_User_Guide.md`

**Changes:**
1. Document new "halb_centric" field
2. Add examples
3. Update topic reference table

**File:** `docs/user/home-assistant-setup.md`

**Changes:**
1. Add screenshot of new SWITCH entity
2. Document usage
3. Add automation examples

**File:** `CLAUDE.md`

**Changes:**
1. Update entity count
2. Add technical note about HALB-centric implementation
3. Update MQTT topics list

**Commit Message:**
```
Update documentation for HALB-centric time style feature

Files updated:
- README.md (entity count, feature description)
- Mqtt_User_Guide.md (command reference)
- docs/user/home-assistant-setup.md (HA integration)
- CLAUDE.md (developer reference)

New content:
- HALB-centric style explanation
- MQTT command examples
- Home Assistant usage guide
```

---

#### STEP 5.3: Create Release Notes (30 min)

**File:** Create `docs/releases/v2.1.0-halb-centric.md`

**Content:**
```markdown
# Release v2.1.0 - HALB-Centric Time Expressions

**Release Date:** 2025-10-XX
**Type:** Feature Addition (MVP)

## What's New

### HALB-Centric Time Expression Style

Users can now choose between two German time expression styles:

**NACH/VOR Style (Default):**
- 14:20 → "ES IST ZWANZIG NACH ZWEI"
- 14:40 → "ES IST ZWANZIG VOR DREI"
- 14:45 → "ES IST VIERTEL VOR DREI"

**HALB-Centric Style (New):**
- 14:20 → "ES IST ZEHN VOR HALB DREI"
- 14:40 → "ES IST ZEHN NACH HALB DREI"
- 14:45 → "ES IST DREIVIERTEL DREI"

### How to Use

**Home Assistant:**
1. Find the "HALB-Centric Time Style" switch in your Word Clock device
2. Toggle ON to enable HALB-centric style
3. Toggle OFF to return to NACH/VOR style

**MQTT:**
```bash
# Enable HALB-centric
mosquitto_pub -h broker -t "home/esp32_core/brightness/config/set" \
  -m '{"halb_centric": true}'

# Disable
mosquitto_pub -h broker -t "home/esp32_core/brightness/config/set" \
  -m '{"halb_centric": false}'
```

### Technical Details

- Configuration stored in NVS (persists across reboots)
- Integrated into existing brightness configuration
- Thread-safe implementation
- No new components added (uses brightness_config)
- Immediate display refresh on change

### Compatibility

- ✅ Fully backward compatible
- ✅ Default: NACH/VOR style (no behavior change)
- ✅ Works with all existing features (transitions, brightness control)
- ✅ Home Assistant entity auto-discovery

### Known Limitations

- HALB-centric style affects only :20, :40, :45 time slots
- Other time slots unchanged (no alternative expressions exist)
- :25 and :35 variants not possible (LED matrix constraint)

## Upgrade Instructions

1. Flash new firmware: `idf.py flash`
2. Reboot device
3. New SWITCH entity appears automatically in Home Assistant
4. Default behavior unchanged (NACH/VOR style)

## Testing

- Tested on real hardware: ESP32 with 10×16 LED matrix
- Tested for 24+ hours continuous operation
- All time slots verified
- NVS persistence verified
- MQTT integration verified
- Home Assistant integration verified

## Contributors

- MVP Implementation: [Your Name]
- Feature Request: Community feedback
```

**Commit Message:**
```
Add release notes for v2.1.0 HALB-centric feature

- Complete feature documentation
- Usage instructions
- Technical details
- Upgrade guide
```

---

### PHASE 5 CHECKPOINT: Testing & Documentation Complete ✅

**Final Verification:**
```bash
# 1. Full build from clean state
idf.py fullclean && idf.py build

# 2. Flash and verify all functionality
idf.py flash monitor

# 3. Run through complete test matrix (Step 5.1)

# 4. Verify all documentation is accurate

# 5. Create final commit
git add .
git commit -m "Release v2.1.0: HALB-centric time expressions (MVP)

Complete MVP implementation of alternative German time expressions.

Features:
- HALB-centric style toggle (MQTT + Home Assistant)
- DREIVIERTEL support for :45
- NVS persistence
- Thread-safe implementation
- Full documentation

Technical:
- Reuses brightness_config component
- No new dependencies
- Fully backward compatible
- Tested 24+ hours

Changes affected time slots: :20, :40, :45
Default behavior: unchanged (NACH/VOR style)
"
```

---

## 4. Testing Strategy

### 4.1 Unit Testing Approach

**Per-Step Verification:**
- Each micro-step has specific test criteria
- Build verification before proceeding
- Functional verification where applicable
- Log verification for correct behavior

**Automated Tests (if framework exists):**
```c
// Example unit tests (if using Unity or similar)
TEST_CASE("halb_centric_style_default_is_false") {
    TEST_ASSERT_FALSE(brightness_config_get_halb_centric_style());
}

TEST_CASE("halb_centric_style_nvs_persistence") {
    brightness_config_set_halb_centric_style(true);
    brightness_config_save_to_nvs();

    // Simulate reboot
    brightness_config_load_from_nvs();

    TEST_ASSERT_TRUE(brightness_config_get_halb_centric_style());
}

TEST_CASE("display_20_minutes_nach_vor_style") {
    brightness_config_set_halb_centric_style(false);

    wordclock_time_t time = {.hours = 14, .minutes = 20};
    uint8_t led_state[10][16] = {0};

    build_led_state_matrix(&time, led_state);

    // Verify ZWANZIG and NACH are lit
    // Verify ZEHN, VOR, HALB are NOT lit
}

TEST_CASE("display_20_minutes_halb_centric_style") {
    brightness_config_set_halb_centric_style(true);

    wordclock_time_t time = {.hours = 14, .minutes = 20};
    uint8_t led_state[10][16] = {0};

    build_led_state_matrix(&time, led_state);

    // Verify ZEHN, VOR, HALB are lit
    // Verify ZWANZIG, NACH are NOT lit
}
```

### 4.2 Integration Testing

**Cross-Component Tests:**
```
Configuration ↔ Display:
- Set HALB-centric → Verify display changes
- Load from NVS → Verify display uses saved style

MQTT ↔ Configuration:
- MQTT command → Verify config changes
- Config change → Verify state published

Home Assistant ↔ MQTT:
- HA toggle → Verify MQTT command sent
- MQTT state → Verify HA entity updates
```

### 4.3 Hardware Testing

**LED Matrix Verification:**
- Visual inspection of each time slot
- Photo documentation of before/after toggle
- Verify no LED flickering or corruption
- Verify smooth transitions (if enabled)

**Long-Running Tests:**
- 24-hour continuous operation
- Toggle style every hour (automated)
- Monitor memory usage
- Check for mutex deadlocks

### 4.4 Regression Testing

**Ensure No Breakage:**
- All existing features still work
- Brightness control unaffected
- Transitions unaffected
- MQTT commands unaffected
- NTP sync unaffected

---

## 5. Rollback Procedures

### 5.1 Per-Step Rollback

Each step includes specific rollback instructions:
- Delete added code
- Revert to previous commit
- Rebuild and verify

### 5.2 Full Feature Rollback

**If Entire Feature Must Be Removed:**

```bash
# 1. Identify commits
git log --oneline --grep="halb.centric"

# 2. Create rollback branch
git checkout -b rollback-halb-centric

# 3. Revert commits (in reverse order)
git revert <commit-hash-phase-5>
git revert <commit-hash-phase-4>
git revert <commit-hash-phase-3>
git revert <commit-hash-phase-2>
git revert <commit-hash-phase-1>

# 4. Test rollback build
idf.py fullclean && idf.py build

# 5. Flash and verify
idf.py flash monitor

# 6. Merge rollback if needed
git checkout main
git merge rollback-halb-centric
```

**Alternative: Reset to Previous State**
```bash
# Find last good commit before feature
git log --oneline

# Hard reset (CAREFUL - loses all commits)
git reset --hard <commit-before-feature>

# Force push if needed (CAREFUL)
git push --force origin main
```

### 5.3 Partial Rollback

**Keep Configuration, Remove Display Logic:**
- Revert Phase 2 commits only
- Configuration persists but has no effect
- Can re-implement display logic later

**Keep Display Logic, Remove MQTT:**
- Revert Phase 4 commits only
- Feature works but not controllable via HA
- Can manually toggle in code for testing

---

## 6. Risk Mitigation

### 6.1 Identified Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Display corruption** | High | Low | Extensive testing per step, LED validation |
| **NVS corruption** | Medium | Very Low | Use proven NVS patterns, version field |
| **Mutex deadlock** | High | Low | Reuse existing mutex, test concurrent access |
| **MQTT compatibility** | Medium | Low | Follow existing patterns, validate JSON |
| **HA discovery failure** | Low | Low | Test discovery, fallback to manual config |
| **Grammar regression** | Medium | Low | Test EIN UHR edge cases explicitly |
| **Transition glitches** | Low | Medium | Test transitions at :20, :40, :45 |

### 6.2 Mitigation Strategies

**Before Each Step:**
1. Commit current working state
2. Read step completely
3. Understand expected outcome

**During Each Step:**
1. Make one change at a time
2. Compile frequently (catch errors early)
3. Test before proceeding

**After Each Step:**
1. Verify success criteria met
2. Commit with clear message
3. Document any deviations

### 6.3 Emergency Procedures

**If Build Breaks:**
```bash
# 1. Don't panic - revert last change
git diff  # See what changed
git checkout -- <file>  # Revert specific file

# 2. Rebuild
idf.py build

# 3. If still broken, revert last commit
git reset --hard HEAD~1
```

**If Device Won't Boot:**
```bash
# 1. Check serial output
idf.py monitor

# 2. Look for panic/crash
# 3. Revert to last working commit
git reset --hard <last-working-commit>

# 4. Reflash
idf.py flash
```

**If Display Corrupted:**
```bash
# 1. Try reset
esp_restart() via MQTT or button

# 2. Clear NVS (if suspected corruption)
idf.py erase_flash  # CAREFUL - erases WiFi credentials too

# 3. Reflash clean firmware
idf.py flash
```

---

## 7. Success Criteria

### 7.1 Phase-by-Phase Success

**Phase 1 (Configuration):** ✅
- ✅ Field added to struct
- ✅ NVS save/load works
- ✅ Default value is false
- ✅ Getter/setter functions work

**Phase 2 (Display Logic):** ✅
- ✅ :20, :40, :45 show correct words (both styles)
- ✅ All other time slots unchanged
- ✅ Grammar edge cases work
- ✅ No display corruption

**Phase 3 (Thread Safety):** ✅
- ✅ Thread-safe accessors work
- ✅ No mutex warnings
- ✅ No deadlocks
- ✅ Concurrent access safe

**Phase 4 (MQTT):** ✅
- ✅ MQTT command toggles style
- ✅ State published correctly
- ✅ HA entity appears
- ✅ HA toggle works bidirectionally

**Phase 5 (Testing):** ✅
- ✅ All hardware tests pass
- ✅ 24-hour stability verified
- ✅ Documentation complete
- ✅ Release notes written

### 7.2 Overall Success Criteria

**Functional Requirements:**
- ✅ User can toggle HALB-centric style via Home Assistant
- ✅ Display shows correct German time expressions for both styles
- ✅ Configuration persists across reboots
- ✅ Default behavior unchanged (NACH/VOR style)
- ✅ DREIVIERTEL displays correctly at :45 (HALB style)

**Non-Functional Requirements:**
- ✅ No performance degradation (<20ms display updates)
- ✅ No memory leaks (24+ hour test)
- ✅ Thread-safe implementation (no race conditions)
- ✅ Fully backward compatible (old configs still work)
- ✅ Home Assistant auto-discovery works

**Quality Requirements:**
- ✅ Code compiles without warnings
- ✅ All commits have clear messages
- ✅ Documentation is accurate and complete
- ✅ Testing is comprehensive
- ✅ Rollback procedures documented

### 7.3 Definition of Done

**Feature is complete when:**

1. ✅ All code changes committed to git
2. ✅ All tests pass (unit, integration, hardware)
3. ✅ Documentation updated (README, MQTT guide, CLAUDE.md)
4. ✅ Release notes written
5. ✅ 24-hour stability test passed
6. ✅ Home Assistant integration verified
7. ✅ Rollback procedure tested
8. ✅ No known bugs or issues
9. ✅ Code review completed (if applicable)
10. ✅ Ready for production deployment

---

## 8. Timeline Summary

### Optimistic Timeline (3 days)

**Day 1:** Configuration + Display + Thread Safety (6-8 hours)
- Morning: Phase 1 (Configuration) - 3-4 hours
- Afternoon: Phase 2 (Display Logic) - 2-3 hours
- Evening: Phase 3 (Thread Safety) - 1-2 hours

**Day 2:** MQTT Integration (4-5 hours)
- Phase 4 (MQTT + HA Discovery) - 4-5 hours

**Day 3:** Testing & Documentation (4-6 hours)
- Phase 5 (Testing + Docs) - 4-6 hours

**Total:** 14-19 hours (3 days part-time or 2 days full-time)

### Realistic Timeline (4 days)

**Day 1:** Configuration (3-4 hours)
**Day 2:** Display Logic + Thread Safety (4-5 hours)
**Day 3:** MQTT Integration (4-5 hours)
**Day 4:** Testing & Documentation (6-8 hours)

**Total:** 17-22 hours (4 days part-time or 2.5 days full-time)

### Conservative Timeline (5 days)

**Day 1:** Configuration (4 hours) + Buffer
**Day 2:** Display Logic (4 hours) + Extensive Testing
**Day 3:** Thread Safety (2 hours) + MQTT Start (2 hours)
**Day 4:** MQTT Integration (4 hours) + HA Testing
**Day 5:** Comprehensive Testing (4 hours) + Documentation (4 hours)

**Total:** 24 hours (5 days part-time or 3 days full-time)

---

## 9. Daily Checklists

### Day 1 Checklist

**Morning (Phase 1: Configuration)**
- [ ] STEP 1.1: Add configuration field (15 min)
- [ ] STEP 1.2: Initialize default value (10 min)
- [ ] STEP 1.3: Add NVS save support (20 min)
- [ ] STEP 1.4: Add NVS load support (20 min)
- [ ] STEP 1.5: Update reset function (10 min)
- [ ] STEP 1.6: Add getter/setter functions (15 min)
- [ ] Phase 1 Checkpoint: Full build and verification

**Afternoon (Phase 2: Display Logic)**
- [ ] STEP 2.1: Add include for config (5 min)
- [ ] STEP 2.2: Modify :20 case (20 min)
- [ ] STEP 2.3: Modify :40 case (20 min)
- [ ] STEP 2.4: Modify :45 case (DREIVIERTEL) (20 min)
- [ ] STEP 2.5: Test all 12 time slots (60 min)
- [ ] Phase 2 Checkpoint: All time slots verified

**Evening (Phase 3: Thread Safety)**
- [ ] STEP 3.1: Add thread-safe getter/setter (15 min)
- [ ] STEP 3.2: Update display to use thread-safe accessor (10 min)
- [ ] Phase 3 Checkpoint: Thread safety verified

**End of Day 1:**
- [ ] All code compiles
- [ ] Configuration persists across reboots
- [ ] Display shows correct words for both styles
- [ ] Thread-safe access working
- [ ] Git commits up to date

### Day 2 Checklist

**MQTT Integration (Phase 4)**
- [ ] STEP 4.1: Add MQTT command handler (30 min)
- [ ] STEP 4.2: Update state publishing (20 min)
- [ ] STEP 4.3: Add Home Assistant discovery (45 min)
- [ ] Phase 4 Checkpoint: End-to-end MQTT test

**End of Day 2:**
- [ ] MQTT commands work
- [ ] HA entity appears and functions
- [ ] Bidirectional control working
- [ ] State synchronization verified
- [ ] Git commits up to date

### Day 3-4 Checklist

**Testing & Documentation (Phase 5)**
- [ ] STEP 5.1: Comprehensive hardware testing (2-3 hours)
  - [ ] Test 1: All time slots (both styles)
  - [ ] Test 2: Style toggle at different times
  - [ ] Test 3: Persistence
  - [ ] Test 4: MQTT integration
  - [ ] Test 5: Edge cases
  - [ ] Test 6: Transitions
  - [ ] Test 7: Brightness control
  - [ ] Test 8: 24-hour stability
- [ ] STEP 5.2: Update user documentation (1-2 hours)
  - [ ] README.md
  - [ ] Mqtt_User_Guide.md
  - [ ] docs/user/home-assistant-setup.md
  - [ ] CLAUDE.md
- [ ] STEP 5.3: Create release notes (30 min)
- [ ] Phase 5 Checkpoint: All documentation complete

**Final Verification:**
- [ ] Full clean build succeeds
- [ ] All tests pass
- [ ] Documentation accurate
- [ ] Ready for deployment

---

## 10. Post-Implementation

### 10.1 Deployment

```bash
# 1. Create release tag
git tag -a v2.1.0 -m "Release v2.1.0: HALB-centric time expressions (MVP)"

# 2. Push to remote
git push origin main
git push origin v2.1.0

# 3. Flash to production device(s)
idf.py flash

# 4. Verify in production
# - Check logs
# - Test toggle via HA
# - Monitor for 24 hours
```

### 10.2 Monitoring

**First 24 Hours:**
- Monitor error logs
- Check MQTT traffic
- Verify no unexpected reboots
- Monitor memory usage

**First Week:**
- Collect user feedback
- Monitor for bugs
- Check NVS wear leveling
- Verify stability

### 10.3 User Feedback

**Collect Data On:**
- Is feature being used?
- Which style is preferred?
- Any confusion or issues?
- Requests for expansion (independent DREIVIERTEL toggle?)

### 10.4 Future Expansion

**If Successful:**
- Consider migrating to Strategy A (full implementation)
- Add independent DREIVIERTEL toggle
- Add more time expression variants (if hardware allows)
- Consider regional dialect support

**If Not Used:**
- Keep as-is (no expansion)
- Document as complete
- Focus efforts elsewhere

---

## Conclusion

This micro-step implementation plan provides a **thorough, testable roadmap** for adding HALB-centric time expressions to the word clock. By following small, verifiable steps with clear rollback points, we minimize risk while ensuring high-quality implementation.

**Key Success Factors:**
1. ✅ Small, incremental changes
2. ✅ Test before proceeding
3. ✅ Clear rollback at every step
4. ✅ Comprehensive documentation
5. ✅ Realistic timeline with buffer

**Estimated Effort:** 3-4 days part-time or 2-3 days full-time

**Risk Level:** LOW (proven patterns, extensive testing, clear rollbacks)

**Ready to begin implementation!** 🚀

---

**Document Version:** 1.0
**Last Updated:** 2025-10-15
**Status:** Ready for Implementation
**Related Documents:**
- [flexible-time-expressions-analysis.md](flexible-time-expressions-analysis.md) - Full analysis
- Release notes will be in: `docs/releases/v2.1.0-halb-centric.md`
