# TLC59116 LED Controller Power Surge Behavior Analysis

**Last Updated:** November 2025
**Component:** TLC59116 I2C LED Driver (10 devices)
**Platform:** ESP32-S3 Word Clock
**I2C Bus:** Port 0, 100kHz, 10 devices @ 0x60-0x6A

---

## Overview

This document analyzes the behavior of the **10× TLC59116 LED controllers** during power surges, voltage sags, brownout events, and ESP32-S3 resets. The TLC59116 is a critical component controlling all 160 LEDs in the word clock matrix.

**Critical Question:**
> "What happens to the TLC59116 LED controllers when ESP32-S3 experiences a power surge or brownout reset?"

---

## TLC59116 Architecture Overview

### Hardware Configuration

**10 Devices on Single I2C Bus:**
```
I2C Port 0 (GPIO 8=SDA, 9=SCL) @ 100kHz:
├─ TLC59116 #0  (0x60) → Row 0 (16 LEDs)
├─ TLC59116 #1  (0x61) → Row 1 (16 LEDs)
├─ TLC59116 #2  (0x62) → Row 2 (16 LEDs)
├─ TLC59116 #3  (0x63) → Row 3 (16 LEDs)
├─ TLC59116 #4  (0x64) → Row 4 (16 LEDs)
├─ TLC59116 #5  (0x65) → Row 5 (16 LEDs)
├─ TLC59116 #6  (0x66) → Row 6 (16 LEDs)
├─ TLC59116 #7  (0x67) → Row 7 (16 LEDs)
├─ TLC59116 #8  (0x69) → Row 8 (16 LEDs)
└─ TLC59116 #9  (0x6A) → Row 9 (16 LEDs)
```

**Total LED Count:** 160 LEDs (10 rows × 16 columns)

---

### TLC59116 Key Registers (Power-On State)

| Register | Address | Purpose | Power-On Default | Our Setting |
|----------|---------|---------|------------------|-------------|
| MODE1 | 0x00 | Operation mode | 0x00 (normal) | 0x05 (auto-increment) |
| MODE2 | 0x01 | Output config | 0x00 (open-drain) | 0x00 (normal) |
| PWM0-15 | 0x02-0x11 | Individual LED brightness | 0x00 (off) | Variable (0-80) |
| GRPPWM | 0x12 | Global brightness | 0xFF (max) | 120-180 |
| LEDOUT0-3 | 0x14-0x17 | Output mode select | 0x00 (all off) | 0xFF (all GRPPWM) |

**Critical Observation:**
- **Power-on default:** All LEDs OFF (PWM=0, LEDOUT=0)
- **After init:** LEDs in GRPPWM mode with individual PWM values
- **Registers are VOLATILE** - lost on power cycle!

---

## TLC59116 Power-On Reset (POR) Behavior

### Hardware POR Circuit

**TLC59116 Datasheet (Section 7.5):**
> "The TLC59116 contains a power-on reset (POR) circuit that resets all internal registers to their default values when VDD rises above the POR threshold (~2.5V)."

**POR Threshold Voltage:**
- **Minimum:** ~2.3V (VDD rising)
- **Typical:** ~2.5V (VDD rising)
- **Hysteresis:** ~100mV

**POR Timing:**
- **POR Active:** VDD < 2.5V
- **POR Release:** VDD > 2.5V + settling time (~1ms)
- **Total POR Time:** ~2-5ms from VDD valid

---

## Power Surge Scenario Analysis

### Scenario 1: **Clean Brownout (ESP32 Brownout @ 2.80V)**

**Event Sequence:**
1. **Voltage drops:** 3.3V → 2.8V
2. **ESP32 brownout detector triggers** @ 2.80V
3. **ESP32 resets** immediately (<1µs)
4. **Voltage continues dropping:** 2.8V → 2.3V (power loss)
5. **TLC59116 POR triggers** @ 2.5V
6. **Both devices reset**

**TLC59116 State During Brownout:**

| Phase | Voltage | ESP32 State | TLC59116 State | LED State |
|-------|---------|-------------|----------------|-----------|
| 1. Normal | 3.3V | Running | Configured | Display lit |
| 2. Brownout | 2.8V | **RESET** | Still configured | LEDs stay lit |
| 3. POR threshold | 2.5V | Reset (dead) | **POR TRIGGERED** | **LEDs turn OFF** |
| 4. Power loss | <2.3V | Dead | POR active | LEDs OFF |
| 5. Power restored | 3.3V | Booting | Waiting for I2C | LEDs OFF (default) |
| 6. Re-init | 3.3V | Running | **Re-configured** | Display lit again |

**Critical Window: 2.80V → 2.50V (300mV, ~10-50ms depending on power supply capacitance)**

**During this window:**
- ✅ ESP32 is **RESET** (safe, no I2C activity)
- ⚠️ TLC59116 is **STILL ACTIVE** with old configuration
- ⚠️ **LEDs may remain lit** with last PWM values
- ⚠️ **No I2C communication possible** (ESP32 reset)

**Result After Power Restored:**
- ✅ TLC59116 resets to defaults (all LEDs OFF)
- ✅ ESP32 reboots and re-initializes TLC59116
- ✅ Display restored from RTC time

---

### Scenario 2: **Voltage Dip (ESP32 Survives, TLC59116 Resets)**

**Event:** Brief voltage dip: 3.3V → 2.4V → 3.3V (50ms)

**Event Sequence:**
1. **Voltage drops:** 3.3V → 2.4V
2. **ESP32 brownout detector triggers** @ 2.80V → ESP32 **resets**
3. **TLC59116 POR triggers** @ 2.5V → TLC **resets to defaults**
4. **Voltage recovers:** 2.4V → 3.3V (fast)
5. **ESP32 boots:** ~500ms boot time
6. **TLC59116 ready:** Immediately (already at defaults)
7. **ESP32 re-initializes TLC59116:** ~2 seconds after boot

**TLC59116 State:**

| Time | Voltage | TLC59116 State | LED Matrix State | Issue |
|------|---------|----------------|------------------|-------|
| T+0ms | 3.3V→2.4V | Configured | LEDs lit | Normal |
| T+10ms | 2.4V | **POR reset** | **All LEDs OFF** | Power dip |
| T+60ms | 2.4V→3.3V | Default state | LEDs OFF | Power restored |
| T+500ms | 3.3V | Default state | LEDs OFF | ESP32 booting |
| T+2000ms | 3.3V | **Re-init** | LEDs ON (correct time) | Recovery complete |

**Result:**
- ⚠️ **LEDs go dark for ~2 seconds** during ESP32 boot
- ✅ Display automatically restored to correct time
- ✅ No stuck/random LED states (defaults are clean)

---

### Scenario 3: **Voltage Spike (ESP32 Watchdog Reset, TLC59116 OK)**

**Event:** Voltage spike: 3.3V → 4.2V → 3.3V (10ms)

**Assumptions:**
- ESP32 survives spike (internal LDO protects to ~3.6V)
- TLC59116 survives spike (rated to 7V absolute max)
- Spike **corrupts ESP32 execution** (enters undefined state)

**Event Sequence:**
1. **Voltage spikes:** 3.3V → 4.2V
2. **ESP32 execution corrupted** (random memory corruption)
3. **TLC59116 continues operating** (registers intact)
4. **ESP32 watchdog expires** (300ms - 5s)
5. **ESP32 resets** via watchdog
6. **TLC59116 still running** with old configuration
7. **ESP32 reboots** and re-initializes TLC59116

**TLC59116 State:**

| Time | Event | TLC59116 State | LED Matrix State | Risk |
|------|-------|----------------|------------------|------|
| T+0ms | Spike 4.2V | Configured | LEDs lit | Normal |
| T+10ms | ESP32 corrupted | **Still configured** | **LEDs frozen** | ⚠️ Stuck state |
| T+11ms | ESP32 hung | Still configured | LEDs frozen | ⚠️ Stuck state |
| T+300ms | ESP32 WDT reset | Still configured | LEDs frozen | ⚠️ Stuck state |
| T+800ms | ESP32 booting | Still configured | **LEDs frozen** | ⚠️ Stuck state |
| T+2300ms | ESP32 re-init | **Re-configured** | LEDs correct | ✅ Recovered |

**Critical Issue:**
- ⚠️ **TLC59116 does NOT reset** when ESP32 watchdog resets!
- ⚠️ **LEDs remain in last state** for ~2 seconds
- ⚠️ **No automatic TLC reset mechanism**

**Result:**
- ⚠️ LEDs show **frozen/incorrect state** during ESP32 boot
- ⚠️ Random pattern if last I2C write was corrupted
- ✅ Eventually corrected when ESP32 re-initializes

---

### Scenario 4: **Partial I2C Transaction During Brownout**

**Event:** ESP32 brownout **during** I2C write to TLC59116

**Event Sequence:**
1. **ESP32 starts I2C write:** Update LEDs for new time
2. **Voltage drops:** 3.3V → 2.8V (during I2C transaction)
3. **ESP32 brownout reset** → I2C transaction **aborted mid-byte**
4. **TLC59116 receives partial command**
5. **Voltage continues dropping** → TLC59116 POR reset

**I2C Transaction Corruption Scenarios:**

#### A. Partial Register Write (Most Likely)
```
ESP32 sending: [START] [ADDR=0x60] [REG=0x05] [DATA=60] [STO-
                                                          ^
                                                      Reset here
TLC59116 receives: [START] [ADDR=0x60] [REG=0x05] [DATA=60] [no STOP]
Result: Register 0x05 (PWM3) = 60, waiting for STOP condition
```

**TLC59116 Behavior:**
- **Without STOP:** Transaction incomplete, register **MAY** be updated
- **I2C state:** Waiting for STOP or repeated START
- **LED affected:** Single LED (channel 3) brightness = 60
- **Recovery:** POR reset clears all registers

#### B. Partial Address (Less Likely)
```
ESP32 sending: [START] [ADDR=0x-
                              ^
                          Reset here
TLC59116 receives: [START] [incomplete address] [no ACK]
Result: No register change, I2C bus idle
```

**TLC59116 Behavior:**
- **No ACK sent:** Transaction rejected
- **No register update:** Safe state
- **I2C state:** Returns to idle

**Result:**
- ✅ **Partial transactions are SAFE** - POR clears inconsistent state
- ✅ **No stuck registers** - defaults applied on power-up
- ⚠️ **Brief incorrect LED state possible** (until POR triggers)

---

## Critical Gaps & Risks

### ❌ **Gap 1: No TLC59116 Reset on ESP32 Watchdog Reset**

**Problem:**
When ESP32 resets via **watchdog timer** (not brownout):
- ESP32 voltage stays at 3.3V (no brownout)
- ESP32 resets and reboots
- **TLC59116 does NOT reset** (voltage never dropped)
- TLC59116 **retains corrupted state** until ESP32 re-init

**Impact:**
- LEDs show **frozen/incorrect pattern** for ~2 seconds
- If last I2C write was **corrupted**, random LEDs may be lit
- **User-visible:** Display glitch during reset recovery

**Severity:** ⚠️ **MEDIUM** (cosmetic issue, self-correcting)

**Occurs When:**
- Voltage spike corrupts ESP32 → watchdog reset
- Software bug causes hang → watchdog reset
- Panic/exception → watchdog reset

---

### ❌ **Gap 2: No Hardware Reset Line for TLC59116**

**Problem:**
TLC59116 has a **hardware RESET pin** (active-low) that:
- Resets all registers to defaults
- Clears I2C state machine
- Can be controlled by ESP32 GPIO

**Current Design:**
- **No GPIO connected to TLC RESET pins**
- **Only software reset** via I2C (sleep/wake MODE1 sequence)
- Software reset **requires working I2C bus**

**Impact:**
- ⚠️ Cannot force TLC reset if I2C bus corrupted
- ⚠️ Cannot guarantee clean state after ESP32 reset
- ⚠️ No recovery if TLC I2C state machine stuck

**Severity:** ⚠️ **MEDIUM** (rare, usually self-corrects on power cycle)

---

### ❌ **Gap 3: No TLC State Verification After Reset**

**Problem:**
After ESP32 reset, initialization code (`tlc59116_init_device()`):
1. Performs software reset (sleep/wake)
2. Configures MODE1, MODE2
3. Sets LEDOUT registers
4. Clears all PWM values
5. **Assumes success if I2C ACK received**

**What's Missing:**
- ❌ No readback verification of critical registers
- ❌ No detection of stuck/corrupted TLC state
- ❌ No retry mechanism if TLC in undefined state

**Impact:**
- ⚠️ If TLC in corrupted state, init may **silently fail**
- ⚠️ LEDs may not respond to commands
- ⚠️ No error reported to user

**Severity:** ⚠️ **LOW** (very rare, usually caught by LED validation)

---

### ❌ **Gap 4: No Detection of TLC Power-On Reset**

**Problem:**
When TLC59116 experiences POR (brownout), ESP32 doesn't know:
- ❌ No way to detect TLC reset occurred
- ❌ No flag indicating "TLC needs re-init"
- ❌ Re-init only happens when ESP32 also resets

**Failure Scenario:**
1. Brief voltage dip: 3.3V → 2.4V → 3.3V (20ms)
2. ESP32 brownout detector **misses it** (threshold is 2.80V)
3. **ESP32 continues running** without reset
4. **TLC59116 resets** (POR @ 2.5V) → all LEDs OFF
5. **ESP32 doesn't know** TLC reset occurred
6. **Display stuck OFF** until next ESP32 reset or manual re-init

**Likelihood:** ⚠️ **LOW** (ESP32 brownout @ 2.80V usually triggers before TLC POR @ 2.5V)

**But possible if:**
- Localized voltage drop on TLC power rail (not ESP32)
- Noisy power supply with fast transients

**Severity:** ⚠️ **MEDIUM** (display dead until manual intervention)

---

## Current Protection Mechanisms

### ✅ **Protection 1: ESP32 Brownout Detector (2.80V)**

**How It Helps:**
- Triggers **before** TLC59116 POR (2.50V)
- Ensures ESP32 resets **cleanly** before TLC corrupts
- ESP32 re-initialization restores TLC state

**Effectiveness:** ⭐⭐⭐⭐⭐ Excellent

**Coverage:** Voltage drops, brownouts, power loss

---

### ✅ **Protection 2: Software Reset in Init (Sleep/Wake)**

**Code:** `tlc59116_reset_device()` in `i2c_devices.c:327`

```c
// Set MODE1 to sleep mode
i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, TLC59116_MODE1_SLEEP);
vTaskDelay(pdMS_TO_TICKS(10));
// Wake up device (clears registers)
i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, 0x00);
```

**How It Helps:**
- Resets TLC internal state machine
- Clears error conditions
- Re-synchronizes I2C communication

**Effectiveness:** ⭐⭐⭐⭐ Very Good

**Limitation:** Requires **working I2C bus** (fails if I2C corrupted)

---

### ✅ **Protection 3: LED Validation System (Post-Transition)**

**Code:** `led_validation.c` - Validates LED hardware state

**How It Helps:**
- Detects mismatches between software state and hardware state
- Reads back PWM values from TLC59116
- Publishes validation results via MQTT
- **User can trigger manual recovery** via Home Assistant

**Effectiveness:** ⭐⭐⭐ Good (detection only, not automatic recovery)

**Limitation:** Only runs **after transitions** (~5 minutes intervals)

---

### ⚠️ **Protection 4: Re-Init on Every ESP32 Reset**

**Code:** `app_main()` → `initialize_hardware()` → `i2c_devices_init()` → `tlc59116_init_all()`

**How It Helps:**
- Every ESP32 reset triggers full TLC re-initialization
- Restores known-good state
- Clears any corrupted configuration

**Effectiveness:** ⭐⭐⭐⭐ Very Good

**Limitation:** Only helps if **ESP32 also resets** (not if only TLC resets)

---

## Recommended Improvements

### 🔧 **Improvement 1: Add Hardware RESET Line (Best Solution)**

**Implementation:**
1. Connect TLC59116 RESET pins to ESP32 GPIO (e.g., GPIO 4)
2. Pull-down resistor (10kΩ) to keep RESET low during ESP32 boot
3. Assert RESET in `tlc59116_init_all()` before I2C init

**Code Changes:**
```c
#define TLC_RESET_GPIO  GPIO_NUM_4

// In tlc59116_init_all():
gpio_reset_pin(TLC_RESET_GPIO);
gpio_set_direction(TLC_RESET_GPIO, GPIO_MODE_OUTPUT);

// Assert reset (active-low)
gpio_set_level(TLC_RESET_GPIO, 0);
vTaskDelay(pdMS_TO_TICKS(10));  // Hold reset for 10ms

// Release reset
gpio_set_level(TLC_RESET_GPIO, 1);
vTaskDelay(pdMS_TO_TICKS(10));  // Wait for TLC to initialize

// Now all TLC devices are in clean default state
// Proceed with I2C configuration...
```

**Benefits:**
- ✅ **Guaranteed clean state** on every ESP32 reset
- ✅ **No dependency on I2C** (works even if I2C corrupted)
- ✅ **Synchronizes TLC reset with ESP32 reset**
- ✅ **Eliminates stuck LED states**

**Drawback:**
- ⚠️ **Requires hardware modification** (GPIO connection)
- ⚠️ **Not possible on current hardware** (no GPIO available)

**Recommendation:** **Consider for future hardware revision**

---

### 🔧 **Improvement 2: Add Register Readback Verification**

**Implementation:**
Add verification after TLC initialization to confirm registers are correct.

**Code Changes:**
```c
// In tlc59116_init_device() after configuration:

// Verify MODE1 register
uint8_t mode1_readback;
ret = i2c_read_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, &mode1_readback);
if (ret != ESP_OK || mode1_readback != mode1_val) {
    ESP_LOGE(TAG, "TLC %d MODE1 verification failed! Expected 0x%02X, got 0x%02X",
             tlc_index, mode1_val, mode1_readback);
    error_log_add(ERROR_SOURCE_I2C_BUS, ERROR_SEVERITY_ERROR,
                  "TLC init verification failed - possible hardware issue");
    return ESP_FAIL;
}

// Verify GRPPWM register
uint8_t grppwm_readback;
ret = i2c_read_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_GRPPWM, &grppwm_readback);
if (ret != ESP_OK || grppwm_readback != 120) {
    ESP_LOGE(TAG, "TLC %d GRPPWM verification failed! Expected 120, got %d",
             tlc_index, grppwm_readback);
    return ESP_FAIL;
}

ESP_LOGI(TAG, "TLC %d register verification passed", tlc_index);
```

**Benefits:**
- ✅ **Detects initialization failures**
- ✅ **Catches corrupted TLC state**
- ✅ **Logs errors for debugging**
- ✅ **No hardware changes needed**

**Drawback:**
- ⚠️ Adds ~20ms to init time (10 devices × 2 reads)

**Recommendation:** **Implement in next firmware update**

---

### 🔧 **Improvement 3: Periodic TLC Health Check**

**Implementation:**
Background task that periodically verifies TLC devices are responding.

**Code Changes:**
```c
// New task in i2c_devices.c
static void tlc_health_check_task(void *pvParameters)
{
    const TickType_t check_interval = pdMS_TO_TICKS(60000);  // 60 seconds

    while (1) {
        vTaskDelay(check_interval);

        // Quick health check: read MODE1 from all devices
        for (int i = 0; i < TLC59116_COUNT; i++) {
            uint8_t mode1_val;
            esp_err_t ret = i2c_read_byte(I2C_LEDS_MASTER_PORT,
                                         tlc_addresses[i],
                                         TLC59116_MODE1,
                                         &mode1_val);

            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "TLC %d health check failed - attempting recovery", i);
                error_log_add(ERROR_SOURCE_I2C_BUS, ERROR_SEVERITY_WARNING,
                              "TLC health check failed - re-initializing");

                // Attempt re-initialization
                tlc59116_init_device(i);
            }
        }
    }
}

// Call from i2c_devices_init():
xTaskCreate(tlc_health_check_task, "tlc_health", 2048, NULL, 3, NULL);
```

**Benefits:**
- ✅ **Detects silent TLC failures**
- ✅ **Automatic recovery** without user intervention
- ✅ **Early warning** of hardware issues

**Drawback:**
- ⚠️ Extra I2C traffic (10 reads every 60s = negligible)
- ⚠️ Additional 2KB stack usage

**Recommendation:** **Consider for future update** (optional feature)

---

### 🔧 **Improvement 4: Reset Cause Logging (Already Recommended)**

**Implementation:**
Detect ESP32 reset cause and log TLC re-initialization.

**Code Changes:**
```c
// In app_main():
esp_reset_reason_t reset_reason = esp_reset_reason();

if (reset_reason == ESP_RST_BROWNOUT) {
    ESP_LOGW(TAG, "Brownout detected - TLC59116 devices likely reset to defaults");
    error_log_add(ERROR_SOURCE_POWER, ERROR_SEVERITY_WARNING,
                  "Brownout reset - LED controllers re-initialized");
} else if (reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_INT_WDT) {
    ESP_LOGW(TAG, "Watchdog reset - TLC59116 devices MAY be in corrupted state");
    error_log_add(ERROR_SOURCE_SYSTEM, ERROR_SEVERITY_WARNING,
                  "Watchdog reset - LED controllers re-initialized (check for glitches)");
}
```

**Benefits:**
- ✅ **Correlate display glitches with reset events**
- ✅ **Track power stability** over time
- ✅ **Remote diagnostics** via MQTT

**Drawback:**
- None (pure telemetry)

**Recommendation:** **Implement immediately** (already recommended in power-surge-protection.md)

---

## Power Surge Recovery Matrix

| Surge Type | ESP32 State | TLC59116 State | LED Display | Recovery Time | Risk Level |
|------------|-------------|----------------|-------------|---------------|------------|
| **Brownout (< 2.8V)** | Reset | POR reset | OFF → Restored | ~2 seconds | ✅ Safe |
| **Voltage dip (2.4V)** | Reset | POR reset | OFF → Restored | ~2 seconds | ✅ Safe |
| **Voltage spike (4.2V)** | Watchdog reset | **No reset** | **Frozen** → Restored | ~2 seconds | ⚠️ Cosmetic |
| **Partial I2C transaction** | Reset mid-write | POR clears | Corrupted → Restored | ~2 seconds | ✅ Safe |
| **TLC-only voltage dip** | Running | POR reset | **OFF (stuck)** | Until next ESP32 reset | ⚠️ Display dead |
| **EMI-induced hang** | Watchdog reset | **No reset** | **Frozen** | ~2 seconds | ⚠️ Cosmetic |

**Legend:**
- ✅ **Safe:** Self-correcting, no user intervention needed
- ⚠️ **Cosmetic:** Brief visual glitch, self-correcting
- ❌ **Stuck:** Requires manual intervention or power cycle

---

## Testing Power Surge Impact on TLC59116

### Test 1: Simulated Brownout

**Procedure:**
1. Display time normally (LEDs lit)
2. Trigger ESP32 software reset: `esp_restart()`
3. Observe LED behavior during reset

**Expected Behavior:**
- LEDs remain lit briefly (~100ms)
- LEDs turn OFF when ESP32 stops I2C updates
- LEDs remain OFF during boot (~2 seconds)
- LEDs restore correct time display

**Pass Criteria:**
- ✅ No random LED patterns
- ✅ Display restored within 3 seconds

---

### Test 2: Watchdog Timeout (TLC Stays Alive)

**Procedure:**
1. Display time normally
2. Inject infinite loop in high-priority task (trigger watchdog)
3. Observe LED behavior during watchdog reset

**Code:**
```c
// Temporary test code (remove after testing!)
void test_watchdog_reset(void) {
    ESP_LOGW(TAG, "TRIGGERING WATCHDOG RESET IN 5 SECONDS...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_task_wdt_delete(NULL);  // Unsubscribe from watchdog
    while(1) {
        // Infinite loop - watchdog will trigger after 5s
    }
}
```

**Expected Behavior:**
- LEDs freeze in last state (may show incorrect time)
- LEDs stay frozen during watchdog timeout (~5s)
- LEDs stay frozen during ESP32 boot (~2s)
- **Total frozen time: ~7 seconds**
- LEDs restore correct time after re-init

**Pass Criteria:**
- ✅ LEDs eventually restore (even if frozen initially)
- ⚠️ **Expected:** Frozen state for ~7 seconds
- ⚠️ **Cosmetic issue only**

---

### Test 3: I2C Transaction Corruption

**Procedure:**
1. Trigger brownout **during** I2C write (difficult to time)
2. Alternative: Force I2C bus error and observe recovery

**Code:**
```c
// Simulate I2C error (test only!)
void test_i2c_corruption(void) {
    // Start writing to TLC
    for (int i = 0; i < 5; i++) {
        tlc_set_matrix_led(0, i, 100);
        vTaskDelay(pdMS_TO_TICKS(10));

        if (i == 2) {
            // Simulate brownout mid-transaction
            esp_restart();
        }
    }
}
```

**Expected Behavior:**
- Partial write may leave some LEDs in incorrect state
- ESP32 resets and re-initializes TLC
- All LEDs restored to correct state

**Pass Criteria:**
- ✅ No stuck LEDs after recovery
- ✅ Display eventually correct

---

## Summary & Recommendations

### ✅ Current Protections (Good)

1. **ESP32 brownout @ 2.80V** triggers before TLC POR @ 2.50V
2. **Software reset** (sleep/wake) on every init
3. **LED validation** detects hardware mismatches
4. **Full re-init** on every ESP32 reset

**Overall Rating:** ⭐⭐⭐⭐ Very Good (4/5 stars)

---

### ⚠️ Identified Risks (Medium Severity)

1. **TLC doesn't reset on ESP32 watchdog timeout**
   - Impact: Frozen LED display for ~2 seconds
   - Severity: **Cosmetic** (self-correcting)

2. **No hardware RESET line**
   - Impact: Cannot force TLC reset if I2C stuck
   - Severity: **Low** (rare, power cycle fixes)

3. **No register verification after init**
   - Impact: Silent initialization failures
   - Severity: **Low** (caught by LED validation)

4. **TLC-only voltage dip not detected**
   - Impact: Display dead until ESP32 reset
   - Severity: **Medium** (unlikely scenario)

---

### 🎯 Recommended Actions

**Immediate (Firmware Update):**
1. ✅ **Add reset cause logging** (already recommended)
2. ✅ **Add register readback verification** to `tlc59116_init_device()`
3. ✅ **Document expected recovery behavior** in user guide

**Optional (Future Enhancement):**
4. 🔧 **Add periodic TLC health check** task
5. 🔧 **Implement manual TLC re-init** via MQTT command

**Future Hardware Revision:**
6. 🔧 **Add hardware RESET line** (GPIO → TLC RESET pins)

---

## Related Documentation

- [Power Surge & Brownout Protection](power-surge-protection.md) - ESP32 protection mechanisms
- [Watchdog Configuration](watchdog-configuration.md) - Watchdog timer details
- [LED Validation](../implementation/led-validation/) - LED validation system
- [TLC59116 Datasheet](https://www.ti.com/lit/ds/symlink/tlc59116.pdf) - Official datasheet

---

**Document Version:** 1.0
**Status:** Complete ✅
**TLC Protection Rating:** ⭐⭐⭐⭐ Very Good (cosmetic issues only, no safety risks)
