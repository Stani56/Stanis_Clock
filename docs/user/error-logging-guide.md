# Error Logging System - User Guide

## Overview

The error logging system provides **persistent storage** of system errors in NVS (Non-Volatile Storage), ensuring that error information survives ESP32 reboots and power cycles. This is critical for diagnosing intermittent issues that occur when nobody is watching (e.g., overnight errors).

## Key Features

- **50-entry circular buffer** in NVS flash
- **Survives reboots** - errors persist across power cycles
- **MQTT query interface** - retrieve errors from Home Assistant
- **Automatic error logging** from LED validation system
- **Rich context** - timestamps, uptime, error source, severity, and context data
- **Statistics tracking** - total errors, errors by source, errors by severity

## MQTT Topics

### Query Error Log
**Topic:** `home/Clock_Stani_1/error_log/query`

**Payload (JSON):**
```json
{
  "count": 10
}
```

**Parameters:**
- `count`: Number of recent entries to retrieve (default: 10, max: 50)

**Response Topic:** `home/Clock_Stani_1/error_log/response`

**Response Example:**
```json
{
  "entries": [
    {
      "timestamp": "2025-10-14T17:38:08Z",
      "uptime_sec": 8143,
      "source": "LED_VALIDATION",
      "severity": "WARNING",
      "code": 0,
      "message": "Validation failed: SOFTWARE_ERROR (25 mismatches)",
      "context": {
        "mismatch_count": 25,
        "failure_type": 0,
        "affected_rows": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
      }
    }
  ],
  "count": 1,
  "requested": 10
}
```

### Get Error Log Statistics
**Topic:** `home/Clock_Stani_1/error_log/query` (any payload triggers stats)

**Alternative:** Statistics are automatically published on MQTT connect

**Response Topic:** `home/Clock_Stani_1/error_log/stats`

**Response Example:**
```json
{
  "total_entries_written": 145,
  "current_entry_count": 50,
  "buffer_capacity": 50,
  "head_index": 45,
  "total_errors": 145,
  "errors_by_source": {
    "validation": 120,
    "i2c": 15,
    "wifi": 8,
    "mqtt": 2,
    "ntp": 0,
    "system": 0
  },
  "errors_by_severity": {
    "info": 0,
    "warning": 100,
    "error": 40,
    "critical": 5
  },
  "last_errors": {
    "validation": "2025-10-14T17:38:08Z",
    "i2c": "2025-10-14T15:22:15Z",
    "wifi": "2025-10-13T03:45:22Z"
  }
}
```

### Clear Error Log
**Topic:** `home/Clock_Stani_1/error_log/clear`

**Payloads:**
- `"clear"` - Clear all error log entries (preserves statistics counters)
- `"reset_stats"` - Reset statistics counters (preserves current entries)

**Response:** Updated statistics published to `home/Clock_Stani_1/error_log/stats`

## Home Assistant Integration

### MQTT Explorer View

1. Connect MQTT Explorer to your HiveMQ broker
2. Navigate to `home/Clock_Stani_1/error_log/`
3. Publish to `query` with payload `{"count":20}` to retrieve 20 recent errors
4. View response in `response` topic
5. View statistics in `stats` topic

### Node-RED Example

**Query Last 10 Errors:**
```javascript
// Trigger: Inject node or button
msg.topic = "home/Clock_Stani_1/error_log/query";
msg.payload = {"count": 10};
return msg;
```

**Process Response:**
```javascript
// Input: MQTT subscription to home/Clock_Stani_1/error_log/response
const data = msg.payload;
msg.payload = `Found ${data.count} errors:\n`;

data.entries.forEach((entry, index) => {
    msg.payload += `\n${index + 1}. [${entry.severity}] ${entry.source}\n`;
    msg.payload += `   ${entry.timestamp}: ${entry.message}\n`;
    if (entry.context) {
        msg.payload += `   Mismatches: ${entry.context.mismatch_count}\n`;
    }
});

return msg;
```

### Home Assistant Automation Example

**Notify on Critical Errors:**
```yaml
automation:
  - alias: "Word Clock Critical Error Alert"
    trigger:
      - platform: mqtt
        topic: "home/Clock_Stani_1/error_log/stats"
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.errors_by_severity.critical > 0 }}"
    action:
      - service: notify.mobile_app
        data:
          title: "Word Clock Critical Error"
          message: >
            Critical errors detected: {{ trigger.payload_json.errors_by_severity.critical }}
            Last validation error: {{ trigger.payload_json.last_errors.validation }}
```

## Error Sources

| Source | Description |
|--------|-------------|
| `LED_VALIDATION` | LED validation failures (mismatches, hardware faults) |
| `I2C_BUS` | I2C communication errors |
| `WIFI` | WiFi connection issues |
| `MQTT` | MQTT connection/publishing errors |
| `NTP` | NTP synchronization failures |
| `SYSTEM` | General system errors |
| `POWER` | Power supply issues |
| `SENSOR` | Sensor read failures |
| `DISPLAY` | Display update errors |
| `TRANSITION` | Transition animation errors |
| `BRIGHTNESS` | Brightness control errors |

## Error Severities

| Severity | Description | When Used |
|----------|-------------|-----------|
| `INFO` | Informational | Non-error events for tracking |
| `WARNING` | Warning | Recoverable issues, partial failures |
| `ERROR` | Error | Failures requiring attention |
| `CRITICAL` | Critical | System-level failures, hardware faults |

## LED Validation Error Context

When error source is `LED_VALIDATION`, the context includes:

```json
"context": {
  "mismatch_count": 25,
  "failure_type": 0,
  "affected_rows": [0, 1, 2, 3, 4, 5]
}
```

**Failure Types:**
- `0` = SOFTWARE_ERROR (LED state mismatch)
- `1` = SYSTEMATIC_MISMATCH (multiple rows affected)
- `2` = PARTIAL_MISMATCH (few LEDs wrong)
- `3` = HARDWARE_FAULT (TLC fault detected)
- `4` = I2C_BUS_FAILURE (communication error)
- `5` = GRPPWM_MISMATCH (global brightness wrong)

**Affected Rows:** Array of row indices (0-9) that had mismatches

## Usage Workflow

### 1. Monitor for Errors (Proactive)
- Subscribe to `home/Clock_Stani_1/error_log/stats` in Home Assistant
- Set up automation to alert on critical errors
- Periodically check error counts

### 2. Diagnose Issues (Reactive)
When an issue occurs:
1. Query recent errors: Publish `{"count":20}` to `error_log/query`
2. Review error list in `error_log/response`
3. Check timestamps to correlate with observed behavior
4. Examine context data for root cause

### 3. Clear After Resolution
After fixing an issue:
1. Clear error log: Publish `"clear"` to `error_log/clear`
2. Monitor for new occurrences
3. If issue reappears, errors will be logged again

## Storage Details

- **NVS Namespace:** `error_log`
- **Entry Size:** 112 bytes per entry
- **Total Storage:** ~5.6 KB (50 entries × 112 bytes)
- **Circular Buffer:** Oldest entries overwritten when buffer is full
- **Persistence:** Survives reboots, flash writes, and power cycles

## Example Scenarios

### Scenario 1: Intermittent LED Validation Failures
**Problem:** Display occasionally shows wrong LEDs at 3am

**Solution:**
1. Next morning, query error log: `{"count":50}`
2. Find validation failure at 3:05am with 15 mismatches
3. Context shows rows 2, 5, 7 affected
4. Check those TLC devices for hardware issues
5. Error log preserved the exact failure details

### Scenario 2: WiFi Disconnects Overnight
**Problem:** Clock loses MQTT connection periodically

**Solution:**
1. Check error log statistics
2. See 25 WiFi errors, 12 MQTT errors
3. Timestamps show pattern: every ~2 hours
4. Correlate with router logs
5. Identify router firmware issue

### Scenario 3: System Health Dashboard
**Setup:**
1. Query error log stats every hour via Home Assistant automation
2. Store counts in InfluxDB
3. Create Grafana dashboard showing:
   - Errors per hour by source
   - Errors per hour by severity
   - Time since last critical error
4. Visual health monitoring over time

## Best Practices

1. **Regular Monitoring**
   - Check error statistics weekly
   - Set up alerts for critical errors
   - Query full log after any unusual behavior

2. **Periodic Clearing**
   - Clear old errors after investigation
   - Reset stats after major system changes
   - Keeps data relevant to current configuration

3. **Correlation with Other Logs**
   - Cross-reference with monitor output timestamps
   - Compare with MQTT validation messages
   - Check against Home Assistant logs

4. **Capacity Management**
   - 50 entries fill quickly if errors are frequent
   - Query and archive errors externally if needed
   - Critical errors always preserved (oldest overwritten)

## Troubleshooting

### Error log empty after reboot
**Cause:** First boot after implementation
**Solution:** Errors will be logged as they occur

### Statistics not resetting
**Cause:** Used `"clear"` instead of `"reset_stats"`
**Solution:** Publish `"reset_stats"` to clear counters

### Missing context data
**Cause:** Error source is not LED_VALIDATION
**Solution:** Context is source-specific, not all errors have context

### Old errors showing
**Cause:** Buffer not cleared after fixing issue
**Solution:** Publish `"clear"` to remove old entries

## Related Documentation

- [LED Validation Guide](led-validation-guide.md) - Details on validation errors
- [MQTT Architecture](../implementation/mqtt-system-architecture.md) - MQTT topic structure
- [NVS Error Logging Proposal](../proposals/nvs-error-logging-proposal.md) - Technical design

---

**System Status:** Production-ready, actively logging validation errors
**Last Updated:** 2025-10-14
**Version:** 1.0
