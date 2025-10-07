# 🔧 Developer Documentation

Welcome to the ESP32 German Word Clock development guide. This section covers architecture, build system, and development procedures.

## 🚀 Quick Developer Setup

### Prerequisites
- **ESP-IDF 5.4.2** installed and configured
- **Git** for version control
- **VS Code** with ESP-IDF extension (recommended)
- **4MB flash ESP32** development board

### 5-Minute Setup
```bash
# Clone repository
git clone https://github.com/Stani56/wordclock.git
cd wordclock

# Build and flash
. $HOME/esp/esp-idf/export.sh
idf.py build
idf.py flash monitor
```

## 📚 Developer Guides

| Guide | Description |
|-------|-------------|
| [Project Structure](project-structure.md) | Codebase organization and components |
| [Build System](build-system.md) | Compilation, flashing, and debugging |
| [Contributing](contributing.md) | How to contribute to the project |
| [API Reference](api-reference.md) | Component APIs and interfaces |
| [Architecture](architecture.md) | System design and data flow |

## 🏗️ Project Architecture

### Component Overview
```
ESP32 German Word Clock (ESP-IDF 5.4.2)
├── 📁 Core Hardware Components
│   ├── i2c_devices          # TLC59116 LED controllers + DS3231 RTC
│   ├── wordclock_display    # German word display logic
│   ├── wordclock_time       # Time calculation and formatting
│   ├── adc_manager          # Potentiometer brightness control
│   └── light_sensor         # BH1750 ambient light sensor
│
├── 📁 IoT Network Components  
│   ├── wifi_manager         # WiFi connectivity and web config
│   ├── ntp_manager          # NTP time sync with timezone
│   ├── mqtt_manager         # Secure MQTT with HiveMQ Cloud
│   └── web_server           # WiFi configuration interface
│
├── 📁 Smart Home Integration
│   ├── mqtt_discovery       # Home Assistant auto-discovery
│   ├── button_manager       # Reset button functionality
│   ├── status_led_manager   # Network status indicators
│   └── nvs_manager          # Persistent configuration
│
├── 📁 Advanced Features
│   ├── transition_manager   # Smooth LED animations
│   ├── brightness_config    # Advanced brightness calibration
│   └── transition_config    # Animation configuration
│
└── 📁 Tier 1 MQTT (Professional)
    ├── mqtt_command_processor    # Structured command handling
    ├── mqtt_schema_validator     # JSON validation framework
    └── mqtt_message_persistence  # Reliable message delivery
```

### System Data Flow
```
Hardware Layer → Control Layer → Network Layer → Integration Layer
     ↓               ↓              ↓               ↓
- TLC59116      - Display       - WiFi          - Home Assistant
- DS3231        - Time calc     - NTP           - MQTT Discovery  
- BH1750        - Brightness    - MQTT          - 37 Entities
- Potentiometer - Transitions   - Web Server    - Remote Control
```

## 🛠️ Development Workflow

### 1. Development Setup
```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure project
idf.py menuconfig

# Build project
idf.py build
```

### 2. Testing Workflow
```bash
# Flash and monitor
idf.py flash monitor

# Run automated tests
./test_environment/run_tests.sh

# MQTT testing
./mqtt_test_comprehensive.py
```

### 3. Code Quality
- **ESP-IDF 5.4.2** modern driver APIs
- **Component-based architecture** for modularity
- **Comprehensive error handling** with graceful degradation
- **Professional logging** with appropriate levels
- **Memory management** with static allocation preferred

## 🔧 Key Development Areas

### Hardware Integration
- **I2C Communication**: Dual-bus architecture (LEDs + Sensors)
- **LED Control**: TLC59116 PWM controllers with global brightness
- **Time Management**: DS3231 RTC with NTP sync backup
- **Sensor Integration**: ADC for potentiometer, I2C for light sensor

### Network Programming
- **WiFi Management**: Auto-connect with AP fallback mode
- **MQTT Integration**: Secure TLS with HiveMQ Cloud
- **NTP Synchronization**: Timezone-aware with periodic sync
- **Web Interface**: HTTP server for WiFi configuration

### Smart Home Integration
- **MQTT Discovery**: 37 Home Assistant entities with auto-configuration
- **Entity Management**: Professional device grouping and organization
- **Real-time Updates**: Status publishing and command processing
- **Configuration Persistence**: NVS storage for settings

## 📊 Technical Specifications

### Performance Metrics
- **Build Size**: ~1.1MB (56% of 2.5MB partition)
- **RAM Usage**: Optimized with static allocation
- **Flash Usage**: 4MB flash minimum required
- **Update Rate**: 20Hz transitions, 1Hz main loop
- **Network**: <1 second response time for MQTT commands

### Hardware Specifications
- **CPU**: ESP32 @ 240MHz (configurable)
- **Flash**: 4MB minimum (2.5MB app partition)
- **I2C Speed**: 100kHz (conservative for reliability)
- **GPIO Usage**: 8 pins (I2C×4, ADC×1, LED×2, Button×1)
- **Power**: 5V input, 3.3V logic throughout

## 🧪 Testing Framework

### Automated Testing
- **Unit Tests**: Component-level testing
- **Integration Tests**: System-level verification  
- **MQTT Tests**: Command and discovery testing
- **Real-world Tests**: Extended operation validation

### Testing Tools
```bash
# Quick functionality test
./test_environment/quick_test.sh

# Comprehensive test suite
./test_environment/run_tests.sh

# MQTT command testing
./test_mqtt_commands.sh

# Real-world extended testing
./real_world_test.sh
```

## 🔐 Security Considerations

### Network Security
- **TLS Encryption**: All MQTT communication encrypted
- **Certificate Validation**: ESP32 certificate bundle verification
- **No Credential Storage**: WiFi passwords in encrypted NVS
- **Secure Defaults**: Professional security configuration

### Code Security
- **Input Validation**: All user inputs validated
- **Buffer Management**: Proper bounds checking
- **Error Handling**: Graceful failure modes
- **No Debug Info**: Production builds exclude debug symbols

## 📈 Performance Optimization

### Memory Optimization
- **Static Allocation**: Minimize heap usage
- **Component Isolation**: Clear memory boundaries
- **Efficient Data Structures**: Optimized for ESP32
- **Stack Management**: Proper task stack sizing

### I2C Optimization
- **Conservative Timing**: 100kHz for maximum reliability
- **Differential Updates**: Only change what's needed (95% reduction)
- **Direct Device Handles**: Modern ESP-IDF APIs
- **Error Recovery**: Comprehensive retry logic

## 🔄 Development Process

### Version Control
- **Single Branch**: Main branch contains all features
- **Clean History**: Meaningful commit messages
- **Change Documentation**: Update relevant docs with changes
- **Testing**: Verify build and basic functionality

### Code Standards
- **ESP-IDF Style**: Follow ESP-IDF coding conventions
- **Component Structure**: Consistent organization
- **Documentation**: API documentation for public functions
- **Error Handling**: Comprehensive error checking

## 🆘 Developer Support

### Common Development Issues
| Issue | Solution |
|-------|----------|
| Build errors | Check ESP-IDF 5.4.2 compatibility |
| Flash size issues | Use 4MB flash, custom partition table |
| I2C timeouts | Verify hardware connections |
| MQTT connection fails | Check HiveMQ Cloud credentials |

### Debug Resources
- **Serial Monitor**: `idf.py monitor` for real-time logs
- **GDB Debugging**: `idf.py gdb` for code debugging
- **Component Tests**: Individual component verification
- **MQTT Tools**: MQTT.fx or similar for protocol testing

---
🎯 **Target**: Software developers and embedded engineers  
📝 **Level**: Intermediate to advanced  
⏱️ **Onboarding Time**: 30-60 minutes