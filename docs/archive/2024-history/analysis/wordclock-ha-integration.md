# ESP32 Word Clock to Home Assistant Integration Guide

## Implementation Todo List

### High Priority Tasks
- [ ] Verify MQTT broker connection between Word Clock and Home Assistant
- [ ] Test current MQTT communication with MQTT Explorer
- [ ] Create light entity configuration in configuration.yaml
- [ ] Create sensor entities for WiFi, NTP, and brightness status
- [ ] Test integration and verify all controls work

### Medium Priority Tasks
- [ ] Create control switches for transitions and restart
- [ ] Create input numbers for brightness and transition controls
- [ ] Set up automations for brightness and transition updates
- [ ] Create scripts for night mode, day mode, and transition testing
- [ ] Design and implement Lovelace dashboard cards

### Low Priority Tasks
- [ ] Create time-based automations for auto brightness
- [ ] Document troubleshooting steps

---

## Step-by-Step Plan: Integrating ESP32 Word Clock into Home Assistant

### Phase 1: Prerequisites & Initial Setup

#### Step 1: Verify MQTT Broker Connection
1. **Ensure MQTT Add-on is installed in Home Assistant**
   - If not using external broker, install Mosquitto broker add-on
   - Configure with username/password authentication
   
2. **Configure MQTT Bridge (if using HiveMQ Cloud)**
   - Set up MQTT bridge between HiveMQ Cloud and Home Assistant
   - Alternative: Point ESP32 directly to Home Assistant MQTT broker

#### Step 2: Test Current MQTT Communication
1. **Install MQTT Explorer or use HA Developer Tools**
   - Subscribe to `home/esp32_core/#` to see all messages
   - Verify device is publishing status messages
   - Test sending commands to ensure bidirectional communication

### Phase 2: Home Assistant Integration Options

#### Option A: Manual MQTT Configuration (Immediate)
1. **Create MQTT entities in configuration.yaml**
2. **Define sensors, switches, and lights**
3. **Set up automations and scripts**

#### Option B: MQTT Discovery (Recommended - Requires Code Changes)
1. **Plan discovery topics and payloads**
2. **Auto-configure all entities**
3. **Zero manual configuration needed**

### Phase 3: Manual Configuration Plan (No Code Changes)

#### Step 3: Create Light Entity for Word Clock
```yaml
# configuration.yaml
mqtt:
  light:
    - name: "Word Clock Display"
      unique_id: "wordclock_display"
      state_topic: "home/esp32_core/availability"
      command_topic: "home/esp32_core/command"
      brightness_state_topic: "home/esp32_core/brightness/status"
      brightness_command_topic: "home/esp32_core/brightness/set"
      brightness_scale: 255
      payload_on: "online"
      payload_off: "offline"
      optimistic: false
      qos: 1
      retain: false
      value_template: "{{ 'ON' if value == 'online' else 'OFF' }}"
      brightness_value_template: "{{ value_json.global }}"
      on_command_type: 'brightness'
      set_position_template: '{"global": {{ brightness }}}'
```

#### Step 4: Create Sensors
```yaml
sensor:
  # WiFi Status
  - platform: mqtt
    name: "Word Clock WiFi Status"
    unique_id: "wordclock_wifi_status"
    state_topic: "home/esp32_core/wifi"
    icon: "mdi:wifi"
    
  # NTP Status
  - platform: mqtt
    name: "Word Clock NTP Status"
    unique_id: "wordclock_ntp_status"
    state_topic: "home/esp32_core/ntp"
    icon: "mdi:clock-check"
    
  # Individual Brightness
  - platform: mqtt
    name: "Word Clock LED Brightness"
    unique_id: "wordclock_individual_brightness"
    state_topic: "home/esp32_core/brightness/status"
    unit_of_measurement: "%"
    value_template: "{{ ((value_json.individual / 255) * 100) | round(0) }}"
    icon: "mdi:brightness-6"
    
  # Global Brightness
  - platform: mqtt
    name: "Word Clock Display Brightness"
    unique_id: "wordclock_global_brightness"
    state_topic: "home/esp32_core/brightness/status"
    unit_of_measurement: "%"
    value_template: "{{ ((value_json.global / 255) * 100) | round(0) }}"
    icon: "mdi:brightness-7"
```

#### Step 5: Create Control Switches
```yaml
switch:
  # Transition System
  - platform: mqtt
    name: "Word Clock Smooth Transitions"
    unique_id: "wordclock_transitions"
    state_topic: "home/esp32_core/transition/status"
    command_topic: "home/esp32_core/transition/set"
    value_template: "{{ value_json.enabled }}"
    payload_on: '{"enabled": true}'
    payload_off: '{"enabled": false}'
    state_on: true
    state_off: false
    icon: "mdi:transition"
    
  # Restart Switch
  - platform: mqtt
    name: "Word Clock Restart"
    unique_id: "wordclock_restart"
    command_topic: "home/esp32_core/command"
    payload_on: "restart"
    icon: "mdi:restart"
```

#### Step 6: Create Input Numbers for Controls
```yaml
input_number:
  wordclock_transition_duration:
    name: "Transition Duration"
    min: 200
    max: 5000
    step: 100
    unit_of_measurement: "ms"
    icon: "mdi:timer"
    
  wordclock_individual_brightness:
    name: "LED Brightness"
    min: 5
    max: 255
    step: 5
    icon: "mdi:brightness-6"
    
  wordclock_global_brightness:
    name: "Display Brightness"
    min: 10
    max: 255
    step: 5
    icon: "mdi:brightness-7"
```

### Phase 4: Automations & Scripts

#### Step 7: Create Automations
```yaml
automation:
  # Update transition duration
  - alias: "Word Clock Update Transition Duration"
    trigger:
      - platform: state
        entity_id: input_number.wordclock_transition_duration
    action:
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/transition/set"
          payload_template: '{"duration": {{ states("input_number.wordclock_transition_duration") | int }}}'
          
  # Update brightness
  - alias: "Word Clock Update Brightness"
    trigger:
      - platform: state
        entity_id: 
          - input_number.wordclock_individual_brightness
          - input_number.wordclock_global_brightness
    action:
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/brightness/set"
          payload_template: >
            {
              "individual": {{ states("input_number.wordclock_individual_brightness") | int }},
              "global": {{ states("input_number.wordclock_global_brightness") | int }}
            }
```

#### Step 8: Create Scripts
```yaml
script:
  wordclock_test_transitions:
    alias: "Test Word Clock Transitions"
    sequence:
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/command"
          payload: "test_transitions_start"
      - delay: "00:05:00"
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/command"
          payload: "test_transitions_stop"
          
  wordclock_night_mode:
    alias: "Word Clock Night Mode"
    sequence:
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/brightness/set"
          payload: '{"individual": 20, "global": 50}'
          
  wordclock_day_mode:
    alias: "Word Clock Day Mode"
    sequence:
      - service: mqtt.publish
        data:
          topic: "home/esp32_core/brightness/set"
          payload: '{"individual": 80, "global": 200}'
```

### Phase 5: Lovelace Dashboard

#### Step 9: Create Dashboard Cards
```yaml
# Lovelace card configuration
type: vertical-stack
cards:
  # Status Card
  - type: entities
    title: Word Clock Status
    entities:
      - entity: light.word_clock_display
      - entity: sensor.word_clock_wifi_status
      - entity: sensor.word_clock_ntp_status
      
  # Brightness Controls
  - type: entities
    title: Brightness Control
    entities:
      - entity: input_number.wordclock_individual_brightness
      - entity: input_number.wordclock_global_brightness
      - entity: sensor.word_clock_led_brightness
      - entity: sensor.word_clock_display_brightness
      
  # Advanced Controls
  - type: entities
    title: Advanced Settings
    entities:
      - entity: switch.word_clock_smooth_transitions
      - entity: input_number.wordclock_transition_duration
      
  # Actions
  - type: horizontal-stack
    cards:
      - type: button
        name: Night Mode
        tap_action:
          action: call-service
          service: script.wordclock_night_mode
        icon: mdi:weather-night
        
      - type: button
        name: Day Mode
        tap_action:
          action: call-service
          service: script.wordclock_day_mode
        icon: mdi:weather-sunny
        
      - type: button
        name: Test Transitions
        tap_action:
          action: call-service
          service: script.wordclock_test_transitions
        icon: mdi:animation-play
        
      - type: button
        name: Restart
        tap_action:
          action: call-service
          service: switch.turn_on
          target:
            entity_id: switch.word_clock_restart
        icon: mdi:restart
```

### Phase 6: Testing & Validation

#### Step 10: Test Integration
1. **Restart Home Assistant** after adding configuration
2. **Check Configuration** → Settings → System → Check Configuration
3. **Verify Entities** appear in Developer Tools → States
4. **Test Controls**:
   - Toggle smooth transitions switch
   - Adjust brightness sliders
   - Run test scripts
   - Monitor MQTT messages in Developer Tools

#### Step 11: Create Time-Based Automations
```yaml
automation:
  # Auto dim at night
  - alias: "Word Clock Auto Night Mode"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: script.wordclock_night_mode
      
  # Brighten in morning
  - alias: "Word Clock Auto Day Mode"
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: script.wordclock_day_mode
```

### Phase 7: Future Enhancements (Requires Code Changes)

1. **Implement MQTT Discovery**
   - Auto-configure all entities
   - Support for Home Assistant device registry
   - Automatic UI generation

2. **Add Time Display Sensor**
   - Show current displayed time as text
   - "ES IST HALB DREI" as sensor state

3. **Add Diagnostics Sensors**
   - I2C error count
   - Uptime
   - Free heap memory
   - CPU temperature

4. **Implement Effects**
   - Rainbow mode
   - Breathing effect
   - Custom animations

### Troubleshooting Guide

1. **Entities Not Appearing**
   - Check MQTT integration is configured
   - Verify MQTT broker connection
   - Check topic paths match exactly
   - Look for errors in HA logs

2. **Commands Not Working**
   - Use MQTT Explorer to verify messages sent
   - Check QoS settings
   - Ensure topics match device expectations
   - Verify JSON payload format

3. **Status Not Updating**
   - Confirm device is publishing to correct topics
   - Check retain flags
   - Verify value_template parsing

### Additional Resources

- [Home Assistant MQTT Integration](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Discovery Documentation](https://www.home-assistant.io/docs/mqtt/discovery/)
- [Lovelace UI Documentation](https://www.home-assistant.io/lovelace/)

This plan provides a complete path to integrate your Word Clock into Home Assistant without any code changes, giving you full control over the device through HA's interface.