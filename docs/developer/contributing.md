# Contributing to ESP32 German Word Clock

Thank you for your interest in contributing to this project! This document provides guidelines and information for contributors.

## 📋 Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Contributing Process](#contributing-process)
- [Code Standards](#code-standards)
- [Testing Guidelines](#testing-guidelines)
- [Documentation](#documentation)
- [Community Guidelines](#community-guidelines)

## Getting Started

Before contributing, please:

1. **Read the Documentation**: Familiarize yourself with [CLAUDE.md](CLAUDE.md) for comprehensive technical details
2. **Check Issues**: Look for existing issues or create a new one to discuss your proposed changes
3. **Start Small**: Consider starting with documentation improvements or small bug fixes

## Development Setup

### Prerequisites

- **ESP-IDF v5.3.1**: [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/get-started/index.html)
- **Hardware**: ESP32 development board and required components (see [README.md](README.md))
- **MQTT Client**: For testing IoT features (MQTTX recommended)

### Environment Setup

```bash
# Clone your fork
git clone https://github.com/yourusername/wordclock.git
cd wordclock

# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Build the project
idf.py build

# Flash and monitor (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash monitor
```

## Contributing Process

### 1. Fork and Branch

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/yourusername/wordclock.git
cd wordclock

# Create a feature branch
git checkout -b feature/your-feature-name
```

### 2. Make Changes

- Follow the [Code Standards](#code-standards) below
- Add tests for new functionality
- Update documentation if needed
- Test thoroughly on actual hardware when possible

### 3. Commit Guidelines

Use clear, descriptive commit messages:

```bash
# Good examples:
git commit -m "Add support for custom animation curves"
git commit -m "Fix I2C timeout issues in light sensor component"
git commit -m "Update README with Home Assistant setup instructions"

# Follow conventional commits format when possible:
# type(scope): description
# Examples:
git commit -m "feat(mqtt): add new brightness control command"
git commit -m "fix(i2c): resolve TLC59116 communication timeout"
git commit -m "docs(readme): add troubleshooting section"
```

### 4. Pull Request

1. Push your branch to your fork
2. Create a Pull Request with:
   - Clear title and description
   - Reference to related issues
   - Screenshots/videos if applicable
   - Testing performed

## Code Standards

### General Guidelines

- **Language**: All code, comments, and documentation in English
- **Style**: Follow ESP-IDF coding standards
- **Components**: Maintain the modular component architecture
- **Error Handling**: Use ESP_LOG* macros for logging, handle errors gracefully

### ESP-IDF Specific

```c
// Use modern ESP-IDF 5.3.1 APIs
#include "driver/i2c_master.h"  // Not legacy i2c.h

// Proper error handling
esp_err_t ret = some_function();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Function failed: %s", esp_err_to_name(ret));
    return ret;
}

// Use appropriate log levels
ESP_LOGI(TAG, "System initialized successfully");
ESP_LOGW(TAG, "Using fallback configuration");
ESP_LOGE(TAG, "Critical error occurred");
```

### Component Structure

When adding new components:

```
components/your_component/
├── CMakeLists.txt           # Component build configuration
├── your_component.c         # Implementation
└── include/
    └── your_component.h     # Public API
```

### Hardware Considerations

- **I2C Operations**: Use differential updates to minimize bus traffic
- **Memory Management**: Prefer static allocation, avoid dynamic memory in loops
- **Task Safety**: Use appropriate FreeRTOS synchronization primitives
- **Error Recovery**: Implement graceful degradation for hardware failures

## Testing Guidelines

### Hardware Testing

- **Required**: Test on actual hardware when modifying hardware-related code
- **I2C Components**: Verify all TLC59116 and sensor communications
- **Network Features**: Test WiFi, NTP, and MQTT functionality
- **Edge Cases**: Test network disconnection, power cycles, button presses

### Debug Mode Testing

Enable comprehensive testing by uncommenting debug functions in `main/wordclock.c`:

```c
// Uncomment for full diagnostic testing:
// adc_test_gpio_connectivity();
// test_light_sensor_instant_response();
// test_german_time_display();
```

### MQTT Testing

Test MQTT commands using a client like MQTTX:

```json
// Test basic functionality
{"command": "status"}
{"command": "test_transitions_start"}

// Test error handling
{"command": "invalid_command"}
```

## Documentation

### Code Documentation

- **Function Headers**: Document complex functions with clear descriptions
- **Component APIs**: Update header files when changing public interfaces
- **Configuration**: Document new configuration options

### User Documentation

- **README.md**: Update for user-facing changes
- **CLAUDE.md**: Technical implementation details
- **Examples**: Provide usage examples for new features

## Community Guidelines

### Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and contribute
- Maintain a professional tone in all interactions

### Communication

- **Issues**: Use GitHub issues for bug reports and feature requests
- **Discussions**: Use GitHub discussions for questions and ideas
- **Pull Requests**: Provide clear descriptions and be responsive to feedback

### Issue Reporting

When reporting bugs, include:

```
**Hardware Setup:**
- ESP32 board: [your board type]
- ESP-IDF version: [version]
- Components: [list any additional hardware]

**Expected Behavior:**
[What should happen]

**Actual Behavior:**
[What actually happens]

**Steps to Reproduce:**
1. [First step]
2. [Second step]
3. [And so on...]

**Logs/Console Output:**
```
[paste relevant console output]
```

**Additional Context:**
[Any other relevant information]
```

## Types of Contributions

We welcome various types of contributions:

### 🐛 Bug Fixes
- I2C communication issues
- Network connectivity problems
- Display rendering bugs
- Memory leaks or crashes

### ✨ Features
- New animation effects
- Additional language support
- Home Assistant integrations
- Hardware component support

### 📚 Documentation
- Code comments and documentation
- Usage examples
- Troubleshooting guides
- Video tutorials

### 🧪 Testing
- Hardware compatibility testing
- Edge case testing
- Performance benchmarking
- Automated testing frameworks

### 🎨 Hardware Designs
- PCB layouts
- 3D printed cases
- Wiring diagrams
- Component sourcing guides

## Recognition

Contributors will be recognized in:
- README.md contributors section
- Release notes for significant contributions
- Special mentions for major features

## Questions?

If you have questions about contributing:

1. Check existing [Issues](https://github.com/stani56/wordclock/issues)
2. Create a new issue with the "question" label
3. Reference the comprehensive documentation in [CLAUDE.md](CLAUDE.md)

Thank you for contributing to the ESP32 German Word Clock project! 🕐✨