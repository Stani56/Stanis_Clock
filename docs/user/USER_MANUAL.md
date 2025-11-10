# ESP32-S3 German Word Clock - Complete User Manual

**Version:** 2.6.3+
**Last Updated:** November 2025
**Platform:** YB-ESP32-S3-AMP / ESP32-S3-DevKitC-1
**Status:** Production Ready

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Getting Started](#2-getting-started)
3. [Initial Setup](#3-initial-setup)
4. [Home Assistant Integration](#4-home-assistant-integration)
5. [Westminster Chimes](#5-westminster-chimes)
6. [MQTT Control](#6-mqtt-control)
7. [OTA Updates](#7-ota-updates)
8. [Brightness Control](#8-brightness-control)
9. [LED Transitions](#9-led-transitions)
10. [Time Management](#10-time-management)
11. [Troubleshooting](#11-troubleshooting)
12. [Advanced Features](#12-advanced-features)
13. [Maintenance](#13-maintenance)
14. [Technical Specifications](#14-technical-specifications)

---

## 1. Introduction

### What is the German Word Clock?

The German Word Clock is an elegant LED matrix display that shows the current time using illuminated German words instead of numbers. It combines traditional craftsmanship with modern IoT technology to create a unique timepiece that's both functional and artistic.

**Key Features:**
- ✅ **German Word Display:** Shows time in natural German phrases
- ✅ **160 LEDs:** 10×16 LED matrix with smooth transitions
- ✅ **Home Assistant Integration:** 39+ entities for complete control
- ✅ **Westminster Chimes:** Authentic hourly and quarter-hour chimes
- ✅ **MQTT over TLS:** Secure cloud connectivity via HiveMQ
- ✅ **OTA Updates:** Wireless firmware updates from GitHub
- ✅ **Adaptive Brightness:** Automatic adjustment based on ambient light
- ✅ **Manual Controls:** Potentiometer for instant brightness adjustment

### How It Works

The clock uses a 10-row × 16-column LED matrix to illuminate specific German words that form time phrases:

**Examples:**
- **2:00 PM** → "ES IST ZWEI UHR"
- **2:23 PM** → "ES IST ZWANZIG NACH ZWEI" + 3 minute indicator LEDs
- **2:30 PM** → "ES IST HALB DREI" (half past two = half to three in German)
- **2:45 PM** → "ES IST VIERTEL VOR DREI"

**Minute Indicators:** Four corner LEDs show 1-4 minutes past the 5-minute mark.

### Hardware Platforms

**YB-ESP32-S3-AMP (Default):**
- Integrated audio amplifiers (stereo)
- Integrated microSD card slot
- 8MB Flash, 2MB PSRAM
- Compact design with onboard peripherals

**ESP32-S3-DevKitC-1 (Legacy):**
- External audio amplifiers
- External microSD card module
- 16MB Flash, 8MB PSRAM
- Modular design for prototyping

---

## 2. Getting Started

### What You'll Need

**Hardware:**
- ✅ German Word Clock (assembled)
- ✅ USB-C power supply (5V, minimum 2A recommended)
- ✅ WiFi network (2.4GHz, WPA2)
- ✅ Smartphone or computer for initial setup
- ⚙️ MicroSD card (optional, for Westminster chimes)

**Software/Services:**
- ✅ Home Assistant (optional, for advanced control)
- ✅ MQTT client (optional, for direct MQTT control)
- ✅ Web browser (for WiFi configuration)

### First Power-On

**What to Expect:**

1. **Boot Sequence (5-10 seconds):**
   - ESP32 initializes hardware
   - Tests LED matrix (brief flash)
   - Starts WiFi in Access Point mode

2. **AP Mode Indicator:**
   - WiFi indicator LED blinks rapidly
   - Device creates WiFi network: `ESP32-LED-Config`
   - Password: `12345678`

3. **Initial Display:**
   - Shows "ES IST" (it is)
   - Displays approximate time (RTC not yet synced)
   - LEDs at default brightness (medium)

### Safety First

⚠️ **Important Safety Information:**

- **Power:** Use only 5V USB-C power supply
- **Heat:** Device may become warm during operation (normal)
- **Brightness:** Default safety limit prevents eye strain (80/255 PWM)
- **Reset Button:** GPIO 0 boot button (hold 5 seconds to reset WiFi)

---

## 3. Initial Setup

### Step 1: Connect to Configuration WiFi

1. **Find the Network:**
   - WiFi SSID: `ESP32-LED-Config`
   - Password: `12345678`

2. **Connect Your Device:**
   - Open WiFi settings on smartphone/computer
   - Select `ESP32-LED-Config`
   - Enter password: `12345678`
   - Wait for connection (10-30 seconds)

3. **Access Configuration Portal:**
   - Open web browser
   - Navigate to: `http://192.168.4.1`
   - Configuration page should load automatically

### Step 2: Configure WiFi Credentials

**Configuration Portal Interface:**

```
┌─────────────────────────────────────┐
│  ESP32 LED Matrix Clock Setup       │
├─────────────────────────────────────┤
│                                     │
│  Your WiFi SSID:                    │
│  [________________]                 │
│                                     │
│  WiFi Password:                     │
│  [________________] (show/hide)     │
│                                     │
│  [Save & Connect]                   │
│                                     │
└─────────────────────────────────────┘
```

1. **Enter WiFi SSID:** Your home network name
2. **Enter Password:** Your WiFi password
3. **Click "Save & Connect"**
4. **Wait for Connection:** Device will restart (10-20 seconds)

### Step 3: Verify Connection

**Success Indicators:**

1. **WiFi Status LED:**
   - ✅ **Solid ON:** Connected to WiFi
   - ❌ **Blinking:** Not connected (retry setup)

2. **NTP Status LED:**
   - ✅ **Solid ON:** Time synchronized
   - ❌ **Blinking:** Syncing in progress

3. **Display:**
   - Shows current time in German
   - LEDs transition smoothly
   - Brightness adjusts to room lighting

**If Connection Fails:**
- Device returns to AP mode after 30 seconds
- WiFi indicator blinks rapidly
- Repeat Step 1-2 with correct credentials

### Step 4: Reset WiFi (If Needed)

**Reset Button Long Press:**

1. **Locate Reset Button:** GPIO 0 (usually labeled "BOOT")
2. **Press and Hold:** 5+ seconds
3. **Wait for Confirmation:**
   - Log message: "WiFi credentials cleared"
   - Device restarts in AP mode
4. **Repeat Setup:** Follow Steps 1-3

---

## 4. Home Assistant Integration

### Overview

The word clock automatically integrates with Home Assistant via MQTT Discovery, creating **39+ entities** for complete control and monitoring.

### Prerequisites

**Required:**
- ✅ Home Assistant installation (any version)
- ✅ MQTT Broker addon installed (Mosquitto recommended)
- ✅ MQTT Integration enabled in Home Assistant

**MQTT Broker Configuration:**
- Broker: `broker.hivemq.com`
- Port: `8883` (MQTT over TLS)
- Username: `StaniWirdWild`
- Password: `ClockWirdWild`

### Automatic Discovery

**How It Works:**

1. **Clock Connects to MQTT:** Within 5-10 seconds of boot
2. **Publishes Discovery Configs:** 39 Home Assistant discovery messages
3. **Entities Appear Automatically:** Check Devices & Services

**Find Your Clock in HA:**

1. Navigate to: **Settings → Devices & Services → MQTT**
2. Look for device: **"German Word Clock"** or **"Clock_Stani_1"**
3. Click device name to see all entities

### Entity Overview (39 Entities)

#### Control Entities

**Light Entity:**
- `light.clock_stani_1` - Main display control
  - Turn on/off
  - Adjust brightness
  - Enable/disable transitions

**Switches:**
- `switch.clock_stani_1_chimes_enabled` - Westminster chimes on/off
- Additional configuration switches (advanced)

**Numbers (Sliders):**
- `number.clock_stani_1_chime_volume` - Chime volume (0-100%)
- 24 brightness configuration numbers (zone settings)

#### Sensor Entities

**Status Sensors:**
- `sensor.clock_stani_1_wifi_status` - WiFi connection state
- `sensor.clock_stani_1_ntp_status` - Time sync status
- `sensor.clock_stani_1_mqtt_status` - MQTT connection state

**Brightness Sensors:**
- `sensor.clock_stani_1_individual_brightness` - LED PWM level
- `sensor.clock_stani_1_global_brightness` - Overall brightness
- `sensor.clock_stani_1_light_sensor` - Ambient light (lux)
- `sensor.clock_stani_1_potentiometer` - Manual brightness knob

**Chime Sensors:**
- `sensor.clock_stani_1_chime_status` - "Enabled" or "Disabled"
- `sensor.clock_stani_1_chime_volume` - Current volume %

**System Sensors:**
- `sensor.clock_stani_1_validation_health` - LED health score
- `sensor.clock_stani_1_ota_version` - Firmware version info

#### Button Entities

**System Controls:**
- `button.clock_stani_1_restart` - Reboot device
- `button.clock_stani_1_refresh_sensors` - Update sensor readings
- `button.clock_stani_1_test_transitions` - Demo transition effects
- `button.clock_stani_1_set_time` - Manual time adjustment

### Using Home Assistant Dashboard

**Create a Basic Card:**

```yaml
type: entities
title: Word Clock Control
entities:
  - entity: light.clock_stani_1
    name: Display
  - entity: switch.clock_stani_1_chimes_enabled
    name: Westminster Chimes
  - entity: number.clock_stani_1_chime_volume
    name: Chime Volume
  - entity: sensor.clock_stani_1_light_sensor
    name: Room Light Level
  - entity: button.clock_stani_1_restart
    name: Restart Clock
```

**Advanced Dashboard Example:**

```yaml
type: vertical-stack
cards:
  - type: light
    entity: light.clock_stani_1
    name: Word Clock Display

  - type: entities
    title: Westminster Chimes
    entities:
      - switch.clock_stani_1_chimes_enabled
      - number.clock_stani_1_chime_volume
      - sensor.clock_stani_1_chime_status

  - type: sensor
    entity: sensor.clock_stani_1_light_sensor
    name: Ambient Light
    graph: line

  - type: entities
    title: System Status
    entities:
      - sensor.clock_stani_1_wifi_status
      - sensor.clock_stani_1_ntp_status
      - sensor.clock_stani_1_validation_health
```

### Home Assistant Automations

**Example 1: Bedtime Auto-Off**

```yaml
automation:
  - alias: "Word Clock: Bedtime"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: light.turn_off
        target:
          entity_id: light.clock_stani_1
      - service: switch.turn_off
        target:
          entity_id: switch.clock_stani_1_chimes_enabled
```

**Example 2: Morning Wake-Up**

```yaml
automation:
  - alias: "Word Clock: Morning"
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: light.turn_on
        target:
          entity_id: light.clock_stani_1
        data:
          brightness: 200
      - service: switch.turn_on
        target:
          entity_id: switch.clock_stani_1_chimes_enabled
```

**Example 3: Adaptive Brightness**

```yaml
automation:
  - alias: "Word Clock: Adaptive Brightness"
    trigger:
      - platform: state
        entity_id: sensor.clock_stani_1_light_sensor
    action:
      - choose:
          - conditions:
              - condition: numeric_state
                entity_id: sensor.clock_stani_1_light_sensor
                below: 10
            sequence:
              - service: light.turn_on
                target:
                  entity_id: light.clock_stani_1
                data:
                  brightness: 30
          - conditions:
              - condition: numeric_state
                entity_id: sensor.clock_stani_1_light_sensor
                above: 500
            sequence:
              - service: light.turn_on
                target:
                  entity_id: light.clock_stani_1
                data:
                  brightness: 255
```

---

## 5. Westminster Chimes

### Overview

The word clock features authentic Westminster chime sounds that play at quarter-hour intervals, providing audible time notifications throughout the day.

### Features

**Chime Schedule:**
- `:00` - Full Westminster sequence + hour strikes
- `:15` - First quarter chime
- `:30` - Half hour chime (two quarters)
- `:45` - Three-quarter chime

**Customization:**
- ✅ Enable/disable chimes
- ✅ Volume control (0-100%)
- ✅ Quiet hours (configurable start/end time)
- ✅ Test mode for previewing sounds

### Setting Up Chimes

**Requirements:**
- ✅ MicroSD card (formatted as FAT32)
- ✅ Westminster chime audio files (PCM format)
- ✅ SD card inserted before power-on

**Audio File Structure (8.3 Format - FAT32 Compatibility):**
```
/sdcard/
└── CHIMES/
    └── WESTMI~1/          (WESTMINSTER truncated to 8.3 format)
        ├── QUARTE~1.PCM   (QUARTER_PAST.PCM - First quarter :15)
        ├── HALF_P~1.PCM   (HALF_PAST.PCM - Half hour :30)
        ├── QUARTE~2.PCM   (QUARTER_TO.PCM - Three quarters :45)
        ├── HOUR.PCM       (Full hour sequence :00)
        └── STRIKE.PCM     (Hour strike sound)
```

**⚠️ CRITICAL: 8.3 Filename Format**
- FAT32 SD cards use DOS 8.3 filenames (8 chars + 3 extension)
- Long filenames are automatically truncated by Windows
- Use EXACT filenames shown above (with tilde ~ and numbers)

**Audio Format Specifications:**
- Sample Rate: 16 kHz
- Bit Depth: 16-bit
- Channels: Mono
- Encoding: PCM (uncompressed)

### Controlling Chimes via Home Assistant

**Enable/Disable:**
```yaml
service: switch.turn_on
target:
  entity_id: switch.clock_stani_1_chimes_enabled
```

**Set Volume:**
```yaml
service: number.set_value
target:
  entity_id: number.clock_stani_1_chime_volume
data:
  value: 75  # 75% volume
```

### Controlling Chimes via MQTT

**Enable Chimes:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "chimes_enable"
```

**Set Volume to 75%:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "chimes_volume_75"
```

**Test Single Strike:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "chimes_test_strike"
```

### Quiet Hours

**Set Quiet Hours (10 PM - 7 AM):**

Via MQTT (default quiet hours):
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "chimes_set_quiet_hours_default"
```

Disable quiet hours (24/7 chimes):
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "chimes_set_quiet_hours_off"
```

### Troubleshooting Chimes

**No Sound:**
1. ✅ Check SD card is inserted and formatted (FAT32)
2. ✅ Verify audio files present in `/sdcard/CHIMES/WESTMINSTER/`
3. ✅ Check chimes enabled: `switch.clock_stani_1_chimes_enabled`
4. ✅ Verify volume > 0: `number.clock_stani_1_chime_volume`
5. ✅ Test audio system: Send `test_audio` command

**Distorted Sound:**
- Lower volume to 50-70%
- Check power supply is adequate (2A minimum)
- Verify audio files are correct format (16kHz, 16-bit, mono)

---

## 6. MQTT Control

### MQTT Connection Details

**Broker Information:**
- **Host:** `broker.hivemq.com`
- **Port:** `8883` (MQTT over TLS)
- **Protocol:** `mqtts://`
- **Username:** `StaniWirdWild`
- **Password:** `ClockWirdWild`
- **Device Name:** `Clock_Stani_1` (customizable)

**Topic Structure:**
- Command topic: `home/Clock_Stani_1/command`
- Status topic: `home/Clock_Stani_1/status`
- State topics: `home/Clock_Stani_1/*/status`

### Command Format Rules

**CRITICAL: Command Syntax**
- ✅ All lowercase
- ✅ Use underscores (`_`), NOT spaces
- ✅ Exact spelling required

**Common Mistakes:**
| ❌ Wrong | ✅ Correct |
|----------|-----------|
| `ota_get version` | `ota_get_version` |
| `chimes volume 75` | `chimes_volume_75` |
| `OTA_CHECK_UPDATE` | `ota_check_update` |

### Essential Commands

#### System Commands

**Get Status:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "status"
```

**Restart Device:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "restart"
```

**Reset WiFi Credentials:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "reset_wifi"
```

#### Chime Commands

**Enable/Disable:**
```bash
# Enable
mosquitto_pub ... -m "chimes_enable"

# Disable
mosquitto_pub ... -m "chimes_disable"
```

**Set Volume:**
```bash
# Volume 0-100%
mosquitto_pub ... -m "chimes_volume_50"
mosquitto_pub ... -m "chimes_volume_100"
```

**Test Chimes:**
```bash
mosquitto_pub ... -m "chimes_test_strike"
```

#### Time Commands

**Set Manual Time:**
```bash
# Format: set_time HH:MM (24-hour)
mosquitto_pub ... -m "set_time 14:30"
mosquitto_pub ... -m "set_time 09:05"
```

**Force NTP Sync:**
```bash
mosquitto_pub ... -m "force_ntp_sync"
```

### Subscribe to Status Updates

**Monitor All Topics:**
```bash
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/#" -v
```

**Monitor Specific Topics:**
```bash
# Chime status
mosquitto_sub ... -t "home/Clock_Stani_1/chimes/status"

# OTA version
mosquitto_sub ... -t "home/Clock_Stani_1/ota/version"

# System status
mosquitto_sub ... -t "home/Clock_Stani_1/status"
```

### Complete Command Reference

See: [MQTT Commands Documentation](mqtt/mqtt-commands.md) for all 42 commands with examples.

---

## 7. OTA Updates

### Overview

Over-The-Air (OTA) updates allow you to wirelessly update the clock's firmware without physical access. Updates are downloaded securely from GitHub Releases.

### Automatic Update Check

The clock automatically checks for updates:
- On boot (after MQTT connects)
- Every 24 hours
- When manually triggered

### Manual Update Check

**Via Home Assistant:**
- Navigate to device page
- Look for `sensor.clock_stani_1_ota_version`
- Sensor shows current and available versions

**Via MQTT:**
```bash
# Check for updates
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "ota_check_update"

# Get version info
mosquitto_pub ... -m "ota_get_version"
```

**Response:**
```json
{
  "current_version": "v2.6.3-2-g1052452-dirty",
  "available_version": "2.6.3",
  "update_available": false
}
```

### Performing an Update

**⚠️ IMPORTANT: Read Before Updating**
- ✅ Ensure stable power supply
- ✅ Clock will restart automatically
- ✅ Update takes 2-5 minutes
- ✅ Do NOT power off during update
- ✅ Automatic rollback if update fails

**Start Update (MQTT):**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "ota_start_update"
```

### Update Process

**Stages:**

1. **Checking (5-10 seconds)**
   - Connects to GitHub releases
   - Downloads version.json
   - Compares versions

2. **Downloading (1-3 minutes)**
   - Downloads firmware binary (1.3MB)
   - Shows progress via MQTT
   - Verifies SHA256 checksum

3. **Flashing (30-60 seconds)**
   - Writes to alternate OTA partition
   - Verifies write success

4. **Rebooting (10 seconds)**
   - Switches to new partition
   - Runs health checks

5. **Validation (First Boot)**
   - Tests WiFi connection
   - Tests MQTT connection
   - Tests I2C devices
   - Tests RTC
   - Marks firmware valid if all pass

### Monitoring Update Progress

**Subscribe to Progress:**
```bash
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/ota/progress" -v
```

**Progress Updates:**
```json
{
  "state": "DOWNLOADING",
  "progress_percent": 45,
  "bytes_downloaded": 589824,
  "total_bytes": 1310720,
  "time_remaining_ms": 12500
}
```

**States:**
- `IDLE` - No update in progress
- `CHECKING` - Checking for updates
- `DOWNLOADING` - Downloading firmware
- `VERIFYING` - Verifying checksum
- `FLASHING` - Writing to flash
- `COMPLETE` - Update successful
- `FAILED` - Update failed (see logs)

### Cancelling an Update

**⚠️ WARNING:** Only cancel during download phase!

```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "ota_cancel_update"
```

### Automatic Rollback

**Safety Feature:**

If the new firmware fails health checks on first boot:
1. Device automatically reverts to previous firmware
2. Boots from old partition
3. Publishes rollback status via MQTT
4. Continues normal operation

**Health Checks:**
- WiFi connection test
- MQTT connection test
- I2C bus communication
- RTC time read
- LED controller verification

### Troubleshooting Updates

**Update Failed:**
1. Check internet connection
2. Verify MQTT connected
3. Ensure adequate free space
4. Check GitHub releases accessible
5. Review serial logs for errors

**Update Stuck:**
1. Wait 10 minutes (large download on slow connection)
2. If still stuck, restart device (long-press reset button)
3. Device will boot from previous firmware
4. Retry update

**Version Mismatch:**
- If "update available" shown incorrectly, it's a display bug
- Check `update_available` field in JSON response
- Should show `false` if versions match

---

## 8. Brightness Control

### Dual Brightness System

The clock uses TWO brightness controls that work together:

**1. Individual Brightness (LED PWM)**
- Controlled by: Potentiometer (manual knob)
- Range: 5-80 (safety limited)
- Response: Immediate (< 1 second)
- Purpose: Quick manual adjustment

**2. Global Brightness (Master Dimming)**
- Controlled by: Light sensor (automatic)
- Range: 5-255
- Response: 10 seconds (smooth transitions)
- Purpose: Adaptive ambient lighting

**Combined Formula:**
```
Final LED Brightness = (Individual × Global) ÷ 255
```

### Manual Brightness (Potentiometer)

**Hardware:**
- Analog potentiometer on GPIO 3
- Rotation range: 0-4095 (12-bit ADC)
- Mapped to: 5-80 PWM

**Usage:**
1. Turn knob clockwise → Brighter
2. Turn knob counter-clockwise → Dimmer
3. Changes apply within 15 seconds

**Limits:**
- Minimum: 5 (prevents complete darkness)
- Maximum: 80 (eye safety limit)
- Default: 60 (balanced brightness)

### Automatic Brightness (Light Sensor)

**Hardware:**
- BH1750 I2C light sensor
- Measurement range: 1-65535 lux
- Update rate: 10Hz (100ms intervals)

**Adaptive Zones:**

| Light Level | Range (lux) | Global Brightness | Use Case |
|-------------|-------------|-------------------|----------|
| Very Dark | 1-10 | 5-30 | Night (bedroom) |
| Dim | 10-50 | 30-60 | Evening (living room) |
| Normal | 50-200 | 60-120 | Day (indirect light) |
| Bright | 200-500 | 120-200 | Day (window light) |
| Very Bright | 500-1500 | 200-255 | Day (direct sunlight) |

**Transition Smoothing:**
- Changes apply gradually over 10 seconds
- Prevents sudden brightness jumps
- Configurable transition duration

### Brightness Configuration (Advanced)

**Via MQTT Topics:**

Each zone has two configurable parameters:

1. **Threshold:** Light level at zone boundary (lux)
2. **Brightness:** Target brightness for that zone (5-255)

**Example: Configure "Normal" Zone**
```json
Topic: home/Clock_Stani_1/brightness/config/set
Payload: {
  "light_sensor": {
    "normal": {
      "threshold": 100,
      "brightness": 100
    }
  }
}
```

**Reset to Factory Defaults:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/brightness/config/reset" -m "reset"
```

### Home Assistant Brightness Control

**Via Light Entity:**
```yaml
service: light.turn_on
target:
  entity_id: light.clock_stani_1
data:
  brightness: 200  # 0-255
```

**Via Configuration Numbers:**
- Navigate to device: Clock_Stani_1
- Find brightness configuration entities
- Adjust sliders for each zone

### Brightness Monitoring

**Current Brightness:**
```bash
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/brightness/status" -v
```

**Sensor Values:**
```bash
# Light sensor (lux)
mosquitto_sub ... -t "home/Clock_Stani_1/sensors/light_sensor"

# Potentiometer (0-4095)
mosquitto_sub ... -t "home/Clock_Stani_1/sensors/potentiometer"
```

---

## 9. LED Transitions

### Overview

LED transitions provide smooth fade effects when the time changes, creating elegant animations instead of instant on/off switching.

### Transition Settings

**Duration:** 200-5000 milliseconds (default: 1500ms)
**Curves:** ease_in, ease_out, ease_in_out, linear

**Enable/Disable:**
```json
Topic: home/Clock_Stani_1/transition/set
Payload: {
  "enabled": true,
  "duration": 1500,
  "fadein_curve": "ease_out",
  "fadeout_curve": "ease_in"
}
```

### Transition Curves

**ease_out (Recommended for Fade-In):**
- Fast start, slow end
- Natural light-up effect
- Smooth arrival

**ease_in (Recommended for Fade-Out):**
- Slow start, fast end
- Gentle disappearance
- Clean removal

**ease_in_out:**
- Slow start and end
- Smooth throughout
- Symmetric animation

**linear:**
- Constant speed
- Mechanical feel
- Fastest update rate (20Hz)

### Test Mode

**5-Minute Transition Demo:**
```bash
# Start test mode (cycles every 5 minutes)
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "test_transitions_start"

# Stop test mode
mosquitto_pub ... -m "test_transitions_stop"

# Check status
mosquitto_pub ... -m "test_transitions_status"
```

### Performance

**Update Rate:** 20Hz (50ms per frame)
**Buffer Size:** 32 simultaneous transitions
**I2C Optimization:** Differential updates (only changed LEDs)

### Instant Mode

**Disable Transitions:**
```json
Topic: home/Clock_Stani_1/transition/set
Payload: {"enabled": false}
```

**Use Cases:**
- Low latency required
- Testing LED matrix
- Troubleshooting display issues

---

## 10. Time Management

### Time Sources

**Priority Order:**
1. **NTP (Network Time Protocol)** - Primary source
2. **DS3231 RTC (Real-Time Clock)** - Backup when offline
3. **Manual Override** - Emergency fallback

### NTP Synchronization

**Automatic Sync:**
- On boot (after WiFi connects)
- Every 1 hour
- After WiFi reconnection

**Time Zone:**
- **Europe/Vienna** (CET/CEST)
- Automatic DST (Daylight Saving Time) adjustment

**Manual NTP Sync:**
```bash
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "force_ntp_sync"
```

### DS3231 RTC

**Hardware:**
- I2C Real-Time Clock module
- Battery backup (CR2032)
- Accuracy: ±2 minutes/year
- Temperature-compensated crystal

**Battery Maintenance:**
- Replace every 5-7 years
- Clock continues during power loss
- Battery low detection (via I2C)

### Manual Time Setting

**Set Time via MQTT:**
```bash
# Format: set_time HH:MM (24-hour)
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/command" -m "set_time 14:30"
```

**Examples:**
```bash
set_time 09:00  # 9:00 AM
set_time 14:30  # 2:30 PM
set_time 23:59  # 11:59 PM
```

**⚠️ NOTE:** Manual time sets DS3231 RTC only. Next NTP sync will overwrite.

### Time Display Format

**German Time Phrases:**

| Time | Display |
|------|---------|
| 1:00 | ES IST EIN UHR |
| 1:05 | ES IST FÜNF NACH EINS |
| 1:15 | ES IST VIERTEL NACH EINS |
| 1:30 | ES IST HALB ZWEI |
| 1:45 | ES IST VIERTEL VOR ZWEI |
| 2:00 | ES IST ZWEI UHR |

**Minute Indicators:**
- 4 corner LEDs show 1-4 minutes past the 5-minute mark
- Example: 1:23 = "ES IST ZWANZIG NACH EINS" + 3 LEDs

### Troubleshooting Time

**Clock Shows Wrong Time:**
1. Check WiFi connected: `sensor.clock_stani_1_wifi_status`
2. Check NTP synced: `sensor.clock_stani_1_ntp_status`
3. Force NTP sync: `force_ntp_sync` command
4. Verify time zone (Europe/Vienna = CET/CEST)

**Time Drifts After Power Loss:**
1. Check DS3231 battery (CR2032)
2. Replace battery if voltage < 2.5V
3. Re-sync NTP after battery replacement

---

## 11. Troubleshooting

### Common Issues

#### No Display / Black Screen

**Possible Causes:**
1. Brightness set too low
2. All LEDs off (test mode?)
3. Power supply insufficient
4. LED controllers not responding

**Solutions:**
1. Turn potentiometer clockwise (increase brightness)
2. Send `status` command via MQTT
3. Check power supply: 5V, minimum 2A
4. Restart device: Long-press reset button
5. Check serial logs for I2C errors

#### WiFi Won't Connect

**Symptoms:**
- WiFi LED blinking rapidly
- Returns to AP mode repeatedly
- Can't find home network

**Solutions:**
1. Check WiFi SSID and password correct
2. Verify 2.4GHz network (not 5GHz)
3. Move closer to WiFi router
4. Reset WiFi: Long-press reset button (5 seconds)
5. Check router allows new devices (MAC filter?)

#### MQTT Not Connecting

**Symptoms:**
- WiFi connected but no MQTT
- Home Assistant entities missing
- Commands don't work

**Solutions:**
1. Check HiveMQ broker accessible: `ping broker.hivemq.com`
2. Verify MQTT credentials in code
3. Check firewall allows port 8883
4. Restart device
5. Check serial logs for TLS errors

#### Chimes Not Playing

**Symptoms:**
- No sound at quarter hours
- Chimes enabled but silent

**Solutions:**
1. Check SD card inserted (FAT32 formatted)
2. Verify audio files present: `/sdcard/CHIMES/WESTMINSTER/`
3. Check volume > 0: `number.clock_stani_1_chime_volume`
4. Test audio: Send `test_audio` command
5. Check not in quiet hours period

#### Brightness Too Dark/Bright

**Symptoms:**
- Display always too dim
- Can't see in daylight
- Too bright at night

**Solutions:**
1. Adjust potentiometer (manual control)
2. Check light sensor working: Read `sensor.clock_stani_1_light_sensor`
3. Reset brightness config: Send `brightness/config/reset`
4. Verify safety limit not too restrictive
5. Check for obstructions covering light sensor

### Diagnostic Commands

**System Status:**
```bash
mosquitto_pub ... -m "status"
```

**Refresh All Sensors:**
```bash
mosquitto_pub ... -m "refresh_sensors"
```

**Test Transitions:**
```bash
mosquitto_pub ... -m "test_transitions_start"
```

**Test Audio:**
```bash
mosquitto_pub ... -m "test_audio"
```

### Serial Monitor Debugging

**Connect USB:**
1. Connect USB-C cable to computer
2. Open serial monitor: 115200 baud
3. Restart device
4. Observe boot logs

**ESP-IDF Monitor:**
```bash
idf.py -p /dev/ttyACM0 monitor
```

**Look For:**
- WiFi connection success/failure
- MQTT connection status
- I2C communication errors
- NTP sync results
- OTA update progress

### Factory Reset

**Complete Device Reset:**

1. **Reset WiFi Credentials:**
   - Long-press GPIO 0 button (5+ seconds)
   - Device restarts in AP mode

2. **Clear All Settings (via MQTT):**
   ```bash
   # Reset brightness config
   mosquitto_pub ... -t "home/Clock_Stani_1/brightness/config/reset" -m "reset"

   # Disable chimes
   mosquitto_pub ... -m "chimes_disable"

   # Reset transitions to defaults
   mosquitto_pub ... -t "home/Clock_Stani_1/transition/set" \
     -m '{"enabled":true,"duration":1500}'
   ```

3. **Flash Firmware (Complete Wipe):**
   ```bash
   idf.py -p /dev/ttyACM0 erase-flash flash
   ```

---

## 12. Advanced Features

### LED Validation System

**Automatic Health Checks:**
- Runs after every display transition (~5 minutes)
- Validates software state matches hardware PWM values
- Detects stuck LEDs, incorrect states
- Publishes health score to Home Assistant

**Health Score:**
```bash
sensor.clock_stani_1_validation_health: 95%
```

**Manual Validation Trigger:**
- Automatically runs during transitions
- Results published to MQTT

**Validation Topics:**
```bash
home/Clock_Stani_1/validation/status
home/Clock_Stani_1/validation/last_result
home/Clock_Stani_1/validation/statistics
```

### Error Logging

**Persistent Error Storage:**
- 50-entry circular buffer
- Survives reboots (stored in NVS)
- Severity levels: INFO, WARNING, ERROR, CRITICAL
- Source tracking: LED_VALIDATION, I2C_BUS, WIFI, MQTT, etc.

**Query Error Log:**
```bash
# Request last 10 errors
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P ClockWirdWild \
  -t "home/Clock_Stani_1/error_log/query" -m '{"count": 10}'
```

**Clear Error Log:**
```bash
mosquitto_pub ... -t "home/Clock_Stani_1/error_log/clear" -m "clear"
```

### Multi-Device Setup

**Multiple Clocks:**

1. **Edit Device Name:**
   - File: `components/mqtt_manager/include/mqtt_manager.h`
   - Change: `#define MQTT_DEVICE_NAME "Clock_Stani_1"`
   - To: `#define MQTT_DEVICE_NAME "Clock_Bedroom"`

2. **Rebuild Firmware:**
   ```bash
   idf.py build flash
   ```

3. **Each Clock Gets:**
   - Unique MQTT topics
   - Separate Home Assistant device
   - Independent control

**Example Setup:**
- Clock_Stani_1 (Living Room)
- Clock_Bedroom
- Clock_Kitchen

### Custom Chime Sounds

**Create Custom Chimes:**

1. **Prepare Audio Files:**
   - Format: 16kHz, 16-bit, mono, PCM
   - Tools: Audacity, FFmpeg

2. **Convert to PCM:**
   ```bash
   ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le output.pcm
   ```

3. **Upload to SD Card (8.3 Format):**
   ```
   /sdcard/CHIMES/CUSTOM/
   ├── QUARTE~1.PCM  (QUARTER_PAST.PCM)
   ├── HALF_P~1.PCM  (HALF_PAST.PCM)
   ├── QUARTE~2.PCM  (QUARTER_TO.PCM)
   ├── HOUR.PCM
   └── STRIKE.PCM
   ```
   **Note:** Use 8.3 filenames (DOS format) for FAT32 compatibility

4. **Update Code (Advanced):**
   - Modify `chime_manager.c`
   - Change path to `/sdcard/CHIMES/CUSTOM/`
   - Rebuild and flash

---

## 13. Maintenance

### Regular Maintenance

**Weekly:**
- ✅ Check display brightness comfortable
- ✅ Verify time accuracy
- ✅ Listen for chime quality

**Monthly:**
- ✅ Dust LED matrix (soft brush/compressed air)
- ✅ Check WiFi signal strength
- ✅ Review error logs
- ✅ Test brightness sensors

**Yearly:**
- ✅ Check firmware version (update if available)
- ✅ Inspect power cable for damage
- ✅ Clean potentiometer contacts
- ✅ Verify all LEDs functioning
- ✅ Test audio amplifiers

### Battery Replacement (DS3231)

**When to Replace:**
- Every 5-7 years
- After prolonged power loss
- If time drifts significantly

**Replacement Procedure:**
1. Power off device
2. Locate DS3231 module
3. Remove old CR2032 battery
4. Insert new CR2032 (positive side up)
5. Power on device
6. Allow NTP to sync time

**⚠️ NOTE:** Replacing battery will NOT erase settings

### Cleaning

**LED Matrix:**
- Use soft brush or compressed air
- Avoid liquids on electronics
- Wipe gently with microfiber cloth

**Enclosure:**
- Wipe with slightly damp cloth
- Avoid harsh chemicals
- Dry thoroughly before power on

**Sensors:**
- Light sensor: Clean lens with soft cloth
- Potentiometer: Contact cleaner spray (rarely needed)

### Backup Configuration

**Settings Stored in NVS:**
- WiFi credentials
- MQTT device name
- Brightness configuration
- Chime settings
- Error log history

**Export Configuration (Manual):**
1. Note current brightness settings
2. Screenshot Home Assistant entities
3. Document custom MQTT commands used
4. Save chime audio files separately

**Restore Configuration:**
- Re-enter WiFi credentials
- Apply brightness config via MQTT
- Re-enable chimes
- Upload audio files to SD card

---

## 14. Technical Specifications

### Hardware Specifications

**ESP32-S3-WROOM-1-N8R2 (YB-AMP):**
- **CPU:** Dual-core Xtensa LX7, 240 MHz
- **Flash:** 8MB
- **PSRAM:** 2MB Quad SPI @ 80MHz
- **WiFi:** 802.11 b/g/n (2.4GHz only)
- **Bluetooth:** BLE 5.0 (not used)
- **GPIO:** 45 pins (30 available)
- **ADC:** 12-bit, 20 channels
- **I2C:** 2 controllers (100 kHz/400 kHz)
- **I2S:** 2 channels (audio output)
- **SPI:** 4 controllers (SD card)

**LED Matrix:**
- **Size:** 10 rows × 16 columns = 160 LEDs
- **Controllers:** 10× TLC59116 (16 channels each)
- **I2C Addresses:** 0x60-0x6A
- **PWM Resolution:** 8-bit (0-255)
- **Update Rate:** 20Hz (50ms)

**Sensors:**
- **DS3231:** I2C Real-Time Clock, ±2 ppm accuracy
- **BH1750:** I2C Light Sensor, 1-65535 lux range
- **Potentiometer:** 10kΩ analog, 12-bit ADC

**Audio:**
- **Amplifiers:** 2× MAX98357A (3W per channel)
- **Interface:** I2S digital audio
- **Sample Rate:** 16 kHz
- **Bit Depth:** 16-bit
- **Channels:** Stereo capable

**Storage:**
- **MicroSD:** SPI mode, FAT32 format
- **Capacity:** Up to 32GB supported

**Power:**
- **Input:** 5V USB-C
- **Current:** 500-1500mA (depending on brightness)
- **Peak:** 2A (all LEDs at maximum)

### Software Specifications

**Firmware:**
- **Version:** 2.6.3+
- **Framework:** ESP-IDF 5.4.2
- **Language:** C99
- **Build System:** CMake
- **Binary Size:** ~1.3MB

**Networking:**
- **WiFi:** WPA2-PSK
- **MQTT:** v3.1.1 over TLS 1.2
- **NTP:** SNTP client
- **Time Zone:** Europe/Vienna (CET/CEST)
- **mDNS:** Clock discovery (planned)

**Partitions:**
- **NVS:** 24KB (settings storage)
- **PHY:** 4KB (WiFi calibration)
- **Factory:** 2.5MB (main firmware)
- **OTA:** Dual partition support
- **Storage:** 1.5MB (FAT filesystem)

### Performance Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Boot Time | 5-10 seconds | Until WiFi connected |
| MQTT Connect | 2-5 seconds | After WiFi |
| NTP Sync | 1-3 seconds | After MQTT |
| LED Update | 12-20ms | Per transition frame |
| I2C Operations | 5-25 per update | Differential updates |
| Free Heap | 2.3MB | With PSRAM enabled |
| WiFi Reconnect | 5-15 seconds | After signal loss |
| OTA Update | 2-5 minutes | Download + flash |
| Chime Latency | <100ms | From trigger to sound |

### Compliance & Safety

**Standards:**
- ✅ FCC Part 15 (WiFi emissions)
- ✅ CE Marking (European conformity)
- ✅ RoHS Compliant (lead-free)

**Safety Features:**
- ✅ Brightness safety limit (80/255 PWM max)
- ✅ Thermal protection (ESP32 built-in)
- ✅ Overcurrent protection (USB-C)
- ✅ Automatic rollback (OTA failures)
- ✅ Watchdog timer (hang prevention)

**Environmental:**
- **Operating Temperature:** 0°C to 50°C
- **Storage Temperature:** -20°C to 60°C
- **Humidity:** 10% to 90% non-condensing

---

## Support & Resources

### Documentation

**User Guides:**
- [MQTT Commands](mqtt/mqtt-commands.md) - Complete MQTT reference
- [Error Logging](error-logging-guide.md) - Error log system
- [LED Validation](led-validation-guide.md) - LED health monitoring

**Developer Documentation:**
- [MQTT Command Audit](../developer/mqtt-command-audit.md) - Code-verified commands
- [MQTT Architecture](../implementation/mqtt/mqtt-system-architecture.md) - Technical details
- [CLAUDE.md](../../CLAUDE.md) - Developer reference

### Project Links

- **GitHub Repository:** https://github.com/stani56/Stanis_Clock
- **Issues:** https://github.com/stani56/Stanis_Clock/issues
- **Releases:** https://github.com/stani56/Stanis_Clock/releases
- **License:** MIT (free for personal and commercial use)

### Community

- **Home Assistant Forum:** Search for "ESP32 Word Clock"
- **ESP32 Forums:** https://esp32.com
- **MQTT Community:** https://mqtt.org/community

### Version History

**Current Version: 2.6.3**

**Major Milestones:**
- **v1.0:** ESP32-PICO-D4 baseline (archived)
- **v2.0:** ESP32-S3 migration
- **v2.1:** Audio system (I2S + MAX98357A)
- **v2.2:** SD card support (SPI)
- **v2.3:** Westminster chimes + Home Assistant controls
- **v2.4:** Multi-board support + OTA system
- **v2.6:** Version comparison fixes + MQTT documentation

---

## Glossary

**ADC** - Analog-to-Digital Converter (reads potentiometer)
**AP Mode** - Access Point mode (WiFi configuration)
**BH1750** - I2C light sensor chip
**CR2032** - 3V coin cell battery (DS3231 backup)
**DS3231** - I2C Real-Time Clock chip
**ESP-IDF** - ESP32 IoT Development Framework
**GPIO** - General Purpose Input/Output pin
**I2C** - Inter-Integrated Circuit (communication protocol)
**I2S** - Inter-IC Sound (digital audio interface)
**LED** - Light Emitting Diode
**lux** - Unit of illuminance (light measurement)
**MAX98357A** - I2S audio amplifier chip
**MQTT** - Message Queuing Telemetry Transport
**NTP** - Network Time Protocol
**NVS** - Non-Volatile Storage (flash memory)
**OTA** - Over-The-Air (wireless updates)
**PCM** - Pulse-Code Modulation (raw audio format)
**PSRAM** - Pseudo-Static RAM (external memory)
**PWM** - Pulse Width Modulation (LED dimming)
**RTC** - Real-Time Clock
**SD** - Secure Digital (memory card)
**semver** - Semantic Versioning (version numbering)
**SPI** - Serial Peripheral Interface
**SSID** - Service Set Identifier (WiFi network name)
**TLC59116** - I2C LED controller chip (16 channels)
**TLS** - Transport Layer Security (encryption)
**USB-C** - USB Type-C connector (power)
**WPA2** - WiFi Protected Access 2 (security)

---

**Document Version:** 1.0
**Last Updated:** November 10, 2025
**Firmware Version:** 2.6.3+
**Platform:** YB-ESP32-S3-AMP / ESP32-S3-DevKitC-1

**End of User Manual**
