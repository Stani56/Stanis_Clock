# Project Structure

**Last Updated:** November 2025 (Post SHA-256 Verification Implementation)
**Platform:** ESP32-S3 (YB-ESP32-S3-AMP) - Production Hardware
**Status:** Phase 2.4 Complete - Multi-board Support + Audio + Westminster Chimes + OTA

This document describes the architecture and organization of the ESP32-S3 German Word Clock project after the November 2025 OTA automation and SHA-256 verification implementation.

## 📁 Repository Layout

```
Stanis_Clock/
├── 📄 README.md                    # Main project documentation
├── 📄 CLAUDE.md                    # Developer reference (quick technical guide)
├── 📄 Mqtt_User_Guide.md           # MQTT command reference
├── 📄 LICENSE                      # MIT License
├── 📄 partitions.csv               # Flash partition table (8MB, dual OTA)
├── 📄 sdkconfig                    # ESP-IDF configuration
├── 📄 CMakeLists.txt               # Top-level build configuration
├── 📄 post_build_ota.sh            # ⭐ NEW: Post-build automation (SHA-256 + version.json)
├── 📄 Makefile                     # ⭐ NEW: Build convenience wrapper
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
├── 📁 ota_files/                   # ⭐ OTA firmware distribution directory
│   ├── wordclock.bin               # Compiled firmware binary
│   ├── wordclock.bin.sha256        # SHA-256 checksum file
│   └── version.json                # Firmware metadata with SHA-256
│
├── 📁 components/                  # ESP-IDF components (30 total)
│   │
│   ├── 📁 Hardware Layer (9 components)
│   │   ├── board_config/          # ⭐ Multi-board hardware abstraction (YB-AMP vs DevKitC)
│   │   ├── i2c_devices/           # TLC59116 LED controllers + DS3231 RTC
│   │   ├── adc_manager/           # Potentiometer input
│   │   ├── light_sensor/          # BH1750 ambient light
│   │   ├── button_manager/        # Reset button handling
│   │   ├── status_led_manager/    # Network status indicators
│   │   ├── audio_manager/         # ⭐ I2S audio output (MAX98357A) - ACTIVE on ESP32-S3
│   │   ├── sdcard_manager/        # ⭐ SD card storage (SPI mode) - ACTIVE on ESP32-S3
│   │   ├── external_flash/        # W25Q64 8MB SPI flash driver (OPTIONAL, archived)
│   │   └── filesystem_manager/    # LittleFS on external flash (OPTIONAL, archived)
│   │
│   ├── 📁 Display Layer (3 components)
│   │   ├── wordclock_display/     # German word logic + LED matrix
│   │   ├── wordclock_time/        # Time calculation + grammar
│   │   └── transition_manager/    # Smooth LED animations (20Hz)
│   │
│   ├── 📁 Network Layer (6 components)
│   │   ├── wifi_manager/          # Auto-connect + AP mode
│   │   ├── ntp_manager/           # Vienna timezone sync
│   │   ├── mqtt_manager/          # HiveMQ Cloud TLS client + Dual OTA source control
│   │   ├── mqtt_discovery/        # Home Assistant integration (36 entities)
│   │   ├── ota_manager/           # ⭐ OTA updates with SHA-256 verification
│   │   └── web_server/            # WiFi configuration interface
│   │
│   ├── 📁 MQTT Tier 1 Components (3 components)
│   │   ├── mqtt_schema_validator/      # Schema validation
│   │   ├── mqtt_command_processor/     # Structured commands
│   │   └── mqtt_message_persistence/   # Reliable delivery
│   │
│   ├── 📁 System Services (6 components)
│   │   ├── nvs_manager/           # Credential storage + OTA source preference
│   │   ├── system_init/           # System initialization
│   │   ├── brightness_config/     # 5-zone adaptive brightness
│   │   ├── transition_config/     # Animation configuration
│   │   ├── error_log_manager/     # Persistent error logging (50 entries)
│   │   └── led_validation/        # LED hardware validation system
│   │
│   └── 📁 Audio Components (2 components - ⭐ ACTIVE on ESP32-S3)
│       ├── chime_manager/         # Westminster chimes scheduler (Quarter/Half/Hour strikes)
│       └── chime_library/         # Chime audio library (PCM playback from SD card)
│
└── 📁 docs/                        # Comprehensive documentation
    ├── README.md                   # Documentation navigation
    ├── user/                       # End-user guides
    ├── developer/                  # Developer documentation
    │   ├── api-reference.md       # ⭐ UPDATED: All 30 components documented
    │   ├── post-build-automation-guide.md  # ⭐ NEW: OTA automation workflow
    │   └── project-structure.md   # This file
    ├── implementation/             # Technical deep-dives
    │   ├── mqtt/                  # MQTT system docs
    │   ├── led-validation/        # LED validation system
    │   └── esp32-s3-migration/    # ESP32-S3 upgrade guide (complete)
    ├── proposals/                  # Feature proposals
    ├── technical/                  # Technical analysis
    │   └── dual-ota-source-system.md  # ⭐ NEW: Dual OTA architecture
    ├── testing/                    # Testing procedures
    ├── maintenance/                # Operations & maintenance
    └── archive/                    # Historical documentation
```

## 🏗️ Software Architecture

### Component Structure (30 Components)

**Hardware Layer (9 components):**
- `board_config` - ⭐ Multi-board hardware abstraction (YB-AMP vs DevKitC-1)
- `i2c_devices` - TLC59116 LED controllers + DS3231 RTC
- `adc_manager` - Potentiometer input
- `light_sensor` - BH1750 ambient light
- `button_manager` - Reset button handling
- `status_led_manager` - Network status indicators
- `audio_manager` - ⭐ I2S audio output (MAX98357A) - ACTIVE on ESP32-S3
- `sdcard_manager` - ⭐ SD card storage (SPI mode) - ACTIVE on ESP32-S3
- `external_flash` - W25Q64 8MB SPI flash driver (OPTIONAL, archived)
- `filesystem_manager` - LittleFS on external flash (OPTIONAL, archived)

**Display Layer (3 components):**
- `wordclock_display` - German word logic + LED matrix
- `wordclock_time` - Time calculation + grammar
- `transition_manager` - Smooth LED animations (20Hz)

**Network Layer (6 components):**
- `wifi_manager` - Auto-connect + AP mode
- `ntp_manager` - Vienna timezone sync
- `mqtt_manager` - HiveMQ Cloud TLS client + Dual OTA source control
- `mqtt_discovery` - Home Assistant integration (36 entities)
- `ota_manager` - ⭐ OTA updates with SHA-256 firmware verification
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

**Audio Components (2 components - ⭐ ACTIVE on ESP32-S3):**
- `chime_manager` - Westminster chimes scheduler (Quarter/Half/Hour strikes)
- `chime_library` - Chime audio library (PCM playback from SD card)

> **Note:** Audio components are fully operational on ESP32-S3 hardware with concurrent WiFi+MQTT+I2S support. Westminster chimes play from SD card at Quarter Past, Half Past, Quarter To, and on the Hour. See [Post-Build Automation Guide](../developer/post-build-automation-guide.md) for firmware deployment.

### Architectural Layers

```
┌───────────────────────────────────────────────────┐
│         Application Layer (main/)                 │
│  wordclock.c, wordclock_display.c,               │
│  wordclock_brightness.c, wordclock_transitions.c  │
├───────────────────────────────────────────────────┤
│         IoT Integration Layer                     │
│  WiFi, NTP, MQTT, Home Assistant Discovery       │
│  ⭐ OTA Manager (SHA-256 firmware verification)   │
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
│         Media Layer (ESP32-S3)                    │
│  ⭐ Audio Manager, Chime Manager, SD Card        │
├───────────────────────────────────────────────────┤
│         Hardware Layer                            │
│  ⭐ Board Config, I2C devices, Sensors, GPIO      │
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

#### board_config ⭐ NEW
- **Multi-board hardware abstraction** for YB-ESP32-S3-AMP and ESP32-S3-DevKitC-1
- **GPIO mapping** - Configures different pins for each board variant
- **PSRAM configuration** - Quad SPI (YB-AMP) vs Octal SPI (DevKitC)
- **Feature detection** - Integrated vs external peripherals
- **Build-time selection** via CONFIG_BOARD_DEVKITC define

#### i2c_devices
- **10× TLC59116 LED controllers** @ 0x60-0x6A (160 LEDs total)
- **DS3231 RTC** @ 0x68
- **BH1750 light sensor** @ 0x23
- **Two I2C buses:** Bus 0 (GPIO 8/9) for LEDs, Bus 1 (GPIO 1/42) for sensors (YB-AMP)
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
- Status: Phase 1 complete (Oct 2025), archived in favor of SD card

#### audio_manager ⭐ ACTIVE
- **I2S audio output** via MAX98357A amplifiers
- **GPIO Configuration:** BCLK=5, LRCLK=6, DIN=7
- **Sample Rate:** 16kHz, 16-bit mono PCM
- **Concurrent WiFi+I2S** - ESP32-S3 eliminates hardware conflicts
- **Status:** Fully operational on ESP32-S3

#### sdcard_manager ⭐ ACTIVE
- **SD card storage** via SPI interface (GPIO 10/11/12/13)
- **FatFS filesystem** with long filename support
- **Mount point:** `/sdcard`
- **Westminster chime storage:** /sdcard/CHIMES/WESTMINSTER/*.PCM
- **Status:** Phase 2.2 complete, production-ready

### Network Components

#### ota_manager ⭐ NEW (November 2025)
- **Dual OTA source support** - GitHub (primary) + Cloudflare R2 (backup)
- **⭐ SHA-256 firmware verification** - Cryptographic integrity checking
- **Automatic failover** - Switches to backup source on failure
- **NVS source preference** - Persists last successful source
- **MQTT control** - Remote OTA triggering and source management
- **Security:**
  - SHA-256 checksum validation before flashing
  - Detects network corruption and tampering
  - 2^256 collision resistance
  - Verification time: 3-5 seconds
  - Auto-abort on mismatch

#### mqtt_manager (Updated)
- **HiveMQ Cloud TLS client** with dual OTA source control
- **New MQTT topics** for OTA source management:
  - `home/wordclock/ota/source/set` - Set OTA source (github/cloudflare)
  - `home/wordclock/ota/source/status` - Current source status
  - `home/wordclock/ota/check` - Trigger update check
  - `home/wordclock/ota/status` - OTA progress monitoring

### Audio Components (ESP32-S3)

#### chime_manager ⭐ ACTIVE
- **Westminster chimes scheduler** - Quarter Past, Half Past, Quarter To, On the Hour
- **PCM playback** - 16kHz 16-bit mono from SD card
- **File structure:**
  - QUARTER_PAST.PCM - 15 minutes past
  - HALF_PAST.PCM - 30 minutes past
  - QUARTER_TO.PCM - 45 minutes past
  - HOUR.PCM - On the hour chime
  - STRIKE.PCM - Hour count (0-12 strikes)
- **Status:** Phase 2.3 complete, production-ready

#### chime_library
- **Chime audio library** - PCM playback engine
- **SD card integration** - Reads from /sdcard/CHIMES/
- **Audio buffering** - Efficient streaming playback
- **Status:** Fully operational on ESP32-S3

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
- **Flash:** 1.27MB application (49% free in 2.5MB ota_0 partition)
- **RAM:** ~160KB static + ~80KB heap (with PSRAM: 2.3MB available)
- **PSRAM:** 2MB Quad SPI (YB-AMP) or 8MB Octal SPI (DevKitC-1)
- **Tasks:** 10+ concurrent FreeRTOS tasks (including audio and chime tasks)
- **Component Count:** 30 ESP-IDF components (28 active, 2 archived)

### Timing Requirements
- **LED Updates:** 12-20ms per display update
- **Animation:** 20Hz (50ms) for smooth transitions
- **Light Sensor:** 10Hz (100ms) task for instant response
- **I2C Operations:** 5-25 operations per update (95% reduction via differential updates)
- **Audio Playback:** 16kHz sample rate, <50ms latency
- **Chime Scheduling:** Second-precision Westminster chime timing
- **OTA SHA-256 Verification:** 3-5 seconds (1.3MB firmware)

### Critical Optimization
- **I2C Differential Updates:** Only modify changed LEDs (not all 160)
- **LED State Tracking:** In-memory state prevents redundant I2C operations
- **Batch Operations:** 2ms spacing between I2C commands
- **Direct Device Handles:** No address lookups on critical path

## 🔧 Build Configuration

### ESP-IDF Settings
- **Platform:** ESP32-S3 (esp32s3 target)
- **ESP-IDF Version:** 5.4.2
- **Flash Size:** 8MB (YB-AMP) or 16MB (DevKitC-1)
- **Partition Table:** Custom dual-OTA (see partitions.csv)
- **FreeRTOS Hz:** 1000 (1ms tick rate)
- **PSRAM:** Quad SPI 2MB (YB-AMP) or Octal SPI 8MB (DevKitC-1)

### Partition Table (8MB Flash - YB-ESP32-S3-AMP)
```csv
# ESP32-S3 German Word Clock - OTA-enabled partition table (8MB flash)
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,    # ⭐ NEW: OTA data partition
phy_init, data, phy,     0x11000, 0x1000,
ota_0,    app,  ota_0,   0x20000, 0x280000,  # ⭐ 2.5MB app (primary)
ota_1,    app,  ota_1,   0x2a0000, 0x280000, # ⭐ 2.5MB app (secondary)
storage,  data, fat,     0x520000, 0x160000, # 1.5MB storage
```

### Build Workflow ⭐ NEW
```bash
# Traditional build
idf.py build flash monitor

# Automated OTA release (interactive)
make ota-prepare

# Automated OTA release (fully automated)
make ota-release

# Just build
make build
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

**Nov 2025: OTA Automation + SHA-256 Verification ⭐**
- Post-build automation script (SHA-256 + version.json generation)
- Makefile build convenience wrapper
- SHA-256 firmware verification in OTA manager
- Dual OTA source documentation
- Complete API reference update (30 components)
- Comprehensive post-build automation guide

**Nov 2025: ESP32-S3 Migration Complete ✅ (Phase 2.4)**
- YelloByte YB-ESP32-S3-AMP board (production hardware)
- Multi-board support with board_config abstraction
- Concurrent WiFi+MQTT+I2S audio playback
- Westminster chimes from SD card (Phase 2.3)
- SD card storage with FatFS (Phase 2.2)
- I2S audio output (Phase 2.1)
- PSRAM optimization (2.3MB heap vs 303KB on ESP32)

**Oct 2025: Production Features**
- LED validation system with hardware readback
- Persistent error logging (50 entries, NVS)
- TLC59116 auto-increment pointer fix
- External flash + LittleFS (Phase 1, archived)

**Legacy: ESP32 Baseline**
- Tagged as `v1.0-esp32-final`
- Audio disabled due to hardware conflicts
- 4MB flash, no PSRAM
- Stable baseline preserved for reference

---

**For detailed technical information, see [CLAUDE.md](../../CLAUDE.md) and [docs/](../README.md)**
