# ESP32 Word Clock - Home Assistant MQTT Discovery Implementation Plan

## Overview

MQTT Discovery allows Home Assistant to automatically create entities with zero manual configuration. The Word Clock will publish discovery messages that tell HA exactly how to configure each entity.

## Discovery Todo List

### High Priority
- [ ] Research Home Assistant MQTT Discovery protocol specifications
- [ ] Analyze current MQTT topic structure for discovery compatibility
- [ ] Design discovery configuration topics and payloads
- [ ] Plan device registry integration with unique identifiers

### Medium Priority
- [ ] Design entity configurations for all device features
- [ ] Plan availability and state synchronization
- [ ] Design configuration flow for discovery messages
- [ ] Plan testing strategy for discovery implementation

### Low Priority
- [ ] Document code changes needed for implementation
- [ ] Create migration guide from manual to discovery

---

## Detailed Step-by-Step Plan for MQTT Discovery

### Phase 1: Understanding MQTT Discovery Protocol

#### Step 1: Discovery Topic Structure
Home Assistant listens for discovery messages on:
```
<discovery_prefix>/<component>/[<node_id>/]<object_id>/config
```

For the Word Clock:
- `discovery_prefix`: `homeassistant` (default)
- `component`: `light`, `sensor`, `switch`, `number`, `select`, `button`
- `node_id`: `esp32_wordclock` (optional but recommended)
- `object_id`: Unique ID for each entity

Example: `homeassistant/light/esp32_wordclock/display/config`

#### Step 2: Required Discovery Payload Structure
Each discovery message must contain:
```json
{
  "name": "Friendly name",
  "unique_id": "unique_identifier",
  "device": {
    "identifiers": ["esp32_wordclock_AABBCC"],
    "name": "German Word Clock",
    "model": "ESP32 IoT Word Clock",
    "manufacturer": "DIY",
    "sw_version": "1.0.0"
  },
  // Component-specific configuration
}
```

### Phase 2: Device Registry Planning

#### Step 3: Design Device Information
Create a consistent device identity that groups all entities:

```json
{
  "device": {
    "identifiers": ["wordclock_5A1E74"],  // MAC-based ID
    "connections": [["mac", "AA:BB:CC:DD:EE:FF"]],
    "name": "German Word Clock",
    "model": "ESP32 LED Matrix Clock",
    "manufacturer": "Custom Build",
    "sw_version": "2.0.0",
    "hw_version": "1.0",
    "configuration_url": "http://192.168.1.100"  // Web config page
  }
}
```

### Phase 3: Entity Discovery Configurations

#### Step 4: Main Light Entity (Word Clock Display)
**Topic**: `homeassistant/light/esp32_wordclock/display/config`

```json
{
  "~": "home/esp32_core",
  "name": "Word Clock Display",
  "unique_id": "wordclock_5A1E74_display",
  "device": { /* device info */ },
  "state_topic": "~/availability",
  "command_topic": "~/brightness/set",
  "brightness_state_topic": "~/brightness/status",
  "brightness_command_topic": "~/brightness/set",
  "brightness_scale": 255,
  "brightness_value_template": "{{ value_json.global }}",
  "on_command_type": "brightness",
  "payload_on": "online",
  "payload_off": "offline",
  "state_value_template": "{{ 'ON' if value == 'online' else 'OFF' }}",
  "brightness_command_template": "{\"global\": {{ brightness }}}",
  "schema": "template",
  "optimistic": false,
  "qos": 1,
  "retain": false,
  "availability": {
    "topic": "~/availability",
    "payload_available": "online",
    "payload_not_available": "offline"
  },
  "effect_list": ["none", "smooth_transitions", "instant"],
  "effect_state_topic": "~/transition/status",
  "effect_command_topic": "~/transition/set",
  "effect_value_template": "{{ 'smooth_transitions' if value_json.enabled else 'instant' }}"
}
```

#### Step 5: Sensor Entities

**WiFi Status Sensor**
**Topic**: `homeassistant/sensor/esp32_wordclock/wifi_status/config`
```json
{
  "~": "home/esp32_core",
  "name": "WiFi Status",
  "unique_id": "wordclock_5A1E74_wifi",
  "device": { /* device info */ },
  "state_topic": "~/wifi",
  "icon": "mdi:wifi",
  "availability_topic": "~/availability"
}
```

**NTP Sync Sensor**
**Topic**: `homeassistant/sensor/esp32_wordclock/ntp_status/config`
```json
{
  "~": "home/esp32_core",
  "name": "Time Sync Status",
  "unique_id": "wordclock_5A1E74_ntp",
  "device": { /* device info */ },
  "state_topic": "~/ntp",
  "icon": "mdi:clock-check",
  "availability_topic": "~/availability"
}
```

**Individual Brightness Sensor**
**Topic**: `homeassistant/sensor/esp32_wordclock/led_brightness/config`
```json
{
  "~": "home/esp32_core",
  "name": "LED Brightness",
  "unique_id": "wordclock_5A1E74_led_brightness",
  "device": { /* device info */ },
  "state_topic": "~/brightness/status",
  "unit_of_measurement": "%",
  "value_template": "{{ ((value_json.individual / 255) * 100) | round(0) }}",
  "icon": "mdi:brightness-6",
  "availability_topic": "~/availability"
}
```

**Current Time Display Sensor**
**Topic**: `homeassistant/sensor/esp32_wordclock/time_display/config`
```json
{
  "~": "home/esp32_core",
  "name": "Current Display",
  "unique_id": "wordclock_5A1E74_time_text",
  "device": { /* device info */ },
  "state_topic": "~/time/text",
  "icon": "mdi:clock-digital",
  "availability_topic": "~/availability"
}
```

#### Step 6: Control Entities

**Smooth Transitions Switch**
**Topic**: `homeassistant/switch/esp32_wordclock/transitions/config`
```json
{
  "~": "home/esp32_core",
  "name": "Smooth Transitions",
  "unique_id": "wordclock_5A1E74_transitions",
  "device": { /* device info */ },
  "state_topic": "~/transition/status",
  "command_topic": "~/transition/set",
  "value_template": "{{ value_json.enabled }}",
  "payload_on": "{\"enabled\": true}",
  "payload_off": "{\"enabled\": false}",
  "state_on": true,
  "state_off": false,
  "icon": "mdi:transition",
  "availability_topic": "~/availability"
}
```

**Transition Duration Number**
**Topic**: `homeassistant/number/esp32_wordclock/transition_duration/config`
```json
{
  "~": "home/esp32_core",
  "name": "Transition Duration",
  "unique_id": "wordclock_5A1E74_duration",
  "device": { /* device info */ },
  "state_topic": "~/transition/status",
  "command_topic": "~/transition/set",
  "value_template": "{{ value_json.duration }}",
  "command_template": "{\"duration\": {{ value | int }}}",
  "min": 200,
  "max": 5000,
  "step": 100,
  "unit_of_measurement": "ms",
  "icon": "mdi:timer",
  "availability_topic": "~/availability"
}
```

**Individual Brightness Control**
**Topic**: `homeassistant/number/esp32_wordclock/individual_brightness/config`
```json
{
  "~": "home/esp32_core",
  "name": "LED Brightness",
  "unique_id": "wordclock_5A1E74_individual",
  "device": { /* device info */ },
  "state_topic": "~/brightness/status",
  "command_topic": "~/brightness/set",
  "value_template": "{{ value_json.individual }}",
  "command_template": "{\"individual\": {{ value | int }}}",
  "min": 5,
  "max": 255,
  "step": 5,
  "icon": "mdi:brightness-6",
  "availability_topic": "~/availability"
}
```

**Animation Curve Selectors**
**Topic**: `homeassistant/select/esp32_wordclock/fadein_curve/config`
```json
{
  "~": "home/esp32_core",
  "name": "Fade In Curve",
  "unique_id": "wordclock_5A1E74_fadein",
  "device": { /* device info */ },
  "state_topic": "~/transition/status",
  "command_topic": "~/transition/set",
  "value_template": "{{ value_json.fadein_curve }}",
  "command_template": "{\"fadein_curve\": \"{{ value }}\"}",
  "options": ["linear", "ease_in", "ease_out", "ease_in_out", "bounce"],
  "icon": "mdi:chart-bell-curve",
  "availability_topic": "~/availability"
}
```

**Action Buttons**
**Topic**: `homeassistant/button/esp32_wordclock/restart/config`
```json
{
  "~": "home/esp32_core",
  "name": "Restart",
  "unique_id": "wordclock_5A1E74_restart",
  "device": { /* device info */ },
  "command_topic": "~/command",
  "payload_press": "restart",
  "icon": "mdi:restart",
  "availability_topic": "~/availability"
}
```

### Phase 4: Discovery Message Flow

#### Step 7: Discovery Publishing Sequence
1. **On Boot/Connection**:
   ```
   1. Connect to MQTT broker
   2. Subscribe to command topics
   3. Publish availability: "online"
   4. Publish all discovery configs (with retain flag)
   5. Publish initial state values
   ```

2. **Discovery Message Order**:
   ```
   1. Device-level entities (main light)
   2. Sensor entities (status/monitoring)
   3. Control entities (switches, numbers)
   4. Action entities (buttons)
   ```

3. **Timing Considerations**:
   - Delay 100ms between discovery messages
   - Total discovery time: ~2 seconds
   - Publish states after all discoveries sent

#### Step 8: State Synchronization Plan

**Initial State Publishing**:
```
After discovery complete:
1. Publish brightness status
2. Publish transition settings
3. Publish WiFi/NTP status
4. Publish availability
```

**Ongoing Updates**:
- State changes trigger immediate publish
- Periodic refresh every 60 seconds
- Retain flags for persistent states

### Phase 5: Advanced Discovery Features

#### Step 9: Device Triggers (Automations)
**Topic**: `homeassistant/device_automation/esp32_wordclock/hour_change/config`
```json
{
  "automation_type": "trigger",
  "type": "time_pattern",
  "subtype": "hour_change",
  "payload": "hour_changed",
  "topic": "home/esp32_core/event",
  "device": { /* device info */ }
}
```

#### Step 10: Diagnostic Entities
**Topic**: `homeassistant/sensor/esp32_wordclock/uptime/config`
```json
{
  "~": "home/esp32_core",
  "name": "Uptime",
  "unique_id": "wordclock_5A1E74_uptime",
  "device": { /* device info */ },
  "state_topic": "~/diagnostic/uptime",
  "unit_of_measurement": "minutes",
  "state_class": "measurement",
  "entity_category": "diagnostic",
  "icon": "mdi:timer-sand",
  "availability_topic": "~/availability"
}
```

### Phase 6: Implementation Requirements

#### Step 11: Code Structure Needed
```
mqtt_discovery.c/h
├── discovery_init()
├── publish_device_config()
├── publish_light_discovery()
├── publish_sensor_discoveries()
├── publish_control_discoveries()
├── publish_button_discoveries()
└── remove_discoveries()
```

#### Step 12: Discovery Configuration Storage
```c
typedef struct {
    bool enabled;
    char discovery_prefix[32];  // "homeassistant"
    char node_id[32];          // "esp32_wordclock"
    uint32_t publish_delay_ms;  // 100ms between messages
} discovery_config_t;
```

### Phase 7: Testing Strategy

#### Step 13: Discovery Testing Steps
1. **Enable MQTT Discovery in HA**:
   ```yaml
   mqtt:
     discovery: true
     discovery_prefix: homeassistant
   ```

2. **Monitor Discovery Topics**:
   ```
   mosquitto_sub -t "homeassistant/#" -v
   ```

3. **Verify Entity Creation**:
   - Check Developer Tools → States
   - Verify device in Settings → Devices
   - Test all controls work

4. **Test Persistence**:
   - Restart HA
   - Entities should reappear
   - States should be restored

#### Step 14: Debugging Discovery Issues
- Use MQTT Explorer to see actual messages
- Check HA logs for discovery errors
- Verify JSON payload validity
- Ensure unique_ids are truly unique

### Phase 8: Migration Path

#### Step 15: Migration from Manual Config
1. **Backup Current Config**
2. **Enable Discovery Mode**
3. **Remove Manual YAML Entities**
4. **Verify Auto-Created Entities**
5. **Update Automations/Scripts**
6. **Test Everything**

### Benefits of Discovery Implementation

1. **Zero Configuration**: Plug and play for users
2. **Device Grouping**: All entities under one device
3. **Automatic UI**: HA generates appropriate cards
4. **Easy Updates**: Change configs without touching HA
5. **Multiple Instances**: Support multiple clocks easily
6. **Professional Integration**: Appears like commercial device

### Required Code Changes Summary

1. **Add discovery module** to MQTT manager
2. **Generate unique device ID** from MAC address
3. **Publish discovery configs** on connection
4. **Handle discovery prefix** configuration
5. **Add remove discovery** command
6. **Implement state caching** for quick restore

This plan provides a complete blueprint for implementing MQTT Discovery without changing any code yet, allowing you to understand exactly what needs to be built.