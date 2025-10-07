# Changelog

All notable changes to the ESP32 German Word Clock project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub repository setup with comprehensive documentation

## [2.0.0] - 2025-07-29

### Added
- **Home Assistant MQTT Discovery**: Automatic entity creation with 12 pre-configured entities
- **Smooth LED Transition System**: Professional 1.5-second animations with multiple curves
- **Priority-Based Transitions**: Word-coherent transitions with 32-LED concurrent support
- **Complete IoT Integration**: WiFi manager, NTP sync, secure MQTT with TLS
- **Dual Brightness Control**: Potentiometer (individual) + light sensor (global) system
- **BH1750 Light Sensor**: Instant ambient light adaptation (100-220ms response)
- **Visual Status LEDs**: WiFi (GPIO 21) and NTP (GPIO 22) connectivity indicators
- **Reset Button Support**: Long-press WiFi credential clearing (GPIO 5)
- **Web Configuration Portal**: AP mode with network scanning for easy setup
- **Persistent Storage**: NVS-based WiFi and MQTT credential management

### Changed
- **I2C System Optimization**: Reduced operations by 95% with differential LED updates
- **ESP-IDF 5.3.1 Migration**: Modern driver APIs throughout entire codebase
- **Component Architecture**: Expanded to 11 modular ESP-IDF components
- **Clock Speed Optimization**: Conservative 100kHz I2C for 100% reliability
- **Memory Management**: Static allocation for transition buffers (prevents stack overflow)

### Fixed
- **I2C Timeout Issues**: Eliminated with conservative timing and retry logic
- **German Grammar**: Proper "EIN UHR" vs "EINS" handling for hour 1
- **WiFi PHY Initialization**: Resolved stack conflicts during boot
- **NTP Synchronization**: Two-phase init prevents TCP/IP stack assertions
- **SIEBEN Word Transitions**: Priority system ensures complete word coherence

### Technical Improvements
- **Transition Manager**: 20Hz animation engine with guaranteed fallback
- **Light Sensor Task**: Dedicated 10Hz monitoring for instant brightness response
- **MQTT Security**: HiveMQ Cloud TLS with ESP32 certificate validation
- **Error Recovery**: Comprehensive I2C bus recovery and graceful degradation
- **Performance Optimization**: CPU frequency tuning for WiFi stability

## [1.0.0] - 2025-01-15

### Added
- **Core Word Clock Functionality**: German word time display with proper grammar
- **LED Matrix Control**: 10×16 LED grid with TLC59116 controllers
- **Real-Time Clock**: DS3231 RTC with temperature monitoring
- **I2C Communication**: Dual-bus architecture (LEDs + sensors)
- **German Time Logic**: Complete time-to-words conversion system
- **Brightness Control**: Global GRPPWM brightness adjustment
- **Hardware Testing**: Comprehensive test suite for debugging

### Technical Features
- **ESP-IDF 5.3.1**: Modern I2C master driver implementation
- **Component Architecture**: Modular design with 3 core components
- **German Word Database**: Complete position mapping for all words
- **Time Calculation**: Proper 5-minute increment handling with indicators
- **I2C Device Management**: Direct device handles for reliable communication

### Hardware Support
- **TLC59116 LED Drivers**: 10 controllers for individual row control
- **DS3231 RTC Module**: I2C timekeeping with battery backup
- **ESP32 Development Board**: Main controller with dual I2C buses
- **160 LED Matrix**: 10×16 layout with minute indicator LEDs

## [0.1.0] - 2024-12-01

### Added
- Initial project setup and research framework
- Basic ESP-IDF 5.3.1 project structure
- Hardware component identification and planning
- German word clock layout design
- Project documentation and requirements analysis

---

## Version History Summary

- **v2.0.0**: Complete IoT system with Home Assistant integration and smooth transitions
- **v1.0.0**: Production-ready German word clock with reliable hardware control
- **v0.1.0**: Initial research project setup and planning

## Migration Notes

### From v1.0.0 to v2.0.0

- **New Dependencies**: WiFi, NTP, MQTT, and light sensor components
- **Hardware Additions**: BH1750 light sensor, potentiometer, status LEDs, reset button
- **Configuration Changes**: NVS storage for network credentials
- **API Changes**: Extended MQTT command system and brightness control APIs

### ESP-IDF Compatibility

- **v2.0.0+**: Requires ESP-IDF v5.3.1 or later
- **v1.0.0+**: Compatible with ESP-IDF v5.3.1
- **v0.1.0**: Development version, various ESP-IDF versions tested

## Known Issues

### Current Limitations
- Maximum 32 concurrent LED transitions (hardware limitation)
- Vienna timezone hardcoded (configurable in source)
- HiveMQ Cloud credentials need manual configuration
- Reset button GPIO pin user-configurable

### Planned Improvements
- Multiple timezone support via web interface
- Dynamic MQTT broker configuration
- Additional animation curve types
- Multi-language word clock support

---

For detailed technical information about any version, see [CLAUDE.md](CLAUDE.md) for comprehensive implementation documentation.