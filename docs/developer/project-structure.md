# Project Structure

**Last Updated:** November 2025
**Platform:** ESP32 Baseline (Audio Disabled)

This document describes the architecture and organization of the ESP32 German Word Clock project after the November 2025 cleanup and restructuring.

## 📁 Repository Layout

```
Stanis_Clock/
├── 📄 README.md                    # Main project documentation
├── 📄 CLAUDE.md                    # Developer reference (quick technical guide)
├── 📄 Mqtt_User_Guide.md           # MQTT command reference
├── 📄 LICENSE                      # MIT License
├── 📄 partitions.csv               # Flash partition table (4MB)
├── 📄 sdkconfig                    # ESP-IDF configuration
├── 📄 CMakeLists.txt               # Top-level build configuration
│
├── 📁 main/                        # ESP-IDF main application
│   ├── wordclock.c                 # Core application logic (397 lines)
│   ├── wordclock_display.c/h       # German word display logic (369 lines)
│   ├── wordclock_brightness.c/h    # Dual brightness control (338 lines)
│   ├── wordclock_transitions.c/h   # LED animation coordination (467 lines)
│   ├── thread_safety.c/h           # Mutex hierarchy + thread-safe accessors (334 lines)
│   ├── wordclock_mqtt_handlers.c/h # MQTT command processing (275 lines)
│   ├── wordclock_error_log_mqtt.c/h # Error log MQTT integration
│   └── wordclock_mqtt_control.h    # MQTT command definitions
│
├── 📁 components/                  # ESP-IDF components (26 total)
│   │
│   ├── 📁 Hardware Layer (6 components)
│   │   ├── i2c_devices/           # TLC59116 LED controllers + DS3231 RTC
│   │   ├── adc_manager/           # Potentiometer input
│   │   ├── light_sensor/          # BH1750 ambient light
│   │   ├── button_manager/        # Reset button handling
│   │   ├── status_led_manager/    # Network status indicators
│   │   ├── external_flash/        # W25Q64 8MB SPI flash driver (OPTIONAL)
│   │   └── filesystem_manager/    # LittleFS on external flash (OPTIONAL)
│   │
│   ├── 📁 Display Layer (3 components)
│   │   ├── wordclock_display/     # German word logic + LED matrix
│   │   ├── wordclock_time/        # Time calculation + grammar
│   │   └── transition_manager/    # Smooth LED animations (20Hz)
│   │
│   ├── 📁 Network Layer (5 components)
│   │   ├── wifi_manager/          # Auto-connect + AP mode
│   │   ├── ntp_manager/           # Vienna timezone sync
│   │   ├── mqtt_manager/          # HiveMQ Cloud TLS client
│   │   ├── mqtt_discovery/        # Home Assistant integration (36 entities)
│   │   └── web_server/            # WiFi configuration interface
│   │
│   ├── 📁 MQTT Tier 1 Components (3 components)
│   │   ├── mqtt_schema_validator/      # Schema validation
│   │   ├── mqtt_command_processor/     # Structured commands
│   │   └── mqtt_message_persistence/   # Reliable delivery
│   │
│   ├── 📁 System Services (5 components)
│   │   ├── nvs_manager/           # Credential storage
│   │   ├── system_init/           # System initialization
│   │   ├── brightness_config/     # 5-zone adaptive brightness
│   │   ├── transition_config/     # Animation configuration
│   │   ├── error_log_manager/     # Persistent error logging (50 entries)
│   │   └── led_validation/        # LED hardware validation system
│   │
│   └── 📁 Audio Components (DISABLED - 2 components)
│       ├── audio_manager/         # Audio playback (NOT INITIALIZED on ESP32)
│       └── chime_library/         # Chime audio library (NOT USED on ESP32)
│
└── 📁 docs/                        # Comprehensive documentation
    ├── README.md                   # Documentation navigation
    ├── user/                       # End-user guides
    ├── developer/                  # Developer documentation
    ├── implementation/             # Technical deep-dives
    │   ├── mqtt/                  # MQTT system docs
    │   ├── led-validation/        # LED validation system
    │   └── esp32-s3-migration/    # ESP32-S3 upgrade guide
    ├── proposals/                  # Feature proposals
    ├── technical/                  # Technical analysis
    ├── testing/                    # Testing procedures
    ├── maintenance/                # Operations & maintenance
    └── archive/                    # Historical documentation
```

## 🏗️ Software Architecture

### Component Structure (26 Components)

**Hardware Layer (7 components):**
- `i2c_devices` - TLC59116 LED controllers + DS3231 RTC
- `adc_manager` - Potentiometer input
- `light_sensor` - BH1750 ambient light
- `button_manager` - Reset button handling
- `status_led_manager` - Network status indicators
- `external_flash` - W25Q64 8MB SPI flash driver (OPTIONAL, Phase 1 complete)
- `filesystem_manager` - LittleFS on external flash (OPTIONAL, Phase 1 complete)

**Display Layer (3 components):**
- `wordclock_display` - German word logic + LED matrix
- `wordclock_time` - Time calculation + grammar
- `transition_manager` - Smooth LED animations (20Hz)

**Network Layer (5 components):**
- `wifi_manager` - Auto-connect + AP mode
- `ntp_manager` - Vienna timezone sync
- `mqtt_manager` - HiveMQ Cloud TLS client
- `mqtt_discovery` - Home Assistant integration (36 entities)
- `web_server` - WiFi configuration interface

**MQTT Tier 1 Components (3 components):**
- `mqtt_schema_validator` - Schema validation
- `mqtt_command_processor` - Structured commands
- `mqtt_message_persistence` - Reliable delivery

**System Services (6 components):**
- `nvs_manager` - Credential storage
- `system_init` - System initialization
- `brightness_config` - 5-zone adaptive brightness
- `transition_config` - Animation configuration
- `error_log_manager` - Persistent error logging (50 entries)
- `led_validation` - LED hardware validation system

**Audio Components (2 components - DISABLED on ESP32):**
- `audio_manager` - Audio playback (code present but NOT initialized)
- `chime_library` - Chime audio library (code present but NOT used)

> **Note:** Audio components are present in the codebase but not initialized on ESP32 due to WiFi+MQTT+I2S hardware conflicts. They will be re-enabled on ESP32-S3 hardware. See [ESP32-S3 Migration Analysis](../implementation/esp32-s3-migration/ESP32-S3-Migration-Analysis.md).

### Architectural Layers

```
┌───────────────────────────────────────────────────┐
│         Application Layer (main/)                 │
│  wordclock.c, wordclock_display.c,               │
│  wordclock_brightness.c, wordclock_transitions.c  │
├───────────────────────────────────────────────────┤
│         IoT Integration Layer                     │
│  WiFi, NTP, MQTT, Home Assistant Discovery       │
├───────────────────────────────────────────────────┤
│         MQTT Tier 1 (Production Features)         │
│  Schema validation, Command processor, Persistence│
├───────────────────────────────────────────────────┤
│         User Interface Layer                      │
│  Web server, Status LEDs, Button, Error logging  │
├───────────────────────────────────────────────────┤
│         Control Layer                             │
│  Display, Transitions, Brightness, LED validation│
├───────────────────────────────────────────────────┤
│         Hardware Layer                            │
│  I2C devices, Sensors, GPIO, External flash      │
└───────────────────────────────────────────────────┘
```

## 🧩 Key Component Details

### Core Application (main/)

**Main Files:**
- **wordclock.c** (397 lines) - System orchestration, initialization sequence
- **wordclock_display.c** (369 lines) - German word display logic
- **wordclock_brightness.c** (338 lines) - Dual brightness control (potentiometer + light sensor)
- **wordclock_transitions.c** (467 lines) - LED animation coordination
- **thread_safety.c** (334 lines) - Mutex hierarchy + 22 thread-safe accessors
- **wordclock_mqtt_handlers.c** (275 lines) - MQTT command processing

### Hardware Components

#### i2c_devices
- **10× TLC59116 LED controllers** @ 0x60-0x6A (160 LEDs total)
- **DS3231 RTC** @ 0x68
- **BH1750 light sensor** @ 0x23
- **Two I2C buses:** Bus 0 (GPIO 25/26) for LEDs, Bus 1 (GPIO 18/19) for sensors
- **I2C Speed:** 100kHz (conservative for 10-device reliability)

#### external_flash (OPTIONAL)
- **W25Q64 8MB SPI flash** on HSPI bus (GPIO 12/13/14/15)
- Dynamic partition registration as "ext_storage"
- Status: Phase 1 complete (Oct 2025)
- System works without W25Q64 installed (graceful degradation)

#### filesystem_manager (OPTIONAL)
- **LittleFS filesystem** on external flash
- Mount point: `/storage`
- Auto-created directories: `/storage/chimes`, `/storage/config`
- Wear leveling, power-loss protection
- Status: Phase 1 complete (Oct 2025)

### Production Features (Oct 2025)

#### error_log_manager
- **50-entry circular buffer** in NVS (survives reboots)
- ~5.6KB storage (50 entries × 112 bytes)
- 8 error sources: LED_VALIDATION, I2C_BUS, WIFI, MQTT, NTP, SYSTEM, POWER, SENSOR
- 4 severities: INFO, WARNING, ERROR, CRITICAL
- MQTT integration for remote error querying

#### led_validation
- **Post-transition validation** (~5 minutes interval)
- Byte-by-byte PWM readback from TLC59116 hardware
- 3-level validation: Software state → Hardware PWM → TLC fault detection
- ~130ms validation time (16 reads × 10 devices)
- MQTT publishing of validation results
- Manual recovery workflow (auto-recovery disabled)

### Thread Safety System

**5-Level Mutex Hierarchy:**
1. Network status (wifi_connected, ntp_synced, mqtt_connected)
2. Brightness (global_brightness, potentiometer_individual)
3. LED state (led_state[10][16] array)
4. Transitions (animation state)
5. I2C devices (bus communication)

**22 Thread-Safe Accessors** - Always use instead of direct global access

## 📊 Performance Characteristics

### Resource Usage
- **Flash:** 1.2MB application (53% free in 2.5MB partition)
- **RAM:** ~120KB static + ~60KB heap
- **Tasks:** 8 concurrent FreeRTOS tasks
- **Component Count:** 26 ESP-IDF components (24 active, 2 disabled)

### Timing Requirements
- **LED Updates:** 12-20ms per display update
- **Animation:** 20Hz (50ms) for smooth transitions
- **Light Sensor:** 10Hz (100ms) task for instant response
- **I2C Operations:** 5-25 operations per update (95% reduction via differential updates)

### Critical Optimization
- **I2C Differential Updates:** Only modify changed LEDs (not all 160)
- **LED State Tracking:** In-memory state prevents redundant I2C operations
- **Batch Operations:** 2ms spacing between I2C commands
- **Direct Device Handles:** No address lookups on critical path

## 🔧 Build Configuration

### ESP-IDF Settings
- **Platform:** ESP32 (esp32 target)
- **ESP-IDF Version:** 5.4.2
- **Flash Size:** 4MB
- **Partition Table:** Custom (see partitions.csv)
- **FreeRTOS Hz:** 1000 (1ms tick rate)

### Partition Table
```csv
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x280000,  # 2.5MB app
storage,  data, fat,     0x290000, 0x160000,  # 1.5MB storage
```

### Component Dependencies

**No Circular Dependencies:**
- Linear dependency chain maintained
- Shared types in central headers
- Each function owned by exactly one component

## 🚀 Development Workflow

### Adding New Components

1. Create directory: `components/new_component/`
2. Add `CMakeLists.txt` with dependencies
3. Implement `.c` and `.h` files
4. Add to `main/CMakeLists.txt` REQUIRES
5. Build and test: `idf.py build`
6. Document in this file and CLAUDE.md

### Modifying Existing Components

1. Check component dependencies (avoid breaking changes)
2. Use thread-safe accessors (never direct globals)
3. Test I2C changes thoroughly (10 devices on bus)
4. Update relevant documentation

### Performance Optimization

1. **Profile first** - Measure actual bottlenecks
2. **Optimize I2C** - Minimize operations (biggest impact)
3. **Static allocation** - Avoid heap in critical paths
4. **Differential updates** - Only change what's needed

## 📈 Project Evolution

**Nov 2025: ESP32 Baseline Cleanup**
- Audio subsystem removed (WiFi+MQTT+I2S conflicts)
- Documentation reorganized (44 files, 26% reduction)
- 25 files archived with context preservation
- Clean, stable baseline for ESP32 hardware

**Oct 2025: Production Features**
- LED validation system with hardware readback
- Persistent error logging (50 entries, NVS)
- TLC59116 auto-increment pointer fix

**Future: ESP32-S3 Migration**
- YelloByte YB-ESP32-S3-AMP board
- Concurrent WiFi+MQTT+I2S support
- Re-enable audio_manager and chime_library
- microSD card storage (FatFS migration)
- See: [ESP32-S3 Migration Analysis](../implementation/esp32-s3-migration/ESP32-S3-Migration-Analysis.md)

---

**For detailed technical information, see [CLAUDE.md](../../CLAUDE.md) and [docs/](../README.md)**
