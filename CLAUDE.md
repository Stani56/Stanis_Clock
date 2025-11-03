# ESP32 German Word Clock - Developer Reference

## Quick Reference

**Platform:** ESP32 with ESP-IDF 5.4.2
**Repository:** https://github.com/stani56/Stanis_Clock
**Status:** Production-ready IoT word clock with Home Assistant integration

## Essential Build Information

### Hardware Setup
```
GPIO 18/19 → I2C Sensors (DS3231 RTC, BH1750 light sensor)
GPIO 25/26 → I2C LEDs (10× TLC59116 controllers @ 0x60-0x6A)
GPIO 34    → Potentiometer (brightness control)
GPIO 21/22 → Status LEDs (WiFi/NTP indicators)
GPIO 5     → Reset button (WiFi credentials clear)
GPIO 12/13/14/15 → SPI External Flash (W25Q64, 8MB for chime audio - OPTIONAL)
                    GPIO 12=MISO, 13=MOSI, 14=CLK, 15=CS
```

**LED Matrix:** 10 rows × 16 columns (160 LEDs)
**I2C Speed:** 100kHz (conservative for reliability)
**Flash:** 4MB minimum, 2.5MB app partition (see `partitions.csv`)
**External Flash:** W25Q64 8MB SPI (optional, for chime system expansion)

### Quick Start
```bash
# Clone and build
git clone https://github.com/stani56/Stanis_Clock.git
cd Stanis_Clock
idf.py build flash monitor

# Multi-device setup: Edit components/mqtt_manager/include/mqtt_manager.h
# Change MQTT_DEVICE_NAME to unique value (e.g., "wordclock_bedroom")
```

**WiFi Setup:** Connect to "ESP32-LED-Config" (password: 12345678), visit http://192.168.4.1

## System Architecture

### Component Structure (23 Components)
```
Hardware Layer:
├── i2c_devices        - TLC59116 LED controllers + DS3231 RTC
├── adc_manager        - Potentiometer input
├── light_sensor       - BH1750 ambient light
├── button_manager     - Reset button handling
├── status_led_manager - Network status indicators
├── external_flash     - W25Q64 8MB SPI flash driver (OPTIONAL, for chime audio)
└── filesystem_manager - LittleFS on external flash (OPTIONAL, Phase 1 complete ✅)

Display Layer:
├── wordclock_display  - German word logic + LED matrix
├── wordclock_time     - Time calculation + grammar
├── transition_manager - Smooth LED animations (20Hz)
└── brightness_config  - 5-zone adaptive brightness

Network Layer:
├── wifi_manager       - Auto-connect + AP mode
├── ntp_manager        - Vienna timezone sync
├── mqtt_manager       - HiveMQ Cloud TLS client
├── mqtt_discovery     - Home Assistant integration (36 entities)
└── web_server         - WiFi configuration interface

System Services:
├── nvs_manager        - Credential storage
├── thread_safety      - Production-grade synchronization
├── error_log_manager  - Persistent error logging (50 entries)
└── led_validation     - LED hardware validation system
```

### Main Application (main/)
| File | Purpose |
|------|---------|
| `wordclock.c` | System orchestration (397 lines) |
| `wordclock_display.c` | German word display logic (369 lines) |
| `wordclock_brightness.c` | Dual brightness control (338 lines) |
| `wordclock_transitions.c` | LED animation coordination (467 lines) |
| `thread_safety.c` | Mutex hierarchy + thread-safe accessors (334 lines) |
| `wordclock_mqtt_handlers.c` | MQTT command processing (275 lines) |

## Critical Technical Details

### German Time Display Logic
```c
// 5-minute floor + 0-4 indicator LEDs
base_minutes = (MM / 5) * 5
indicator_count = MM - base_minutes

// Hour context (25+ minutes uses next hour)
display_hour = (base_minutes >= 25) ? (HH + 1) % 12 : HH % 12

// Grammar: "EIN UHR" at :00, "EINS" elsewhere
hour_word = (base_minutes == 0) ? hour_words_with_uhr[h] : hour_words[h]
```

**Examples:**
- 14:23 → "ES IST ZWANZIG NACH ZWEI" + 3 LEDs
- 14:37 → "ES IST FÜNF NACH HALB DREI" + 2 LEDs

### Brightness System
**Dual Control:**
- **Potentiometer:** Individual LED PWM (5-80, safety limited)
- **Light Sensor:** Global GRPPWM (5-255, 5-zone adaptive)

**Default Values:**
- Individual: 60 (vs old 32 = 42% effective brightness vs 6%)
- Global: 180 (vs old 120)
- Safety limit: 80 PWM max

**Light Sensor Zones:**
```c
Very Dark:  1-10 lux   → 5-30 brightness
Dim:       10-50 lux   → 30-60 brightness
Normal:    50-200 lux  → 60-120 brightness
Bright:   200-500 lux  → 120-200 brightness
Very Bright: 500-1500 lux → 200-255 brightness
```

### I2C Optimization
**95% Operation Reduction:**
- Differential LED updates (only change modified LEDs)
- LED state tracking in memory
- Batch operations with 2ms spacing

**Reliability:**
- 100kHz clock (not 400kHz - more stable for 10 devices)
- 1000ms timeout (generous for recovery)
- Direct device handles (no address lookups)

### Thread Safety
**5-Level Mutex Hierarchy:**
1. Network status (wifi_connected, ntp_synced, mqtt_connected)
2. Brightness (global_brightness, potentiometer_individual)
3. LED state (led_state[10][16] array)
4. Transitions (animation state)
5. I2C devices (bus communication)

**22 Thread-Safe Accessors** - Use these instead of direct global access

### LED Validation System
**Post-Transition Validation:**
- Validates hardware state immediately after transitions complete
- Byte-by-byte PWM reads with explicit no-auto-increment addressing
- Triggers every ~5 minutes when display changes
- 3-level validation: Software state → Hardware PWM → TLC fault detection

**Key Features:**
- I2C mutex protection prevents concurrent access during readback
- Detects both incorrect active LEDs and accidentally lit unused LEDs
- MQTT publishing of validation results with detailed statistics
- ~130ms validation time (16 reads × 10 devices)

**Critical Fix (Oct 2025):**
- TLC59116 auto-increment pointer corruption resolved
- Register address masking with 0x1F disables auto-increment (bits 7:6 = 00)
- Each PWM register read independently with explicit address
- See [docs/implementation/led-validation/tlc59116-validation-fix.md](docs/implementation/led-validation/tlc59116-validation-fix.md) for detailed analysis

**EFLAG Error Detection (Intentionally Disabled):**
- TLC59116 MODE2 register bit 0 (EFAIL) is set to 0 (disabled)
- Reason: ~90 unconnected LED outputs would continuously report false open-circuit errors
- Trade-off: No false positives from partially populated LED matrix, but misses actual LED open/short detection
- Alternative: PWM readback comparison still detects LED failures without false positives
- To enable: Change `mode2_val = 0x00` to `0x01` in i2c_devices.c line 283
- See [docs/implementation/led-validation/LED_VALIDATION_HARDWARE_READBACK.md](docs/implementation/led-validation/LED_VALIDATION_HARDWARE_READBACK.md) section on EFLAG registers

## Home Assistant Integration

### MQTT Discovery (36 Entities)
- **1 Light:** Main display control
- **7 Sensors:** WiFi, NTP, brightness levels, light sensor, potentiometer
- **24 Config Controls:** Zone brightness, curves, safety limits
- **4 Buttons:** Restart, test transitions, refresh sensors, set time

**Topics:** `home/[DEVICE_NAME]/*` (configure via MQTT_DEVICE_NAME)

### Key MQTT Commands
```bash
# Status and control
home/[DEVICE_NAME]/command: "status", "restart", "reset_wifi"

# Brightness control
home/[DEVICE_NAME]/brightness/set: {"individual": 60, "global": 180}

# Transition control
home/[DEVICE_NAME]/transition/set: {"duration": 1500, "enabled": true}

# Brightness config reset
home/[DEVICE_NAME]/brightness/config/reset: "reset"
```

### LED Validation MQTT Topics (Oct 2025)
**Manual Recovery Workflow** - Auto-recovery disabled, user approval required

```bash
# Validation status (published every ~5 minutes after transitions)
home/[DEVICE_NAME]/validation/status
  → {"result": "passed", "timestamp": "2025-10-14T11:15:00Z", "execution_time_ms": 131}

# Detailed validation results
home/[DEVICE_NAME]/validation/last_result
  → {"software_valid": true, "hardware_valid": true, "mismatches": 0, "failure_type": "NONE"}

# Mismatch details (only published when mismatches detected)
home/[DEVICE_NAME]/validation/mismatches
  → {"count": 5, "leds": [{"r": 0, "c": 2, "exp": 0, "sw": 0, "hw": 60, "diff": 60}, ...]}

# Validation statistics and health score
home/[DEVICE_NAME]/validation/statistics
  → {"total_validations": 100, "failed": 2, "health_score": 95, ...}
```

**Recovery Actions:**
- Restart system: `home/[DEVICE_NAME]/command` → "restart"
- Test transitions: HA Button "Test Transitions" (gentle refresh)
- See [docs/user/led-validation-guide.md](docs/user/led-validation-guide.md) for complete guide

### Error Logging MQTT Topics (Oct 2025)
**Persistent Error Storage** - 50-entry circular buffer in NVS, survives reboots

```bash
# Query error log (retrieve recent errors)
home/[DEVICE_NAME]/error_log/query
  → {"count": 10}  # Request 10 recent errors (max: 50)

# Error log response
home/[DEVICE_NAME]/error_log/response
  → {"entries": [{"timestamp": "...", "source": "LED_VALIDATION", "message": "..."}], "count": 10}

# Error log statistics (auto-published on MQTT connect)
home/[DEVICE_NAME]/error_log/stats
  → {"total_errors": 145, "errors_by_source": {...}, "errors_by_severity": {...}}

# Clear error log
home/[DEVICE_NAME]/error_log/clear
  → "clear"        # Clear entries, preserve stats
  → "reset_stats"  # Reset statistics counters
```

**Error Sources:** LED_VALIDATION, I2C_BUS, WIFI, MQTT, NTP, SYSTEM, POWER, SENSOR
**Severities:** INFO, WARNING, ERROR, CRITICAL
**Storage:** ~5.6KB (50 entries × 112 bytes), persists across reboots
**See:** [docs/user/error-logging-guide.md](docs/user/error-logging-guide.md) for complete guide

## Build Configuration

### ESP-IDF Settings
```bash
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_FREERTOS_HZ=1000
```

### Partition Table (partitions.csv)
```csv
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x280000,  # 2.5MB app
storage,  data, fat,     0x290000, 0x160000,  # 1.5MB storage
```

### Common Build Issues
**Partition size error:**
```bash
idf.py menuconfig  # Serial flasher → Flash size → 4MB
idf.py fullclean && idf.py build
```

**IntelliSense not working:** Run `idf.py build` (generates compile_commands.json)

## Performance Metrics

| Metric | Value |
|--------|-------|
| Light sensor response | 100-220ms |
| Potentiometer update | 15 seconds |
| LED update time | 12-20ms |
| I2C ops per update | 5-25 (was 160+) |
| Binary size | 1.2MB (Phase 1 complete) |
| WiFi reconnect | 5-15 seconds |
| System uptime | >99% |

## Critical Lessons Learned

### I2C Reliability
- **100kHz > 400kHz** for multi-device systems (10× TLC59116)
- **Differential updates** reduce bus saturation by 95%
- **Conservative timeouts** (1000ms) prevent system lockup
- **Static allocation** prevents WiFi init timing conflicts
- **I2C mutex protection** critical for all read/write operations (prevents race conditions)

### TLC59116 Auto-Increment Pointer Issue (Oct 2025)
- **Problem:** Auto-increment pointer corruption after differential writes caused readback of LEDOUT registers (0xAA = 170) instead of PWM values
- **Solution:** Explicit no-auto-increment addressing by masking register address with 0x1F (bits 7:6 = 00)
- **Result:** 100% accurate PWM readback for LED validation
- **Key Insight:** "Why write when we only want to read?" - Pure read operations should have no side effects
- **Documentation:** [tlc59116-validation-fix.md](docs/implementation/led-validation/tlc59116-validation-fix.md)

### Brightness System Fixes (Jan 2025)
- Increased defaults: 60 individual (was 32), 180 global (was 120) = 7× brighter startup
- Fixed zone discontinuity causing 50 lux brightness jumps
- Active safety limit enforcement (80 PWM max)
- Faster potentiometer updates (15s vs 100s = 6.7× improvement)

### Thread Safety Requirements
- Fixed 25+ race conditions across 6 components
- Mutex hierarchy prevents deadlocks
- Resolved WiFi init conflicts causing reboots
- All Home Assistant buttons verified thread-safe

### German Grammar Edge Case
**"EIN" vs "EINS":** Use separate word arrays for UHR contexts
```c
hour_words_with_uhr[]  // "EIN UHR" (1:00)
hour_words[]           // "EINS" (e.g., "FÜNF VOR EINS")
```

**NEUN Overlapping:** Row 9: ZEHN (0-3) shares 'N' with NEUN (3-6)

### Transition System
- **32 transition limit:** Priority system ensures complete words
- **Static buffers:** Prevents stack overflow during WiFi init
- **Fallback guarantee:** Instant mode if transitions fail
- **Priority:** Hour words > Minute words > Indicators

### External Flash & Filesystem (Oct 2025) - Phase 1 Complete ✅
**W25Q64 8MB SPI Flash with LittleFS Filesystem**

**Hardware Configuration:**
- **GPIO Pins:** 12 (MISO), 13 (MOSI), 14 (CLK), 15 (CS) ← **Verified correct**
- **SPI Bus:** HSPI (SPI2_HOST) at 10 MHz
- **JEDEC ID:** 0xEF 0x40 0x17 (Winbond W25Q64)
- **Status:** OPTIONAL hardware - system works without W25Q64 installed

**Flash Driver (external_flash component):**
- Operations: Read, write (page programming), erase (sector/range)
- Partition registration: Dynamic via `external_flash_register_partition()`
- Registers entire 8MB flash as virtual ESP partition "ext_storage"
- Test suite: 9 comprehensive tests + quick smoke test

**Filesystem Manager (filesystem_manager component):**
- **Filesystem:** LittleFS (joltwallet/littlefs ^1.14.8)
- **Mount Point:** `/storage`
- **Auto-created Directories:** `/storage/chimes`, `/storage/config`
- **API:** List, read, write, delete, rename, format files
- **Features:** Wear leveling, power-loss protection, POSIX-like API
- **Integration:** Ported from proven Chimes_System (working code)

**Usage:**
```c
// File operations available after initialization
filesystem_write_file("/storage/chimes/test.pcm", buffer, size);
filesystem_read_file("/storage/chimes/test.pcm", &buffer, &size);
filesystem_file_exists("/storage/chimes/test.pcm");  // Returns bool
```

**Initialization:**
- Occurs in `wordclock.c` after I2C initialization
- Graceful degradation if W25Q64 not present
- Logs: "Filesystem ready at /storage" on success

**Documentation:**
  - [external-flash-quick-start.md](docs/technical/external-flash-quick-start.md) - Quick testing guide
  - [external-flash-testing.md](docs/technical/external-flash-testing.md) - Comprehensive test guide
  - [integration-strategy-from-chimes.md](docs/archive/superseded-plans/integration-strategy-from-chimes.md) - Full integration plan (archived - superseded)
  - [chime-system-implementation-plan-w25q64.md](docs/archive/superseded-plans/chime-system-implementation-plan-w25q64.md) - Original plan (archived - superseded)

## Troubleshooting

### Success Indicators
```bash
✅ "Connected to WiFi with stored credentials"
✅ "NTP time synchronization complete!"
✅ "=== SECURE MQTT CONNECTION ESTABLISHED ==="
✅ "mqtt_discovery: ✅ All discovery configurations published successfully"
```

### Common Issues
**Display too dark:** Reset brightness config via MQTT or HA button
**Brightness jumps:** Check zone continuity in config
**WiFi won't connect:** Long-press GPIO 5 button to clear credentials
**MQTT not connecting:** Verify HiveMQ credentials in mqtt_manager.h

## Documentation References

**Full documentation:** See `docs/` directory
- [User Guide](docs/user/) - Setup and configuration
- [Developer Guide](docs/developer/) - API reference and architecture
- [Implementation Details](docs/implementation/) - Technical deep-dives
- [MQTT Architecture](docs/implementation/mqtt/mqtt-system-architecture.md) - Complete MQTT guide

**Related Files:**
- `README.md` - User-facing documentation (15KB)
- `Mqtt_User_Guide.md` - MQTT command reference (23KB)
- `docs/` - Comprehensive organized documentation

## Quick Architecture Reference

### Initialization Sequence
1. **Hardware:** I2C buses → TLC59116 controllers → DS3231 RTC
2. **Sensors:** ADC (potentiometer) → BH1750 (light sensor)
3. **External Storage (OPTIONAL - Phase 1 ✅):**
   - SPI bus (HSPI) → W25Q64 flash init → JEDEC ID verification
   - Filesystem manager → LittleFS mount at /storage
   - Auto-create /storage/chimes and /storage/config directories
4. **Display:** LED state initialization → brightness config
5. **Network:** NVS → WiFi (STA or AP) → NTP → MQTT
6. **IoT:** MQTT Discovery → Home Assistant integration
7. **Tasks:** Status LED task → Light sensor task (10Hz)

### Data Flow
```
DS3231 RTC → German time logic → LED state matrix → Transition manager → TLC59116 PWM
                                                  ↓
Potentiometer → Individual brightness ──────────────→ Combined brightness
Light sensor  → Global brightness    ──────────────→ (Individual × Global) ÷ 255
```

### Thread Safety Pattern
```c
// Never access globals directly
extern uint8_t global_brightness;  // ❌ Don't use directly

// Always use thread-safe accessors
uint8_t value = thread_safe_get_global_brightness();  // ✅ Correct
thread_safe_set_global_brightness(180);               // ✅ Correct
```

## Version Information

**Current:** Single commit repository (fresh start from stable codebase)
**ESP-IDF:** 5.4.2 (latest stable, tested in CI/CD)
**License:** MIT (free for personal and commercial use)

---

**For detailed information, see organized documentation in `docs/` directory**
