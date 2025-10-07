# 👥 User Documentation

Welcome to the ESP32 German Word Clock! This section contains everything you need to set up, configure, and use your word clock.

## 🚀 Quick Start Guide

### 1. Hardware Setup
- [Hardware Setup Guide](hardware-setup.md) - Physical connections and assembly
- Component list and wiring diagrams
- Power requirements and safety

### 2. Initial Configuration
- Flash the ESP32 with the latest firmware
- Connect to WiFi using the configuration portal
- Verify basic functionality

### 3. Smart Home Integration
- [Home Assistant Setup](home-assistant-setup.md) - Complete HA integration guide
- [MQTT Commands](mqtt-commands.md) - Control via MQTT
- [MQTT Discovery Reference](mqtt-discovery-reference.md) - Quick reference

## 📚 User Guides

| Guide | Description |
|-------|-------------|
| [Hardware Setup](hardware-setup.md) | Physical assembly and connections |
| [Home Assistant Setup](home-assistant-setup.md) | Smart home integration |
| [MQTT Commands](mqtt-commands.md) | Remote control commands |
| [MQTT Discovery Reference](mqtt-discovery-reference.md) | Entity reference |
| [Troubleshooting](troubleshooting.md) | Common issues and solutions |

## ⚡ Quick Setup (5 Minutes)

1. **Power on** the ESP32 word clock
2. **Connect to WiFi**: Join "ESP32-LED-Config" network (password: 12345678)
3. **Configure**: Open http://192.168.4.1 and enter your WiFi credentials
4. **Verify**: Check that the clock displays correct time in German

## 🏠 Home Assistant Integration

Your word clock automatically appears in Home Assistant with **37 entities**:

### Core Controls (12 entities)
- **Main Light**: Word Clock Display with brightness control
- **4 Sensors**: WiFi status, NTP status, LED brightness, display brightness
- **3 Controls**: Transitions, duration, brightness
- **2 Selects**: Fade curves
- **2 Buttons**: Restart device, test transitions

### Advanced Brightness (24 entities)
- **Light Sensor Zones**: 5 zones × 4 settings = 20 entities
- **Potentiometer Config**: 3 configuration entities
- **Safety Limit**: 1 configurable safety entity

## 📱 MQTT Commands

Control your word clock remotely:

```bash
# Basic commands
status                    # Get system status
restart                   # Restart device
reset_wifi               # Clear WiFi credentials

# Time control
set_time 14:30           # Set time for testing
force_ntp_sync           # Force NTP synchronization

# Brightness control
{"individual": 50, "global": 180}  # Set brightness levels

# Transition control
test_transitions_start   # Test smooth transitions
test_transitions_stop    # Stop transition test
```

## 🔍 Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| Clock not connecting to WiFi | Use reset button to clear credentials |
| Wrong time displayed | Check NTP sync status in Home Assistant |
| LEDs too dim/bright | Adjust brightness in Home Assistant |
| Home Assistant not discovering | Check MQTT connection status |

### Status Indicators

- **GPIO 21 LED**: WiFi connection status
- **GPIO 22 LED**: NTP synchronization status
- **Word Display**: Current time in German
- **Home Assistant**: Complete system status

## 📊 Features Overview

### ✅ Core Features
- German time display with proper grammar
- Automatic WiFi connection and reconnection
- NTP time synchronization (Vienna timezone)
- Real-time clock backup (DS3231)
- Dual brightness control

### ✅ Smart Home Features
- Complete Home Assistant integration
- 37 auto-discovered entities
- MQTT remote control
- Real-time status monitoring
- Professional device organization

### ✅ Advanced Features
- Smooth LED transitions between time changes
- Configurable light sensor zones
- Potentiometer response customization
- Comprehensive testing framework
- Professional security (TLS encryption)

## 🆘 Need More Help?

- **Technical Issues**: See [Troubleshooting](troubleshooting.md)
- **MQTT Problems**: Check [MQTT Commands](mqtt-commands.md)
- **HA Integration**: Review [Home Assistant Setup](home-assistant-setup.md)
- **Hardware Issues**: Consult [Hardware Setup](hardware-setup.md)

---
🎯 **Target**: End users and smart home enthusiasts  
📝 **Level**: Beginner to intermediate  
⏱️ **Setup Time**: 5-15 minutes