# 🕐 ESP32 German Word Clock - Complete IoT Implementation

<div align="center">

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.4.2-blue.svg)](https://github.com/espressif/esp-idf)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![MQTT](https://img.shields.io/badge/MQTT-HiveMQ%20Cloud-green.svg)](https://www.hivemq.com/mqtt-cloud-broker/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Auto%20Discovery-orange.svg)](https://www.home-assistant.io/)
[![Build Status](https://github.com/stani56/wordclock/actions/workflows/build.yml/badge.svg)](https://github.com/stani56/wordclock/actions/workflows/build.yml)

**Professional IoT German Word Clock with complete Home Assistant integration, smooth LED transitions, and dual brightness control.**

[Features](#-features) • [Quick Start](#-quick-start) • [Documentation](docs/) • [Hardware](docs/user/hardware-setup.md) • [Home Assistant](docs/user/home-assistant-setup.md) • [Contributing](docs/developer/contributing.md)

</div>

---

## 🌟 Features

### 🕐 **German Word Clock Display**
- **Perfect German Grammar**: Displays time in natural German phrases ("ES IST HALB DREI")
- **160 LED Matrix**: 10×16 LED array with 11 columns for time words + 4 minute indicators
- **Smooth Transitions**: Professional 1.5-second crossfade animations between word changes
- **Real-Time Accuracy**: DS3231 RTC with NTP synchronization (Vienna timezone)

### 🏠 **Complete Home Assistant Integration**
- **36 Auto-Discovered Entities**: Zero manual configuration required
- **MQTT Discovery**: Automatically appears in Home Assistant with full device info
- **Comprehensive Controls**: 24 brightness settings, animation controls, system management
- **Real-Time Monitoring**: WiFi status, time sync, brightness levels, sensor readings

### 💡 **Advanced Dual Brightness System**
- **Potentiometer Control**: Manual individual LED brightness (user preference)
- **Light Sensor Adaptation**: Automatic global brightness based on ambient light (BH1750)
- **5-Zone Light Mapping**: Intelligent brightness zones from dark room to bright daylight
- **Safety PWM Limiting**: Configurable maximum brightness for LED protection
- **Instant Response**: 100-220ms light sensor updates, 5-second potentiometer response

### 🌐 **Professional IoT Connectivity**
- **Secure MQTT**: HiveMQ Cloud with TLS 1.2+ encryption and certificate validation
- **WiFi Auto-Connect**: Remembers credentials with web configuration fallback
- **NTP Time Sync**: Automatic time synchronization with timezone support
- **Remote Control**: Complete device management via MQTT commands
- **Status Monitoring**: Real-time system health and connectivity reporting

---

## 🚀 Quick Start

### Prerequisites
- **ESP-IDF 5.4.2** installed and configured
- **ESP32 development board** with 4MB flash memory minimum
- **Hardware components** (see [Hardware Requirements](#-hardware-requirements))

### 1. Flash and Setup
```bash
# Clone the repository
git clone https://github.com/stani56/wordclock.git
cd wordclock

# Build and flash
idf.py build
idf.py flash monitor
```

### 2. WiFi Configuration
1. **Power on ESP32** - Look for WiFi network: `ESP32-LED-Config`
2. **Connect** with password: `12345678`
3. **Open browser** to `http://192.168.4.1`
4. **Select your WiFi** and enter password
5. **Click Connect** - system reboots and auto-connects

### 3. Verify Operation
**Console Success Indicators:**
```
✅ "Connected to WiFi with stored credentials"
✅ "NTP time synchronization complete!"
✅ "=== SECURE MQTT CONNECTION ESTABLISHED ==="
✅ STATUS_LED: WiFi LED: ON (GPIO 21)
✅ STATUS_LED: NTP LED: ON (GPIO 22)
```

**Physical Verification:**
- Word clock displays current Vienna time in German
- Status LEDs indicate network connectivity
- MQTT connection established to HiveMQ Cloud

---

## 🔧 Hardware Requirements

### Core Components
| Component | Specification | Purpose |
|-----------|---------------|---------|
| **ESP32** | ESP-IDF 5.4.2 compatible | Main controller |
| **TLC59116** | 10× I2C LED controllers | Individual LED control |
| **DS3231** | Real-time clock module | Accurate timekeeping |
| **BH1750** | Digital light sensor | Ambient light detection |
| **LEDs** | 160× individual LEDs | 10×16 display matrix |
| **Potentiometer** | Linear, 3.3V compatible | Manual brightness control |

### GPIO Pin Assignment
```
GPIO 18 → I2C SDA (Sensors)     → DS3231, BH1750
GPIO 19 → I2C SCL (Sensors)     → DS3231, BH1750
GPIO 25 → I2C SDA (LEDs)        → TLC59116 controllers
GPIO 26 → I2C SCL (LEDs)        → TLC59116 controllers
GPIO 34 → ADC Input              → Potentiometer
GPIO 21 → Status LED             → WiFi connectivity indicator
GPIO 22 → Status LED             → NTP sync indicator
GPIO 5  → Reset Button           → WiFi credential reset
```

### I2C Device Addresses
```
TLC59116 Controllers: 0x60-0x6A (10 devices)
DS3231 RTC:          0x68
BH1750 Light Sensor: 0x23
```

---

## 🏠 Home Assistant Integration

### Automatic Discovery
The word clock automatically appears in Home Assistant with **36 entities** organized into logical groups:

- **1 Main Light**: Primary display control with brightness and effects
- **7 Sensors**: System status, brightness levels, light readings, potentiometer position
- **24 Configuration Controls**: Zone brightness, thresholds, safety limits, curves
- **4 Action Buttons**: Restart, test transitions, refresh sensors, time setting

### Example Dashboard
```yaml
type: vertical-stack
cards:
  - type: light
    entity: light.word_clock_display
    
  - type: entities
    title: Brightness Control
    entities:
      - number.led_brightness_control
      - number.current_safety_pwm_limit
      - sensor.led_brightness
      - sensor.display_brightness
      
  - type: entities
    title: System Status
    entities:
      - sensor.wifi_status
      - sensor.time_sync_status
      - sensor.current_light_level
      - button.refresh_sensor_values
```

### MQTT Topics
```
home/esp32_core/command         # Device commands
home/esp32_core/brightness/set  # Brightness control
home/esp32_core/transition/set  # Animation settings
home/esp32_core/sensors/status  # Sensor readings
home/esp32_core/availability    # Online/offline status
```

---

## 🎨 German Time Display Examples

| Time | German Display | Minute LEDs |
|------|----------------|-------------|
| 14:00 | ES IST ZWEI UHR | - |
| 14:05 | ES IST FÜNF NACH ZWEI | - |
| 14:23 | ES IST ZWANZIG NACH ZWEI | ●●● (3 LEDs) |
| 14:30 | ES IST HALB DREI | - |
| 14:37 | ES IST FÜNF NACH HALB DREI | ●● (2 LEDs) |
| 21:55 | ES IST FÜNF VOR ZEHN | - |
| 09:58 | ES IST FÜNF VOR ZEHN | ●●● (3 LEDs) |

**Logic**: Words show 5-minute increments, minute indicator LEDs show remaining minutes (0-4)

---

## 🎬 Animation System

### Smooth Transitions
- **Duration**: 200-5000ms (configurable, default 1500ms)
- **Animation Curves**: Linear, Ease-In, Ease-Out, Ease-In-Out, Bounce
- **Word Coherence**: Complete German words transition as visual units
- **Priority System**: Hour words get priority for smooth transitions
- **Instant Fallback**: Guaranteed display updates even if transitions fail

### MQTT Control
```bash
# Enable 2-second transitions with bounce effect
Topic: home/esp32_core/transition/set
Payload: {"duration": 2000, "enabled": true, "fadein_curve": "bounce"}

# Test transitions (5-minute demo)
Topic: home/esp32_core/command
Payload: test_transitions_start
```

---

## 📊 System Performance

### Response Times
- **Light Sensor**: 100-220ms (instant ambient adaptation)
- **Potentiometer**: 5-8 seconds (user brightness control)
- **LED Updates**: 12-20ms (typical word changes)
- **WiFi Connection**: 5-15 seconds (automatic reconnection)
- **MQTT Commands**: <1 second (remote control)

### Resource Usage
- **CPU**: ESP32 @ 160MHz (conservative for stability)
- **Memory**: 4MB flash minimum, 520KB RAM, 2.5MB application partition
- **I2C Operations**: 5-25 per display update (95% reduction via optimization)
- **Network**: Pull-based sensor updates (no flooding)
- **Flash Writes**: Debounced configuration saves (95% reduction)

---

## 📚 Documentation

📁 **[Complete Documentation Hub](docs/)** - Well-organized documentation for all audiences

### 👥 For Users
- **[User Guide](docs/user/)** - Setup, configuration, and daily use
- **[Hardware Setup](docs/user/hardware-setup.md)** - Physical assembly guide
- **[Home Assistant Integration](docs/user/home-assistant-setup.md)** - Smart home setup
- **[MQTT Commands](docs/user/mqtt-commands.md)** - Remote control reference
- **[Troubleshooting](docs/user/troubleshooting.md)** - Common issues and solutions

### 🔧 For Developers  
- **[Developer Guide](docs/developer/)** - Architecture and development
- **[API Reference](docs/developer/api-reference.md)** - Component APIs
- **[Build System](docs/developer/build-system.md)** - Compilation and deployment
- **[Contributing Guide](docs/developer/contributing.md)** - How to contribute

### ⚙️ For System Architects & Advanced Users
- **[Implementation Details](docs/implementation/)** - Technical deep-dives and system architecture
- **[🌟 MQTT System Architecture](docs/implementation/mqtt-system-architecture.md)** - **Complete MQTT technical guide**
- **[Testing Framework](docs/testing/)** - Comprehensive testing procedures
- **[Maintenance Guide](docs/maintenance/)** - Operations and deployment

### 📜 Legacy Documentation
- **[Historical Documents](docs/legacy/)** - Development history and lessons learned

**Quick Links**: [CLAUDE.md](CLAUDE.md) (development reference) • [Hardware Setup](docs/user/hardware-setup.md) • [HA Integration](docs/user/home-assistant-setup.md) • [**MQTT Technical Guide**](docs/implementation/mqtt-system-architecture.md)

---

## 🔧 Development

### System Architecture
```
Main Application Layer
├── German Word Clock Logic    ← Time display and LED control
├── Dual Brightness System    ← Potentiometer + light sensor
├── Network Layer             ← WiFi, NTP, MQTT with TLS
├── Home Assistant Layer      ← MQTT Discovery, 36 entities
└── Hardware Abstraction      ← I2C devices, ADC, GPIO
```

### Component Structure
- **11 ESP-IDF Components** with linear dependency chain
- **Modern ESP-IDF 5.4.2 APIs** throughout
- **Comprehensive error handling** and graceful degradation
- **Production-ready logging** and diagnostics

### Testing
```bash
# Hardware connectivity test
idf.py monitor  # Look for device detection logs

# MQTT integration test
mosquitto_sub -h broker.host -t "home/esp32_core/+"

# Home Assistant discovery test
# Check Settings → Devices & Services → MQTT
```

### CI/CD Pipeline
- **GitHub Actions**: Automated build testing with ESP-IDF 5.4.2
- **Build Matrix**: Tests multiple ESP32 targets and configurations
- **Pull Request Validation**: Ensures code quality before merge
- **Release Automation**: Automatic version tagging and release notes

### Build System Configuration
**Flash Requirements:**
- **Minimum Flash Size**: 4MB
- **Application Partition**: 2.5MB (increased for large application)
- **Custom Partition Table**: Required for complex IoT features

**Configuration Files:**
- `sdkconfig.defaults` - Default build configuration
- `sdkconfig.ci` - CI/CD specific optimizations
- `partitions.csv` - Custom partition table layout

**Common Build Issues:**
```bash
# Fix partition size too small
idf.py set-target esp32
idf.py menuconfig  # Serial flasher config → Flash size → 4MB

# Clean build for configuration changes
idf.py fullclean
idf.py build

# IntelliSense support (VS Code)
idf.py build  # Generates compile_commands.json automatically
```

---

## 🛡️ Security Features

### Network Security
- **TLS 1.2+ Encryption**: All MQTT communication encrypted
- **Certificate Validation**: ESP32 built-in certificate bundle
- **Secure Credential Storage**: WiFi/MQTT credentials in NVS
- **AP Mode Protection**: Configuration interface password protected

### Hardware Protection
- **Safety PWM Limiting**: Prevents LED overheating and damage
- **I2C Bus Protection**: Conservative timing prevents bus saturation  
- **Flash Wear Protection**: Debounced writes prevent excessive wear
- **Graceful Degradation**: System continues operation with partial failures

---

## 🤝 Contributing

We welcome contributions from the community! Here's how you can help:

### Ways to Contribute
- 🐛 **Bug Reports**: Found an issue? Please report it!
- 💡 **Feature Requests**: Have an idea? We'd love to hear it!
- 📝 **Documentation**: Help improve our guides and examples
- 🔧 **Code**: Submit pull requests for fixes and enhancements
- 🏠 **Home Assistant**: Share automations and dashboard configs
- 🔌 **Hardware**: Design PCBs, enclosures, or hardware variants

### Getting Started
1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Make** your changes with tests
4. **Commit** your changes (`git commit -m 'Add amazing feature'`)
5. **Push** to the branch (`git push origin feature/amazing-feature`)
6. **Open** a Pull Request

### Development Setup
```bash
# Install ESP-IDF 5.4.2
# Clone and build
git clone https://github.com/stani56/wordclock.git
cd wordclock
idf.py build
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**MIT License Summary**: Free to use, modify, and distribute for personal and commercial projects.

---

## 🙏 Acknowledgments

- **Espressif Systems** for ESP-IDF framework
- **Home Assistant Community** for MQTT Discovery standards  
- **HiveMQ** for reliable cloud MQTT broker
- **Open Source Community** for libraries and inspiration

---

## 📞 Support

- 📋 **Issues**: [GitHub Issues](https://github.com/stani56/wordclock/issues)
- 💬 **Discussions**: [GitHub Discussions](https://github.com/stani56/wordclock/discussions)
- 📧 **Security**: See [Security Guide](docs/maintenance/security.md) for vulnerability reporting

---

<div align="center">

**🌟 If this project helped you, please give it a star! 🌟**

Made with ❤️ for the ESP32 and Home Assistant communities

</div>