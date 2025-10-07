# ESP32 German Word Clock - Complete MQTT User Guide

## Overview

The ESP32 German Word Clock features a comprehensive MQTT integration that enables remote control, monitoring, and automation through Home Assistant or any MQTT client. This guide covers all available MQTT functionality.

## Table of Contents

1. [Basic MQTT Configuration](#basic-mqtt-configuration)
2. [Topic Structure](#topic-structure)
3. [Device Control Commands](#device-control-commands)
4. [Sensor Data & Monitoring](#sensor-data--monitoring)
5. [Brightness Control System](#brightness-control-system)
6. [Transition Effects](#transition-effects)
7. [Home Assistant Integration](#home-assistant-integration)
8. [Advanced Features](#advanced-features)
9. [Troubleshooting](#troubleshooting)
10. [Factory Defaults and NVS Configuration](#factory-defaults-and-nvs-configuration)
11. [Command Reference Quick Guide](#command-reference-quick-guide)

---

## Basic MQTT Configuration

### Connection Details
- **Broker:** HiveMQ Cloud (secure TLS connection)
- **Port:** 8883 (TLS encrypted)
- **Authentication:** Username/password based
- **Certificate:** ESP32 built-in certificate bundle validation

### Device Identification
- **Client ID Format:** `[DEVICE_NAME]_XXXXXX` (where XXXXXX = MAC address)
- **Topic Base:** `home/[DEVICE_NAME]/`
- **Device Name:** Configurable in `mqtt_manager.h` (default: `esp32_core`)

### Multi-Device Support
For multiple word clocks on the same network:
```c
// Edit: components/mqtt_manager/include/mqtt_manager.h
#define MQTT_DEVICE_NAME "wordclock_bedroom"    // Device 1
#define MQTT_DEVICE_NAME "wordclock_kitchen"    // Device 2
#define MQTT_DEVICE_NAME "wordclock_living_room" // Device 3
```

Each device gets its own topic namespace:
- Device 1: `home/wordclock_bedroom/*`
- Device 2: `home/wordclock_kitchen/*`
- Device 3: `home/wordclock_living_room/*`

---

## Topic Structure

All MQTT topics follow a consistent pattern: `home/[DEVICE_NAME]/[category]/[subcategory]`

### Core Topics
```
home/[DEVICE_NAME]/
├── status                    # Device status messages
├── availability             # Online/offline status
├── command                  # Remote command input
├── wifi                     # WiFi connection status
├── ntp                      # NTP synchronization status
├── ntp/last_sync           # Last NTP sync timestamp
├── sensors/status          # Current sensor readings
├── brightness/
│   ├── set                 # Brightness control
│   ├── status              # Current brightness values
│   └── config/
│       ├── set             # Advanced brightness configuration
│       ├── status          # Current brightness configuration
│       ├── get             # Request current configuration
│       └── reset           # Reset to factory defaults
└── transition/
    ├── set                 # Transition control
    └── status              # Current transition settings
```

---

## Device Control Commands

Send commands to: `home/[DEVICE_NAME]/command`

### Basic Device Control

#### System Commands
```bash
# Restart the device
Topic: home/[DEVICE_NAME]/command
Payload: restart

# Get current device status
Topic: home/[DEVICE_NAME]/command
Payload: status

# Reset WiFi credentials and restart
Topic: home/[DEVICE_NAME]/command
Payload: reset_wifi
```

#### Time Management
```bash
# Set specific time for testing (format: HH:MM)
Topic: home/[DEVICE_NAME]/command
Payload: set_time 14:30

# Force NTP synchronization
Topic: home/[DEVICE_NAME]/command
Payload: sync_time
```

#### Data Refresh
```bash
# Refresh all sensor readings immediately
Topic: home/[DEVICE_NAME]/command
Payload: refresh_sensors

# Re-announce device to Home Assistant
Topic: home/[DEVICE_NAME]/command
Payload: republish_discovery
```

### Transition Control Commands

#### Transition Testing
```bash
# Start automated transition test (cycles through different times)
Topic: home/[DEVICE_NAME]/command
Payload: test_transitions_start

# Stop transition test
Topic: home/[DEVICE_NAME]/command
Payload: test_transitions_stop

# Check transition test status
Topic: home/[DEVICE_NAME]/command
Payload: test_transitions_status
```

---

## Sensor Data & Monitoring

### Real-time Sensor Data
Subscribe to: `home/[DEVICE_NAME]/sensors/status`

**Sample JSON payload:**
```json
{
  "light_level_lux": 45.2,
  "potentiometer_raw": 2890,
  "potentiometer_voltage_mv": 2847,
  "potentiometer_percentage": 86.3,
  "current_pwm": 75,
  "current_grppwm": 180
}
```

### Status Topics

#### Device Availability
```bash
# Subscribe to device online/offline status
Topic: home/[DEVICE_NAME]/availability
Payloads: "online" | "offline"
```

#### Network Status
```bash
# WiFi connection status
Topic: home/[DEVICE_NAME]/wifi
Payloads: "connected" | "disconnected"

# NTP synchronization status  
Topic: home/[DEVICE_NAME]/ntp
Payloads: "synced" | "not_synced"

# Last NTP sync timestamp (ISO format)
Topic: home/[DEVICE_NAME]/ntp/last_sync
Payload: "2025-01-15T10:30:45Z"
```

#### General Status Messages
```bash
# Subscribe to status messages
Topic: home/[DEVICE_NAME]/status
Examples:
- "system_started"
- "wifi_connected" 
- "ntp_synchronized"
- "sensor_values_refreshed"
- "transition_test_active"
```

---

## Brightness Control System

The word clock features a sophisticated dual brightness control system with extensive MQTT configuration options.

### Basic Brightness Control

#### Simple Brightness Adjustment
```bash
# Set global brightness (0-255)
Topic: home/[DEVICE_NAME]/brightness/set
Payload: {"global": 180}

# Set individual LED brightness (0-255) 
Topic: home/[DEVICE_NAME]/brightness/set
Payload: {"individual": 60}

# Set both simultaneously
Topic: home/[DEVICE_NAME]/brightness/set
Payload: {"global": 180, "individual": 60}
```

#### Current Brightness Status
```bash
# Subscribe to brightness updates
Topic: home/[DEVICE_NAME]/brightness/status
Payload: {"individual": 60, "global": 180}
```

### Advanced Brightness Configuration

#### Light Sensor Zone Calibration
Configure 5 ambient light zones for automatic brightness adaptation:

```bash
# Configure Very Dark Zone (0.1-100 lux, brightness 5-30)
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "light_sensor": {
    "very_dark": {
      "lux_min": 0.1,
      "lux_max": 100.0,
      "brightness_min": 5,
      "brightness_max": 30
    }
  }
}

# Configure Dim Zone (10-200 lux, brightness 30-60)
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "light_sensor": {
    "dim": {
      "lux_min": 10.0,
      "lux_max": 200.0,
      "brightness_min": 30,
      "brightness_max": 60
    }
  }
}

# Configure Normal Zone (50-500 lux, brightness 60-120)
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "light_sensor": {
    "normal": {
      "lux_min": 50.0,
      "lux_max": 500.0,
      "brightness_min": 60,
      "brightness_max": 120
    }
  }
}

# Configure Bright Zone (200-1000 lux, brightness 120-200)
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "light_sensor": {
    "bright": {
      "lux_min": 200.0,
      "lux_max": 1000.0,
      "brightness_min": 120,
      "brightness_max": 200
    }
  }
}

# Configure Very Bright Zone (500-2000 lux, brightness 200-255)
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "light_sensor": {
    "very_bright": {
      "lux_min": 500.0,
      "lux_max": 2000.0,
      "brightness_min": 200,
      "brightness_max": 255
    }
  }
}
```

#### Potentiometer Configuration
```bash
# Configure potentiometer response
Topic: home/[DEVICE_NAME]/brightness/config/set
Payload: {
  "potentiometer": {
    "brightness_min": 5,
    "brightness_max": 80,
    "safety_limit_max": 80,
    "curve_type": "logarithmic"
  }
}

# Supported curve types: "linear", "logarithmic", "exponential"
```

#### Configuration Management
```bash
# Get current brightness configuration
Topic: home/[DEVICE_NAME]/brightness/config/get
Payload: "get"

# Subscribe to configuration updates
Topic: home/[DEVICE_NAME]/brightness/config/status
Sample Response: {
  "light_sensor": {
    "very_dark": {"lux_min": 0.1, "lux_max": 100.0, "brightness_min": 5, "brightness_max": 30},
    "dim": {"lux_min": 10.0, "lux_max": 200.0, "brightness_min": 30, "brightness_max": 60},
    "normal": {"lux_min": 50.0, "lux_max": 500.0, "brightness_min": 60, "brightness_max": 120},
    "bright": {"lux_min": 200.0, "lux_max": 1000.0, "brightness_min": 120, "brightness_max": 200},
    "very_bright": {"lux_min": 500.0, "lux_max": 2000.0, "brightness_min": 200, "brightness_max": 255}
  },
  "potentiometer": {
    "brightness_min": 5,
    "brightness_max": 80,
    "safety_limit_max": 80,
    "curve_type": "logarithmic"
  }
}

# Reset to factory defaults
Topic: home/[DEVICE_NAME]/brightness/config/reset
Payload: "reset"
```

---

## Transition Effects

Control smooth LED animations for professional visual effects.

### Basic Transition Control

#### Enable/Disable Transitions
```bash
# Enable smooth transitions
Topic: home/[DEVICE_NAME]/transition/set
Payload: {"enabled": true}

# Disable transitions (instant updates)
Topic: home/[DEVICE_NAME]/transition/set
Payload: {"enabled": false}
```

#### Transition Duration
```bash
# Set transition duration (200-5000ms)
Topic: home/[DEVICE_NAME]/transition/set
Payload: {"duration_ms": 1500}
```

#### Animation Curves
```bash
# Set fade-in curve
Topic: home/[DEVICE_NAME]/transition/set
Payload: {"fadein_curve": "ease_out"}

# Set fade-out curve
Topic: home/[DEVICE_NAME]/transition/set
Payload: {"fadeout_curve": "ease_in"}

# Set both curves
Topic: home/[DEVICE_NAME]/transition/set
Payload: {
  "fadein_curve": "ease_in_out",
  "fadeout_curve": "ease_in_out"
}

# Available curves: "linear", "ease_in", "ease_out", "ease_in_out", "bounce"
```

#### Combined Transition Settings
```bash
# Configure all transition parameters
Topic: home/[DEVICE_NAME]/transition/set
Payload: {
  "enabled": true,
  "duration_ms": 2000,
  "fadein_curve": "ease_out",
  "fadeout_curve": "ease_in"
}
```

### Transition Status
```bash
# Subscribe to transition status updates
Topic: home/[DEVICE_NAME]/transition/status
Sample Response: {
  "enabled": true,
  "duration_ms": 1500,
  "fadein_curve": "ease_out",
  "fadeout_curve": "ease_in"
}
```

---

## Home Assistant Integration

### Automatic Discovery

The device automatically announces itself to Home Assistant via MQTT Discovery, creating 39 entities:

#### Core Controls (16 entities)
- **1 Light:** Main word clock display with brightness and effects
- **7 Sensors:** WiFi status, NTP status, light level, LED brightness, display brightness, potentiometer reading, potentiometer voltage
- **3 Switches/Controls:** Smooth transitions, transition duration, brightness control
- **2 Selects:** Fade-in curve, fade-out curve selection
- **5 Buttons:** Restart, test transitions, refresh sensors, set time, reset brightness config

#### Advanced Brightness Configuration (23 entities)
- **20 Numbers:** Light sensor zone calibration (4 values × 5 zones)
- **3 Controls:** Potentiometer configuration (min, max, curve type)

### Home Assistant Device Card

In Home Assistant, the device appears as "Word Clock [device_name]" with organized entity groups:

**Core Controls Section:**
- Word Clock Display (light entity with brightness control)
- System sensors and status indicators
- Quick action buttons

**Animation Controls Section:**
- Transition enable/disable
- Duration control (0.2-5 seconds)
- Curve selection dropdowns

**Advanced Brightness Section:**
- 5 zones of light sensor calibration
- Potentiometer response configuration
- Safety limits and curve types

### Home Assistant Automations

#### Room-Based Automation Examples

**Bedroom Word Clock - Night Mode:**
```yaml
automation:
  - alias: "Bedroom Word Clock - Night Mode"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "home/wordclock_bedroom/brightness/config/set"
          payload: >
            {
              "light_sensor": {
                "very_dark": {"brightness_min": 1, "brightness_max": 10},
                "dim": {"brightness_min": 5, "brightness_max": 15}
              }
            }
```

**Kitchen Word Clock - Cooking Mode:**
```yaml
automation:
  - alias: "Kitchen Word Clock - Cooking Brightness"
    trigger:
      - platform: state
        entity_id: light.kitchen_main
        to: "on"
    action:
      - service: mqtt.publish
        data:
          topic: "home/wordclock_kitchen/brightness/set"
          payload: '{"global": 255}'
```

**Living Room Word Clock - Movie Mode:**
```yaml
automation:
  - alias: "Living Room Word Clock - Movie Mode"
    trigger:
      - platform: state
        entity_id: media_player.living_room_tv
        to: "playing"
    action:
      - service: mqtt.publish
        data:
          topic: "home/wordclock_living_room/brightness/set"
          payload: '{"global": 50}'
      - service: mqtt.publish
        data:
          topic: "home/wordclock_living_room/transition/set"
          payload: '{"enabled": true, "duration_ms": 3000}'
```

---

## Advanced Features

### Message Persistence and Reliability

The MQTT system includes advanced reliability features:

#### Quality of Service (QoS)
- **QoS 0:** Status messages, sensor readings (fire-and-forget)
- **QoS 1:** Command messages, configuration changes (delivery confirmation)
- **Retained Messages:** Availability status, configuration state

#### Automatic Reconnection
- Automatic MQTT broker reconnection with exponential backoff
- Message queuing during disconnection periods
- State synchronization on reconnection

### Security Features

#### TLS Encryption
- All MQTT communication uses TLS 1.2+ encryption
- ESP32 built-in certificate bundle validation
- Secure connection to HiveMQ Cloud broker

#### Authentication
- Username/password authentication
- Unique client IDs prevent connection conflicts
- Secure credential storage in ESP32 NVS flash

### Performance Optimizations

#### Efficient Publishing
- Batch sensor updates to minimize bandwidth
- Differential LED updates (95% I2C operation reduction)
- Smart change detection to avoid unnecessary updates

#### Memory Management
- Static memory allocation for reliability
- JSON payload optimization
- Configurable message retention policies

---

## Troubleshooting

### Common Issues and Solutions

#### MQTT Connection Issues

**Problem:** Device not connecting to MQTT broker
```bash
# Check WiFi first
Topic: home/[DEVICE_NAME]/wifi
Expected: "connected"

# Check MQTT status
Topic: home/[DEVICE_NAME]/status
Look for: "mqtt_connected" or "mqtt_connection_failed"

# Solution: Restart device
Topic: home/[DEVICE_NAME]/command
Payload: restart
```

**Problem:** Home Assistant controls are greyed out
1. Delete old device entries in HA
2. Restart Home Assistant
3. Send republish command:
   ```bash
   Topic: home/[DEVICE_NAME]/command
   Payload: republish_discovery
   ```

#### Brightness Control Issues

**Problem:** Brightness not responding
```bash
# Reset brightness configuration
Topic: home/[DEVICE_NAME]/brightness/config/reset
Payload: "reset"

# Check current readings
Topic: home/[DEVICE_NAME]/command
Payload: refresh_sensors

# Verify configuration
Topic: home/[DEVICE_NAME]/brightness/config/get
Payload: "get"
```

**Problem:** Light sensor not working
```bash
# Check sensor readings in status
Topic: home/[DEVICE_NAME]/sensors/status
Look for: "light_level_lux": [value]

# If value is 0 or unchanging, hardware issue
# If value is valid but brightness not changing, check zone configuration
```

#### Transition Issues

**Problem:** Transitions not smooth or not working
```bash
# Verify transitions are enabled
Topic: home/[DEVICE_NAME]/transition/status
Expected: {"enabled": true}

# Reset transition settings
Topic: home/[DEVICE_NAME]/transition/set
Payload: {
  "enabled": true,
  "duration_ms": 1500,
  "fadein_curve": "ease_out",
  "fadeout_curve": "ease_in"
}

# Test transitions
Topic: home/[DEVICE_NAME]/command
Payload: test_transitions_start
```

### Debug Information

#### Enabling Verbose Logging
Monitor these topics for detailed system information:
- `home/[DEVICE_NAME]/status` - System status messages
- `home/[DEVICE_NAME]/sensors/status` - Real-time sensor data
- Console output via serial monitor for hardware debugging

#### System Health Checks
```bash
# Get complete system status
Topic: home/[DEVICE_NAME]/command
Payload: status

# Check all sensor readings
Topic: home/[DEVICE_NAME]/command  
Payload: refresh_sensors

# Verify network connectivity
Topic: home/[DEVICE_NAME]/wifi
Topic: home/[DEVICE_NAME]/ntp
Topic: home/[DEVICE_NAME]/availability
```

---

## Factory Defaults and NVS Configuration

### 1. **WiFi Configuration**
**Namespace:** `wifi_config`

| NVS Key | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `ssid` | *None* (empty string) | string[32] | WiFi network SSID |
| `password` | *None* (empty string) | string[64] | WiFi network password |

### 2. **MQTT Configuration**  
**Namespace:** `mqtt_config`

| NVS Key | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `broker_uri` | `"mqtts://5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud:8883"` | string[128] | MQTT broker URI |
| `username` | `"esp32_led_device"` | string[32] | MQTT username |
| `password` | `"tufcux-3xuwda-Vomnys"` | string[64] | MQTT password |
| `client_id` | `"{MQTT_DEVICE_NAME}_{MAC}"` (e.g., `"Clock_Stani_1_5A1E74"`) | string[32] | MQTT client ID |
| `use_ssl` | `true` | bool | Enable TLS encryption |
| `port` | `8883` | uint16 | MQTT port number |

### 3. **Brightness Configuration**
**Namespace:** `brightness`

| NVS Key | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `version` | `1` | uint32 | Configuration version |
| `global_minimum_brightness` | `3` | uint8 | Safety minimum brightness |

#### Light Sensor Zones (stored as blob)
**Key:** `light_sensor`

| Zone | Lux Min | Lux Max | Brightness Min | Brightness Max |
|------|---------|---------|----------------|----------------|
| **very_dark** | 1.0 | 10.0 | 5 | 30 |
| **dim** | 10.0 | 50.0 | 30 | 60 |
| **normal** | 50.0 | 200.0 | 60 | 120 |
| **bright** | 200.0 | 500.0 | 120 | 200 |
| **very_bright** | 500.0 | 1500.0 | 200 | 255 |

#### Potentiometer Settings (stored as blob)
**Key:** `potentiometer`

| Setting | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `brightness_min` | `5` | uint8 | Minimum potentiometer brightness |
| `brightness_max` | `255` | uint8 | Maximum potentiometer brightness |
| `curve_type` | `0` (LINEAR) | enum | Response curve (0=Linear, 1=Logarithmic, 2=Exponential) |
| `safety_limit_max` | `80` | uint8 | Maximum safe PWM value |

### 4. **Transition Configuration**
**Namespace:** `transition_config`

| NVS Key | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `version` | `1` | uint32 | Configuration version |

**Key:** `config` (stored as blob)

| Setting | Factory Default | Type | Description |
|---------|-----------------|------|-------------|
| `duration_ms` | `1500` | uint16 | Transition duration in milliseconds |
| `enabled` | `true` | bool | Enable smooth transitions |
| `fadein_curve` | `1` (EASE_IN) | enum | Fade-in animation curve |
| `fadeout_curve` | `2` (EASE_OUT) | enum | Fade-out animation curve |

### Animation Curve Values:
- `0` = LINEAR
- `1` = EASE_IN
- `2` = EASE_OUT
- `3` = EASE_IN_OUT
- `4` = BOUNCE

### Other System Defaults (Not in NVS)

| Setting | Default Value | Location | Description |
|---------|--------------|----------|-------------|
| `MQTT_DEVICE_NAME` | `"Clock_Stani_1"` | mqtt_manager.h | Device identifier for MQTT topics |
| Initial PWM | `60` | wordclock_brightness.c | Startup individual LED brightness |
| Initial GRPPWM | `180` | wordclock_brightness.c | Startup global brightness |
| WiFi AP SSID | `"ESP32-LED-Config"` | wifi_manager.c | Access point name for configuration |
| WiFi AP Password | `"12345678"` | wifi_manager.c | Access point password |
| Light Sensor Minimum | `5` | light_sensor.h | Minimum global brightness from sensor |
| Potentiometer Update | `15` seconds | wordclock_brightness.c | Potentiometer check interval |
| Light Sensor Update | `100` ms | wordclock_brightness.c | Light sensor check interval |

### Factory Reset Commands

To restore factory defaults:

```bash
# Reset brightness configuration only
Topic: home/[DEVICE_NAME]/brightness/config/reset
Payload: "reset"

# Clear WiFi credentials (requires device restart)
Topic: home/[DEVICE_NAME]/command
Payload: reset_wifi

# Physical reset: Long press GPIO 5 button for 3+ seconds
```

### NVS Storage Limits

| Namespace | Maximum Keys | Maximum Size |
|-----------|--------------|--------------|
| wifi_config | 2 | ~96 bytes |
| mqtt_config | 6 | ~300 bytes |
| brightness | 3 | ~200 bytes |
| transition_config | 2 | ~20 bytes |

**Total NVS Usage:** Approximately 616 bytes out of available NVS partition size (typically 16KB-24KB).

---

## Command Reference Quick Guide

### Essential Commands
```bash
# Device Control
restart                    # Restart device
status                     # Get system status  
reset_wifi                 # Reset WiFi and restart
refresh_sensors            # Update all sensor readings

# Time Management
set_time HH:MM            # Set specific time
sync_time                 # Force NTP sync

# Transitions
test_transitions_start    # Start transition test
test_transitions_stop     # Stop transition test

# Home Assistant
republish_discovery       # Re-announce to HA
```

### Essential Topics
```bash
# Status Monitoring
home/[DEVICE_NAME]/availability    # Online/offline
home/[DEVICE_NAME]/status         # Status messages
home/[DEVICE_NAME]/sensors/status # Current readings

# Control
home/[DEVICE_NAME]/command               # Send commands
home/[DEVICE_NAME]/brightness/set        # Basic brightness
home/[DEVICE_NAME]/transition/set        # Transition control
home/[DEVICE_NAME]/brightness/config/set # Advanced brightness
```

This comprehensive MQTT system transforms the ESP32 German Word Clock into a fully-featured IoT device with professional-grade remote control, monitoring, and automation capabilities suitable for integration into any smart home system.