# 📚 ESP32 German Word Clock Documentation

Welcome to the comprehensive documentation for the ESP32 German Word Clock project. This documentation is organized by audience and purpose to help you find what you need quickly.

## 🚀 Quick Start

**New to the project?** Start here:
1. [User Guide](user/README.md) - Setup and daily use
2. [Hardware Setup](user/hardware-setup.md) - Physical connections
3. [Home Assistant Integration](user/home-assistant-setup.md) - Smart home setup

**Developer?** Go here:
1. [Developer Guide](developer/README.md) - Code architecture and development
2. [Build System](developer/build-system.md) - Compilation and deployment
3. [Contributing](developer/contributing.md) - How to contribute

## 📁 Documentation Structure

### 👥 [User Documentation](user/)
Everything end-users need to set up, configure, and use the word clock.
- User guides and setup instructions
- **[MQTT Commands](user/mqtt/)** - Complete MQTT reference and discovery
- Hardware setup and connections
- [LED Validation Guide](user/led-validation-guide.md)
- [Error Logging Guide](user/error-logging-guide.md)
- [Troubleshooting](user/troubleshooting.md)

### 🔧 [Developer Documentation](developer/)
Technical documentation for developers working on the codebase.
- Project architecture and structure
- Build system and toolchain
- API reference and component guides
- Contributing guidelines

### ⚙️ [Implementation Details](implementation/)
Deep-dive technical documentation about system components.
- **[MQTT System](implementation/mqtt/)** - Discovery, architecture, issues
- **[LED Validation](implementation/led-validation/)** - Complete validation system docs
- **[ESP32-S3 Migration](implementation/esp32-s3-migration/)** - S3 upgrade guide
- [Brightness Control System](implementation/brightness-system.md)
- [NVS Architecture](implementation/nvs-architecture.md)
- [Phase 1 Completion Summary](implementation/phase1-completion-summary.md)

### 💡 [Proposals](proposals/)
Active and future feature proposals.
- **[Active Proposals](proposals/active/)** - Flexible time expressions
- **[ESP32-S3 Future](proposals/esp32-s3-future/)** - Audio features for S3 hardware

### 🧪 [Testing Documentation](testing/)
Comprehensive testing procedures and frameworks.
- Automated testing guides
- Real-world testing procedures
- MQTT testing setup
- Testing frameworks and tools

### 🛠️ [Technical Documentation](technical/)
Technical analysis and specifications.
- External flash and memory options
- Partition layout analysis
- Dual-core usage analysis
- ESP32 PICO kit GPIO analysis

### 🔧 [Maintenance & Operations](maintenance/)
Operational procedures and maintenance information.
- **[Documentation Cleanup](maintenance/DOCUMENTATION-CLEANUP-SUMMARY.md)**
- Deployment procedures
- Rollback instructions
- Security considerations
- Change management

### 📦 [Archive](archive/)
Historical documentation preserved for reference.
- **[ESP32 Audio (Removed Nov 2025)](archive/esp32-audio-removed/)** - Audio docs obsolete due to WiFi+MQTT+I2S conflicts
- **[Implemented Proposals](archive/implemented-proposals/)** - Successfully implemented features
- **[Superseded Plans](archive/superseded-plans/)** - Plans overtaken by hardware changes
- **[2024 History](archive/2024-history/)** - Legacy issue fixes and early analysis

## 🎯 Quick Links

| Need | Link |
|------|------|
| **Set up the device** | [User Guide](user/README.md) |
| **Connect to Home Assistant** | [HA Setup](user/home-assistant-setup.md) |
| **MQTT Commands** | [MQTT Commands](user/mqtt/mqtt-commands.md) |
| **MQTT Discovery** | [Discovery Reference](user/mqtt/mqtt-discovery-reference.md) |
| **Development Setup** | [Developer Guide](developer/README.md) |
| **Build the Project** | [Build System](developer/build-system.md) |
| **Test the System** | [Testing Guide](testing/README.md) |
| **Troubleshoot Issues** | [User Troubleshooting](user/troubleshooting.md) |
| **ESP32-S3 Upgrade** | [S3 Migration Analysis](implementation/esp32-s3-migration/ESP32-S3-Migration-Analysis.md) |

## 🆘 Need Help?

1. **Common Issues**: Check [User Troubleshooting](user/troubleshooting.md)
2. **Build Problems**: See [Build System](developer/build-system.md)
3. **HA Integration**: Review [HA Setup Guide](user/home-assistant-setup.md)
4. **MQTT Issues**: Check [MQTT Testing](testing/mqtt-testing.md)

## 📊 Project Status

✅ **Production Ready** - Complete IoT German Word Clock with Home Assistant integration
- 36 Home Assistant entities with auto-discovery
- Secure MQTT with HiveMQ Cloud TLS
- NTP time synchronization with DS3231 RTC backup
- Dual brightness control (potentiometer + light sensor)
- LED validation system with hardware readback
- Persistent error logging (50-entry circular buffer)
- ESP-IDF 5.4.2 with comprehensive testing framework

### Current Hardware: ESP32 Baseline (Audio Removed)
- **Platform:** ESP32-PICO-D4
- **Status:** Stable WiFi+MQTT operation (audio removed due to DMA conflicts)
- **Audio:** Disabled on ESP32 (causes WiFi+MQTT crashes)

### Future Hardware: ESP32-S3 Migration
- **Target:** YelloByte YB-ESP32-S3-AMP board
- **Benefits:** Concurrent WiFi+MQTT+I2S audio support
- **Status:** Complete migration plan documented
- **See:** [ESP32-S3 Migration Analysis](implementation/esp32-s3-migration/ESP32-S3-Migration-Analysis.md)

## 📋 Recent Documentation Updates

**Nov 2025:** Major documentation reorganization (Option C)
- 44 files reorganized into topical structure
- 25 files archived with context preservation
- Active documentation reduced by 26%
- See: [Documentation Cleanup Summary](maintenance/DOCUMENTATION-CLEANUP-SUMMARY.md)

**Nov 2025:** ESP32 baseline cleanup
- Audio subsystem removed (WiFi+MQTT+I2S conflicts)
- Complete ESP32-S3 migration plan finalized
- microSD storage decision documented
- See: [ESP32 Baseline Cleanup Plan](maintenance/ESP32-Baseline-Cleanup-Plan.md)

**Oct 2025:** Production features added
- LED validation system with hardware readback
- Persistent error logging (50 entries, survives reboots)
- TLC59116 auto-increment pointer fix

---
📝 **Documentation Version**: Current with main branch
🔄 **Last Updated**: November 2025
🎯 **Target Audience**: End users, developers, and maintainers
📦 **Documentation Cleanup**: Complete (Nov 2025)
