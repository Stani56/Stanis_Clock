# LED Validation - Failure Handling Strategy
## Comprehensive Recovery, Monitoring, and Configurable Actions

## Overview

This document defines the complete failure handling strategy for LED validation, including:
- **Failure classification** (5 severity levels)
- **Recovery actions** for each failure type
- **MQTT monitoring** with detailed failure tracking
- **Home Assistant integration** for alerting and trending
- **Configurable restart behavior** (enable/disable automatic restart per failure type)
- **Failure statistics** and health scoring

---

## Failure Classification

### 1. CRITICAL - Hardware Fault (LED Driver Failure)

**Detection:**
- EFLAG registers show non-zero values (open/short circuit)
- Indicates physical LED or TLC59116 driver failure

**Recovery Strategy:**
```c
1. Log critical error with device index and EFLAG values
2. Attempt TLC59116 device reset
3. Reinitialize device
4. Clear EFLAG registers
5. Re-validate affected device
6. If recovery fails after 3 attempts:
   - Mark device as permanently failed
   - Disable affected row of LEDs
   - Continue operation with remaining 9 devices
```

**MQTT/HA Actions:**
- Publish alert: `validation/alert` with severity CRITICAL
- Update status: `validation/hardware_fault` = true
- Binary sensor triggers HA automation
- Optional: Restart ESP32 if configured

**Restart Behavior:**
- Default: **Disabled** (hardware fault unlikely to be fixed by restart)
- Configurable via MQTT switch

---

### 2. CRITICAL - Complete I2C Bus Failure

**Detection:**
- Cannot read any TLC59116 devices (all 10 fail)
- I2C bus completely unresponsive

**Recovery Strategy:**
```c
1. Log critical I2C bus failure
2. Attempt I2C bus reset (reinitialize I2C driver)
3. Wait 100ms for bus to stabilize
4. Reinitialize all TLC59116 devices
5. Full display refresh
6. Re-validate all devices
7. If recovery fails after 2 attempts:
   - Restart ESP32 (likely severe hardware issue)
```

**MQTT/HA Actions:**
- Publish alert: `validation/alert` with severity CRITICAL
- Update status: `validation/i2c_bus_failure` = true
- Binary sensor triggers HA automation
- Optional: Restart ESP32 if configured

**Restart Behavior:**
- Default: **Enabled** (I2C bus failure often resolved by restart)
- Configurable via MQTT switch

---

### 3. HIGH - Systematic Hardware/Software Mismatch

**Detection:**
- >20% of LEDs (>32 LEDs) have hardware ≠ software
- Indicates widespread I2C write failures

**Recovery Strategy:**
```c
1. Log high-severity mismatch with count and percentage
2. Full display refresh (rewrite all LEDs)
3. Wait 50ms
4. Re-validate hardware state
5. If mismatch persists:
   - Perform I2C bus reset
   - Reinitialize all TLC devices
   - Full display refresh
6. If recovery fails after 3 attempts:
   - Publish persistent failure alert
   - Optional: Restart ESP32 if configured
```

**MQTT/HA Actions:**
- Publish alert: `validation/alert` with severity HIGH
- Update status: `validation/systematic_mismatch` = true
- Sensor shows mismatch percentage
- Optional: Restart ESP32 if configured

**Restart Behavior:**
- Default: **Disabled** (recovery usually successful)
- Configurable via MQTT switch

---

### 4. MEDIUM - Partial Hardware Mismatch

**Detection:**
- 1-20% of LEDs (1-32 LEDs) mismatched
- Indicates intermittent I2C issues

**Recovery Strategy:**
```c
1. Log medium-severity mismatch with affected LEDs
2. Rewrite ONLY mismatched LEDs (selective refresh)
3. Wait 10ms
4. Re-validate affected LEDs
5. If mismatch persists:
   - Full display refresh
   - Re-validate
6. If recovery fails after 5 attempts:
   - Log persistent intermittent failure
   - Continue operation (display mostly correct)
   - Optional: Restart ESP32 if configured
```

**MQTT/HA Actions:**
- Publish alert: `validation/alert` with severity MEDIUM
- Update status: `validation/partial_mismatch` with count
- Sensor shows affected LED count
- Optional: Restart ESP32 if configured

**Restart Behavior:**
- Default: **Disabled** (minor issue, recovery usually successful)
- Configurable via MQTT switch

---

### 5. MEDIUM - GRPPWM Mismatch

**Detection:**
- Global brightness register doesn't match expected value
- One or more TLC devices have wrong GRPPWM

**Recovery Strategy:**
```c
1. Log GRPPWM mismatch with device indices
2. Rewrite GRPPWM to all affected devices
3. Read back to verify
4. If mismatch persists:
   - Reset affected TLC devices
   - Reinitialize with correct GRPPWM
5. If recovery fails after 3 attempts:
   - Log persistent brightness control failure
   - Continue operation (LEDs still functional, just wrong brightness)
```

**MQTT/HA Actions:**
- Publish alert: `validation/alert` with severity MEDIUM
- Update status: `validation/grppwm_mismatch` = true
- Sensor shows expected vs actual GRPPWM
- No restart (brightness issue only)

**Restart Behavior:**
- Default: **Disabled** (does not justify restart)
- Configurable via MQTT switch

---

### 6. LOW - Software State Error Only

**Detection:**
- Software `led_state` array differs from expected
- BUT hardware matches expected (software bug, not hardware issue)

**Recovery Strategy:**
```c
1. Log software inconsistency
2. Update software array from hardware (resync)
3. No hardware writes needed
4. Monitor for recurrence (may indicate software bug)
```

**MQTT/HA Actions:**
- Publish warning: `validation/alert` with severity LOW
- Update status: `validation/software_inconsistency` = true
- Counter tracks occurrences
- No restart (software-only issue)

**Restart Behavior:**
- Default: **Disabled** (software resync sufficient)
- Not configurable (never restart for software-only issue)

---

## Failure Statistics and Health Scoring

### Statistics Tracking

```c
typedef struct {
    // Failure counters (lifetime)
    uint32_t hardware_fault_count;
    uint32_t i2c_bus_failure_count;
    uint32_t systematic_mismatch_count;
    uint32_t partial_mismatch_count;
    uint32_t grppwm_mismatch_count;
    uint32_t software_error_count;

    // Recovery success rates
    uint32_t recovery_attempts;
    uint32_t recovery_successes;
    uint32_t recovery_failures;

    // Recent failure tracking (last 24 hours)
    uint8_t failures_last_24h;
    time_t last_failure_time;

    // Consecutive failures
    uint8_t consecutive_failures;
    uint8_t max_consecutive_failures;

    // Restart counters
    uint32_t automatic_restarts;
    time_t last_restart_time;

} validation_statistics_t;
```

### Health Score Calculation

```c
/**
 * @brief Calculate system health score (0-100)
 *
 * Scoring factors:
 * - Base score: 100
 * - Critical failures: -10 per occurrence (last 24h)
 * - High severity: -5 per occurrence (last 24h)
 * - Medium severity: -2 per occurrence (last 24h)
 * - Low severity: -1 per occurrence (last 24h)
 * - Recovery failure rate: -20 if >10%
 * - Consecutive failures: -5 per consecutive failure
 *
 * @return uint8_t Health score (0-100)
 */
uint8_t calculate_validation_health_score(validation_statistics_t *stats);
```

**Health Score Interpretation:**
- **90-100:** Excellent (green in HA)
- **70-89:** Good (yellow in HA)
- **50-69:** Fair (orange in HA)
- **0-49:** Poor (red in HA)

---

## MQTT Topics and Payloads

### Configuration Topics (Subscribe)

#### 1. Enable/Disable Automatic Restart per Failure Type

```bash
# Hardware Fault Restart
Topic: home/Clock_Stani_1/validation/config/restart/hardware_fault
Payload: "true" | "false"
Default: false

# I2C Bus Failure Restart
Topic: home/Clock_Stani_1/validation/config/restart/i2c_bus_failure
Payload: "true" | "false"
Default: true

# Systematic Mismatch Restart
Topic: home/Clock_Stani_1/validation/config/restart/systematic_mismatch
Payload: "true" | "false"
Default: false

# Partial Mismatch Restart
Topic: home/Clock_Stani_1/validation/config/restart/partial_mismatch
Payload: "true" | "false"
Default: false

# GRPPWM Mismatch Restart
Topic: home/Clock_Stani_1/validation/config/restart/grppwm_mismatch
Payload: "true" | "false"
Default: false
```

#### 2. Maximum Restart Attempts

```bash
# Prevent restart loop
Topic: home/Clock_Stani_1/validation/config/max_restarts
Payload: 0-10
Default: 3

# Reset restart counter
Topic: home/Clock_Stani_1/validation/config/reset_restart_counter
Payload: "reset"
```

#### 3. Validation Configuration

```bash
# Enable/Disable automatic validation
Topic: home/Clock_Stani_1/validation/config/enabled
Payload: "true" | "false"
Default: true

# Validation interval (seconds)
Topic: home/Clock_Stani_1/validation/config/interval
Payload: 60-3600
Default: 300 (5 minutes)
```

### Status Topics (Publish)

#### 1. Real-Time Validation Status

```json
Topic: home/Clock_Stani_1/validation/status
Payload: {
  "timestamp": "2025-01-13T14:35:00Z",
  "validation_running": true,
  "last_validation": "2025-01-13T14:30:00Z",
  "next_validation": "2025-01-13T14:35:00Z",
  "overall_status": "valid" | "warning" | "error" | "critical",
  "health_score": 95,
  "consecutive_failures": 0
}
```

#### 2. Detailed Validation Results

```json
Topic: home/Clock_Stani_1/validation/results
Payload: {
  "timestamp": "2025-01-13T14:35:00Z",
  "validation_time_ms": 108,
  "software_valid": true,
  "hardware_valid": false,
  "hardware_faults": false,
  "details": {
    "software_errors": 0,
    "hardware_mismatches": 3,
    "hardware_mismatch_percent": 1.875,
    "hardware_read_failures": 0,
    "grppwm_mismatch": false,
    "devices_with_faults": 0,
    "mismatched_leds": [
      {"row": 5, "col": 3, "expected": 60, "software": 60, "hardware": 0},
      {"row": 5, "col": 4, "expected": 60, "software": 60, "hardware": 0},
      {"row": 5, "col": 5, "expected": 60, "software": 60, "hardware": 0}
    ]
  }
}
```

#### 3. Failure Alerts

```json
Topic: home/Clock_Stani_1/validation/alert
Payload: {
  "timestamp": "2025-01-13T14:35:00Z",
  "alert_id": "unique-id-12345",
  "severity": "CRITICAL" | "HIGH" | "MEDIUM" | "LOW",
  "failure_type": "hardware_fault" | "i2c_bus_failure" | "systematic_mismatch" | "partial_mismatch" | "grppwm_mismatch" | "software_error",
  "description": "Hardware fault detected on TLC device 5 (EFLAG: 0x08)",
  "affected_devices": [5],
  "affected_leds": [
    {"row": 5, "col": 8}
  ],
  "recovery_attempted": true,
  "recovery_successful": false,
  "restart_scheduled": false,
  "restart_countdown_sec": 0
}
```

#### 4. Statistics and Health

```json
Topic: home/Clock_Stani_1/validation/statistics
Payload: {
  "timestamp": "2025-01-13T14:35:00Z",
  "health_score": 92,
  "uptime_hours": 168,
  "total_validations": 2016,
  "validations_failed": 12,
  "failure_rate_percent": 0.595,
  "lifetime_failures": {
    "hardware_fault": 1,
    "i2c_bus_failure": 0,
    "systematic_mismatch": 2,
    "partial_mismatch": 8,
    "grppwm_mismatch": 1,
    "software_error": 0
  },
  "failures_last_24h": 2,
  "recovery_stats": {
    "attempts": 12,
    "successes": 11,
    "failures": 1,
    "success_rate_percent": 91.67
  },
  "restart_stats": {
    "automatic_restarts": 0,
    "last_restart": null,
    "restart_counter": 0,
    "max_restarts": 3
  }
}
```

#### 5. Recovery Actions

```json
Topic: home/Clock_Stani_1/validation/recovery
Payload: {
  "timestamp": "2025-01-13T14:35:05Z",
  "recovery_id": "unique-id-12345",
  "failure_type": "partial_mismatch",
  "actions_taken": [
    "Rewriting 3 mismatched LEDs",
    "Waiting 10ms",
    "Re-validating affected LEDs"
  ],
  "recovery_duration_ms": 15,
  "recovery_successful": true,
  "verification": {
    "before": {"mismatches": 3},
    "after": {"mismatches": 0}
  }
}
```

---

## Home Assistant Integration

### 1. MQTT Discovery for Validation Entities

```yaml
# Main validation status sensor
sensor:
  - platform: mqtt
    name: "Word Clock Validation Status"
    unique_id: "wordclock_validation_status"
    state_topic: "home/Clock_Stani_1/validation/status"
    value_template: "{{ value_json.overall_status | title }}"
    icon: >
      {% set status = value_json.overall_status %}
      {% if status == 'valid' %}
        mdi:check-circle
      {% elif status == 'warning' %}
        mdi:alert
      {% elif status == 'error' %}
        mdi:alert-circle
      {% else %}
        mdi:close-circle
      {% endif %}
    json_attributes_topic: "home/Clock_Stani_1/validation/status"

# Health score sensor
  - platform: mqtt
    name: "Word Clock Health Score"
    unique_id: "wordclock_health_score"
    state_topic: "home/Clock_Stani_1/validation/statistics"
    value_template: "{{ value_json.health_score }}"
    unit_of_measurement: "%"
    icon: "mdi:heart-pulse"
    json_attributes_topic: "home/Clock_Stani_1/validation/statistics"

# Failure rate sensor
  - platform: mqtt
    name: "Word Clock Failure Rate"
    unique_id: "wordclock_failure_rate"
    state_topic: "home/Clock_Stani_1/validation/statistics"
    value_template: "{{ value_json.failure_rate_percent }}"
    unit_of_measurement: "%"
    icon: "mdi:chart-line"

# Binary sensors for alerts
binary_sensor:
  - platform: mqtt
    name: "Word Clock Validation Alert"
    unique_id: "wordclock_validation_alert"
    state_topic: "home/Clock_Stani_1/validation/status"
    value_template: "{{ value_json.overall_status in ['error', 'critical'] }}"
    payload_on: true
    payload_off: false
    device_class: problem

# Configuration switches
switch:
  - platform: mqtt
    name: "Word Clock Auto Restart - Hardware Fault"
    unique_id: "wordclock_restart_hardware_fault"
    command_topic: "home/Clock_Stani_1/validation/config/restart/hardware_fault"
    state_topic: "home/Clock_Stani_1/validation/config/restart/hardware_fault/state"
    payload_on: "true"
    payload_off: "false"
    icon: "mdi:restart"

  - platform: mqtt
    name: "Word Clock Auto Restart - I2C Bus Failure"
    unique_id: "wordclock_restart_i2c_failure"
    command_topic: "home/Clock_Stani_1/validation/config/restart/i2c_bus_failure"
    state_topic: "home/Clock_Stani_1/validation/config/restart/i2c_bus_failure/state"
    payload_on: "true"
    payload_off: "false"
    icon: "mdi:restart"

  - platform: mqtt
    name: "Word Clock Auto Restart - Systematic Mismatch"
    unique_id: "wordclock_restart_systematic"
    command_topic: "home/Clock_Stani_1/validation/config/restart/systematic_mismatch"
    state_topic: "home/Clock_Stani_1/validation/config/restart/systematic_mismatch/state"
    payload_on: "true"
    payload_off: "false"
    icon: "mdi:restart"

  - platform: mqtt
    name: "Word Clock Auto Restart - Partial Mismatch"
    unique_id: "wordclock_restart_partial"
    command_topic: "home/Clock_Stani_1/validation/config/restart/partial_mismatch"
    state_topic: "home/Clock_Stani_1/validation/config/restart/partial_mismatch/state"
    payload_on: "true"
    payload_off: "false"
    icon: "mdi:restart"

  - platform: mqtt
    name: "Word Clock Validation Enabled"
    unique_id: "wordclock_validation_enabled"
    command_topic: "home/Clock_Stani_1/validation/config/enabled"
    state_topic: "home/Clock_Stani_1/validation/config/enabled/state"
    payload_on: "true"
    payload_off: "false"
    icon: "mdi:check-circle-outline"

# Number entity for validation interval
number:
  - platform: mqtt
    name: "Word Clock Validation Interval"
    unique_id: "wordclock_validation_interval"
    command_topic: "home/Clock_Stani_1/validation/config/interval"
    state_topic: "home/Clock_Stani_1/validation/config/interval/state"
    min: 60
    max: 3600
    step: 60
    unit_of_measurement: "s"
    icon: "mdi:timer"

  - platform: mqtt
    name: "Word Clock Max Auto Restarts"
    unique_id: "wordclock_max_restarts"
    command_topic: "home/Clock_Stani_1/validation/config/max_restarts"
    state_topic: "home/Clock_Stani_1/validation/config/max_restarts/state"
    min: 0
    max: 10
    step: 1
    icon: "mdi:restart-alert"
```

### 2. Home Assistant Automations

#### Alert on Critical Failure

```yaml
automation:
  - alias: "Word Clock Critical Validation Failure Alert"
    trigger:
      - platform: mqtt
        topic: "home/Clock_Stani_1/validation/alert"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.severity == 'CRITICAL' }}"
    action:
      - service: notify.mobile_app
        data:
          title: "Word Clock Critical Failure"
          message: >
            {{ trigger.payload_json.description }}
            Recovery: {{ 'Success' if trigger.payload_json.recovery_successful else 'Failed' }}
            Restart scheduled: {{ trigger.payload_json.restart_scheduled }}
          data:
            priority: high
            ttl: 0
```

#### Health Score Monitoring

```yaml
automation:
  - alias: "Word Clock Health Score Low"
    trigger:
      - platform: numeric_state
        entity_id: sensor.word_clock_health_score
        below: 70
    action:
      - service: notify.persistent_notification
        data:
          title: "Word Clock Health Degraded"
          message: >
            Health score: {{ states('sensor.word_clock_health_score') }}%
            Check validation statistics for details.
```

#### Prevent Restart Loop

```yaml
automation:
  - alias: "Word Clock Restart Loop Protection"
    trigger:
      - platform: mqtt
        topic: "home/Clock_Stani_1/validation/statistics"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.restart_stats.restart_counter >= trigger.payload_json.restart_stats.max_restarts }}"
    action:
      # Disable all auto-restart switches
      - service: switch.turn_off
        target:
          entity_id:
            - switch.word_clock_auto_restart_hardware_fault
            - switch.word_clock_auto_restart_i2c_bus_failure
            - switch.word_clock_auto_restart_systematic_mismatch
            - switch.word_clock_auto_restart_partial_mismatch
      - service: notify.mobile_app
        data:
          title: "Word Clock Auto-Restart Disabled"
          message: "Maximum restart attempts reached. Manual intervention required."
```

---

## Firmware Implementation

### Configuration Storage (NVS)

```c
typedef struct {
    bool restart_on_hardware_fault;
    bool restart_on_i2c_bus_failure;
    bool restart_on_systematic_mismatch;
    bool restart_on_partial_mismatch;
    bool restart_on_grppwm_mismatch;
    uint8_t max_restarts_per_session;
    uint16_t validation_interval_sec;
    bool validation_enabled;
} validation_config_t;

// Save to NVS
esp_err_t save_validation_config(validation_config_t *config);

// Load from NVS
esp_err_t load_validation_config(validation_config_t *config);
```

### Main Validation Task with Failure Handling

```c
void periodic_validation_task(void *pvParameters) {
    validation_config_t config;
    validation_statistics_t stats = {0};
    uint8_t restart_counter = 0;

    load_validation_config(&config);

    while(1) {
        if (!config.validation_enabled) {
            vTaskDelay(pdMS_TO_TICKS(60000));  // Check every minute
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(config.validation_interval_sec * 1000));

        // Run validation
        validation_result_enhanced_t result =
            validate_display_with_hardware(hour, minutes);

        // Publish results to MQTT
        publish_validation_results(&result);

        if (!result.is_valid) {
            // Classify failure
            failure_type_t failure = classify_failure(&result);

            // Log failure
            log_validation_failure(&result, failure);

            // Update statistics
            update_statistics(&stats, failure);

            // Publish alert
            publish_validation_alert(&result, failure);

            // Attempt recovery
            bool recovered = attempt_recovery(&result, failure);

            // Publish recovery result
            publish_recovery_result(&result, failure, recovered);

            if (!recovered) {
                stats.consecutive_failures++;

                // Check if restart is configured for this failure type
                bool should_restart = should_restart_on_failure(&config, failure);

                if (should_restart && restart_counter < config.max_restarts_per_session) {
                    ESP_LOGW(TAG, "Restarting ESP32 due to %s (attempt %d/%d)",
                             failure_type_name(failure),
                             restart_counter + 1,
                             config.max_restarts_per_session);

                    restart_counter++;
                    stats.automatic_restarts++;
                    save_restart_counter(restart_counter);

                    // Publish restart notification
                    publish_restart_notification(failure, restart_counter);

                    vTaskDelay(pdMS_TO_TICKS(5000));  // 5 sec warning
                    esp_restart();
                } else {
                    if (restart_counter >= config.max_restarts_per_session) {
                        ESP_LOGE(TAG, "Maximum restart attempts reached. Manual intervention required.");
                        publish_max_restarts_alert();
                    }
                }
            } else {
                // Recovery successful, reset consecutive failures
                stats.consecutive_failures = 0;
            }
        } else {
            // Validation passed
            stats.consecutive_failures = 0;
        }

        // Calculate and publish health score
        uint8_t health_score = calculate_validation_health_score(&stats);
        publish_validation_statistics(&stats, health_score);
    }
}
```

---

## Summary

### Key Features

✅ **5 Failure Severity Levels** - CRITICAL, HIGH, MEDIUM, LOW with specific recovery actions
✅ **Configurable Restart Behavior** - Enable/disable auto-restart per failure type via MQTT/HA
✅ **Restart Loop Protection** - Configurable maximum restart attempts (default: 3)
✅ **Comprehensive MQTT Monitoring** - Real-time status, alerts, statistics, recovery actions
✅ **Home Assistant Integration** - Sensors, switches, automations for complete monitoring
✅ **Health Score System** - 0-100 score based on failure history and recovery success rate
✅ **Detailed Failure Statistics** - Lifetime counters, 24-hour tracking, recovery rates
✅ **Recovery Verification** - Always validate recovery success, publish results
✅ **Flexible Configuration** - Validation interval, enable/disable, restart thresholds

### Default Restart Configuration

| Failure Type | Auto Restart | Rationale |
|--------------|--------------|-----------|
| Hardware Fault | ❌ Disabled | Hardware issue unlikely fixed by restart |
| I2C Bus Failure | ✅ Enabled | Often resolved by restart |
| Systematic Mismatch | ❌ Disabled | Recovery usually successful |
| Partial Mismatch | ❌ Disabled | Minor issue, recovery usually successful |
| GRPPWM Mismatch | ❌ Disabled | Brightness only, does not justify restart |
| Software Error | ❌ Never | Software resync sufficient |

### Monitoring Dashboard Recommendation

**Home Assistant Lovelace Card:**
```yaml
type: vertical-stack
cards:
  - type: glance
    title: Word Clock Validation
    entities:
      - entity: sensor.word_clock_validation_status
      - entity: sensor.word_clock_health_score
      - entity: sensor.word_clock_failure_rate
      - entity: binary_sensor.word_clock_validation_alert

  - type: entities
    title: Auto-Restart Configuration
    entities:
      - switch.word_clock_auto_restart_hardware_fault
      - switch.word_clock_auto_restart_i2c_bus_failure
      - switch.word_clock_auto_restart_systematic_mismatch
      - switch.word_clock_auto_restart_partial_mismatch
      - number.word_clock_max_auto_restarts

  - type: entities
    title: Validation Configuration
    entities:
      - switch.word_clock_validation_enabled
      - number.word_clock_validation_interval

  - type: history-graph
    title: Health Score Trend (24h)
    hours_to_show: 24
    entities:
      - sensor.word_clock_health_score
```

---

## Implementation Effort

**Total Estimated Effort:** +3-5 days beyond hardware readback implementation

### Phase 1: Failure Classification and Recovery (2 days)
- Implement failure classification function
- Implement recovery strategies for each failure type
- Recovery verification logic

### Phase 2: Configuration and Statistics (1-2 days)
- NVS storage for restart configuration
- Statistics tracking structure
- Health score calculation

### Phase 3: MQTT Integration (1-2 days)
- Publish all status, alert, statistics topics
- Subscribe to configuration topics
- State topic publishing for switches/numbers

### Phase 4: Home Assistant Integration (1 day)
- MQTT Discovery for all entities
- Example automations
- Documentation and testing
