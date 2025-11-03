# Persistent Error Logging System - Proposal

**Date:** October 14, 2025
**Status:** Proposal (No Code Changes)
**Rationale:** Errors can occur at any time and may be long gone by the time someone notices. Monitor output and MQTT messages are transient.

---

## Problem Statement

### Current Limitations

**1. Transient Error Reporting:**
- Monitor output only visible when connected via serial/USB
- MQTT messages only available if Home Assistant is recording history
- After system reboot, all error context is lost
- No way to query "what happened last night at 3am?"

**2. Statistics Reset on Reboot:**
- `validation_statistics_t` stored only in RAM (`g_statistics`)
- System restart resets all failure counters
- Cannot track long-term reliability trends
- Loses evidence of intermittent issues

**3. Missing Historical Context:**
- Cannot answer: "How many times did this fail in the last month?"
- Cannot correlate failures with environmental changes
- Cannot detect degradation patterns over time

---

## Proposed Solution: NVS-Backed Error Log

### Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Error Sources                         │
├─────────────────────────────────────────────────────────┤
│  LED Validation │ I2C Errors │ WiFi │ MQTT │ System    │
└────────┬────────┴──────┬──────┴──────┴──────┴──────────┘
         │               │
         ▼               ▼
    ┌────────────────────────────┐
    │   Error Log Manager        │
    │  (New Component)           │
    ├────────────────────────────┤
    │  • Circular buffer in NVS  │
    │  • Thread-safe writes      │
    │  • Automatic pruning       │
    │  • MQTT query interface    │
    └────────────┬───────────────┘
                 │
                 ▼
    ┌────────────────────────────┐
    │      NVS Storage           │
    │  Namespace: "error_log"    │
    ├────────────────────────────┤
    │  • Last 50 errors (ring)   │
    │  • Statistics snapshot     │
    │  • Boot counters           │
    └────────────────────────────┘
                 │
                 ▼
    ┌────────────────────────────┐
    │    MQTT Query Interface    │
    ├────────────────────────────┤
    │  GET: errors/recent        │
    │  GET: errors/count         │
    │  GET: errors/stats         │
    │  CMD: errors/clear         │
    └────────────────────────────┘
```

---

## Design Details

### 1. Error Log Entry Structure

```c
typedef struct {
    time_t timestamp;           // Unix timestamp (from DS3231 RTC)
    uint32_t uptime_sec;        // System uptime when error occurred
    uint8_t error_source;       // LED_VALIDATION, I2C, WIFI, MQTT, SYSTEM
    uint8_t error_severity;     // INFO, WARNING, ERROR, CRITICAL
    uint16_t error_code;        // Component-specific error code
    uint8_t context[32];        // Error-specific context data
    char message[64];           // Human-readable error message
} error_log_entry_t;

// Total size per entry: 8 + 4 + 1 + 1 + 2 + 32 + 64 = 112 bytes
```

**Error Sources:**
```c
#define ERROR_SOURCE_LED_VALIDATION  0x01
#define ERROR_SOURCE_I2C_BUS         0x02
#define ERROR_SOURCE_WIFI            0x03
#define ERROR_SOURCE_MQTT            0x04
#define ERROR_SOURCE_NTP             0x05
#define ERROR_SOURCE_SYSTEM          0x06
#define ERROR_SOURCE_POWER           0x07
#define ERROR_SOURCE_SENSOR          0x08
```

**Error Severity:**
```c
#define ERROR_SEVERITY_INFO      0x00  // Informational (e.g., validation passed)
#define ERROR_SEVERITY_WARNING   0x01  // Warning (e.g., 1-5 LED mismatches)
#define ERROR_SEVERITY_ERROR     0x02  // Error (e.g., I2C bus failure)
#define ERROR_SEVERITY_CRITICAL  0x03  // Critical (e.g., repeated failures)
```

**Context Data Examples:**
```c
// For LED validation errors:
struct {
    uint8_t mismatch_count;
    uint8_t failure_type;      // PARTIAL_MISMATCH, SYSTEMATIC, etc.
    uint8_t affected_rows[10]; // Bitmask of affected rows
    uint8_t affected_cols[16]; // Bitmask of affected columns
    uint8_t reserved[4];
} validation_context;

// For I2C errors:
struct {
    uint8_t device_addr;
    uint8_t register_addr;
    uint16_t error_code;       // ESP-IDF error code
    uint8_t retry_count;
    uint8_t reserved[27];
} i2c_context;

// For WiFi errors:
struct {
    uint8_t disconnect_reason;
    int8_t rssi;
    uint8_t channel;
    uint8_t reconnect_attempts;
    uint8_t reserved[28];
} wifi_context;
```

### 2. NVS Storage Strategy

**Namespace:** `error_log`

**Circular Buffer Implementation:**
```
NVS Keys:
  "log_head"   → uint16_t (index of next write position, 0-49)
  "log_count"  → uint16_t (total number of entries, max 50)
  "log_00"     → error_log_entry_t (112 bytes)
  "log_01"     → error_log_entry_t (112 bytes)
  ...
  "log_49"     → error_log_entry_t (112 bytes)
```

**Storage Requirements:**
- 50 entries × 112 bytes = 5,600 bytes
- Metadata (head, count) = 4 bytes
- Total: ~5.6 KB (well within NVS limits)

**Circular Buffer Behavior:**
```
Initial state: head=0, count=0
After 10 errors: head=10, count=10
After 50 errors: head=50→0 (wrap), count=50
After 51 errors: head=1, count=50 (oldest entry overwritten)
```

### 3. Persistent Statistics

**Namespace:** `error_stats`

```c
typedef struct {
    // Lifetime counters (never reset except by user)
    uint32_t total_boots;
    uint32_t total_validation_failures;
    uint32_t total_i2c_failures;
    uint32_t total_wifi_disconnects;
    uint32_t total_mqtt_disconnects;

    // Last failure timestamps
    time_t last_validation_failure;
    time_t last_i2c_failure;
    time_t last_wifi_disconnect;
    time_t last_mqtt_disconnect;

    // Uptime tracking
    uint32_t total_uptime_sec;         // Cumulative uptime
    time_t first_boot_time;            // First ever boot
    time_t last_boot_time;             // Most recent boot

    // Failure rate tracking (last 7 days)
    uint16_t failures_per_day[7];      // Rolling 7-day window
    uint8_t current_day_index;         // 0-6

    // Health indicators
    uint8_t overall_health_score;      // 0-100
    uint8_t validation_health_score;   // 0-100
    uint8_t i2c_health_score;          // 0-100
    uint8_t network_health_score;      // 0-100

} persistent_statistics_t;

// Total size: ~100 bytes
```

**Update Strategy:**
- **On boot:** Increment `total_boots`, update `last_boot_time`
- **On error:** Increment relevant counter, update timestamp
- **Every hour:** Sync RAM statistics to NVS (wear-leveling)
- **On graceful shutdown:** Final sync (if power-loss warning available)

### 4. MQTT Query Interface

**Topic Structure:**
```
home/Clock_Stani_1/errors/get         → Request error data
home/Clock_Stani_1/errors/response    → Error data response
home/Clock_Stani_1/errors/clear       → Clear error log (admin)
home/Clock_Stani_1/errors/stats       → Get persistent statistics
```

**Query Commands:**
```json
// Request last 10 errors
Publish to: home/Clock_Stani_1/errors/get
Payload: {"command": "recent", "count": 10}

// Response
Publish to: home/Clock_Stani_1/errors/response
Payload: {
  "count": 10,
  "errors": [
    {
      "timestamp": "2025-10-14T03:15:23Z",
      "uptime": 12345,
      "source": "LED_VALIDATION",
      "severity": "WARNING",
      "code": 0x0102,
      "message": "Partial mismatch: 5 LEDs incorrect",
      "context": {
        "mismatch_count": 5,
        "failure_type": "PARTIAL_MISMATCH"
      }
    }
  ]
}
```

```json
// Request error count by source
Publish to: home/Clock_Stani_1/errors/get
Payload: {"command": "count"}

// Response
Publish to: home/Clock_Stani_1/errors/response
Payload: {
  "total_errors": 123,
  "by_source": {
    "LED_VALIDATION": 45,
    "I2C_BUS": 12,
    "WIFI": 34,
    "MQTT": 8,
    "SYSTEM": 24
  },
  "by_severity": {
    "INFO": 50,
    "WARNING": 40,
    "ERROR": 25,
    "CRITICAL": 8
  }
}
```

```json
// Request persistent statistics
Publish to: home/Clock_Stani_1/errors/stats
Payload: {}

// Response
Payload: {
  "lifetime": {
    "total_boots": 156,
    "total_uptime_hours": 3456,
    "first_boot": "2025-01-15T10:00:00Z",
    "last_boot": "2025-10-14T09:00:00Z"
  },
  "failures": {
    "validation": 234,
    "i2c": 45,
    "wifi": 67,
    "mqtt": 12
  },
  "health_scores": {
    "overall": 92,
    "validation": 95,
    "i2c": 98,
    "network": 88
  },
  "last_7_days": {
    "day_0": 5,  // Today
    "day_1": 3,  // Yesterday
    "day_2": 2,
    "day_3": 4,
    "day_4": 1,
    "day_5": 0,
    "day_6": 2   // 6 days ago
  }
}
```

```json
// Clear error log (admin command)
Publish to: home/Clock_Stani_1/errors/clear
Payload: {"confirm": true}

// Response
Payload: {
  "result": "success",
  "cleared_entries": 50,
  "message": "Error log cleared, statistics preserved"
}
```

### 5. Integration Points

**LED Validation Integration:**
```c
// In led_validation.c, when validation fails:
void log_validation_error(const validation_result_enhanced_t *result,
                         failure_type_t failure) {
    error_log_entry_t entry = {
        .timestamp = time(NULL),
        .uptime_sec = esp_timer_get_time() / 1000000,
        .error_source = ERROR_SOURCE_LED_VALIDATION,
        .error_severity = (failure == FAILURE_PARTIAL_MISMATCH) ?
                         ERROR_SEVERITY_WARNING : ERROR_SEVERITY_ERROR,
        .error_code = (uint16_t)failure
    };

    // Pack context
    validation_context *ctx = (validation_context*)entry.context;
    ctx->mismatch_count = result->hardware_mismatch_count;
    ctx->failure_type = failure;
    // ... fill affected rows/cols bitmask

    snprintf(entry.message, sizeof(entry.message),
             "Validation failed: %s (%d mismatches)",
             get_failure_type_name(failure),
             result->hardware_mismatch_count);

    error_log_write(&entry);
}
```

**I2C Error Integration:**
```c
// In i2c_devices.c, when I2C operation fails:
void log_i2c_error(uint8_t device_addr, uint8_t reg_addr, esp_err_t error) {
    error_log_entry_t entry = {
        .timestamp = time(NULL),
        .uptime_sec = esp_timer_get_time() / 1000000,
        .error_source = ERROR_SOURCE_I2C_BUS,
        .error_severity = ERROR_SEVERITY_ERROR,
        .error_code = error
    };

    i2c_context *ctx = (i2c_context*)entry.context;
    ctx->device_addr = device_addr;
    ctx->register_addr = reg_addr;
    ctx->error_code = error;

    snprintf(entry.message, sizeof(entry.message),
             "I2C error: dev=0x%02X reg=0x%02X err=%s",
             device_addr, reg_addr, esp_err_to_name(error));

    error_log_write(&entry);
}
```

**WiFi Disconnect Integration:**
```c
// In wifi_manager.c, when WiFi disconnects:
void log_wifi_disconnect(uint8_t reason, int8_t rssi) {
    error_log_entry_t entry = {
        .timestamp = time(NULL),
        .uptime_sec = esp_timer_get_time() / 1000000,
        .error_source = ERROR_SOURCE_WIFI,
        .error_severity = ERROR_SEVERITY_WARNING,
        .error_code = reason
    };

    wifi_context *ctx = (wifi_context*)entry.context;
    ctx->disconnect_reason = reason;
    ctx->rssi = rssi;

    snprintf(entry.message, sizeof(entry.message),
             "WiFi disconnected: reason=%d RSSI=%ddBm",
             reason, rssi);

    error_log_write(&entry);
}
```

---

## Implementation Phases

### Phase 1: Core Error Log Manager (Priority: HIGH)
**Goal:** Basic NVS-backed circular buffer for error logging

**Tasks:**
1. Create `error_log_manager` component
2. Implement `error_log_write()` function
3. Implement NVS circular buffer (50 entries)
4. Add thread-safe write operations
5. Test with simulated errors

**Estimated Effort:** 2-3 days
**Dependencies:** None

### Phase 2: MQTT Query Interface (Priority: HIGH)
**Goal:** Enable remote error log queries via MQTT

**Tasks:**
1. Add MQTT topics for error queries
2. Implement `errors/get` handler (recent, count, filter)
3. Implement `errors/response` publisher
4. Add `errors/clear` admin command
5. Document MQTT API

**Estimated Effort:** 1-2 days
**Dependencies:** Phase 1

### Phase 3: LED Validation Integration (Priority: HIGH)
**Goal:** Log all validation failures to NVS

**Tasks:**
1. Add `log_validation_error()` calls in led_validation.c
2. Pack validation context data
3. Test with actual validation failures
4. Verify MQTT query returns correct data

**Estimated Effort:** 0.5 days
**Dependencies:** Phase 1, Phase 2

### Phase 4: Persistent Statistics (Priority: MEDIUM)
**Goal:** Track lifetime statistics across reboots

**Tasks:**
1. Define `persistent_statistics_t` structure
2. Implement NVS read/write for statistics
3. Update statistics on boot, hourly, and on errors
4. Add MQTT topic for statistics query
5. Implement health score calculations

**Estimated Effort:** 1-2 days
**Dependencies:** Phase 1

### Phase 5: System-Wide Integration (Priority: MEDIUM)
**Goal:** Log errors from all major subsystems

**Tasks:**
1. Add I2C error logging (i2c_devices.c)
2. Add WiFi error logging (wifi_manager.c)
3. Add MQTT error logging (mqtt_manager.c)
4. Add system error logging (boot failures, panics, etc.)
5. Test each integration point

**Estimated Effort:** 2-3 days
**Dependencies:** Phase 1, Phase 4

### Phase 6: Home Assistant Integration (Priority: LOW)
**Goal:** Display error logs in Home Assistant dashboard

**Tasks:**
1. Create HA sensor for recent error count
2. Create HA sensor for health scores
3. Create HA automation for critical error alerts
4. Create HA service to query/clear error log
5. Document HA integration

**Estimated Effort:** 1 day
**Dependencies:** Phase 2, Phase 4

---

## Storage Capacity Analysis

### NVS Partition Size
**Current:** 24 KB (0x6000 bytes, see partitions.csv)

**Usage Breakdown:**
- WiFi credentials: ~100 bytes
- MQTT config: ~200 bytes
- Brightness config: ~100 bytes
- Validation config: ~50 bytes
- Transition config: ~50 bytes
- **Subtotal (current):** ~500 bytes

**Proposed Addition:**
- Error log (50 entries): 5,600 bytes
- Persistent statistics: 100 bytes
- **Subtotal (new):** 5,700 bytes

**Total Usage:** ~6,200 bytes (26% of 24 KB)
**Remaining:** ~17,800 bytes (74% free)

**Conclusion:** ✅ Plenty of NVS space available

### Write Endurance

**NVS Flash Endurance:** ~100,000 erase cycles per sector

**Worst-Case Write Frequency:**
- Validation runs every 5 minutes = 288 times/day
- If ALL validations failed: 288 error writes/day
- NVS sector size: 4KB (ESP32 default)
- Error log uses: ~6KB (2 sectors)

**Wear Calculation:**
```
Writes per day: 288
Erase cycles: 100,000
Lifetime: 100,000 / 288 = 347 days = 11 months
```

**Mitigation:**
- **Deduplication:** Don't log identical consecutive errors
- **Throttling:** Max 1 error/minute per source (reduces to 1,440/day)
- **With throttling:** Lifetime = 69 years ✅
- **NVS wear leveling:** ESP-IDF automatically distributes writes

**Conclusion:** ✅ No wear concerns with throttling

---

## Home Assistant Dashboard Example

```yaml
# sensors.yaml
sensor:
  - platform: mqtt
    name: "Word Clock Recent Errors"
    state_topic: "home/Clock_Stani_1/errors/count"
    value_template: "{{ value_json.total_errors }}"
    icon: mdi:alert-circle

  - platform: mqtt
    name: "Word Clock Overall Health"
    state_topic: "home/Clock_Stani_1/errors/stats"
    value_template: "{{ value_json.health_scores.overall }}"
    unit_of_measurement: "%"
    icon: mdi:heart-pulse

# automations.yaml
automation:
  - alias: "Word Clock Critical Error Alert"
    trigger:
      - platform: mqtt
        topic: "home/Clock_Stani_1/errors/response"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.errors[0].severity == 'CRITICAL' }}"
    action:
      - service: notify.mobile_app
        data:
          title: "Word Clock Critical Error"
          message: "{{ trigger.payload_json.errors[0].message }}"
          data:
            actions:
              - action: "VIEW_ERRORS"
                title: "View Error Log"

# scripts.yaml
script:
  wordclock_get_errors:
    alias: "Get Word Clock Errors"
    sequence:
      - service: mqtt.publish
        data:
          topic: "home/Clock_Stani_1/errors/get"
          payload: '{"command": "recent", "count": 10}'

  wordclock_clear_errors:
    alias: "Clear Word Clock Error Log"
    sequence:
      - service: mqtt.publish
        data:
          topic: "home/Clock_Stani_1/errors/clear"
          payload: '{"confirm": true}'
```

---

## Alternative Approaches Considered

### Alternative 1: External Logging Server
**Pros:** Unlimited storage, advanced analytics
**Cons:** Requires network connectivity, external dependency, complexity
**Decision:** ❌ Rejected - too complex for embedded device

### Alternative 2: SD Card Logging
**Pros:** Large storage capacity
**Cons:** Additional hardware, SD card failure risk, complexity
**Decision:** ❌ Rejected - no SD card in current design

### Alternative 3: Cloud Logging (AWS, Azure, etc.)
**Pros:** Professional tools, remote access
**Cons:** Cost, privacy concerns, external dependency
**Decision:** ❌ Rejected - overkill for single device

### Alternative 4: MQTT Retained Messages
**Pros:** Simple, no NVS needed
**Cons:** Broker-dependent, message size limits, not persistent across broker restarts
**Decision:** ❌ Rejected - not truly persistent

### Alternative 5: Proposed Solution (NVS + MQTT Query)
**Pros:** Local storage, MQTT accessible, survives reboots, no dependencies
**Cons:** Limited to 50 entries, requires implementation effort
**Decision:** ✅ Selected - best balance of simplicity and functionality

---

## Security Considerations

**1. Admin Commands:**
- `errors/clear` should require authentication token
- Consider adding password protection

**2. Privacy:**
- Error messages may contain IP addresses, SSIDs
- Consider redacting sensitive data in MQTT responses

**3. DoS Protection:**
- Rate limit MQTT queries (max 1 query/second)
- Throttle error writes (max 1 error/minute per source)

**4. NVS Corruption:**
- Implement CRC checks on error log entries
- Add recovery mechanism if NVS becomes corrupted

---

## Success Metrics

**After Implementation:**
- ✅ Can query "what errors occurred in the last 24 hours?"
- ✅ Error log survives system reboots
- ✅ Can identify patterns (e.g., "validation fails every night at 3am")
- ✅ Lifetime statistics show system reliability trends
- ✅ Health scores provide quick system status overview
- ✅ MQTT interface enables remote diagnostics
- ✅ Home Assistant dashboard shows error history

---

## Conclusion

The proposed NVS-backed error logging system provides:
- **Persistence:** Errors survive reboots and power cycles
- **Accessibility:** Query via MQTT from anywhere
- **Context:** Rich error details with timestamps and context data
- **Trends:** Lifetime statistics show reliability over time
- **Proactive:** Health scores enable predictive maintenance
- **Scalable:** Can expand to log more error types

**Recommendation:** Implement in phases, starting with Phase 1 (Core Error Log Manager) and Phase 2 (MQTT Query Interface). This provides immediate value while allowing incremental feature additions.

**Total Implementation Effort:** ~7-10 days spread across 6 phases
**Storage Impact:** ~6KB (26% of available NVS)
**Performance Impact:** Minimal (<1ms per error log write)
**Maintenance:** Low (NVS wear leveling handles flash lifetime)

This system transforms the word clock from a "fire and forget" device into a **self-diagnosing, remotely monitorable IoT system** with full error traceability.
