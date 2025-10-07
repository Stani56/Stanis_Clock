# Project Structure

This document describes the architecture and organization of the ESP32 German Word Clock project.

## 📁 Repository Layout

```
wordclock/
├── 📄 README.md                    # Main project documentation
├── 📄 CLAUDE.md                    # Comprehensive technical documentation  
├── 📄 LICENSE                      # MIT License
├── 📄 CONTRIBUTING.md              # Contribution guidelines
├── 📄 CHANGELOG.md                 # Version history
├── 📄 SECURITY.md                  # Security policy and guidelines
├── 📄 PROJECT_STRUCTURE.md         # This file
├── 📄 .gitignore                   # Git ignore rules
├── 
├── 📁 .github/                     # GitHub-specific files
│   ├── 📁 workflows/
│   │   └── 📄 build.yml           # CI/CD build workflow
│   ├── 📁 ISSUE_TEMPLATE/
│   │   ├── 📄 bug_report.md       # Bug report template
│   │   ├── 📄 feature_request.md  # Feature request template
│   │   └── 📄 hardware_support.md # Hardware support template
│   └── 📄 pull_request_template.md # PR template
├── 
├── 📁 main/                        # ESP-IDF main application
│   ├── 📄 CMakeLists.txt          # Main build configuration
│   ├── 📄 wordclock.c             # Core application logic
│   └── 📄 wordclock_mqtt_control.h # MQTT command definitions
├── 
├── 📁 components/                  # ESP-IDF components (11 total)
│   ├── 📁 i2c_devices/           # Hardware communication layer
│   ├── 📁 wordclock_display/     # LED matrix control
│   ├── 📁 wordclock_time/        # German time calculations
│   ├── 📁 adc_manager/           # Potentiometer input
│   ├── 📁 light_sensor/          # BH1750 ambient light
│   ├── 📁 transition_manager/    # Smooth LED animations
│   ├── 📁 wifi_manager/          # Network connectivity
│   ├── 📁 ntp_manager/           # Time synchronization
│   ├── 📁 mqtt_manager/          # MQTT communication
│   ├── 📁 mqtt_discovery/        # Home Assistant auto-discovery
│   ├── 📁 nvs_manager/           # Persistent storage
│   ├── 📁 status_led_manager/    # Network status LEDs
│   ├── 📁 button_manager/        # Reset button handling
│   ├── 📁 web_server/            # WiFi configuration portal
│   ├── 📁 brightness_config/     # Brightness configuration
│   └── 📁 transition_config/     # Animation configuration
├── 
├── 📁 build/                       # Build artifacts (generated)
├── 📄 CMakeLists.txt              # Top-level build configuration
├── 📄 partitions.csv              # Flash partition table
├── 📄 sdkconfig                   # ESP-IDF configuration
├── 📄 sdkconfig.defaults          # Default configuration
└── 📄 sdkconfig.old               # Previous configuration backup
```

## 🏗️ Software Architecture

### Component Dependency Graph

```
main/
├─→ i2c_devices (hardware layer)
├─→ wordclock_time (calculation layer)
├─→ wordclock_display (display layer)
├─→ transition_manager (animation layer)
├─→ adc_manager (potentiometer input)
├─→ light_sensor (ambient light)
├─→ wifi_manager (network connectivity)
├─→ ntp_manager (time synchronization)
├─→ mqtt_manager (IoT communication)
├─→ mqtt_discovery (Home Assistant)
├─→ nvs_manager (persistent storage)
├─→ status_led_manager (visual status)
├─→ button_manager (user input)
├─→ web_server (configuration portal)
├─→ brightness_config (brightness settings)
└─→ transition_config (animation settings)
```

### Architectural Layers

```
┌─────────────────────────────────────────┐
│               Application Layer          │
│  (main/wordclock.c - German word logic) │
├─────────────────────────────────────────┤
│              IoT Integration Layer       │
│     (WiFi, NTP, MQTT, Home Assistant)   │
├─────────────────────────────────────────┤
│             User Interface Layer         │
│  (Web server, status LEDs, buttons)     │
├─────────────────────────────────────────┤
│              Control Layer               │
│  (Display, transitions, brightness)     │
├─────────────────────────────────────────┤
│              Hardware Layer              │
│   (I2C devices, sensors, GPIO)          │
└─────────────────────────────────────────┘
```

## 🧩 Component Details

### Core Components

#### `i2c_devices/` - Hardware Communication
- **Purpose**: Low-level I2C device drivers
- **Devices**: TLC59116 LED controllers, DS3231 RTC
- **APIs**: Device initialization, register access, error handling
- **Dependencies**: ESP-IDF `driver` component

#### `wordclock_display/` - Display Control  
- **Purpose**: High-level LED matrix control
- **Features**: German word positioning, brightness control, state tracking
- **APIs**: Display update, brightness setting, LED control
- **Dependencies**: `i2c_devices`, `wordclock_time`

#### `wordclock_time/` - Time Processing
- **Purpose**: German time calculation and word conversion
- **Features**: Grammar rules, minute indicators, time validation
- **APIs**: Time-to-words conversion, indicator calculation
- **Dependencies**: `i2c_devices` (for time structures)

### Enhanced Features

#### `transition_manager/` - Animation System
- **Purpose**: Smooth LED transitions and animations
- **Features**: Multiple curves, priority system, 20Hz updates
- **APIs**: Transition requests, animation control, curve selection
- **Dependencies**: `i2c_devices`, FreeRTOS tasks

#### `adc_manager/` - Potentiometer Input
- **Purpose**: Manual brightness control via potentiometer
- **Features**: 8-sample averaging, calibration, smoothing
- **APIs**: ADC reading, voltage-to-brightness mapping
- **Dependencies**: ESP-IDF `driver` (ADC)

#### `light_sensor/` - Ambient Light Detection
- **Purpose**: Automatic brightness adaptation
- **Features**: BH1750 integration, 5-zone mapping, instant response
- **APIs**: Light reading, brightness calculation, task management
- **Dependencies**: `i2c_devices`, FreeRTOS

### IoT Components

#### `wifi_manager/` - Network Connectivity
- **Purpose**: WiFi connection management
- **Features**: Auto-connect, AP mode, credential storage
- **APIs**: Connection management, status monitoring, event handling
- **Dependencies**: ESP-IDF WiFi stack, `nvs_manager`

#### `mqtt_manager/` - IoT Communication
- **Purpose**: MQTT client with TLS encryption
- **Features**: HiveMQ Cloud integration, command processing, status publishing
- **APIs**: Message publishing, command handling, connection management
- **Dependencies**: ESP-IDF MQTT client, `wifi_manager`

#### `ntp_manager/` - Time Synchronization
- **Purpose**: Internet time synchronization
- **Features**: Vienna timezone, RTC integration, automatic sync
- **APIs**: Time sync, RTC update, status monitoring
- **Dependencies**: ESP-IDF SNTP, `wifi_manager`, `i2c_devices`

#### `mqtt_discovery/` - Home Assistant Integration
- **Purpose**: Automatic Home Assistant entity creation
- **Features**: 12 entity types, device grouping, zero configuration
- **APIs**: Discovery message generation, entity management
- **Dependencies**: `mqtt_manager`

### System Components

#### `nvs_manager/` - Persistent Storage
- **Purpose**: Non-volatile storage for configuration
- **Features**: WiFi credentials, MQTT settings, error handling
- **APIs**: Save/load credentials, clear storage, validation
- **Dependencies**: ESP-IDF NVS

#### `status_led_manager/` - Visual Status
- **Purpose**: Network status indication via LEDs
- **Features**: WiFi and NTP status, multiple states (off/blink/on)
- **APIs**: LED control, status updates, test sequences
- **Dependencies**: ESP-IDF GPIO, FreeRTOS tasks

#### `button_manager/` - User Input
- **Purpose**: Reset button handling
- **Features**: Long-press detection, debouncing, WiFi reset
- **APIs**: Button monitoring, event handling
- **Dependencies**: ESP-IDF GPIO, `nvs_manager`

#### `web_server/` - Configuration Portal
- **Purpose**: WiFi setup via web interface
- **Features**: Network scanning, credential entry, responsive UI
- **APIs**: Server control, request handling, page generation
- **Dependencies**: ESP-IDF HTTP server, `wifi_manager`

## 🔧 Build System

### ESP-IDF Component Structure

Each component follows ESP-IDF conventions:

```
component_name/
├── CMakeLists.txt              # Build configuration
├── component_name.c            # Implementation
└── include/
    └── component_name.h        # Public API
```

### Component Registration

```cmake
# CMakeLists.txt example
idf_component_register(
    SRCS "component_name.c"
    INCLUDE_DIRS "include"
    REQUIRES "dependency1" "dependency2"
)
```

### Dependency Management

- **Linear Dependencies**: Avoid circular component dependencies
- **Minimal Requires**: Only include necessary component dependencies  
- **Shared Types**: Define common types in central header files
- **Function Ownership**: Each function defined in exactly one component

## 🔍 Code Organization Principles

### Modularity
- Each component has a single, well-defined responsibility
- Public APIs are minimal and focused
- Internal implementation details are hidden

### Separation of Concerns
- **Hardware Layer**: Low-level device communication
- **Logic Layer**: Application-specific algorithms
- **Integration Layer**: IoT and networking features
- **Interface Layer**: User interaction and configuration

### Error Handling
- Consistent error codes using ESP-IDF conventions
- Graceful degradation when components fail
- Comprehensive logging at appropriate levels

### Memory Management
- Prefer static allocation for predictable memory usage
- Avoid dynamic allocation in interrupt contexts
- Use FreeRTOS primitives for task synchronization

## 📊 Performance Characteristics

### Resource Usage
- **Flash**: ~1.5MB total (application + components)
- **RAM**: ~100KB static + ~50KB heap usage
- **Tasks**: 6 concurrent FreeRTOS tasks
- **Stack**: 4KB per major task

### Timing Requirements
- **Main Loop**: 5-second cycle for responsive operation
- **Animation**: 20Hz (50ms) for smooth transitions
- **Light Sensor**: 10Hz (100ms) for instant response
- **I2C Operations**: 5-25 operations per display update

### Scalability Limits
- **LED Transitions**: Maximum 32 concurrent animations
- **Component Count**: Practical limit of 11 ESP-IDF components
- **I2C Devices**: 10 TLC59116 + 2 sensors on dual buses

## 🚀 Development Workflow

### Adding New Components

1. **Create Directory**: `components/new_component/`
2. **Add CMakeLists.txt**: Define build rules and dependencies
3. **Implement API**: Create `.c` and `.h` files
4. **Update Main**: Add component to main application dependencies
5. **Test Integration**: Verify build and runtime functionality
6. **Document**: Update this file and relevant documentation

### Modifying Existing Components

1. **Review Dependencies**: Check which components depend on changes
2. **Maintain APIs**: Preserve backward compatibility when possible
3. **Update Tests**: Modify or add tests for changed functionality
4. **Update Documentation**: Reflect changes in relevant docs

### Performance Optimization

1. **Profile First**: Measure actual performance bottlenecks
2. **Optimize Critical Path**: Focus on main loop and interrupt handlers
3. **Minimize I2C Operations**: Use differential updates and batching
4. **Static Allocation**: Prefer compile-time memory allocation

---

This structure supports the project's evolution from a simple word clock to a comprehensive IoT device while maintaining clean architecture and ESP-IDF best practices.