# Phase 2.3: Home Assistant Chime Controls Integration

**Date:** 2025-11-07
**Status:** ✅ COMPLETE
**Impact:** HIGH - Westminster chimes now fully controllable via Home Assistant UI

## Summary

Added Home Assistant MQTT discovery integration for Westminster chime controls, creating three new entities that allow users to control chimes directly from the Home Assistant UI.

## New Home Assistant Entities

### 1. Switch: "Westminster Chimes"
- **Entity Type:** `switch.clock_stani_1_chimes_enabled`
- **Function:** Enable/disable Westminster chimes
- **Icon:** `mdi:bell-ring`
- **State Topic:** `home/Clock_Stani_1/chimes/status`
- **Command Topic:** `home/Clock_Stani_1/command`
- **Payloads:**
  - ON: `chimes_enable`
  - OFF: `chimes_disable`

### 2. Number: "Chime Volume"
- **Entity Type:** `number.clock_stani_1_chime_volume`
- **Function:** Adjust chime volume (0-100%)
- **Icon:** `mdi:volume-high`
- **Range:** 0-100%
- **Step:** 5%
- **State Topic:** `home/Clock_Stani_1/chimes/volume`
- **Command Topic:** `home/Clock_Stani_1/command`
- **Command Template:** `chimes_volume_{{ value | int }}`

### 3. Sensor: "Chime Status"
- **Entity Type:** `sensor.clock_stani_1_chime_status`
- **Function:** Display current chime state
- **Icon:** `mdi:bell`
- **State Topic:** `home/Clock_Stani_1/chimes/status`
- **Values:** "Enabled" or "Disabled"

## Implementation Details

### MQTT Discovery Configuration

**Discovery Function:** `mqtt_discovery_publish_chime_controls()`
- Location: `components/mqtt_discovery/mqtt_discovery.c`
- Called from: `mqtt_discovery_publish_all()`
- Publishes: 3 Home Assistant discovery configurations

**Discovery Topics Published:**
```
homeassistant/switch/Clock_Stani_1_8466A8/chimes_enabled/config
homeassistant/number/Clock_Stani_1_8466A8/chime_volume/config
homeassistant/sensor/Clock_Stani_1_8466A8/chime_status/config
```

### State Publishing Functions

**Location:** `components/mqtt_manager/mqtt_manager.c`

**Function:** `mqtt_publish_chime_status()`
- Publishes current enabled/disabled state
- Topic: `home/Clock_Stani_1/chimes/status`
- Payload: `{"enabled": true/false}`
- Called when:
  - MQTT connects (initial state)
  - Chimes enabled via command
  - Chimes disabled via command

**Function:** `mqtt_publish_chime_volume()`
- Publishes current volume level
- Topic: `home/Clock_Stani_1/chimes/volume`
- Payload: `{"volume": 0-100}`
- Called when:
  - MQTT connects (initial state)
  - Volume changed via command

### Integration with Command Handlers

**Modified Commands in `mqtt_manager.c`:**

1. **chimes_enable** (line 894-904)
   - Added: `mqtt_publish_chime_status()` after successful enable
   - Updates Home Assistant switch state

2. **chimes_disable** (line 906-917)
   - Added: `mqtt_publish_chime_status()` after successful disable
   - Updates Home Assistant switch state

3. **chimes_volume_N** (line 949-969)
   - Added: `mqtt_publish_chime_volume()` after successful volume set
   - Updates Home Assistant number slider

## Technical Architecture

### Data Flow

```
Home Assistant UI
    ↓
MQTT Command Topic: home/Clock_Stani_1/command
    ↓
mqtt_event_handler() (MQTT_EVENT_DATA)
    ↓
mqtt_handle_command() (parse command)
    ↓
chime_manager_enable() / chime_manager_set_volume()
    ↓
chime_manager_save_config() (NVS persistence)
    ↓
mqtt_publish_chime_status() / mqtt_publish_chime_volume()
    ↓
MQTT State Topics → Home Assistant UI Update
```

### State Synchronization

**Bidirectional Updates:**
- User toggles switch in HA → ESP32 publishes new state → HA UI updates
- User sends MQTT command → ESP32 publishes new state → HA UI updates
- ESP32 boots → Publishes current state → HA UI shows correct state

**Initial State Publication:**
```c
// In mqtt_event_handler(), MQTT_EVENT_CONNECTED:
mqtt_publish_chime_status();
mqtt_publish_chime_volume();
```

This ensures Home Assistant always shows the correct state after ESP32 reboot or reconnection.

## Files Modified

### Headers
- `components/mqtt_discovery/include/mqtt_discovery.h`
  - Added: `esp_err_t mqtt_discovery_publish_chime_controls(void);`

- `components/mqtt_manager/include/mqtt_manager.h`
  - Added: `esp_err_t mqtt_publish_chime_status(void);`
  - Added: `esp_err_t mqtt_publish_chime_volume(void);`

### Implementation
- `components/mqtt_discovery/mqtt_discovery.c`
  - Added: `mqtt_discovery_publish_chime_controls()` function (98 lines)
  - Modified: `mqtt_discovery_publish_all()` to call chime controls publish

- `components/mqtt_manager/mqtt_manager.c`
  - Added: `mqtt_publish_chime_status()` function (23 lines)
  - Added: `mqtt_publish_chime_volume()` function (23 lines)
  - Modified: `mqtt_event_handler()` MQTT_EVENT_CONNECTED to publish initial state
  - Modified: `chimes_enable` command to publish status update
  - Modified: `chimes_disable` command to publish status update
  - Modified: `chimes_volume_N` command to publish volume update

## Configuration Details

### Discovery Configuration Format

**Switch Configuration:**
```json
{
  "~": "home/Clock_Stani_1",
  "name": "Westminster Chimes",
  "unique_id": "Clock_Stani_1_8466A8_chimes_enabled",
  "state_topic": "~/chimes/status",
  "command_topic": "~/command",
  "value_template": "{{ value_json.enabled }}",
  "payload_on": "chimes_enable",
  "payload_off": "chimes_disable",
  "state_on": true,
  "state_off": false,
  "icon": "mdi:bell-ring",
  "availability_topic": "~/availability",
  "device": { ... }
}
```

**Number Configuration:**
```json
{
  "~": "home/Clock_Stani_1",
  "name": "Chime Volume",
  "unique_id": "Clock_Stani_1_8466A8_chime_volume",
  "state_topic": "~/chimes/volume",
  "command_topic": "~/command",
  "value_template": "{{ value_json.volume }}",
  "command_template": "chimes_volume_{{ value | int }}",
  "min": 0,
  "max": 100,
  "step": 5,
  "unit_of_measurement": "%",
  "icon": "mdi:volume-high",
  "availability_topic": "~/availability",
  "device": { ... }
}
```

**Sensor Configuration:**
```json
{
  "~": "home/Clock_Stani_1",
  "name": "Chime Status",
  "unique_id": "Clock_Stani_1_8466A8_chime_status",
  "state_topic": "~/chimes/status",
  "value_template": "{{ 'Enabled' if value_json.enabled else 'Disabled' }}",
  "icon": "mdi:bell",
  "availability_topic": "~/availability",
  "device": { ... }
}
```

## Testing Procedure

### Verification Steps

1. **Check Entity Discovery:**
   - Open Home Assistant
   - Navigate to device: Clock_Stani_1
   - Verify 3 new entities appear:
     - Switch: Westminster Chimes
     - Number: Chime Volume
     - Sensor: Chime Status

2. **Test Switch Control:**
   - Toggle "Westminster Chimes" switch ON
   - Verify sensor changes to "Enabled"
   - Wait for next quarter hour - chimes should play
   - Toggle switch OFF
   - Verify sensor changes to "Disabled"
   - Chimes should not play

3. **Test Volume Control:**
   - Adjust "Chime Volume" slider
   - Send test command: `chimes_test_quarter`
   - Verify volume change is audible
   - Check logs for: `📤 Published chime volume: X%`

4. **Test State Persistence:**
   - Set chimes to enabled, volume to 75%
   - Restart ESP32
   - Verify Home Assistant shows correct state after reconnection

### Expected Logs

**On MQTT Connect:**
```
I (xxxxx) mqtt_manager: 📤 Published chime status: {"enabled":true}
I (xxxxx) mqtt_manager: 📤 Published chime volume: 80%
```

**On Switch Toggle:**
```
I (xxxxx) MQTT_MANAGER: 🔔 Enabling Westminster chimes
I (xxxxx) chime_mgr: Chimes ENABLED
I (xxxxx) mqtt_manager: 📤 Published chime status: {"enabled":true}
```

**On Volume Change:**
```
I (xxxxx) MQTT_MANAGER: 🔊 Setting chime volume to 75%
I (xxxxx) chime_mgr: Chime volume set to: 75%
I (xxxxx) mqtt_manager: 📤 Published chime volume: 75%
```

## Benefits

### User Experience Improvements

1. **Visual Control:** No need to remember MQTT commands - use intuitive HA UI
2. **Real-time Feedback:** Sensor shows current state at a glance
3. **Easy Volume Adjustment:** Slider makes volume changes quick and easy
4. **Integration:** Works with HA automations, scenes, and dashboards

### Technical Benefits

1. **State Synchronization:** HA always reflects actual ESP32 state
2. **Automatic Discovery:** Entities appear without manual HA configuration
3. **Consistent Patterns:** Follows same patterns as other entities (brightness, transitions)
4. **Persistence:** NVS ensures settings survive reboots

## Future Enhancements (Optional)

### Possible Additions

1. **Quiet Hours Control:**
   - Number entities for quiet_start_hour and quiet_end_hour
   - Time selector for easier configuration

2. **Chime Set Selection:**
   - Select entity to choose between Westminster, Cambridge, Whittington
   - Requires multiple chime sets on SD card

3. **Test Buttons:**
   - Button entities for test_quarter, test_half, test_three_quarter, test_full
   - Quick testing without MQTT commands

4. **Schedule Automation:**
   - HA automation examples in documentation
   - Seasonal chime schedules
   - Weekend vs weekday configurations

## Related Documentation

- [Phase 2.3 Westminster Chimes Complete](phase-2.3-westminster-chimes-complete.md)
- [Phase 2.3 NVS Persistence Fix](phase-2.3-nvs-persistence-fix.md)
- [MQTT System Architecture](mqtt/mqtt-system-architecture.md)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)

## Conclusion

**Status:** ✅ PRODUCTION READY

The Home Assistant chime controls integration is complete and fully functional. Users can now control Westminster chimes entirely through the Home Assistant UI, with automatic state synchronization and persistent settings.

**Total New Entities:** 3 (1 switch, 1 number, 1 sensor)
**Total Word Clock Entities:** 39 (previously 36 + 3 new chime entities)

**Phase 2.3 Westminster Chimes System: FULLY COMPLETE** 🎉
