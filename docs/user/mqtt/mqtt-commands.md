# ESP32 German Word Clock - Complete MQTT Commands for MQTTX

## MQTTX Connection Setup

**Connection Details:**
- **Host**: `5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud`
- **Port**: `8883`
- **Protocol**: `mqtts://` (MQTT over TLS)
- **Username**: `esp32_led_device`
- **Password**: `tufcux-3xuwda-Vomnys`
- **Client ID**: Any unique ID (e.g., `MQTTX_Client`)

## MQTTX Command Examples

### 1. System Control Commands

#### Get System Status
```
Topic: home/esp32_core/command
Payload: status
```

#### Restart Device
```
Topic: home/esp32_core/command
Payload: restart
```

#### Reset WiFi Credentials
```
Topic: home/esp32_core/command
Payload: reset_wifi
```

#### Set DS3231 RTC Time (for Testing)
```
Topic: home/esp32_core/command
Payload: set_time 09:05
```
**Format**: `set_time HH:MM` (24-hour format)  
**Examples**:
- `set_time 08:50` → Test "ES IST ZEHN VOR NEUN"
- `set_time 09:00` → Test "ES IST NEUN UHR" 
- `set_time 14:30` → Test "ES IST HALB DREI"
- `set_time 21:37` → Test "ES IST FÜNF NACH HALB ZEHN" + 2 minute indicators

### 2. Transition Test Commands

#### Start 5-Minute Transition Demo
```
Topic: home/esp32_core/command
Payload: test_transitions_start
```

#### Stop Transition Test
```
Topic: home/esp32_core/command
Payload: test_transitions_stop
```

#### Check Transition Test Status
```
Topic: home/esp32_core/command
Payload: test_transitions_status
```

### 3. Transition System Control

#### Enable Smooth Transitions (1.5 seconds)
```
Topic: home/esp32_core/transition/set
Payload: {"duration": 1500, "enabled": true, "fadein_curve": "ease_out", "fadeout_curve": "ease_in"}
```

#### Disable Transitions (Instant Mode)
```
Topic: home/esp32_core/transition/set
Payload: {"enabled": false}
```

#### Set 2-Second Transitions with Bounce Effect
```
Topic: home/esp32_core/transition/set
Payload: {"duration": 2000, "enabled": true, "fadein_curve": "bounce", "fadeout_curve": "ease_out"}
```

#### Fast Transitions (500ms)
```
Topic: home/esp32_core/transition/set
Payload: {"duration": 500, "enabled": true, "fadein_curve": "linear", "fadeout_curve": "linear"}
```

### 4. Brightness Control

#### Set Individual and Global Brightness
```
Topic: home/esp32_core/brightness/set
Payload: {"individual": 60, "global": 180}
```

#### Set Only Individual Brightness (User Control)
```
Topic: home/esp32_core/brightness/set
Payload: {"individual": 80}
```

#### Set Only Global Brightness (Room Lighting)
```
Topic: home/esp32_core/brightness/set
Payload: {"global": 200}
```

#### Dim Display (Night Mode)
```
Topic: home/esp32_core/brightness/set
Payload: {"individual": 20, "global": 50}
```

#### Bright Display (Day Mode)
```
Topic: home/esp32_core/brightness/set
Payload: {"individual": 100, "global": 255}
```

### 5. Brightness Configuration

#### Configure Light Sensor Ranges
```
Topic: home/esp32_core/brightness/config/set
Payload: {
  "light_sensor": {
    "very_dark": {
      "lux_min": 0.0,
      "lux_max": 5.0,
      "brightness_min": 20,
      "brightness_max": 40
    },
    "dim": {
      "lux_min": 5.0,
      "lux_max": 20.0,
      "brightness_min": 40,
      "brightness_max": 80
    },
    "normal": {
      "lux_min": 20.0,
      "lux_max": 100.0,
      "brightness_min": 80,
      "brightness_max": 150
    }
  }
}
```

#### Configure Potentiometer Settings
```
Topic: home/esp32_core/brightness/config/set
Payload: {
  "potentiometer": {
    "brightness_min": 5,
    "brightness_max": 200,
    "curve_type": "linear",
    "safety_limit_max": 80
  }
}
```

#### Configure Safety PWM Limit Only
```
Topic: home/esp32_core/brightness/config/set
Payload: {
  "potentiometer": {
    "safety_limit_max": 120
  }
}
```

**Safety Limit Parameters:**
- **Range:** 20-255 PWM units
- **Default:** 80 PWM units (conservative, recommended)
- **Purpose:** Maximum safe PWM to prevent LED overheating
- **Impact:** Constrains effective brightness regardless of other settings

#### Get Current Configuration
```
Topic: home/esp32_core/brightness/config/get
Payload: {}
```

#### Refresh Sensor Values (Manual Update)
```
Topic: home/esp32_core/command
Payload: refresh_sensors
```
**Note:** This command reads current hardware states and publishes to MQTT
**NVS Impact:** None - only reads values without storing anything
**Updates:** Current PWM, Current GRPPWM, light level, potentiometer readings

### 6. Monitor Device Status

#### Subscribe to All Status Updates
```
Topic: home/esp32_core/+
```

#### Subscribe to Specific Status Topics
```
Topic: home/esp32_core/status          # System status
Topic: home/esp32_core/wifi            # WiFi connection
Topic: home/esp32_core/ntp             # NTP sync status
Topic: home/esp32_core/availability    # Online/offline
Topic: home/esp32_core/transition/status   # Transition settings
Topic: home/esp32_core/brightness/status   # Current brightness
```

## MQTTX Usage Tips

### Publishing Messages
1. Click **"+ New Connection"** in MQTTX
2. Enter connection details above
3. Click **"Connect"**
4. In the **"Publish"** section:
   - Enter the **Topic** (e.g., `home/esp32_core/command`)
   - Enter the **Payload** (e.g., `status`)
   - Click **"Publish"**

### Subscribing to Updates
1. In the **"Subscribe"** section:
   - Enter **Topic** (e.g., `home/esp32_core/+`)
   - Click **"Subscribe"**
2. You'll see real-time messages from the device

### Common MQTTX Workflows

#### Quick Status Check
```
1. Publish: Topic: home/esp32_core/command, Payload: status
2. Subscribe: Topic: home/esp32_core/+
3. Watch for response messages
```

#### Transition Demo
```
1. Publish: Topic: home/esp32_core/command, Payload: test_transitions_start
2. Subscribe: Topic: home/esp32_core/status
3. Watch for "transition_test_started" confirmation
4. Observe LED transitions for 5 minutes
5. Publish: Topic: home/esp32_core/command, Payload: test_transitions_stop
```

#### Brightness Adjustment
```
1. Subscribe: Topic: home/esp32_core/brightness/status
2. Publish: Topic: home/esp32_core/brightness/set, Payload: {"individual": 40, "global": 120}
3. Watch for updated brightness values
```

## Animation Curve Options

**Available Curves for Transitions:**
- `"linear"` - Constant speed
- `"ease_in"` - Slow start, fast finish
- `"ease_out"` - Fast start, slow finish *(recommended)*
- `"ease_in_out"` - Slow start and finish
- `"bounce"` - Slight overshoot with settle

## Parameter Ranges

**Transition Duration**: 200-5000 milliseconds  
**Individual Brightness**: 5-255  
**Global Brightness**: 10-255  
**Light Sensor Lux**: 0.0-2000.0  
**Potentiometer Brightness**: 5-255  

## Expected Responses

When you send commands, watch for these status messages:
- `"transition_test_started"` - Test mode activated
- `"individual_brightness_updated"` - Brightness changed
- `"transition_duration_updated"` - Duration changed
- `"transitions_enabled"` / `"transitions_disabled"` - Mode changed
- `"time_set_successfully"` - DS3231 time updated
- `"time_set_to_09_05"` - Confirmation of specific time set
- `"invalid_time_format"` - Time command format error
- `"heartbeat"` - Periodic status (every 60 seconds)

The device will respond to most commands with status confirmations, making it easy to verify your commands were received and processed.