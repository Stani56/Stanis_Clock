# ESP32-S3 German Word Clock - Complete MQTT Command Reference

**Current Platform:** YB-ESP32-S3-AMP (ESP32-S3-WROOM-1-N8R2)
**Device Name:** Clock_Stani_1
**Last Updated:** November 2025
**Status:** Production with Audio ✅ | Westminster Chimes ✅ | OTA Updates ✅

## MQTT Connection Setup

**HiveMQ Cloud TLS Connection:**
- **Broker:** `broker.hivemq.com`
- **Port:** `8883` (MQTT over TLS)
- **Protocol:** `mqtts://`
- **Username:** `StaniWirdWild`
- **Password:** `ClockWirdWild`
- **Client ID:** Any unique ID (e.g., `MQTTX_Client`)
- **CA Certificate:** Required for TLS (`/etc/ssl/certs/ca-certificates.crt` on Linux)

**mosquitto_pub Template:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "COMMAND"
```

**mosquitto_sub Template:**
```bash
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/#" -v
```

---

## Table of Contents

1. [System Control Commands](#1-system-control-commands)
2. [Westminster Chimes Commands](#2-westminster-chimes-commands-phase-23-)
3. [OTA Update Commands](#3-ota-update-commands-phase-24-)
4. [Transition System](#4-transition-system)
5. [Brightness Control](#5-brightness-control)
6. [Brightness Configuration](#6-brightness-configuration)
7. [Monitoring & Status](#7-monitoring--status)
8. [Home Assistant Integration](#8-home-assistant-integration)

---

## 1. System Control Commands

### Get System Status
```bash
Topic: home/Clock_Stani_1/command
Payload: status
```

### Restart Device
```bash
Topic: home/Clock_Stani_1/command
Payload: restart
```
**Note:** Triggers clean reboot with health checks for OTA validation

### Reset WiFi Credentials
```bash
Topic: home/Clock_Stani_1/command
Payload: reset_wifi
```
**Warning:** Device will reboot and start in AP mode (`ESP32-LED-Config`)

### Set RTC Time (Manual Override)
```bash
Topic: home/Clock_Stani_1/command
Payload: set_time 09:05
```
**Format:** `set_time HH:MM` (24-hour format)
**Examples:**
- `set_time 08:50` → Test "ES IST ZEHN VOR NEUN"
- `set_time 09:00` → Test "ES IST NEUN UHR"
- `set_time 14:30` → Test "ES IST HALB DREI"
- `set_time 21:37` → Test "ES IST FÜNF NACH HALB ZEHN" + 2 minute indicators

### Refresh Sensor Values
```bash
Topic: home/Clock_Stani_1/command
Payload: refresh_sensors
```
**Updates:** Light sensor (BH1750), potentiometer (ADC), current brightness levels

---

## 2. Westminster Chimes Commands (Phase 2.3 ✅)

### Enable/Disable Chimes

**Enable Westminster Chimes:**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_enable
```
**State Topic:** `home/Clock_Stani_1/chimes/status` → `{"enabled": true}`

**Disable Westminster Chimes:**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_disable
```
**State Topic:** `home/Clock_Stani_1/chimes/status` → `{"enabled": false}`

### Volume Control

**Set Chime Volume (0-100%):**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_volume_75
```
**Format:** `chimes_volume_N` where N = 0-100
**Examples:**
- `chimes_volume_50` → 50% volume (moderate)
- `chimes_volume_80` → 80% volume (default)
- `chimes_volume_25` → 25% volume (quiet)
- `chimes_volume_100` → 100% volume (maximum)

**State Topic:** `home/Clock_Stani_1/chimes/volume` → `{"volume": 75}`

### Test Chimes

**Test Single Strike:**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_test_strike
```
**Plays:** Single Westminster strike (tests audio output)

### Quiet Hours

**Disable Quiet Hours (24/7 Chimes):**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_set_quiet_hours_off
```
**Effect:** Chimes play at all hours

**Set Default Quiet Hours (22:00-07:00):**
```bash
Topic: home/Clock_Stani_1/command
Payload: chimes_set_quiet_hours_default
```
**Effect:** Chimes silent from 10 PM to 7 AM

### Chime State Topics (Read-Only)

**Subscribe to Chime Status:**
```bash
Topic: home/Clock_Stani_1/chimes/status
Payload: {"enabled": true/false}
```

**Subscribe to Chime Volume:**
```bash
Topic: home/Clock_Stani_1/chimes/volume
Payload: {"volume": 0-100}
```

**Chime Schedule:**
- **:00** - Full Westminster sequence (4 quarters + hour strikes)
- **:15** - First quarter chime
- **:30** - Two quarters (half hour)
- **:45** - Three quarters

---

## 3. OTA Update Commands (Phase 2.4 ✅)

### Check for Updates

**Manual Update Check:**
```bash
Topic: home/Clock_Stani_1/command
Payload: ota_check_update
```
**Response:** Checks GitHub releases for newer firmware version
**Status:** Publishes `ota_update_available` or `ota_up_to_date`

### Start OTA Update

**Start Firmware Update (Auto-Reboot):**
```bash
Topic: home/Clock_Stani_1/command
Payload: ota_start_update
```
**Process:**
1. Downloads firmware from GitHub releases (latest)
2. Verifies SHA256 checksum
3. Flashes to alternate OTA partition
4. Automatically reboots to new firmware
5. Validates health checks on first boot
6. Marks firmware valid if all checks pass

**Cancel Ongoing Update:**
```bash
Topic: home/Clock_Stani_1/command
Payload: ota_cancel_update
```

### Get OTA Information

**Get Current Progress:**
```bash
Topic: home/Clock_Stani_1/command
Payload: ota_get_progress
```
**Response Topic:** `home/Clock_Stani_1/ota/progress`

**Get Version Information:**
```bash
Topic: home/Clock_Stani_1/command
Payload: ota_get_version
```
**Response Topic:** `home/Clock_Stani_1/ota/version`

### OTA State Topics (Read-Only)

**Subscribe to OTA Progress:**
```bash
Topic: home/Clock_Stani_1/ota/progress
```
**Payload Example:**
```json
{
  "state": "DOWNLOADING",
  "progress_percent": 45,
  "bytes_downloaded": 589824,
  "total_bytes": 1310720,
  "time_remaining_ms": 12500
}
```

**States:** `IDLE`, `CHECKING`, `DOWNLOADING`, `VERIFYING`, `FLASHING`, `COMPLETE`, `FAILED`

**Subscribe to OTA Version:**
```bash
Topic: home/Clock_Stani_1/ota/version
```
**Payload Example:**
```json
{
  "current_version": "2.6.3",
  "available_version": "2.7.0",
  "update_available": true,
  "running_partition": "ota_0",
  "boot_count": 1
}
```

**OTA Firmware Source:**
- **GitHub Release:** https://github.com/stani56/Stanis_Clock/releases/latest
- **Binary:** `wordclock.bin` (1.3MB)
- **Metadata:** `version.json` (SHA256 checksum + changelog)

---

## 4. Transition System

### Enable/Disable Transitions

**Enable Smooth Transitions (1.5 seconds):**
```bash
Topic: home/Clock_Stani_1/transition/set
Payload: {"duration": 1500, "enabled": true, "fadein_curve": "ease_out", "fadeout_curve": "ease_in"}
```

**Disable Transitions (Instant Mode):**
```bash
Topic: home/Clock_Stani_1/transition/set
Payload: {"enabled": false}
```

**Fast Transitions (500ms):**
```bash
Topic: home/Clock_Stani_1/transition/set
Payload: {"duration": 500, "enabled": true, "fadein_curve": "linear", "fadeout_curve": "linear"}
```

**Slow Dramatic Transitions (3 seconds):**
```bash
Topic: home/Clock_Stani_1/transition/set
Payload: {"duration": 3000, "enabled": true, "fadein_curve": "ease_in_out", "fadeout_curve": "ease_in_out"}
```

### Transition Test Commands

**Start 5-Minute Transition Demo:**
```bash
Topic: home/Clock_Stani_1/command
Payload: test_transitions_start
```
**Behavior:** Cycles through 5-minute time intervals to demonstrate smooth LED transitions

**Stop Transition Test:**
```bash
Topic: home/Clock_Stani_1/command
Payload: test_transitions_stop
```

**Check Transition Test Status:**
```bash
Topic: home/Clock_Stani_1/command
Payload: test_transitions_status
```

### Animation Curves

**Available Curves:**
- `"linear"` - Constant speed throughout
- `"ease_in"` - Slow start, fast finish
- `"ease_out"` - Fast start, slow finish *(recommended for fade-in)*
- `"ease_in_out"` - Slow start and finish
- `"bounce"` - Slight overshoot with settle

**Typical Configuration:**
- **Fade-in:** `ease_out` (snappy start, gentle settle)
- **Fade-out:** `ease_in` (smooth departure)
- **Duration:** 1500ms (1.5 seconds) - balanced elegance

---

## 5. Brightness Control

### Direct Brightness Setting

**Set Individual and Global Brightness:**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"individual": 60, "global": 180}
```

**Individual Brightness Only (User Control):**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"individual": 80}
```
**Range:** 5-255 (constrained by safety limit, default max: 80)
**Source:** Potentiometer input (GPIO 3, ADC1_CH2)

**Global Brightness Only (Room Lighting):**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"global": 200}
```
**Range:** 5-255
**Source:** BH1750 light sensor (I2C, adaptive 5-zone mapping)

### Common Brightness Presets

**Night Mode (Minimal Brightness):**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"individual": 20, "global": 50}
```

**Day Mode (Maximum Brightness):**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"individual": 80, "global": 255}
```
**Note:** Individual capped at 80 by default safety limit

**Ambient Auto (Sensor-Driven):**
```bash
Topic: home/Clock_Stani_1/brightness/set
Payload: {"individual": 60, "global": 180}
```
**Effective Brightness:** (60 × 180) ÷ 255 = 42 PWM

### Brightness Calculation

**Formula:** `Effective_PWM = (Individual × Global) ÷ 255`
**TLC59116 PWM:** Individual brightness per LED (PWM0-PWM15 registers)
**TLC59116 GRPPWM:** Global group PWM applied to all LEDs

---

## 6. Brightness Configuration

### Light Sensor Configuration

**Configure 5-Zone Adaptive Brightness:**
```bash
Topic: home/Clock_Stani_1/brightness/config/set
Payload: {
  "light_sensor": {
    "very_dark": {
      "lux_min": 1.0,
      "lux_max": 10.0,
      "brightness_min": 5,
      "brightness_max": 30
    },
    "dim": {
      "lux_min": 10.0,
      "lux_max": 50.0,
      "brightness_min": 30,
      "brightness_max": 60
    },
    "normal": {
      "lux_min": 50.0,
      "lux_max": 200.0,
      "brightness_min": 60,
      "brightness_max": 120
    },
    "bright": {
      "lux_min": 200.0,
      "lux_max": 500.0,
      "brightness_min": 120,
      "brightness_max": 200
    },
    "very_bright": {
      "lux_min": 500.0,
      "lux_max": 1500.0,
      "brightness_min": 200,
      "brightness_max": 255
    }
  }
}
```

**BH1750 Light Sensor Details:**
- **I2C Address:** 0x23
- **Resolution:** 0.5-1 lux
- **Range:** 1-65535 lux
- **Update Rate:** 10 Hz (light sensor task)

### Potentiometer Configuration

**Configure Potentiometer Mapping:**
```bash
Topic: home/Clock_Stani_1/brightness/config/set
Payload: {
  "potentiometer": {
    "brightness_min": 5,
    "brightness_max": 200,
    "curve_type": "linear",
    "safety_limit_max": 80
  }
}
```

**Curve Types:**
- `"linear"` - Direct 1:1 mapping (default)
- `"logarithmic"` - More control in lower brightness range
- `"exponential"` - More control in higher brightness range

**Safety Limit:**
```bash
Topic: home/Clock_Stani_1/brightness/config/set
Payload: {
  "potentiometer": {
    "safety_limit_max": 120
  }
}
```
**Range:** 20-255 PWM units
**Default:** 80 PWM (conservative, prevents LED overheating)
**Purpose:** Maximum safe PWM value regardless of other settings

### Reset Configuration

**Reset All Brightness Configs to Factory Defaults:**
```bash
Topic: home/Clock_Stani_1/brightness/config/reset
Payload: reset
```
**Effect:** Restores all 5-zone ranges, potentiometer settings, and safety limits

**Get Current Configuration:**
```bash
Topic: home/Clock_Stani_1/brightness/config/get
Payload: {}
```
**Response Topic:** `home/Clock_Stani_1/brightness/config/status`

---

## 7. Monitoring & Status

### Subscribe to All Topics

**Monitor Everything:**
```bash
Topic: home/Clock_Stani_1/#
```

### Core Status Topics

**System Status:**
```bash
Topic: home/Clock_Stani_1/status
```
**Payloads:** `heartbeat`, `chimes_enabled`, `ota_complete`, `transition_updated`, etc.

**Availability:**
```bash
Topic: home/Clock_Stani_1/availability
```
**Payloads:** `online` (Last Will: `offline`)

**WiFi Status:**
```bash
Topic: home/Clock_Stani_1/wifi
```
**Payload Example:** `{"state": "connected", "ssid": "MyNetwork", "rssi": -65}`

**NTP Sync Status:**
```bash
Topic: home/Clock_Stani_1/ntp
```
**Payload Example:** `{"synced": true, "timezone": "Europe/Vienna", "time": "2025-11-09T14:30:00"}`

### Brightness Status

**Current Brightness Levels:**
```bash
Topic: home/Clock_Stani_1/brightness/status
```
**Payload Example:**
```json
{
  "individual": 60,
  "global": 180,
  "effective_pwm": 42,
  "light_sensor_lux": 125.5,
  "potentiometer_raw": 2048,
  "zone": "normal"
}
```

### Transition Status

**Current Transition Settings:**
```bash
Topic: home/Clock_Stani_1/transition/status
```
**Payload Example:**
```json
{
  "enabled": true,
  "duration": 1500,
  "fadein_curve": "ease_out",
  "fadeout_curve": "ease_in",
  "active_transitions": 12
}
```

### LED Validation Status (Phase 2.0)

**Validation Results:**
```bash
Topic: home/Clock_Stani_1/validation/status
```
**Payload Example:**
```json
{
  "result": "passed",
  "timestamp": "2025-11-09T14:30:00Z",
  "execution_time_ms": 131,
  "health_score": 98
}
```

**Detailed Validation:**
```bash
Topic: home/Clock_Stani_1/validation/last_result
```
**Published:** After every display change (~5 minutes)

### Error Logging (Phase 2.0)

**Query Error Log:**
```bash
Topic: home/Clock_Stani_1/error_log/query
Payload: {"count": 10}
```
**Response Topic:** `home/Clock_Stani_1/error_log/response`

**Error Log Statistics:**
```bash
Topic: home/Clock_Stani_1/error_log/stats
```
**Auto-published:** On MQTT connection

**Clear Error Log:**
```bash
Topic: home/Clock_Stani_1/error_log/clear
Payload: clear
```

---

## 8. Home Assistant Integration

### MQTT Discovery

The device automatically publishes Home Assistant MQTT Discovery configurations for **39 entities** on connection:

**Control Entities:**
- **Light:** `light.clock_stani_1` (main display control)
- **Switch:** `switch.clock_stani_1_chimes_enabled` (Westminster chimes)
- **Number:** `number.clock_stani_1_chime_volume` (0-100%)
- **24 Config Numbers:** Zone brightness, curves, safety limits

**Sensor Entities:**
- **WiFi/NTP Status:** Connection states
- **Brightness Sensors:** Individual, global, light sensor, potentiometer
- **Chime Status:** Enabled/disabled state
- **OTA Status:** Current/available versions
- **Validation Health:** LED validation score

**Button Entities:**
- **Restart:** Trigger clean reboot
- **Refresh Sensors:** Update sensor readings
- **Test Transitions:** Start transition demo
- **Set Time:** Manual RTC time override

### Discovery Topic Pattern

**Discovery Topics:**
```
homeassistant/{component_type}/Clock_Stani_1_{MAC}/entity_name/config
```

**Example Discovery Topics:**
- `homeassistant/switch/Clock_Stani_1_8466A8/chimes_enabled/config`
- `homeassistant/number/Clock_Stani_1_8466A8/chime_volume/config`
- `homeassistant/sensor/Clock_Stani_1_8466A8/wifi_status/config`

**Device Information:**
```json
{
  "identifiers": ["Clock_Stani_1_8466A8"],
  "name": "German Word Clock",
  "model": "YB-ESP32-S3-AMP Word Clock",
  "manufacturer": "Custom Build",
  "sw_version": "2.6.3"
}
```

---

## Parameter Ranges & Limits

**Transition Duration:** 200-5000 milliseconds (default: 1500ms)
**Individual Brightness:** 5-255 (constrained by safety limit)
**Global Brightness:** 5-255
**Safety PWM Limit:** 20-255 (default: 80)
**Light Sensor Range:** 0.0-2000.0 lux (BH1750)
**Potentiometer ADC:** 0-4095 (12-bit ADC1_CH2)
**Chime Volume:** 0-100% (I2S audio output)

---

## Expected Response Messages

**Common Status Payloads:**
- `"transition_test_started"` - Test mode activated
- `"individual_brightness_updated"` - Brightness changed
- `"transition_duration_updated"` - Duration changed
- `"transitions_enabled"` / `"transitions_disabled"` - Mode changed
- `"chimes_enabled"` / `"chimes_disabled"` - Chime state changed
- `"chimes_volume_set"` - Volume updated
- `"ota_update_available"` - Newer firmware version found
- `"ota_up_to_date"` - Already running latest version
- `"ota_update_started"` - Firmware download started
- `"ota_update_complete"` - Update successful, rebooting
- `"ota_cancelled"` - Update cancelled successfully
- `"time_set_successfully"` - RTC time updated
- `"heartbeat"` - Periodic status (every 60 seconds)

**Error Responses:**
- `"invalid_time_format"` - Time command format error
- `"chimes_enable_failed"` - Chime enable error
- `"ota_check_failed"` - Failed to check for updates
- `"ota_update_failed"` - OTA download/flash error
- `"ota_cancel_failed"` - Cannot cancel at this stage
- `"ota_already_running"` - Update already in progress
- `"transition_duration_out_of_range"` - Invalid duration value

---

## Quick Command Reference

**Restart:** `restart`
**Enable Chimes:** `chimes_enable`
**Disable Chimes:** `chimes_disable`
**Volume 75%:** `chimes_volume_75`
**Test Strike:** `chimes_test_strike`
**OTA Check:** `ota_check_update`
**OTA Update:** `ota_start_update`
**OTA Cancel:** `ota_cancel_update`
**Get OTA Progress:** `ota_get_progress`
**Get OTA Version:** `ota_get_version`
**Start Transitions:** `test_transitions_start`
**Stop Transitions:** `test_transitions_stop`
**Refresh Sensors:** `refresh_sensors`
**Set Time:** `set_time HH:MM`

---

## Troubleshooting Common Issues

### Getting `unknown_command` Response?

**CRITICAL: Command Format Rules**
- ✅ **All commands use underscores (_), NOT spaces**
- ✅ **Commands are case-sensitive** (all lowercase)
- ✅ **Exact spelling required**

**Common Mistakes:**

| Wrong ❌ | Correct ✅ | Issue |
|---------|-----------|-------|
| `ota_get version` | `ota_get_version` | Space instead of underscore |
| `ota get version` | `ota_get_version` | Multiple spaces |
| `OTA_GET_VERSION` | `ota_get_version` | Uppercase (case-sensitive!) |
| `chimes volume 75` | `chimes_volume_75` | Space instead of underscore |
| `set time 14:30` | `set_time 14:30` | Missing underscore in command |

**Debug Steps:**
1. **Verify command spelling** - Check Quick Command Reference above
2. **Check for spaces** - Replace all spaces with underscores (except parameters like time)
3. **Use lowercase** - Commands are case-sensitive
4. **Monitor MQTT traffic:**
   ```bash
   mosquitto_sub -h broker.hivemq.com -p 8883 \
     --cafile /etc/ssl/certs/ca-certificates.crt \
     -u StaniWirdWild -P ClockWirdWild \
     -t "home/Clock_Stani_1/#" -v
   ```
5. **Check serial logs** - Look for: `"Unknown command: 'YOUR_COMMAND'"`

**Example of Correct Usage:**
```bash
# ✅ CORRECT - with underscore
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "ota_get_version"

# ❌ WRONG - with space
mosquitto_pub ... -m "ota_get version"  # Returns unknown_command
```

---

## Related Documentation

- **[MQTT Command Audit](../../developer/mqtt-command-audit.md)** - Complete command list verified against code
- [MQTT System Architecture](../../implementation/mqtt/mqtt-system-architecture.md)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/)
- [Phase 2.3 Westminster Chimes](../../implementation/phase-2.3-ha-chime-controls.md)
- [Phase 2.4 OTA Manager](../../../components/ota_manager/README.md)
- [LED Validation Guide](../led-validation-guide.md)
- [Error Logging Guide](../error-logging-guide.md)

---

**Last Updated:** November 2025
**Firmware Version:** v2.6.3+
**Platform:** YB-ESP32-S3-AMP (ESP32-S3-WROOM-1-N8R2)
