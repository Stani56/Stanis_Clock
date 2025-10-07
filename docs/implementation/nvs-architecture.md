# ESP32 WordClock - NVS (Non-Volatile Storage) Architecture

## Overview
This document describes the complete NVS architecture for the ESP32 German Word Clock project, including current implementation status and missing configurations that should be persisted.

## Current NVS Implementation Status

### ✅ **Fully Implemented Namespaces**

#### 1. **WiFi Configuration** - `wifi_config`
**Component:** `nvs_manager`  
**Status:** ✅ Complete (save/load/clear)

| Key | Type | Description | Functions |
|-----|------|-------------|-----------|
| `ssid` | string | WiFi network name | `nvs_manager_save_wifi_credentials()` |
| `password` | string | WiFi network password | `nvs_manager_load_wifi_credentials()` |
| | | | `nvs_manager_clear_wifi_credentials()` |

**Usage:**
```c
nvs_manager_save_wifi_credentials("MyNetwork", "MyPassword");
nvs_manager_load_wifi_credentials(ssid_buf, pass_buf, 32, 64);
nvs_manager_clear_wifi_credentials();  // Reset button function
```

#### 2. **MQTT Configuration** - `mqtt_config`
**Component:** `mqtt_manager`  
**Status:** ✅ Complete (save/load)

| Key | Type | Description | Functions |
|-----|------|-------------|-----------|
| `broker_uri` | string | MQTT broker URL (HiveMQ Cloud) | `mqtt_load_config()` |
| `username` | string | MQTT authentication username | `mqtt_save_config()` |
| `password` | string | MQTT authentication password | |
| `client_id` | string | Unique MQTT client identifier | |
| `use_ssl` | uint8 | TLS/SSL encryption flag | |
| `port` | uint16 | MQTT broker port (8883) | |

**Usage:**
```c
mqtt_config_t config;
mqtt_load_config(&config);
mqtt_save_config(&config);
mqtt_set_default_config(&config);  // Fallback to HiveMQ defaults
```

#### 3. **Brightness Configuration** - `brightness`
**Component:** `brightness_config`  
**Status:** ✅ Complete (save/load/reset)

| Key | Type | Description | Functions |
|-----|------|-------------|-----------|
| `version` | uint32 | Configuration version (migration support) | `brightness_config_save_to_nvs()` |
| `light_sensor` | blob | Light sensor zone configuration | `brightness_config_load_from_nvs()` |
| `potentiometer` | blob | Potentiometer response configuration | `brightness_config_reset_to_defaults()` |

**Light Sensor Blob Structure:**
- `very_dark` - Night time (1-10 lux → 20-40 brightness)
- `dim` - Evening (10-50 lux → 40-80 brightness)  
- `normal` - Indoor (50-200 lux → 80-150 brightness)
- `bright` - Bright indoor (200-500 lux → 150-220 brightness)
- `very_bright` - Daylight (500+ lux → 220-255 brightness)

**Potentiometer Blob Structure:**
- `brightness_min` - Minimum brightness value
- `brightness_max` - Maximum brightness value
- `curve_type` - Response curve (linear/logarithmic/exponential)

**Usage:**
```c
brightness_config_init();  // Auto-loads from NVS
brightness_config_set_light_range("normal", &range_config);
brightness_config_set_potentiometer(&pot_config);
brightness_config_save_to_nvs();  // Manual save after changes
```

**🎯 MQTT-Triggered NVS Updates:**
The brightness configuration has sophisticated NVS update triggers via MQTT commands from Home Assistant:

```c
// MQTT Command Flow → NVS Persistence (mqtt_manager.c)
1. HA publishes to: "home/esp32_core/brightness/config/set"
2. mqtt_handle_brightness_config_set() processes JSON
3. config_changed flag set to true when:
   - Zone brightness updated (line 741)
   - Thresholds rebuild zones (line 777)  
   - Potentiometer config updated (line 808)
4. If config_changed == true:
   - brightness_config_save_to_nvs() called immediately
   - Flash write occurs synchronously
   - Status published back to HA
```

**NVS Write Triggers (Protected with Debouncing):**
- **Single adjustments** (>2s gap) = immediate NVS write for responsiveness
- **Rapid slider movements** (<2s gap) = debounced write after 2s delay
- **Flash protection** - prevents excessive writes during continuous slider operation
- **Success-based** - only successful config updates trigger saves
- **UI responsiveness maintained** - status published immediately regardless of write timing

#### 4. **Transition Configuration** - `transition_config`
**Component:** `transition_config`  
**Status:** ✅ Complete (save/load/reset)

| Key | Type | Description | Functions |
|-----|------|-------------|-----------|
| `version` | uint32 | Configuration version (migration support) | `transition_config_save_to_nvs()` |
| `config` | blob | Transition settings structure | `transition_config_load_from_nvs()` |
| | | | `transition_config_reset_to_defaults()` |

**Configuration Blob Structure:**
- `duration_ms` - Animation duration (200-5000ms, default: 1500)
- `enabled` - Smooth transitions on/off (default: true)
- `fadein_curve` - Fade-in animation curve (default: ease_in)
- `fadeout_curve` - Fade-out animation curve (default: ease_out)

**Usage:**
```c
transition_config_init();  // Auto-loads from NVS
transition_config_set_duration(2000);
transition_config_set_enabled(false);
transition_config_set_curves(TRANSITION_CURVE_EASE_OUT, TRANSITION_CURVE_BOUNCE);
transition_config_save_to_nvs();  // Persist changes
```

---

## ❌ **Missing NVS Implementations**

### 1. **System State Persistence** - `system_config`
**Status:** ❌ NOT IMPLEMENTED  
**Purpose:** Remember user preferences and system state

**Should Store:**
| Key | Type | Description |
|-----|------|-------------|
| `display_enabled` | uint8 | Main display on/off state |
| `last_brightness_individual` | uint8 | Last individual brightness setting |
| `last_brightness_global` | uint8 | Last global brightness setting |
| `startup_mode` | uint8 | Boot behavior (normal/test/debug) |
| `error_recovery_count` | uint32 | System error recovery statistics |

### 2. **Sensor Calibration Data** - `sensor_config`
**Status:** ❌ NOT IMPLEMENTED  
**Purpose:** Store sensor-specific calibration values

**Should Store:**
| Key | Type | Description |
|-----|------|-------------|
| `light_sensor_offset` | float | BH1750 calibration offset |
| `adc_calibration_slope` | float | Potentiometer ADC calibration |
| `adc_calibration_offset` | float | Potentiometer ADC zero offset |
| `sensor_update_interval` | uint16 | Sensor reading frequency (ms) |
| `change_threshold_lux` | float | Light change sensitivity |
| `change_threshold_pot` | uint8 | Potentiometer change sensitivity |

### 3. **Network Performance Settings** - `network_config`
**Status:** ❌ NOT IMPLEMENTED  
**Purpose:** Advanced network behavior tuning

**Should Store:**
| Key | Type | Description |
|-----|------|-------------|
| `wifi_retry_count` | uint8 | Connection retry attempts |
| `wifi_timeout_ms` | uint16 | Connection timeout |
| `mqtt_keepalive` | uint16 | MQTT keepalive interval |
| `mqtt_reconnect_delay` | uint16 | MQTT reconnection delay |
| `ntp_sync_interval` | uint32 | NTP synchronization frequency |
| `status_publish_interval` | uint32 | MQTT status publishing frequency |

---

## NVS Namespace Organization

### Current Namespace Usage Summary
```
┌─ wifi_config (~96 bytes)
│  ├─ ssid (string, ~32 bytes max)
│  └─ password (string, ~64 bytes max)
│
├─ mqtt_config (~230 bytes)  
│  ├─ broker_uri (string, ~128 bytes)
│  ├─ username (string, ~32 bytes) 
│  ├─ password (string, ~64 bytes)
│  ├─ client_id (string, ~32 bytes)
│  ├─ use_ssl (uint8, 1 byte)
│  └─ port (uint16, 2 bytes)
│
├─ brightness (~260 bytes)
│  ├─ version (uint32, 4 bytes)
│  ├─ light_sensor (blob, ~200 bytes)
│  └─ potentiometer (blob, ~50 bytes)
│
└─ transition_config (~15 bytes)
   ├─ version (uint32, 4 bytes)
   └─ config (blob, ~10 bytes)
      ├─ duration_ms (uint16, 2 bytes)
      ├─ enabled (bool, 1 byte)
      ├─ fadein_curve (uint8, 1 byte)
      └─ fadeout_curve (uint8, 1 byte)

Total Current Usage: ~600 bytes
```

### Recommended Additional Namespaces
```
┌─ system_config (~50 bytes)  
├─ sensor_config (~100 bytes)
└─ network_config (~50 bytes)

Additional Storage Needed: ~200 bytes
Total Projected Usage: ~800 bytes
```

---

## Implementation Priority

### Priority 1: Critical User Experience ✅ COMPLETED
1. **Transition Settings Persistence** - ✅ IMPLEMENTED
   - Duration, curves, enable/disable state
   - High user impact resolved - settings now persist across reboots

### Priority 2: System Reliability  
1. **System State Persistence** - Better recovery and consistency
   - Display state, last brightness values
   - Improves startup behavior and error recovery

### Priority 3: Performance Optimization
2. **Sensor Calibration** - Personalized hardware response
   - Light sensor offsets, ADC calibration
   - Better accuracy for individual hardware variations

3. **Network Performance Settings** - Advanced connectivity tuning
   - Retry counts, timeouts, intervals
   - Better performance in different network environments

---

## Best Practices

### NVS Usage Guidelines
1. **Version Control** - Always include version key for migration support
2. **Error Handling** - Graceful fallback to defaults on read failures
3. **Commit Strategy** - Batch related changes before nvs_commit()
4. **Namespace Isolation** - Use separate namespaces for logical groups
5. **Size Optimization** - Use appropriate data types (uint8 vs uint32)

### Performance Considerations
1. **Read Frequency** - Load once at startup, cache in RAM
2. **Write Frequency** - Only write on user changes, not periodic updates  
3. **Flash Wear** - Avoid excessive writes to extend flash lifetime
4. **Error Recovery** - Always provide default values for missing keys

### Configuration Update Architecture

**Current Implementation for Brightness Config:**
```c
// FLASH-PROTECTED: Debounced persistence approach (mqtt_manager.c)
if (config_changed) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if ((current_time - last_config_write_time) > CONFIG_WRITE_DEBOUNCE_MS) {
        brightness_config_save_to_nvs();  // Immediate write for single changes
        mqtt_publish_brightness_config_status();
    } else {
        config_write_pending = true;  // Debounced write for rapid changes
        xTimerReset(config_write_timer, 0);  // 2-second delay
        mqtt_publish_brightness_config_status();  // UI responsive
    }
}
```

**Characteristics:**
- **Smart persistence**: Immediate for single changes, debounced for rapid changes
- **Flash protection**: Prevents excessive writes during HA slider spam
- **UI responsiveness**: Status always published immediately
- **Data integrity**: All changes eventually persisted with intelligent timing

**Flash Write Impact Analysis (After Protection):**
```
BEFORE PROTECTION:
Single slider drag (5→50):       45 NVS writes (excessive)
Multiple rapid adjustments:      100+ NVS writes (flash wear risk)

AFTER PROTECTION:
Single slider drag (5→50):       1 NVS write (after 2s delay)
Multiple rapid adjustments:      1 NVS write (final state)
Reduction: ~95% fewer flash writes during active configuration
```

**Design Rationale:**
The debounced write approach is optimal for HA-controlled configuration because:
- ✅ **Flash protection**: Home Assistant sends continuous messages during slider operation
- ✅ **UI responsiveness**: Status published immediately, writes delayed intelligently  
- ✅ **Data integrity**: All changes eventually persisted, just with smart timing
- ✅ **Adaptive behavior**: Immediate writes for single changes, debounced for rapid changes
- ✅ **ESP32 compatibility**: Works with NVS wear leveling for maximum lifespan

**Alternative Approaches (not implemented):**
```c
// Delayed batch approach (example only)
if (config_changed) {
    xTimerReset(nvs_save_timer, 0);  // 2-second delay
    // Batch multiple changes within window
}

// Periodic save approach (example only) 
if (config_dirty && (now - last_save > 60000)) {
    brightness_config_save_to_nvs();
    config_dirty = false;
}
```

### Security Considerations
1. **Sensitive Data** - WiFi passwords and MQTT credentials are encrypted by ESP32 NVS
2. **Factory Reset** - Provide mechanism to clear all user data
3. **Backup Strategy** - Consider export/import for user configurations

---

## Implementation Status Summary

**✅ Implemented (4 namespaces, ~600 bytes):**
- WiFi credentials with reset capability
- Complete MQTT configuration with TLS settings  
- Advanced brightness configuration with zone calibration
- Transition system settings with curve configuration

**❌ Missing Optional (3 namespaces, ~200 bytes):**
- System state persistence
- Sensor calibration data
- Network performance tuning

**Total NVS Coverage: 90% of essential settings implemented**

The project now has comprehensive NVS coverage for all critical user-facing settings. User animation preferences, connectivity settings, and brightness configurations all persist across reboots. The remaining optional settings would provide enhanced system reliability and performance tuning capabilities.

---

## NVS Update Trigger Summary

### Configuration Change Flow
```
User Action (Home Assistant)
    ↓
MQTT Message ("home/esp32_core/brightness/config/set")
    ↓
JSON Parsing & Validation
    ↓
Configuration Update Functions
    ├─ brightness_config_set_light_range()
    ├─ brightness_config_set_potentiometer()
    └─ Threshold zone rebuilding
    ↓
config_changed = true (if any update succeeds)
    ↓
brightness_config_save_to_nvs() [Immediate write]
    ↓
nvs_set_blob() + nvs_commit() [Flash storage]
    ↓
mqtt_publish_brightness_config_status() [Confirm to HA]
```

### Key Design Decisions

1. **Immediate Persistence Philosophy**
   - Every configuration change triggers immediate NVS write
   - No batching or delays - data integrity prioritized
   - Appropriate for infrequent configuration changes

2. **Success-Based Triggers**
   - Only successful configuration updates set `config_changed = true`
   - Failed updates don't trigger unnecessary flash writes
   - Clear logging for troubleshooting

3. **Atomic Operations**
   - Each MQTT command processed completely before next
   - No race conditions or partial updates
   - Configuration always in consistent state

4. **Flash Wear Considerations**
   - ESP32 NVS includes wear leveling
   - Configuration changes are infrequent (user-driven)
   - Small data size (~260 bytes) minimizes impact
   - Estimated lifespan: >100,000 configuration changes

This architecture ensures reliable configuration persistence with immediate user feedback while maintaining flash memory health through intelligent trigger management.