# LED Validation - Hardware Readback Addendum
## TLC59116 Register Readback for Hardware State Verification

## Critical Issue Identified

**Problem:** The original validation proposal only checks the software `led_state[10][16]` array, **not the actual hardware state** of the TLC59116 LED controllers.

**Risk:** I2C communication failures could result in:
- Software array says LEDs are ON, but hardware LEDs are OFF (I2C write failed)
- Software array says LEDs are OFF, but hardware LEDs are stuck ON (hardware fault)
- Mismatched brightness values between software and hardware

**Solution:** Read back TLC59116 PWM registers via I2C to verify actual hardware state.

---

## TLC59116 Register Readback Capability

### Available Registers for Validation

The TLC59116 supports reading back several registers that reflect the actual hardware state:

#### 1. PWM Registers (0x02-0x11) - Individual LED Brightness
- **Address:** `TLC59116_PWM0` through `TLC59116_PWM15` (0x02-0x11)
- **Function:** Individual PWM duty cycle for each of 16 LEDs
- **Value Range:** 0-255 (0 = OFF, 255 = full brightness)
- **Readable:** ✅ YES - Can read current PWM value

#### 2. GRPPWM Register (0x12) - Global Brightness
- **Address:** `TLC59116_GRPPWM` (0x12)
- **Function:** Global PWM dimming control
- **Value Range:** 0-255 (0 = all LEDs OFF, 255 = full brightness)
- **Readable:** ✅ YES - Can read global brightness

#### 3. LEDOUT Registers (0x14-0x17) - LED Output State
- **Address:** `TLC59116_LEDOUT0` through `TLC59116_LEDOUT3` (0x14-0x17)
- **Function:** Controls LED driver output state (OFF/ON/PWM/GRPPWM)
- **Format:** 2 bits per LED, 4 LEDs per register
- **Readable:** ✅ YES - Can verify LED is in GRPPWM mode

#### 4. EFLAG Registers (0x1D-0x1E) - Error Flags
- **Address:** `TLC59116_EFLAG1` (0x1D), `TLC59116_EFLAG2` (0x1E)
- **Function:** LED open-circuit and short-circuit detection
- **Readable:** ✅ YES - Hardware fault detection

---

## Enhanced Validation Architecture

### Complete Validation Strategy

**Design Philosophy:** Run FULL validation every 5 minutes. The 110ms execution time has negligible system impact (0.037% CPU time), and I2C bus capacity is underutilized. Fast fault detection (within 5 minutes) outweighs any theoretical performance concerns.

```
┌─────────────────────────────────────────────────────────────────┐
│ Complete LED Validation (runs every 5 minutes)                  │
│                                                                  │
│ 1. Software State Validation                                    │
│    ├─ Compare led_state[10][16] vs expected bitmap              │
│    ├─ Check for invalid LED positions active                    │
│    └─ Time: ~2ms (no I2C)                                       │
│                                                                  │
│ 2. Hardware State Readback                                      │
│    ├─ Read PWM registers from all 10 TLC59116 devices           │
│    ├─ Compare hardware PWM vs software led_state                │
│    ├─ Read GRPPWM to verify global brightness                   │
│    └─ Time: ~80ms (10 I2C read transactions)                    │
│                                                                  │
│ 3. Hardware Fault Detection                                     │
│    ├─ Read EFLAG registers for LED errors                       │
│    ├─ Detect open-circuit or short-circuit conditions           │
│    ├─ Check LEDOUT registers for correct mode                   │
│    └─ Time: ~30ms (20 I2C read transactions)                    │
│                                                                  │
│ Total Time: ~110ms every 5 minutes                              │
│ CPU Impact: 0.037% (negligible)                                 │
│ Fault Detection: Within 5 minutes (excellent)                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Implementation Details

### Hardware Readback Functions

#### Function 1: Read Single TLC59116 PWM Values
```c
/**
 * @brief Read all PWM values from a single TLC59116 device
 *
 * @param tlc_index TLC device index (0-9, corresponds to row)
 * @param pwm_values Output array for 16 PWM values
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tlc_read_pwm_values(uint8_t tlc_index, uint8_t pwm_values[16]) {
    if (tlc_index >= TLC59116_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!tlc_devices[tlc_index].initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read all 16 PWM registers (0x02-0x11) in one transaction
    // Using auto-increment mode configured during init
    uint8_t reg_addr = TLC59116_PWM0;
    return i2c_read_bytes(
        I2C_LEDS_MASTER_PORT,
        tlc_addresses[tlc_index],
        reg_addr,
        pwm_values,
        16  // Read all 16 PWM registers
    );
}
```

#### Function 2: Read All TLC59116 Devices (Complete Matrix)
```c
/**
 * @brief Read PWM values from all TLC59116 devices (entire LED matrix)
 *
 * @param hardware_state Output 10x16 array with actual hardware PWM values
 * @return esp_err_t ESP_OK if at least one device read successfully
 */
esp_err_t tlc_read_all_pwm_values(uint8_t hardware_state[10][16]) {
    esp_err_t ret = ESP_OK;
    int success_count = 0;

    for (uint8_t row = 0; row < TLC59116_COUNT; row++) {
        esp_err_t row_ret = tlc_read_pwm_values(row, hardware_state[row]);

        if (row_ret == ESP_OK) {
            success_count++;
        } else {
            // Mark row as invalid on read failure
            memset(hardware_state[row], 0xFF, 16);
            ret = row_ret;
            ESP_LOGW(TAG, "Failed to read PWM from TLC %d: %s",
                     row, esp_err_to_name(row_ret));
        }

        // Small delay between devices to prevent I2C bus saturation
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    ESP_LOGI(TAG, "Hardware readback: %d/10 devices read successfully",
             success_count);

    return (success_count > 0) ? ESP_OK : ret;
}
```

#### Function 3: Read Global Brightness (GRPPWM)
```c
/**
 * @brief Read GRPPWM register from all TLC59116 devices
 *
 * @param grppwm_values Output array for 10 GRPPWM values
 * @return esp_err_t ESP_OK on success
 */
esp_err_t tlc_read_global_brightness(uint8_t grppwm_values[10]) {
    esp_err_t ret = ESP_OK;

    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            grppwm_values[i] = 0xFF;  // Invalid marker
            continue;
        }

        esp_err_t device_ret = i2c_read_bytes(
            I2C_LEDS_MASTER_PORT,
            tlc_addresses[i],
            TLC59116_GRPPWM,
            &grppwm_values[i],
            1
        );

        if (device_ret != ESP_OK) {
            grppwm_values[i] = 0xFF;
            ret = device_ret;
        }
    }

    return ret;
}
```

#### Function 4: Read Error Flags (EFLAG)
```c
/**
 * @brief Read error flags from all TLC59116 devices
 *
 * @param eflag_values Output array for 10 devices × 2 EFLAG registers
 * @return esp_err_t ESP_OK if no errors detected
 */
esp_err_t tlc_read_error_flags(uint8_t eflag_values[10][2]) {
    esp_err_t ret = ESP_OK;
    bool errors_found = false;

    for (uint8_t i = 0; i < TLC59116_COUNT; i++) {
        if (!tlc_devices[i].initialized) {
            eflag_values[i][0] = 0xFF;
            eflag_values[i][1] = 0xFF;
            continue;
        }

        // Read EFLAG1 and EFLAG2
        esp_err_t device_ret = i2c_read_bytes(
            I2C_LEDS_MASTER_PORT,
            tlc_addresses[i],
            TLC59116_EFLAG1,
            eflag_values[i],
            2  // Read both EFLAG registers
        );

        if (device_ret != ESP_OK) {
            ret = device_ret;
        } else if (eflag_values[i][0] != 0 || eflag_values[i][1] != 0) {
            // Non-zero EFLAG indicates hardware fault
            errors_found = true;
            ESP_LOGW(TAG, "TLC %d EFLAG errors: 0x%02X 0x%02X",
                     i, eflag_values[i][0], eflag_values[i][1]);
        }
    }

    return errors_found ? ESP_FAIL : ret;
}
```

---

## Enhanced Validation Result Structure

```c
typedef struct {
    // Original software validation results
    bool software_valid;
    uint8_t software_errors;

    // NEW: Hardware readback results
    bool hardware_valid;
    uint8_t hardware_mismatch_count;
    uint8_t hardware_read_failures;

    // NEW: Hardware fault detection
    bool hardware_faults_detected;
    uint8_t devices_with_faults;

    // Detailed error tracking
    struct {
        uint8_t row;
        uint8_t col;
        uint8_t expected;     // Expected PWM value
        uint8_t software;     // Software array value
        uint8_t hardware;     // Actual hardware PWM value
    } mismatches[50];

    // NEW: GRPPWM verification
    uint8_t expected_grppwm;
    uint8_t actual_grppwm[10];  // One per device
    bool grppwm_mismatch;

    // NEW: Hardware fault details
    uint8_t eflag_values[10][2];  // EFLAG1 and EFLAG2 per device

    // Overall status
    bool is_valid;  // True only if ALL checks pass

} validation_result_enhanced_t;
```

---

## Complete Validation Function

```c
/**
 * @brief Complete LED display validation with hardware readback
 *
 * Performs comprehensive validation:
 * 1. Software state vs expected state (~2ms)
 * 2. Hardware PWM readback vs software state (~80ms)
 * 3. Hardware fault detection via EFLAG registers (~30ms)
 *
 * Total execution time: ~110ms
 * Recommended frequency: Every 5 minutes (0.037% CPU impact)
 *
 * @param current_hour Current hour (1-12)
 * @param current_minutes Current minutes (0-59)
 * @return validation_result_enhanced_t Complete validation results
 */
validation_result_enhanced_t validate_display_with_hardware(
    uint8_t current_hour,
    uint8_t current_minutes
) {
    validation_result_enhanced_t result = {0};

    // ===================================================================
    // LEVEL 1: Software State Validation (original proposal)
    // ===================================================================

    // Generate expected state
    wordclock_time_t ref_time = {
        .hours = current_hour,
        .minutes = current_minutes,
        .seconds = 0
    };
    uint8_t expected_bitmap[10][16] = {0};
    build_led_state_matrix(&ref_time, expected_bitmap);

    // Compare software state vs expected
    for (uint8_t row = 0; row < 10; row++) {
        for (uint8_t col = 0; col < 16; col++) {
            bool expected = (expected_bitmap[row][col] > 0);
            bool software = (led_state[row][col] > 0);

            if (expected != software) {
                result.software_errors++;
                // Record mismatch details...
            }
        }
    }

    result.software_valid = (result.software_errors == 0);

    // ===================================================================
    // LEVEL 2: Hardware State Readback
    // ===================================================================

    uint8_t hardware_pwm[10][16];
    esp_err_t hw_ret = tlc_read_all_pwm_values(hardware_pwm);

    if (hw_ret == ESP_OK) {
            // Compare hardware vs software state
            for (uint8_t row = 0; row < 10; row++) {
                for (uint8_t col = 0; col < 16; col++) {
                    if (hardware_pwm[row][col] == 0xFF) {
                        // Read failed for this device
                        result.hardware_read_failures++;
                        continue;
                    }

                    // Check if hardware matches software
                    // Allow small tolerance (±2) for timing issues
                    int16_t diff = abs((int16_t)hardware_pwm[row][col] -
                                       (int16_t)led_state[row][col]);

                    if (diff > 2) {
                        result.hardware_mismatch_count++;

                        // Record mismatch
                        if (result.hardware_mismatch_count < 50) {
                            int idx = result.hardware_mismatch_count - 1;
                            result.mismatches[idx].row = row;
                            result.mismatches[idx].col = col;
                            result.mismatches[idx].expected = expected_bitmap[row][col];
                            result.mismatches[idx].software = led_state[row][col];
                            result.mismatches[idx].hardware = hardware_pwm[row][col];
                        }
                    }
                }
            }

            result.hardware_valid = (result.hardware_mismatch_count == 0 &&
                                    result.hardware_read_failures == 0);
    } else {
        ESP_LOGE(TAG, "Hardware readback failed completely");
        result.hardware_valid = false;
    }

    // ===================================================================
    // LEVEL 3: Hardware Fault Detection
    // ===================================================================

    esp_err_t eflag_ret = tlc_read_error_flags(result.eflag_values);

    if (eflag_ret == ESP_FAIL) {
        result.hardware_faults_detected = true;

        // Count devices with faults
        for (uint8_t i = 0; i < 10; i++) {
            if (result.eflag_values[i][0] != 0 ||
                result.eflag_values[i][1] != 0) {
                result.devices_with_faults++;
            }
        }
    }

    // Verify GRPPWM consistency
    result.expected_grppwm = tlc_get_global_brightness();
    tlc_read_global_brightness(result.actual_grppwm);

    for (uint8_t i = 0; i < 10; i++) {
        if (result.actual_grppwm[i] != 0xFF &&
            result.actual_grppwm[i] != result.expected_grppwm) {
            result.grppwm_mismatch = true;
            break;
        }
    }

    // ===================================================================
    // Overall Validation Result
    // ===================================================================

    result.is_valid = result.software_valid &&
                     result.hardware_valid &&
                     !result.hardware_faults_detected &&
                     !result.grppwm_mismatch;

    return result;
}
```

---

## Error Severity with Hardware Readback

### Enhanced Severity Levels

1. **CRITICAL - Hardware Fault Detected**
   - EFLAG registers show open-circuit or short-circuit
   - **Action:** Log critical error, disable affected device, notify via MQTT
   - **Recovery:** May require hardware repair

2. **CRITICAL - Complete Hardware Read Failure**
   - Cannot read any TLC59116 PWM registers
   - I2C bus communication failure
   - **Action:** Emergency I2C bus reset, full display refresh

3. **HIGH - Significant Hardware/Software Mismatch**
   - >20% of LEDs have different PWM between hardware and software
   - Indicates systematic I2C write failures
   - **Action:** Full display refresh with verification retry

4. **MEDIUM - Partial Hardware Mismatch**
   - 1-20% of LEDs mismatched
   - May indicate intermittent I2C issues
   - **Action:** Log warning, refresh affected LEDs only

5. **MEDIUM - GRPPWM Mismatch**
   - Global brightness register doesn't match expected value
   - **Action:** Rewrite GRPPWM, verify readback

6. **LOW - Software State Error Only**
   - Software state wrong, but hardware matches expected
   - **Action:** Update software array from hardware (resync)

---

## Performance Analysis

### Execution Time Breakdown

```
Complete Validation (All 3 Levels):

  Software Validation:
    - 160 LED comparisons
    - No I2C operations
    - Time: ~2ms

  Hardware PWM Readback:
    - 10 TLC devices × 16 registers each
    - I2C reads: 10 transactions (with auto-increment)
    - Time per device: ~8ms
    - Total: ~80ms

  Hardware Fault Detection:
    - 10 EFLAG reads (2 bytes each)
    - 10 GRPPWM reads (1 byte each)
    - Time: ~30ms

Total Validation Time: ~110ms
```

### System Impact Analysis

**Running every 5 minutes (300 seconds):**
- Execution time: 110ms per validation
- CPU usage: 110ms / 300000ms = **0.037%**
- I2C operations: 30 reads / 300 seconds = **0.1 reads/second average**
- User-visible impact: **None** (110ms is imperceptible)

**I2C Bus Capacity:**
- Current system: Hundreds of writes during transitions
- Validation adds: 30 reads every 5 minutes
- Bus utilization: **Massively underutilized**
- Conclusion: **No bottleneck concerns**

### Why NOT to Use Progressive Validation

**Arguments AGAINST complexity:**
1. **Delayed Fault Detection:** Hardware fault at 14:01 not detected until 19:01 (5 hours!)
2. **Added Complexity:** Tracking validation counters, multiple code paths to test
3. **False Economy:** "Saving" 80ms every 5 minutes has zero benefit
4. **Debugging Difficulty:** "Which validation level ran when?" becomes confusing
5. **No Real Benefit:** ESP32 is mostly idle between time updates

**Simple is Better:**
- One validation function that always does the same thing
- Predictable execution time and behavior
- Consistent monitoring data for Home Assistant
- Fast fault detection (within 5 minutes maximum)

---

## Integration Strategy

### Recommended Implementation

**Simple Periodic Validation (Recommended)**
```c
void periodic_validation_task(void *pvParameters) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(300000));  // 5 minutes

        // Always run FULL validation (110ms)
        validation_result_enhanced_t result =
            validate_display_with_hardware(hour, minutes);

        if (!result.is_valid) {
            handle_validation_failure(&result);
        }

        // Publish results to MQTT for Home Assistant monitoring
        publish_validation_results(&result);
    }
}
```

**Why this approach:**
- Simple: No complex scheduling logic
- Fast fault detection: Maximum 5 minutes to detect any issue
- Predictable: Always runs the same validation
- Negligible impact: 0.037% CPU time
- Consistent data: Home Assistant graphs show continuous monitoring

**On-Demand Validation (MQTT Command)**
```bash
# Trigger immediate validation
Topic: home/Clock_Stani_1/display/validation/trigger
Payload: "validate"

# Response will be published to status topic
Topic: home/Clock_Stani_1/display/validation/hardware
```

---

## MQTT Diagnostic Enhancement

### Hardware Readback Status Topic

```json
Topic: home/Clock_Stani_1/display/validation/hardware
Payload: {
  "timestamp": "2025-01-08T14:35:00Z",
  "hardware_valid": false,
  "hardware_mismatches": 5,
  "hardware_read_failures": 0,
  "hardware_faults": false,
  "grppwm_mismatch": false,
  "details": {
    "expected_grppwm": 180,
    "mismatches": [
      {"row": 5, "col": 3, "expected": 60, "software": 60, "hardware": 0},
      {"row": 5, "col": 4, "expected": 0, "software": 0, "hardware": 60},
      ...
    ],
    "eflag_errors": [
      {"device": 5, "eflag1": 0x08, "eflag2": 0x00}
    ]
  },
  "i2c_stats": {
    "read_duration_ms": 75,
    "successful_devices": 10,
    "failed_devices": 0
  }
}
```

### Home Assistant Sensor Configuration

```yaml
sensor:
  - platform: mqtt
    name: "Word Clock Hardware Validation"
    state_topic: "home/Clock_Stani_1/display/validation/hardware"
    value_template: >
      {% if value_json.hardware_valid %}
        Valid
      {% elif value_json.hardware_faults %}
        Hardware Fault
      {% elif value_json.hardware_mismatches > 0 %}
        I2C Mismatch ({{ value_json.hardware_mismatches }})
      {% elif value_json.hardware_read_failures > 0 %}
        Read Failure
      {% else %}
        Unknown
      {% endif %}
    json_attributes_topic: "home/Clock_Stani_1/display/validation/hardware"

binary_sensor:
  - platform: mqtt
    name: "Word Clock Hardware Fault"
    state_topic: "home/Clock_Stani_1/display/validation/hardware"
    value_template: "{{ value_json.hardware_faults }}"
    payload_on: true
    payload_off: false
    device_class: problem
```

---

## Recovery Actions

### Hardware Mismatch Recovery

```c
void recover_hardware_mismatch(validation_result_enhanced_t *result) {
    ESP_LOGW(TAG, "Hardware mismatch detected, attempting recovery");

    // Strategy 1: Rewrite mismatched LEDs
    for (int i = 0; i < result->hardware_mismatch_count && i < 50; i++) {
        uint8_t row = result->mismatches[i].row;
        uint8_t col = result->mismatches[i].col;
        uint8_t expected = result->mismatches[i].expected;

        ESP_LOGI(TAG, "Rewriting LED[%d][%d]: %d", row, col, expected);
        tlc_set_matrix_led(row, col, expected);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    // Strategy 2: Wait and verify
    vTaskDelay(pdMS_TO_TICKS(50));

    // Re-read hardware to confirm
    uint8_t verify_pwm[16];
    for (int i = 0; i < result->hardware_mismatch_count && i < 50; i++) {
        uint8_t row = result->mismatches[i].row;

        if (tlc_read_pwm_values(row, verify_pwm) == ESP_OK) {
            uint8_t col = result->mismatches[i].col;
            if (verify_pwm[col] == result->mismatches[i].expected) {
                ESP_LOGI(TAG, "LED[%d][%d] recovery successful", row, col);
            } else {
                ESP_LOGE(TAG, "LED[%d][%d] recovery FAILED", row, col);
            }
        }
    }
}
```

### Hardware Fault Recovery

```c
void recover_hardware_fault(validation_result_enhanced_t *result) {
    ESP_LOGE(TAG, "Hardware faults detected on %d devices",
             result->devices_with_faults);

    for (uint8_t i = 0; i < 10; i++) {
        if (result->eflag_values[i][0] != 0 ||
            result->eflag_values[i][1] != 0) {

            ESP_LOGE(TAG, "Device %d EFLAG: 0x%02X 0x%02X",
                     i, result->eflag_values[i][0], result->eflag_values[i][1]);

            // Attempt device reset
            ESP_LOGI(TAG, "Resetting TLC device %d", i);
            tlc59116_reset_device(i);
            vTaskDelay(pdMS_TO_TICKS(10));
            tlc59116_init_device(i);

            // Clear error flags by reading
            uint8_t clear_eflag[2];
            i2c_read_bytes(I2C_LEDS_MASTER_PORT, tlc_addresses[i],
                          TLC59116_EFLAG1, clear_eflag, 2);
        }
    }
}
```

---

## Testing Requirements

### Hardware Readback Tests

**Test 1: I2C Read Reliability**
```
- Read all 10 TLC devices PWM registers 1000 times
- Verify: 100% success rate
- Measure: Average read time per device
```

**Test 2: Software/Hardware Consistency**
```
- Set known pattern (checkerboard, all on, all off)
- Read back hardware state
- Verify: 100% match between software and hardware
```

**Test 3: Fault Injection**
```
- Disconnect one TLC device
- Run validation
- Verify: Correctly detects read failure
- Verify: Other 9 devices still validate correctly
```

**Test 4: Recovery Effectiveness**
```
- Inject mismatches by corrupting led_state array
- Run validation
- Trigger recovery
- Verify: Hardware state restored to expected
```

---

## Summary

### Key Additions to Original Proposal

✅ **Hardware PWM Readback** - Verify actual TLC59116 register values
✅ **I2C Write Verification** - Detect failed I2C transactions
✅ **Hardware Fault Detection** - Read EFLAG registers for LED errors
✅ **GRPPWM Verification** - Ensure global brightness is correct
✅ **Recovery Mechanisms** - Automatic correction of hardware mismatches
✅ **Three-Level Validation** - Software → Hardware → Faults

### Performance Impact

| Metric | Value | Impact |
|--------|-------|--------|
| Total execution time | 110ms | Imperceptible to user |
| CPU usage (5 min intervals) | 0.037% | Negligible |
| I2C operations | 30 reads / 5 min | 0.1 reads/sec average |
| Fault detection latency | Max 5 minutes | Excellent |
| Recommended frequency | Every 5 minutes | Always run FULL validation |

### Benefits Over Software-Only Validation

1. **Detects I2C Failures:** Catches communication errors between ESP32 and TLC59116
2. **Hardware Fault Detection:** Identifies LED open/short circuits via EFLAG
3. **Root Cause Analysis:** Distinguishes software bugs from hardware issues
4. **Verification of Writes:** Confirms I2C writes actually reached hardware
5. **System Health Monitoring:** Trending of I2C reliability over time

---

## Implementation Priority

### Phase 1: Hardware Readback Functions (2-3 days)
- Implement `tlc_read_pwm_values()` and `tlc_read_all_pwm_values()`
- Implement `tlc_read_global_brightness()` and `tlc_read_error_flags()`
- Unit test all I2C read functions on real hardware
- Verify auto-increment mode works for batch reads

### Phase 2: Complete Validation Function (2-3 days)
- Implement `validate_display_with_hardware()` with all 3 levels
- Implement `validation_result_enhanced_t` structure
- Add hardware comparison logic with tolerance (±2 PWM)
- Test validation on known-good and known-bad states

### Phase 3: Recovery and Integration (2 days)
- Implement `handle_validation_failure()` with recovery actions
- Implement `recover_hardware_mismatch()` and `recover_hardware_fault()`
- Create periodic validation task (5-minute interval)
- Add MQTT command for on-demand validation

### Phase 4: MQTT Diagnostics (1-2 days)
- Implement `publish_validation_results()` function
- Add Home Assistant sensor configurations
- Test trending and alerting in Home Assistant
- Document MQTT topics and payloads

**Total Estimated Effort:** 7-10 days of development + testing

**Note:** No progressive validation complexity needed. Always run full 110ms validation every 5 minutes.
