# LED Validation System - User Guide

**Last Updated:** October 14, 2025
**Status:** Production-ready with manual recovery workflow

---

## Overview

The LED Validation System provides **real-time hardware verification** of the word clock display. It validates that every LED is displaying the correct brightness by comparing:

1. **Software State** - What the code thinks should be displayed
2. **Hardware State** - What the TLC59116 LED controllers are actually outputting
3. **Hardware Faults** - Physical LED failures detected by the controllers

---

## How It Works

### Validation Timing

**Post-Transition Validation:**
- Runs automatically **after every time display change** (~5 minutes)
- Validation time: ~130ms
- Publishes results to MQTT/Home Assistant

**Why After Transitions?**
- Display is stable (no more changes for ~5 minutes)
- TLC59116 auto-increment pointer in known state
- Minimal performance impact

### Three-Level Validation

#### Level 1: Software State Validation
Verifies internal consistency between:
- Expected LED state (calculated from current time)
- Software tracking array (led_state[][])

**Pass Criteria:** 0 errors
**Failure Indicates:** Software bug or memory corruption

#### Level 2: Hardware PWM Validation
Reads actual PWM values from TLC59116 chips via I2C:
- Byte-by-byte reads with no auto-increment (avoids pointer corruption)
- Compares hardware PWM vs software state
- Detects incorrect LED brightness or accidentally lit LEDs

**Pass Criteria:** 0 mismatches
**Failure Indicates:** I2C glitch, EMI, power issue, or hardware fault

#### Level 3: Hardware Fault Detection
Checks TLC59116 EFLAG registers:
- LED open circuit detection
- LED short circuit detection
- Global brightness (GRPPWM) verification

**Pass Criteria:** No hardware faults, GRPPWM correct
**Failure Indicates:** Physical LED failure or power supply issue

---

## MQTT Integration

### Published Topics

All topics under: `home/Clock_Stani_1/validation/`

#### 1. **status**
**Payload:**
```json
{
  "result": "passed",
  "timestamp": "2025-10-14T11:15:00Z",
  "execution_time_ms": 131
}
```

**Values:**
- `result`: "passed" or "failed"
- `execution_time_ms`: How long validation took

#### 2. **last_result**
**Payload:**
```json
{
  "software_valid": true,
  "hardware_valid": true,
  "hardware_faults": false,
  "mismatches": 0,
  "failure_type": "NONE",
  "recovery": "failed",
  "software_errors": 0,
  "hardware_read_failures": 0,
  "grppwm_mismatch": false
}
```

**Failure Types:**
- `NONE` - All validation passed
- `SOFTWARE_ERROR` - Internal state mismatch
- `PARTIAL_MISMATCH` - Some LEDs incorrect (1-50% of display)
- `SYSTEMATIC_MISMATCH` - Many LEDs incorrect (>50% of display)
- `HARDWARE_FAULT` - TLC59116 detected physical fault
- `GRPPWM_MISMATCH` - Global brightness incorrect
- `I2C_BUS_FAILURE` - Cannot communicate with LED controllers

#### 3. **mismatches**
**Only published when mismatches detected**

**Payload:**
```json
{
  "count": 5,
  "leds": [
    {"r": 0, "c": 2, "exp": 0, "sw": 0, "hw": 60, "diff": 60},
    {"r": 0, "c": 6, "exp": 0, "sw": 0, "hw": 60, "diff": 60},
    {"r": 0, "c": 7, "exp": 0, "sw": 0, "hw": 60, "diff": 60}
  ]
}
```

**Fields:**
- `r`: Row (0-9)
- `c`: Column (0-15)
- `exp`: Expected brightness (0-255)
- `sw`: Software state brightness (0-255)
- `hw`: Hardware readback brightness (0-255)
- `diff`: Absolute difference |hw - sw|

**Maximum:** 50 LEDs per report (prevents message overflow)

#### 4. **statistics**
**Payload:**
```json
{
  "total_validations": 100,
  "failed": 2,
  "health_score": 95,
  "consecutive_failures": 0,
  "recovery_attempts": 0,
  "recovery_successes": 0,
  "hardware_faults": 0,
  "i2c_failures": 0,
  "systematic_mismatches": 0,
  "partial_mismatches": 2,
  "grppwm_mismatches": 0,
  "software_errors": 0
}
```

**Health Score:**
- 100 = Perfect (no failures)
- 90-99 = Excellent (rare failures)
- 80-89 = Good (occasional issues)
- 70-79 = Fair (frequent failures)
- <70 = Poor (hardware/software issues)

---

## Manual Recovery Workflow

### Current Configuration

**Auto-Recovery:** ❌ DISABLED (except I2C bus failures)
**Recovery Mode:** Manual approval required
**Rationale:** User prefers to review and approve corrections

### When Validation Fails

**1. Check MQTT Messages in Home Assistant**

Navigate to: Developer Tools → States → `sensor.clock_stani_1_validation_*`

**2. Review Mismatch Details**

```json
{
  "count": 3,
  "leds": [
    {"r": 0, "c": 2, "exp": 0, "sw": 0, "hw": 60, "diff": 60}
  ]
}
```

**Questions to Ask:**
- How many LEDs are affected? (1-5 = minor, >20 = serious)
- Are they clustered (same row/column)? → Wiring issue
- Are they random? → EMI or transient glitch
- Is the display visually correct despite mismatch? → False positive (shouldn't happen with fixed validation)

**3. Decide on Action**

| Scenario | Recommended Action |
|----------|-------------------|
| **1-5 LEDs, random positions** | Transient glitch - monitor, no action needed |
| **6-20 LEDs, clustered** | Check wiring/connections on that row/column |
| **>20 LEDs, systematic pattern** | Power supply issue or TLC59116 failure |
| **All LEDs reading 170 (0xAA)** | Auto-increment pointer bug (should be fixed!) |
| **Display looks correct but validation fails** | Report as bug (shouldn't happen) |
| **Display looks incorrect** | Apply recovery |

**4. Manual Recovery Options**

#### Option A: Restart System (Safest)
```
Home Assistant → Button → "Restart"
OR
MQTT: home/Clock_Stani_1/command → "restart"
```

**Effect:** Full system reboot, reinitializes all hardware
**Downtime:** ~10 seconds
**Risk:** None

#### Option B: Test Transitions (Gentle Refresh)
```
Home Assistant → Button → "Test Transitions"
```

**Effect:** Executes full transition sequence, rewrites all LEDs
**Downtime:** 2 seconds (animation)
**Risk:** Low - normal display operation

#### Option C: SSH/Serial Console Commands (Advanced)

**Rewrite specific LED:**
```bash
# Not currently exposed via MQTT - requires firmware update
# Future: home/Clock_Stani_1/led/set → {"row": 0, "col": 2, "pwm": 0}
```

**Full display refresh:**
```bash
# Trigger time display update
# Currently automatic every 5 seconds
```

**5. Verify Recovery**

Wait for next validation cycle (~5 minutes) or trigger transition:
- Check `validation/status` → should be "passed"
- Check `validation/statistics` → `consecutive_failures` should reset to 0

---

## Home Assistant Automation Examples

### Alert on Validation Failure

```yaml
automation:
  - alias: "Word Clock Validation Failed"
    trigger:
      - platform: state
        entity_id: sensor.clock_stani_1_validation_status
        to: "failed"
    condition:
      - condition: numeric_state
        entity_id: sensor.clock_stani_1_validation_consecutive_failures
        above: 2
    action:
      - service: notify.mobile_app
        data:
          title: "Word Clock Issue"
          message: "Validation failed {{ states('sensor.clock_stani_1_validation_consecutive_failures') }} times"
          data:
            actions:
              - action: "RESTART_CLOCK"
                title: "Restart Clock"
```

### Auto-Restart After Multiple Failures

```yaml
automation:
  - alias: "Word Clock Auto-Restart on Persistent Failure"
    trigger:
      - platform: numeric_state
        entity_id: sensor.clock_stani_1_validation_consecutive_failures
        above: 5
    action:
      - service: button.press
        target:
          entity_id: button.clock_stani_1_restart
      - service: notify.mobile_app
        data:
          message: "Word clock auto-restarted after 5 validation failures"
```

### Health Score Monitoring

```yaml
automation:
  - alias: "Word Clock Health Degrading"
    trigger:
      - platform: numeric_state
        entity_id: sensor.clock_stani_1_validation_health_score
        below: 80
    action:
      - service: persistent_notification.create
        data:
          title: "Word Clock Health Alert"
          message: "Health score dropped to {{ states('sensor.clock_stani_1_validation_health_score') }}"
```

---

## Troubleshooting

### Validation Always Passes (No Failures Ever)

**Possible Causes:**
- System is working perfectly! ✅
- Validation disabled in configuration
- MQTT not publishing results

**Check:** Look for log messages `=== VALIDATION PASSED ===` or `=== VALIDATION FAILED ===`

### Frequent Validation Failures (>10% failure rate)

**Possible Causes:**
- **Power supply instability** → Check voltage under load
- **EMI/RFI interference** → Check proximity to RF sources
- **Loose I2C connections** → Inspect SDA/SCL wiring
- **TLC59116 overheating** → Check ambient temperature
- **Software bug** → Report with logs

**Diagnostic Steps:**
1. Check `validation/statistics` → Look for patterns (hardware_faults, i2c_failures, etc.)
2. Review `validation/mismatches` → Clustered or random?
3. Monitor power supply voltage during display transitions
4. Check I2C signal quality with oscilloscope/logic analyzer

### Validation Fails But Display Looks Correct

**This should NOT happen with the fixed validation system (Oct 2025).**

**If it does:**
1. **Report as bug** - capture logs showing mismatch details
2. Check if you're running latest firmware (commit fe53cdd or later)
3. Verify TLC59116 auto-increment pointer fix is applied

### I2C Bus Failure Recovery Loops

**Symptom:** Repeated I2C reinitializations

**Possible Causes:**
- Physical I2C bus issue (damaged wire, bad connection)
- I2C bus conflict (multiple masters?)
- TLC59116 chip failure

**Recovery:**
1. Power cycle the entire system
2. Inspect I2C wiring (SDA = GPIO 25, SCL = GPIO 26)
3. Check pull-up resistors (should be 4.7kΩ)
4. Replace suspected TLC59116 chip

---

## Future Enhancements (Roadmap)

### Planned Features

1. **Configurable Auto-Recovery** (via MQTT/HA)
   - User-controlled thresholds for automatic correction
   - Conservative defaults (only 1-5 LED mismatches)
   - Rate limiting (max 3 auto-recoveries per hour)

2. **Individual LED Control via MQTT**
   - `home/Clock_Stani_1/led/set` topic
   - Manual LED correction without restart

3. **Validation History**
   - Store last 24 hours of validation results
   - Trend analysis in Home Assistant
   - Predictive failure detection

4. **Advanced Diagnostics**
   - I2C bus quality metrics (error rate, timing)
   - Per-TLC59116 health scores
   - LED lifetime estimation

---

## Technical Details

### TLC59116 Auto-Increment Pointer Fix

**Problem Solved (Oct 2025):**
- Old validation read LEDOUT registers (0xAA = 170) instead of PWM registers
- Caused false positive failures
- Root cause: TLC59116 auto-increment pointer corruption from differential writes

**Solution:**
- Byte-by-byte PWM reads with explicit no-auto-increment addressing
- Register address masking with 0x1F (bits 7:6 = 00)
- 100% accurate readback

**See:** [docs/implementation/tlc59116-validation-fix.md](../implementation/tlc59116-validation-fix.md)

### Performance Impact

- **Validation time:** 130ms per validation
- **I2C transactions:** 160 per validation (16 × 10 devices)
- **Frequency:** Every ~5 minutes
- **CPU impact:** Negligible (<0.1%)
- **Memory:** 512 bytes per validation result

---

## Related Documentation

- [TLC59116 Validation Fix](../implementation/tlc59116-validation-fix.md) - Technical deep-dive
- [MQTT User Guide](../../Mqtt_User_Guide.md) - Complete MQTT reference
- [CLAUDE.md](../../CLAUDE.md) - Developer reference

---

## Support

For issues or questions:
- **GitHub Issues:** https://github.com/stani56/Stanis_Clock/issues
- **Documentation:** Check docs/ directory
- **Logs:** Use `idf.py monitor` or MQTT sensor entities

**When Reporting Issues:**
1. Include `validation/last_result` payload
2. Include `validation/mismatches` payload (if available)
3. Include `validation/statistics` payload
4. Describe visual display state (correct/incorrect)
5. Include recent system logs
