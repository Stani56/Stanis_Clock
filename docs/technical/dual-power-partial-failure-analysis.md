# Dual Power Supply Partial System Failure Analysis

**Last Updated:** December 2025
**Incident:** USB plug/unplug causing display failure while ESP32 core remains operational
**Platform:** ESP32-S3 Word Clock with dual power sources
**Severity:** HIGH - Undetected peripheral failure without automatic recovery

---

## Executive Summary

**Problem:** USB plug/unplug events on dual-powered ESP32-S3 system cause complete LED display failure while ESP32 core remains fully functional (WiFi, MQTT, heartbeat active). System watchdogs do NOT trigger, leaving the device in a "zombie state" with dark display but operational networking.

**Root Cause:** Voltage transients from dual power sources (USB + external 3.3V) propagate to TLC59116 I2C bus, corrupting device state. ESP32 core survives due to decoupling capacitors, but I2C peripherals enter corrupted/undefined state. Task watchdog doesn't trigger because main loop continues executing (I2C failures are handled gracefully).

**Protection Gap:** No detection mechanism for peripheral failure when core remains operational. System continues running indefinitely with non-functional display.

---

## Incident Details

### Trigger Event
**Action:** Plug/unplug USB connector while external 3.3V supply is connected

**Dual Power Configuration:**
- **USB Power:** 5V USB → ESP32-S3 onboard regulator → 3.3V
- **External Power:** External 3.3V power supply → ESP32-S3 3.3V rail
- **Ground Loop:** Two power sources share common ground via USB and external supply

### Observed Behavior

| Component | Status After Event | Evidence |
|-----------|-------------------|----------|
| ESP32 Core | ✅ **OPERATIONAL** | WiFi connected, MQTT publishing, heartbeat active |
| Network Stack | ✅ **OPERATIONAL** | WiFi association maintained, TCP connections alive |
| MQTT Client | ✅ **OPERATIONAL** | Publishing heartbeat messages every 100 seconds |
| Main Task | ✅ **OPERATIONAL** | 5-second loop executing (vTaskDelay working) |
| RTC (DS3231) | ❓ **UNKNOWN** | Not validated - may be corrupted or functional |
| Light Sensor (BH1750) | ❓ **UNKNOWN** | Not validated - may be corrupted or functional |
| LED Controllers (TLC59116) | ❌ **FAILED** | All 160 LEDs dark, no display output |
| System Watchdog | ❌ **DID NOT TRIGGER** | No reset, no panic, system continues running |

**Key Observation:** **Partial peripheral failure** - LED controllers fail while ESP32 core and network stack remain functional.

---

## Technical Analysis

### 1. Watchdog Configuration Status

#### A. Interrupt Watchdog Timer (IWDT)
```
CONFIG_ESP_INT_WDT=y
CONFIG_ESP_INT_WDT_TIMEOUT_MS=300
```

**Status:** ✅ **ACTIVE** but did not trigger

**Why it didn't trigger:**
- IWDT detects when interrupts are disabled for >300ms
- USB plug/unplug causes voltage transient (~1-50ms)
- Main loop continues executing normally
- No deadlock, no infinite loop with interrupts disabled
- **Conclusion:** IWDT correctly did not trigger (system not frozen)

#### B. Task Watchdog Timer (TWDT)
```
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_INIT=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=y
```

**Status:** ✅ **ACTIVE** but did not trigger

**Why it didn't trigger:**
- TWDT monitors IDLE tasks on both CPU cores
- IDLE tasks run whenever no user task is executing
- Main loop executes `vTaskDelay(5000)` every 5 seconds
- IDLE task runs during 5-second delay window
- Display update failures return ESP_FAIL gracefully (no hang)
- **Conclusion:** TWDT correctly did not trigger (IDLE tasks running normally)

#### C. Bootloader Watchdog (BWDT)
```
CONFIG_BOOTLOADER_WDT_ENABLE=y
CONFIG_BOOTLOADER_WDT_TIME_MS=9000
```

**Status:** ✅ **ACTIVE** during boot only (disabled after app starts)

**Conclusion:** Not relevant to runtime failures

### 2. Display Update Task Architecture

**Main Loop Structure** ([wordclock.c:376-495](../main/wordclock.c#L376-L495)):
```c
while (1) {
    loop_count++;

    // Read current time from RTC
    ret = wordclock_time_get(&current_time);  // May fail if RTC corrupted
    if (ret == ESP_OK) {
        // Display time with transitions
        display_german_time_with_transitions(&current_time);  // May fail if TLC corrupted

        // Validation (only if display changed)
        if (should_validate) {
            sync_led_state_after_transitions();
            trigger_validation_post_transition();
        }
    } else {
        ESP_LOGW(TAG, "Failed to read RTC - displaying test time");
        display_test_time();  // Fallback display
    }

    // Continue with brightness updates, network status, etc.

    // 5 second update interval
    vTaskDelay(pdMS_TO_TICKS(5000));  // Allows IDLE task to run → TWDT happy
}
```

**Failure Handling:**
- **RTC read failure:** Logs warning, falls back to test display
- **Display update failure:** Logs warning, continues to next iteration
- **I2C timeout:** Returns ESP_ERR_TIMEOUT after 1000ms, main loop continues
- **All failures are graceful** - no task blocking, no infinite loops

**CRITICAL INSIGHT:**
> The main loop is **designed to survive I2C failures**, which means **watchdogs cannot detect peripheral failures** because the task continues executing normally.

### 3. I2C Bus Behavior During Power Disturbance

#### I2C Timeout Configuration
**Every I2C operation has 1000ms timeout:**
```c
// i2c_devices.c:401
esp_err_t ret = i2c_master_transmit(tlc_dev_handles[tlc_index],
                                    write_buffer, sizeof(write_buffer),
                                    pdMS_TO_TICKS(1000));  // 1 second timeout
```

**Timeout behavior:**
- I2C driver waits up to 1000ms for ACK from device
- If device doesn't respond → returns ESP_ERR_TIMEOUT
- Caller logs error and continues (no retry loop, no hang)

#### I2C Mutex Protection
**All I2C operations are mutex-protected:**
```c
// i2c_devices.c:391-409
if (thread_safe_i2c_lock(MUTEX_TIMEOUT_TICKS) != ESP_OK) {
    return ESP_ERR_TIMEOUT;  // Graceful failure if mutex timeout
}

esp_err_t ret = i2c_master_transmit(...);  // I2C operation

thread_safe_i2c_unlock();  // Always releases mutex
```

**Mutex timeout:** 1000ms (1 second)

**Protection against:**
- Concurrent I2C access from multiple tasks
- Deadlocks from nested I2C calls
- Bus corruption from interrupted operations

**CRITICAL INSIGHT:**
> I2C mutex is released even on failure, preventing deadlock. This means **I2C bus corruption does not cause task blocking**, which is why **watchdogs don't trigger**.

#### Voltage Transient Effects on I2C Bus

**USB Plug/Unplug Transient Characteristics:**
```
Event:    USB connector insertion/removal with external 3.3V connected
Duration: 1-50ms (mechanical contact bounce + inrush current)
Voltage:  3.3V ±10% → 3.3V ±50% (worst case: 1.65V - 4.95V spikes)
Ground:   Ground loop current surge via USB shield and external supply GND
```

**I2C Bus During Transient:**
1. **SDA/SCL pulled to VCC (internal pull-ups):** Voltage follows VCC fluctuation
2. **ESP32 I2C peripheral:** May detect spurious START/STOP conditions
3. **TLC59116 I2C interface:** May interpret noise as commands
4. **Auto-increment pointer corruption:** TLC internal register pointer becomes undefined
5. **Register state corruption:** PWM, LEDOUT, MODE registers may contain garbage

**Why TLC59116 Fails:**
- **TLC VCC threshold:** 2.3V minimum (below this = POR reset)
- **Voltage transient:** If VCC dips 2.5V → 1.8V → 3.3V (too fast for clean POR)
- **Incomplete POR:** TLC may partially reset but not complete full POR sequence
- **I2C state machine:** May enter undefined state from mid-transaction glitch
- **No hardware reset:** TLC RESET pin not pulsed after transient

**Why ESP32 Survives:**
- **ESP32 VCC decoupling:** Bulk capacitors on power rail (10-100µF typical)
- **Brownout detector:** 2.80V threshold prevents operation in undefined voltage range
- **Flash/PSRAM protection:** ESP32 enters brownout reset before memory corruption
- **Different power domains:** WiFi/BLE/CPU cores have separate regulation

**CRITICAL INSIGHT:**
> ESP32 has **brownout protection** and **larger decoupling capacitors**. TLC59116 has **no brownout detector** and relies on clean power-on reset. **Voltage transients can corrupt TLC without triggering ESP32 brownout.**

### 4. Dual Power Supply Ground Loop Analysis

#### Ground Loop Formation

**Physical Configuration:**
```
USB Power Path:
USB 5V ──[Diode]──> ESP32-S3 LDO (3.3V) ──> 3.3V Rail ──> TLC59116 VCC
  │                                            │
  └─── USB GND ──────────────────────────> Common GND

External Power Path:
External 3.3V ──────────────────────────> 3.3V Rail ──> TLC59116 VCC
  │                                            │
  └─── External GND ────────────────────> Common GND

Ground Loop:
USB GND ←──[Path 1: USB shield]──→ PC GND ←──[Path 2: External supply]──→ External GND
                                                    ↑
                                                    └── Two ground paths = LOOP
```

**Ground Loop Current Flow:**
When USB is plugged/unplugged:
1. **Mechanical contact bounce:** USB VBUS makes/breaks contact multiple times (~5-50ms)
2. **Inrush current surge:** USB 5V charges ESP32 input capacitors (up to 1-2A peak)
3. **Ground current surge:** Return current flows through both USB GND and external GND
4. **Ground voltage shift:** Ground reference temporarily shifts by ΔV = I × Rloop
5. **Common-mode noise:** All signals referenced to "ground" see voltage shift

**Estimated Ground Loop Resistance:**
- USB cable GND: ~50-100mΩ (1m cable)
- PCB traces: ~10-20mΩ
- External power GND: ~50-100mΩ
- **Total loop resistance:** ~100-200mΩ

**Ground Voltage Shift:**
```
ΔV_ground = I_surge × R_loop
          = 1A × 0.15Ω
          = 150mV ground shift
```

**Impact on I2C Bus:**
- **I2C LOW threshold:** 0.3 × VCC = 0.99V (VCC = 3.3V)
- **I2C HIGH threshold:** 0.7 × VCC = 2.31V
- **Ground shift effect:** Effective signal level changes by ±150mV
- **Noise margin reduction:** Reduces noise immunity from 0.99V to 0.84V
- **Spurious edges:** Fast ground shifts may trigger false START/STOP conditions

**CRITICAL INSIGHT:**
> **Ground loop current creates common-mode noise** that corrupts I2C communication without causing ESP32 brownout. External devices see voltage shifts that ESP32 core does not experience.

---

## Protection Gaps Identified

### 1. **No Peripheral Health Monitoring**

**Current State:**
- System monitors WiFi, NTP, MQTT connection status
- **No monitoring of I2C peripheral health**
- **No detection of TLC59116 failure state**

**Gap:**
- Display can fail silently while system continues operating
- User has no indication of failure (heartbeat still active)
- Manual power cycle required to recover

**Evidence:**
```c
// wordclock.c:464-472 - Only network status monitored
if (thread_safe_get_mqtt_connected()) {
    ESP_LOGI(TAG, "MQTT Status: Connected");
    mqtt_publish_sensor_status();
    mqtt_publish_heartbeat_with_ntp();
}
// ❌ No equivalent check for display health
```

### 2. **No Display Update Timeout Detection**

**Current State:**
- Display updates return ESP_FAIL on I2C errors
- Errors are logged but **no escalation mechanism**
- **No cumulative failure tracking**

**Gap:**
- Single I2C timeout is recoverable (transient error)
- **Sustained I2C failures are not detected**
- System doesn't distinguish between "temporary glitch" and "permanent failure"

**Evidence:**
```c
// wordclock_display.c:319-327 - Graceful failure, no escalation
ret = update_led_if_changed(row, col, new_led_state[row][col]);
if (ret == ESP_OK) {
    changes_made++;
} else if (!device_row_failed[row]) {
    ESP_LOGW(TAG, "Failed to update LED at (%d,%d): %s", row, col, esp_err_to_name(ret));
    device_row_failed[row] = true;  // ❌ Flag is never checked again!
}
```

### 3. **No Automatic Recovery After Peripheral Failure**

**Current State:**
- Hardware reset capability exists (`tlc59116_hardware_reset_all()`)
- Only triggered manually via MQTT command
- **Not integrated into automatic recovery system**

**Gap:**
- Manual intervention required to recover from peripheral failure
- User must notice display is dark and send MQTT command
- **No automatic recovery even when hardware reset is available**

**Evidence:**
```c
// mqtt_manager.c:1197-1209 - Manual reset only
if (strcmp(topic_suffix, "/command") == 0) {
    if (strcmp((char*)data, "tlc_hardware_reset") == 0) {
        // Only executes when user sends MQTT command
        tlc59116_init_all();
        memset(led_state, 0, sizeof(led_state));
    }
}
// ❌ No automatic trigger when display fails
```

### 4. **LED Validation Not Integrated into Health Monitoring**

**Current State:**
- LED validation system exists and runs post-transition
- Validation detects mismatches between software and hardware state
- **Validation results are not used for health decisions**

**Gap:**
- Validation failures are logged and published to MQTT
- **No automatic action on repeated validation failures**
- **No connection between validation and recovery system**

**Evidence:**
```c
// led_validation.c - Validation publishes results but doesn't trigger recovery
mqtt_publish_validation_result(&result);  // ✅ Publishes
mqtt_publish_validation_mismatches(&mismatch_data);  // ✅ Publishes
// ❌ But doesn't call tlc59116_hardware_reset_all() automatically
```

### 5. **No Task Watchdog for Display Updates**

**Current State:**
- Task watchdog (TWDT) only monitors IDLE tasks
- **Display task is not subscribed to TWDT**
- **No timeout detection for display operations**

**Gap:**
- Main loop continues even if display permanently fails
- TWDT doesn't detect "partial system failure"
- **Watchdog only catches complete system freeze**

**Recommendation:**
```c
// NOT IMPLEMENTED - Would catch display failures
#include "esp_task_wdt.h"

void test_live_word_clock(void) {
    esp_task_wdt_add(NULL);  // Subscribe main task to watchdog

    while (1) {
        ret = display_german_time_with_transitions(&current_time);

        if (ret == ESP_OK) {
            esp_task_wdt_reset();  // Reset watchdog on successful display update
        }
        // ❌ If display permanently fails, watchdog expires after 5 seconds

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## Root Cause Summary

### Primary Cause: **Voltage Transient from Dual Power Sources**

1. **USB plug/unplug** creates inrush current surge (1-2A peak)
2. **Ground loop current** flows through USB shield and external supply
3. **Ground voltage shift** (±150mV) corrupts I2C bus common-mode level
4. **TLC59116 I2C state machine** enters undefined state from spurious edges
5. **ESP32 survives** due to brownout protection and decoupling capacitors
6. **TLC59116 fails** without clean power-on reset

### Contributing Factors:

- **No hardware reset pulse** after voltage transient
- **No peripheral health monitoring** to detect failure
- **No automatic recovery** even though hardware reset capability exists
- **Graceful failure handling** prevents watchdog trigger (design feature becomes gap)
- **No task watchdog subscription** for display operations

---

## Recommendations

### Immediate Actions (Priority 1)

#### 1. **Add Display Health Monitoring**
Track consecutive display update failures and trigger recovery:

```c
// In wordclock.c main loop
static uint8_t consecutive_display_failures = 0;
#define MAX_DISPLAY_FAILURES 3

while (1) {
    ret = display_german_time_with_transitions(&current_time);

    if (ret == ESP_OK) {
        consecutive_display_failures = 0;  // Reset on success
    } else {
        consecutive_display_failures++;
        ESP_LOGW(TAG, "Display update failed (%d/%d)",
                 consecutive_display_failures, MAX_DISPLAY_FAILURES);

        if (consecutive_display_failures >= MAX_DISPLAY_FAILURES) {
            ESP_LOGE(TAG, "⚠️ DISPLAY FAILURE DETECTED - Triggering recovery");
            trigger_display_recovery();
            consecutive_display_failures = 0;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
}
```

#### 2. **Implement Automatic Recovery Function**
Multi-step recovery with escalation:

```c
esp_err_t trigger_display_recovery(void) {
    ESP_LOGW(TAG, "=== DISPLAY RECOVERY SEQUENCE STARTED ===");

    // Step 1: Hardware reset (if available)
    if (tlc59116_has_hardware_reset()) {
        ESP_LOGI(TAG, "Step 1: TLC hardware reset via GPIO 4...");
        esp_err_t ret = tlc59116_hardware_reset_all();
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Wait for stabilization

            ret = tlc59116_init_all();
            if (ret == ESP_OK) {
                memset(led_state, 0, sizeof(led_state));  // Clear LED state cache
                ESP_LOGI(TAG, "✅ Hardware reset recovery successful");
                return ESP_OK;
            }
        }
    }

    // Step 2: Software reset (fallback)
    ESP_LOGI(TAG, "Step 2: TLC software reset via I2C...");
    esp_err_t ret = tlc59116_init_all();  // Re-initialize all devices
    if (ret == ESP_OK) {
        memset(led_state, 0, sizeof(led_state));
        ESP_LOGI(TAG, "✅ Software reset recovery successful");
        return ESP_OK;
    }

    // Step 3: I2C bus reset (last resort)
    ESP_LOGE(TAG, "Step 3: I2C bus reset - devices not responding");
    // Could call i2c_driver_delete() + i2c_devices_init() here

    // Step 4: System restart (nuclear option)
    ESP_LOGE(TAG, "❌ Recovery failed - system restart required");
    mqtt_publish_status("display_recovery_failed");
    vTaskDelay(pdMS_TO_TICKS(1000));  // Allow MQTT publish to complete
    esp_restart();  // Last resort

    return ESP_FAIL;
}
```

#### 3. **Integrate LED Validation into Health System**
Use existing validation to detect hardware failures:

```c
// In led_validation.c - modify validation task
if (!result.is_valid) {
    consecutive_validation_failures++;

    if (consecutive_validation_failures >= MAX_VALIDATION_FAILURES) {
        ESP_LOGE(TAG, "⚠️ PERSISTENT VALIDATION FAILURES - Triggering recovery");
        trigger_display_recovery();
        consecutive_validation_failures = 0;
    }
} else {
    consecutive_validation_failures = 0;
}
```

### Medium-Term Actions (Priority 2)

#### 4. **Add Task Watchdog Subscription for Display**
Catch display task hangs:

```c
#include "esp_task_wdt.h"

void test_live_word_clock(void) {
    // Subscribe main display task to watchdog
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "Main display task subscribed to watchdog (5s timeout)");

    while (1) {
        ret = display_german_time_with_transitions(&current_time);

        if (ret == ESP_OK) {
            esp_task_wdt_reset();  // Reset watchdog on successful display
        }
        // If display fails repeatedly, consecutive_display_failures triggers recovery
        // before watchdog expires

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

#### 5. **Publish Display Health to MQTT**
Allow Home Assistant to monitor display status:

```c
// New MQTT topic: home/Clock_Stani_1/display/health
{
    "status": "ok",  // "ok", "degraded", "failed"
    "consecutive_failures": 0,
    "last_update_time": "2025-12-10T14:30:00Z",
    "recovery_count": 0,
    "i2c_error_count": 0
}
```

#### 6. **Add Display Health Sensor to Home Assistant Discovery**
```c
// In mqtt_discovery.c
cJSON *config = cJSON_CreateObject();
cJSON_AddStringToObject(config, "name", "Display Health");
cJSON_AddStringToObject(config, "state_topic", "~/display/health");
cJSON_AddStringToObject(config, "value_template", "{{ value_json.status }}");
cJSON_AddStringToObject(config, "device_class", "problem");
```

### Long-Term Actions (Priority 3)

#### 7. **Hardware Modification: Eliminate Ground Loop**
**Option A: Single Power Source**
- Use either USB power OR external 3.3V, never both
- Add Schottky diode ORing circuit to prevent backfeed
- Ensures clean power source switchover

**Option B: Isolated Power Supplies**
- Add DC-DC isolation between USB and external supply
- Breaks ground loop with isolated converter
- More expensive but eliminates common-mode noise

#### 8. **Hardware Modification: TLC Hardware Reset on Brownout**
**Add brownout detection circuit:**
```
ESP32 GPIO 4 ──┬──[10kΩ Pull-up to 3.3V]──> TLC RESET
               │
               └──[Reset Controller IC (e.g., TPS3840)]
                   └── Monitors 3.3V rail, asserts RESET if V < 2.9V
```

**Benefits:**
- Automatic TLC reset when voltage sags below safe threshold
- Ensures TLC gets clean POR after voltage transient
- Independent of ESP32 software state

#### 9. **Add I2C Bus Monitoring Circuitry**
**Hardware I2C watchdog:**
- Monitor SDA/SCL for stuck-low conditions
- Automatically pulse bus reset if stuck >100ms
- Prevents permanent I2C lockup

---

## Testing Recommendations

### Test 1: **Reproduce Failure Under Controlled Conditions**
1. Connect USB + external 3.3V simultaneously
2. Plug/unplug USB repeatedly (10× cycles)
3. Monitor display status and MQTT heartbeat
4. Confirm: Display fails, heartbeat continues

### Test 2: **Validate Automatic Recovery**
1. Implement display health monitoring (Recommendation #1)
2. Implement automatic recovery (Recommendation #2)
3. Repeat Test 1
4. Verify: System detects failure and recovers automatically within 15 seconds

### Test 3: **Ground Loop Current Measurement**
1. Insert current probe in ground path
2. Measure ground loop current during USB plug/unplug
3. Verify: Current surge matches calculation (1-2A peak)

### Test 4: **Voltage Transient Characterization**
1. Measure 3.3V rail voltage with oscilloscope (100MHz, 1GSa/s)
2. Capture voltage during USB plug/unplug
3. Verify: Voltage dips/spikes exceed TLC tolerance but stay within ESP32 brownout threshold

---

## Conclusion

The dual power supply partial failure is a **critical protection gap** that leaves the system in an undetected failed state. The ESP32 core's resilience (brownout protection, decoupling) actually works *against* failure detection because the watchdogs don't trigger when peripherals fail but the core survives.

**Key Findings:**
1. ✅ **Watchdogs are working correctly** - they detect system freezes, not peripheral failures
2. ✅ **I2C timeout handling is working correctly** - prevents task blocking
3. ❌ **No peripheral health monitoring** - critical gap
4. ❌ **No automatic recovery** - despite hardware reset capability existing
5. ❌ **Ground loop from dual power** - hardware design issue

**Priority Recommendations:**
1. **Immediate:** Implement display health monitoring + automatic recovery (Software)
2. **Medium:** Add MQTT health reporting + Home Assistant integration (Software)
3. **Long-term:** Eliminate ground loop with power source OR-ing (Hardware)

**Expected Outcome:**
After implementing Recommendations #1-3, system should detect display failure within 15 seconds and automatically recover without manual intervention. User experience changes from "device appears dead, needs power cycle" to "display flickers briefly during recovery, continues operation."

---

## References

- [watchdog-configuration.md](watchdog-configuration.md) - ESP32 watchdog system details
- [tlc59116-power-surge-analysis.md](tlc59116-power-surge-analysis.md) - TLC POR behavior
- [wordclock.c:376-495](../main/wordclock.c#L376-L495) - Main loop structure
- [i2c_devices.c](../components/i2c_devices/i2c_devices.c) - I2C timeout handling
- [thread_safety.c](../main/thread_safety.c) - I2C mutex implementation
- [led_validation.c](../components/led_validation/led_validation.c) - Validation system

---

**Next Steps:**
1. User approval of Priority 1 recommendations
2. Implement display health monitoring in main loop
3. Implement automatic recovery function
4. Test recovery with controlled USB plug/unplug cycles
5. Measure ground loop current and voltage transients for validation
