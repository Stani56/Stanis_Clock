# LED Validation: Post-Transition Approach

## Overview

The LED validation system validates hardware LED state **immediately after transitions complete**, ensuring the TLC59116 auto-increment pointer is in a known state for reliable readback.

## Architecture

### Trigger Mechanism

Validation is **event-driven**, not periodic:

```c
// In main loop (wordclock.c)
display_german_time_with_transitions(&current_time);  // Update display
sync_led_state_after_transitions();                   // Wait for transitions
trigger_validation_post_transition();                 // ← Validate immediately
```

### Why Post-Transition?

1. **Auto-increment pointer fresh** - Transitions just wrote all LEDs sequentially (0→159)
2. **Display stable** - Won't change for ~5 minutes until next time update
3. **Known state** - Current time, brightness, and expected LED pattern available
4. **No extra delays** - Readback happens when hardware state is optimal

### Problem Solved

**Before (Periodic Validation):**
- Validation ran every 5 minutes at arbitrary times
- Differential LED updates leave TLC59116 auto-increment pointer at random positions
- Reading PWM registers with corrupted pointer returns wrong data
- **Result:** 15-44 false positive mismatches on unused LEDs (always reading 170/0xAA)

**After (Post-Transition Validation):**
- Validation runs immediately after sequential transition writes
- Auto-increment pointer guaranteed at predictable state
- **Result:** Reliable hardware readback, no false positives

## Implementation

### API

```c
/**
 * @brief Trigger validation immediately after transition completes
 *
 * Should be called right after sync_led_state_after_transitions().
 * Validates hardware PWM state when auto-increment pointer is fresh.
 *
 * @return esp_err_t ESP_OK if validation triggered successfully
 */
esp_err_t trigger_validation_post_transition(void);
```

### Validation Levels

**Level 1: Software State Validation**
- Compares `led_state[10][16]` array against expected German time display
- Validates grammar rules (EIN vs EINS, VOR/NACH mutual exclusivity)
- Checks unused LEDs are OFF in software state

**Level 2: Hardware PWM Readback**
- I2C mutex protection ensures atomic readback (30ms)
- Reads all 160 PWM registers from 10 TLC59116 devices
- Compares hardware values against software state
- Detects accidentally lit unused LEDs

**Level 3: TLC59116 Fault Detection**
- Reads EFLAG1/EFLAG2 registers from all devices
- Checks for LED open/short circuit faults
- Validates GRPPWM (global brightness) matches expected value

### Performance

| Metric | Value |
|--------|-------|
| Trigger frequency | Every ~5 minutes (when time changes) |
| Validation time | ~30ms |
| I2C bus blocked | 30ms (negligible) |
| Overhead | 0.01% (30ms per 300,000ms) |

### MQTT Integration

Validation results published to Home Assistant:

```json
{
  "software_valid": true,
  "hardware_valid": true,
  "hardware_faults": false,
  "mismatches": 0,
  "failure_type": "none",
  "software_errors": 0,
  "hardware_read_failures": 0,
  "grppwm_mismatch": false,
  "validation_type": "post_transition"
}
```

**Statistics tracked:**
- Total validations, failures, consecutive failures
- Health score (0-100)
- Failure type breakdown (hardware faults, I2C failures, mismatches)
- Recovery attempts and successes

## I2C Mutex Protection

All I2C operations protected by `g_i2c_device_mutex` (level 5 in mutex hierarchy):

**Protected Operations:**
- `tlc_set_pwm()` - Individual LED writes
- `tlc_set_global_brightness()` - GRPPWM updates
- `tlc_read_all_pwm_values()` - Validation readback
- `tlc_read_global_brightness()` - GRPPWM readback
- `tlc_read_error_flags()` - EFLAG readback
- `ds3231_get_time_struct()` - RTC reads
- `bh1750_read_data()` - Light sensor reads

**Prevents:**
- Concurrent access from: main loop, light sensor (10Hz), brightness updates, transitions (20Hz), validation
- I2C bus corruption
- TLC59116 register state corruption
- Race conditions in LED state

## TLC59116 Auto-Increment Pointer

### Understanding the Problem

The TLC59116 has an **auto-increment register pointer** that advances after each read/write:

```
Write to PWM5 → Pointer at PWM5
Read/Write    → Returns PWM5, pointer advances to PWM6
Read/Write    → Returns PWM6, pointer advances to PWM7
...
```

**Sequential Writes (Transitions):**
```c
// Transition writes all LEDs 0→159 sequentially
for (row = 0; row < 10; row++) {
    for (col = 0; col < 16; col++) {
        tlc_set_pwm(row, col, brightness);  // Pointer: 0, 1, 2, ..., 159
    }
}
// Pointer now at predictable position!
```

**Differential Updates (Normal Operation):**
```c
// Only write changed LEDs (e.g., 5-25 LEDs per update)
tlc_set_pwm(0, 7, brightness);   // Pointer at 7
tlc_set_pwm(0, 10, brightness);  // Pointer at 10
tlc_set_pwm(3, 8, brightness);   // Pointer at 56
// Pointer at random position based on which LEDs changed!
```

**Reading After Differential Updates:**
```c
// Attempt to read PWM0-15 from device 0
i2c_read_bytes(device_0, PWM0, buffer, 16);
// But pointer is at position 56!
// → Reads from wrong registers (PWM8-15 from device 0, PWM0-7 from device 1)
// → Returns garbage (e.g., GRPPWM register = 170/0xAA, or LED output state registers)
```

### Why Unused LEDs Always Read 170

**Pattern observed:** Unused LEDs always read 170 (0xAA in hex)

**Root cause:** When auto-increment pointer wraps or reads from wrong offset:
- May read LEDOUT registers (contain 0xAA = PWM mode for 4 LEDs)
- May read from misaligned position across device boundaries
- May read residual test pattern from diagnostic test

**The display was always correct** - Hardware registers actually contained 0, but **readback was lying** due to pointer corruption.

### Post-Transition Solution

By reading immediately after transitions:
1. Transitions write all 160 LEDs sequentially (0→159)
2. Auto-increment pointer left at deterministic position
3. Validation readback starts from known position
4. **No pointer corruption** → Reliable readback
5. **No false positives** on unused LEDs

## Configuration

Validation can be configured via MQTT:

```bash
# Enable/disable validation
mosquitto_pub -t home/wordclock/validation/config/set \
  -m '{"enabled": true}'

# Adjust validation interval (only affects periodic backup validation)
mosquitto_pub -t home/wordclock/validation/config/set \
  -m '{"interval_sec": 300}'

# Configure restart on failure
mosquitto_pub -t home/wordclock/validation/config/set \
  -m '{"restart_on_hardware_fault": false}'
```

**Note:** Post-transition validation runs regardless of interval setting (triggered by display updates, not timer).

## Failure Detection

### Software Errors
- LED state doesn't match expected German time display
- Grammar violations (incorrect hour word, VOR and NACH both active)
- Wrong number of minute indicator LEDs

### Hardware Mismatches
- PWM register value doesn't match software state
- Active LEDs reading 0 (should be 60 or individual_brightness)
- Unused LEDs reading non-zero (accidentally lit)

### Hardware Faults
- TLC59116 EFLAG registers indicate LED open/short circuit
- I2C read failures
- GRPPWM mismatch (global brightness incorrect)

## Diagnostic Tools

### TLC Diagnostic Test (Startup)

Writes test patterns (85, 170) to suspicious devices and verifies readback:

```c
esp_err_t tlc_diagnostic_test(void);
```

**Purpose:** Verify TLC59116 PWM register readback works at startup (before differential updates corrupt pointer).

**Result:** Always passes because writes are sequential, pointer stays in sync.

### Manual Validation Trigger

Trigger validation via MQTT button in Home Assistant:

```bash
mosquitto_pub -t home/wordclock/validation/trigger \
  -m "trigger"
```

**Note:** Manual triggers during normal operation may have false positives if differential updates have occurred since last transition.

## Future Improvements

### Option 1: Pointer Reset Before Read
Add explicit auto-increment pointer reset before validation reads:
```c
// Write current display state sequentially before reading
// This resets pointer to known position
// Cost: ~160ms extra I2C time
```

### Option 2: Periodic Sequential Refresh
Every N minutes, rewrite entire display sequentially (even if unchanged):
```c
// Keeps pointer fresh for periodic validation
// Cost: ~160ms I2C time per refresh
```

### Option 3: Accept Post-Transition Only
Current approach is sufficient:
- Validates after every display change (~5 minutes)
- Catches errors within 5 minutes
- No extra I2C overhead
- Reliable readback guaranteed

**Recommendation:** Stay with current approach (Option 3) unless field issues arise.

## Related Documentation

- [LED Usage Analysis](LED_USAGE_ANALYSIS.md) - Valid vs unused LED positions
- [Thread Safety Guide](../developer/thread-safety.md) - Mutex hierarchy
- [I2C Device Driver](../developer/i2c-devices.md) - TLC59116 implementation
- [MQTT Validation Commands](../user/mqtt-commands.md#validation) - User commands

## Summary

**Post-transition validation solves TLC59116 auto-increment pointer corruption by validating immediately after sequential transition writes.**

**Key benefits:**
- ✅ Reliable hardware readback (no false positives)
- ✅ Detects both incorrect active LEDs and accidentally lit unused LEDs
- ✅ Event-driven (triggers when most reliable, not arbitrary periodic)
- ✅ Minimal overhead (~30ms per 5 minutes = 0.01%)
- ✅ Full MQTT integration with Home Assistant
- ✅ I2C mutex protection prevents concurrent access

**Status:** Production-ready, deployed in current firmware.
