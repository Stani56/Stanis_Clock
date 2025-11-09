# YB-ESP32-S3-AMP Migration Checklist

## Overview

Migration from **ESP32-S3-DevKitC-1 N16R8** to **YB-ESP32-S3-AMP** board.

**Purpose**: The YB-ESP32-S3-AMP board features integrated audio amplifiers and optimized audio routing, eliminating the need for external MAX98357A modules.

**Current Status**: Pre-migration planning phase (hardware not yet arrived)

**Document Version**: 1.0
**Created**: 2025-11-08
**Last Updated**: 2025-11-08

---

## Hardware Comparison

### Current Board: ESP32-S3-DevKitC-1 N16R8

| Specification | Value |
|--------------|-------|
| **Module** | ESP32-S3-WROOM-1-N16R8 |
| **Flash** | 16MB (Quad SPI) |
| **PSRAM** | 8MB (Octal PSRAM) |
| **Audio** | None (external MAX98357A required) |
| **USB** | USB-C (UART/JTAG) |
| **GPIO** | All standard ESP32-S3 GPIOs |
| **Power** | 5V USB or external |
| **Size** | Standard DevKit form factor |

### Target Board: YB-ESP32-S3-AMP

| Specification | Value |
|--------------|-------|
| **Module** | ESP32-S3-WROOM-1 (verify N16R8 variant) |
| **Flash** | 16MB (expected) |
| **PSRAM** | 8MB (expected) |
| **Audio** | **Integrated dual MAX98357A amplifiers** |
| **I2S Routing** | Pre-configured audio pins |
| **Speaker Outputs** | 2× speaker terminals (stereo) |
| **SD Card** | **Integrated microSD slot** |
| **USB** | USB-C |
| **GPIO** | May have some pins reserved for audio |
| **Power** | 5V USB or external (verify ampere requirements) |

### Key Differences

✅ **Advantages of YB-ESP32-S3-AMP:**
- Integrated MAX98357A amplifiers (no external wiring)
- Built-in microSD card slot (no external wiring)
- Optimized PCB layout for audio quality
- Cleaner build (fewer external modules)
- Professional audio routing

⚠️ **Potential Concerns:**
- GPIO availability (some pins may be hardwired for audio/SD)
- Pin mapping changes (audio, SD card pins)
- Power consumption with amplifiers
- Speaker connector type/pinout
- SD card SPI bus conflicts with other peripherals

---

## Pre-Arrival Preparation Checklist

### 📋 Phase 1: Documentation & Research

- [ ] **Obtain YB-ESP32-S3-AMP datasheet/schematic**
  - [ ] Download from manufacturer website
  - [ ] Save to `docs/hardware/datasheets/`
  - [ ] Identify exact module variant (N16R8 vs N8R2, etc.)

- [ ] **Identify pin mappings**
  - [ ] I2S audio pins (BCLK, LRCLK, DIN for both channels)
  - [ ] microSD card pins (CS, MOSI, MISO, CLK)
  - [ ] Available GPIO pins after audio/SD allocation
  - [ ] USB pins (if different from DevKit)
  - [ ] Power pins (voltage levels, current capacity)

- [ ] **Document hardwired connections**
  - [ ] Which pins are hardwired to MAX98357A #1
  - [ ] Which pins are hardwired to MAX98357A #2
  - [ ] Which pins are hardwired to SD card
  - [ ] Any pull-up/pull-down resistors on board
  - [ ] Boot strapping pins (if any constraints)

- [ ] **Research audio amplifier specifications**
  - [ ] MAX98357A gain settings (fixed or configurable)
  - [ ] Output power per channel
  - [ ] Recommended speaker impedance (4Ω, 8Ω)
  - [ ] Shutdown/enable pin control (if exposed)

- [ ] **Verify component compatibility**
  - [ ] Confirm 16MB flash (same as current)
  - [ ] Confirm 8MB PSRAM (same as current)
  - [ ] Check if board has RTC battery backup
  - [ ] Verify USB-C interface compatibility

### 📋 Phase 2: GPIO Conflict Analysis

- [ ] **Create GPIO allocation spreadsheet**
  - [ ] List all current GPIO assignments
  - [ ] List YB-ESP32-S3-AMP reserved pins
  - [ ] Identify conflicts
  - [ ] Plan GPIO remapping if needed

#### Current GPIO Usage (ESP32-S3-DevKitC-1)

```
I2C LEDs (TLC59116):
  GPIO 8  → SDA (I2C0)
  GPIO 9  → SCL (I2C0)

I2C Sensors (DS3231, BH1750):
  GPIO 1  → SDA (I2C1)
  GPIO 18 → SCL (I2C1)

ADC (Potentiometer):
  GPIO 3  → Brightness control (ADC1_CH2)

Status LEDs:
  GPIO 21 → WiFi LED
  GPIO 38 → NTP LED

Button:
  GPIO 0  → Reset button (boot button)

External Audio (MAX98357A - to be replaced):
  GPIO 5  → I2S_BCLK
  GPIO 6  → I2S_LRCLK
  GPIO 7  → I2S_DIN

External SD Card (to be replaced):
  GPIO 10 → SD_CS
  GPIO 11 → SD_MOSI
  GPIO 12 → SD_CLK
  GPIO 13 → SD_MISO
```

**Action Items:**
- [ ] Check if GPIO 5/6/7 are hardwired on YB-AMP (likely YES)
- [ ] Check if GPIO 10/11/12/13 are hardwired on YB-AMP (likely YES)
- [ ] Verify GPIO 1/3/8/9/18/21/38 are still available
- [ ] Identify alternative pins if conflicts exist

### 📋 Phase 3: Software Preparation

- [ ] **Create hardware abstraction layer (HAL)**
  - [ ] Define board-specific configuration file
  - [ ] Create `board_config.h` with pin definitions
  - [ ] Implement compile-time board selection

- [ ] **Backup current configuration**
  - [ ] Git tag current state: `git tag v2.6.3-devkitc-final`
  - [ ] Create branch: `git branch devkitc-stable`
  - [ ] Document working pin configuration in README

- [ ] **Review sdkconfig for board-specific settings**
  - [ ] Flash size configuration
  - [ ] PSRAM configuration
  - [ ] I2S audio configuration
  - [ ] SPI bus configuration

- [ ] **Plan code changes (DO NOT IMPLEMENT YET)**
  - [ ] List all files that reference GPIO pins
  - [ ] List all files that reference audio configuration
  - [ ] List all files that reference SD card configuration
  - [ ] Estimate scope of changes

### 📋 Phase 4: Test Equipment & Tools

- [ ] **Prepare test equipment**
  - [ ] USB-C cable (known good)
  - [ ] 5V power supply (sufficient amperage for amplifiers)
  - [ ] Multimeter for voltage verification
  - [ ] Logic analyzer (optional, for I2S debugging)
  - [ ] Oscilloscope (optional, for audio signal analysis)

- [ ] **Prepare test speakers**
  - [ ] 2× speakers (4Ω or 8Ω, check datasheet)
  - [ ] Speaker wire/connectors
  - [ ] Verify speaker impedance matches amplifier specs

- [ ] **Prepare microSD card**
  - [ ] Format as FAT32
  - [ ] Load test audio files (WAV, PCM format)
  - [ ] Prepare 1-2GB card for testing

- [ ] **Software tools**
  - [ ] ESP-IDF 5.4.2 (already installed)
  - [ ] esptool.py (already installed)
  - [ ] Serial monitor software
  - [ ] Audio editing software (Audacity) for test file creation

### 📋 Phase 5: Migration Strategy Planning

- [ ] **Define migration phases**
  - [ ] Phase 1: Basic board bring-up (no peripherals)
  - [ ] Phase 2: Core functionality (WiFi, NTP, MQTT)
  - [ ] Phase 3: I2C devices (LEDs, RTC, light sensor)
  - [ ] Phase 4: Audio system (I2S, amplifiers, speakers)
  - [ ] Phase 5: SD card integration
  - [ ] Phase 6: Full system integration

- [ ] **Create test plan for each phase**
  - [ ] Smoke tests (power on, USB connection)
  - [ ] GPIO tests (verify all expected pins work)
  - [ ] I2C bus tests (scan, device detection)
  - [ ] Audio tests (tone generation, file playback)
  - [ ] SD card tests (mount, read, write)
  - [ ] Integration tests (full system functionality)

- [ ] **Define rollback plan**
  - [ ] Keep DevKitC-1 board operational
  - [ ] Maintain separate firmware builds for both boards
  - [ ] Document any breaking changes

---

## Post-Arrival Initial Testing Checklist

### 📋 Phase 6: Visual Inspection

- [ ] **Unbox and inspect hardware**
  - [ ] Check for shipping damage
  - [ ] Verify all components are present
  - [ ] Inspect solder joints and PCB quality
  - [ ] Identify all connectors and labels

- [ ] **Photograph the board**
  - [ ] Top view (component side)
  - [ ] Bottom view (solder side)
  - [ ] Close-ups of all connectors
  - [ ] Label photos with component locations

- [ ] **Document board markings**
  - [ ] Board revision number
  - [ ] ESP32-S3 module part number
  - [ ] Manufacturer markings
  - [ ] Any silk-screen labels for pins

### 📋 Phase 7: Power-Up Testing

⚠️ **IMPORTANT: Do not connect speakers or peripherals yet**

- [ ] **Initial power-up (USB only)**
  - [ ] Connect USB-C cable to computer
  - [ ] Verify power LED illuminates
  - [ ] Check USB enumeration (lsusb on Linux)
  - [ ] Measure voltage on 3.3V pin (should be 3.3V ±5%)
  - [ ] Measure voltage on 5V pin (should be 5.0V ±5%)

- [ ] **Verify USB serial connection**
  - [ ] Identify serial port device (/dev/ttyUSB0 or /dev/ttyACM0)
  - [ ] Test connection with: `screen /dev/ttyACM0 115200`
  - [ ] Verify bootloader messages appear on reset

- [ ] **Flash test firmware (minimal blink program)**
  - [ ] Create simple LED blink program
  - [ ] Flash using: `idf.py -p /dev/ttyACM0 flash`
  - [ ] Verify successful flash
  - [ ] Monitor output for errors

### 📋 Phase 8: GPIO Verification

- [ ] **Test all expected available GPIOs**
  - [ ] Write GPIO test firmware (toggle each pin)
  - [ ] Use multimeter to verify voltage changes
  - [ ] Document any pins that don't respond as expected
  - [ ] Note any pins with unexpected pull-up/down behavior

- [ ] **Test I2C bus 0 (LEDs)**
  - [ ] Connect to GPIO 8 (SDA) and GPIO 9 (SCL)
  - [ ] Run i2c_scan_bus(0)
  - [ ] Verify TLC59116 devices detected at 0x60-0x6A

- [ ] **Test I2C bus 1 (Sensors)**
  - [ ] Connect to GPIO 1 (SDA) and GPIO 18 (SCL)
  - [ ] Run i2c_scan_bus(1)
  - [ ] Verify DS3231 (0x68) and BH1750 (0x23) detected

### 📋 Phase 9: Audio System Testing

- [ ] **Identify I2S pin mapping on YB-AMP**
  - [ ] Locate BCLK, LRCLK, DIN pins (likely hardwired)
  - [ ] Document pin numbers in spreadsheet
  - [ ] Check if both amplifiers share same I2S bus or separate

- [ ] **Test I2S output (no speakers yet)**
  - [ ] Configure I2S with identified pins
  - [ ] Generate test tone (1kHz sine wave)
  - [ ] Use oscilloscope/logic analyzer to verify signals
  - [ ] Verify BCLK frequency (e.g., 64× sample rate)
  - [ ] Verify LRCLK frequency (sample rate, e.g., 44.1kHz)

- [ ] **Test amplifier enable/shutdown pins (if exposed)**
  - [ ] Identify enable/shutdown GPIO (if controllable)
  - [ ] Test enabling/disabling amplifiers
  - [ ] Measure current consumption difference

- [ ] **Connect speakers and test audio**
  - [ ] ⚠️ **Start with low volume**
  - [ ] Connect speakers to terminals
  - [ ] Generate test tone
  - [ ] Verify both channels work
  - [ ] Check for distortion, noise, or DC offset
  - [ ] Test volume control (if supported)

### 📋 Phase 10: SD Card Testing

- [ ] **Identify SD card pin mapping**
  - [ ] Locate CS, MOSI, MISO, CLK pins
  - [ ] Document pin numbers
  - [ ] Check SPI bus number (SPI2 or SPI3)

- [ ] **Test SD card detection**
  - [ ] Insert formatted microSD card
  - [ ] Configure SPI bus with identified pins
  - [ ] Attempt to mount FAT filesystem
  - [ ] Read SD card capacity and free space

- [ ] **Test SD card read/write**
  - [ ] Create test file
  - [ ] Write data to file
  - [ ] Read data back and verify
  - [ ] Test directory operations

- [ ] **Test audio file playback from SD**
  - [ ] Copy WAV file to SD card
  - [ ] Open file and parse WAV header
  - [ ] Stream audio data to I2S
  - [ ] Verify playback quality

---

## Pin Mapping Migration Plan

### 📋 Phase 11: Create Pin Mapping Configuration

- [ ] **Create board-specific header files**

#### Option A: Compile-time board selection
```c
// components/board_config/include/board_config.h
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Board selection (set in sdkconfig or CMakeLists.txt)
#if CONFIG_BOARD_DEVKITC_N16R8
    #include "board_devkitc.h"
#elif CONFIG_BOARD_YB_ESP32_S3_AMP
    #include "board_yb_amp.h"
#else
    #error "No board type defined!"
#endif

#endif // BOARD_CONFIG_H
```

```c
// components/board_config/include/board_devkitc.h
#ifndef BOARD_DEVKITC_H
#define BOARD_DEVKITC_H

// I2C Configuration
#define BOARD_I2C_LEDS_SDA      8
#define BOARD_I2C_LEDS_SCL      9
#define BOARD_I2C_SENSORS_SDA   1
#define BOARD_I2C_SENSORS_SCL   18

// Audio Configuration (External MAX98357A)
#define BOARD_I2S_BCLK          5
#define BOARD_I2S_LRCLK         6
#define BOARD_I2S_DIN           7
#define BOARD_AUDIO_EXTERNAL    true

// SD Card Configuration (External SPI)
#define BOARD_SD_CS             10
#define BOARD_SD_MOSI           11
#define BOARD_SD_CLK            12
#define BOARD_SD_MISO           13
#define BOARD_SD_EXTERNAL       true

// Other GPIOs
#define BOARD_ADC_POT           3
#define BOARD_LED_WIFI          21
#define BOARD_LED_NTP           38
#define BOARD_BUTTON_RESET      0

#endif // BOARD_DEVKITC_H
```

```c
// components/board_config/include/board_yb_amp.h
#ifndef BOARD_YB_AMP_H
#define BOARD_YB_AMP_H

// I2C Configuration (same as DevKit, verify!)
#define BOARD_I2C_LEDS_SDA      8
#define BOARD_I2C_LEDS_SCL      9
#define BOARD_I2C_SENSORS_SDA   1
#define BOARD_I2C_SENSORS_SCL   18

// Audio Configuration (Integrated MAX98357A)
// TODO: Update with actual YB-AMP pin numbers
#define BOARD_I2S_BCLK          TBD  // Check schematic
#define BOARD_I2S_LRCLK         TBD  // Check schematic
#define BOARD_I2S_DIN           TBD  // Check schematic
#define BOARD_AUDIO_INTEGRATED  true
#define BOARD_AUDIO_CHANNELS    2    // Stereo

// SD Card Configuration (Integrated)
// TODO: Update with actual YB-AMP pin numbers
#define BOARD_SD_CS             TBD  // Check schematic
#define BOARD_SD_MOSI           TBD  // Check schematic
#define BOARD_SD_CLK            TBD  // Check schematic
#define BOARD_SD_MISO           TBD  // Check schematic
#define BOARD_SD_INTEGRATED     true

// Other GPIOs (verify availability!)
#define BOARD_ADC_POT           3    // Verify not used by AMP
#define BOARD_LED_WIFI          21   // Verify not used by AMP
#define BOARD_LED_NTP           38   // Verify not used by AMP
#define BOARD_BUTTON_RESET      0    // Boot button (should be same)

#endif // BOARD_YB_AMP_H
```

**Action Items:**
- [ ] Create directory: `components/board_config/include/`
- [ ] Create `board_config.h` template
- [ ] Create `board_devkitc.h` with current pins
- [ ] Create `board_yb_amp.h` with TBD placeholders
- [ ] Add Kconfig options for board selection

### 📋 Phase 12: Code Refactoring Plan

**Files that need updating:**

- [ ] **i2c_devices component**
  - [ ] `include/i2c_devices.h` - Replace hardcoded GPIOs with BOARD_* macros
  - [ ] Verify: Lines 16-19 (I2C pin definitions)

- [ ] **audio_manager component** (Phase 2.1)
  - [ ] `audio_manager.c` - Replace GPIO 5/6/7 with BOARD_I2S_* macros
  - [ ] Update I2S channel configuration for stereo vs mono
  - [ ] Add support for integrated vs external amplifiers

- [ ] **sdcard_manager component** (Phase 2.2)
  - [ ] `sdcard_manager.c` - Replace GPIO 10/11/12/13 with BOARD_SD_* macros
  - [ ] Verify SPI bus number matches board

- [ ] **adc_manager component**
  - [ ] `adc_manager.c` - Replace GPIO 3 with BOARD_ADC_POT
  - [ ] Verify ADC channel mapping

- [ ] **status_led_manager component**
  - [ ] `status_led_manager.c` - Replace GPIO 21/38 with BOARD_LED_* macros

- [ ] **button_manager component**
  - [ ] `button_manager.c` - Replace GPIO 0 with BOARD_BUTTON_RESET

- [ ] **Main application**
  - [ ] `main/wordclock.c` - Include board_config.h
  - [ ] Conditional compilation for board-specific features

**Refactoring Strategy:**
1. **Do NOT make changes until board arrives and pins are verified**
2. Create git branch: `feature/yb-amp-support`
3. Update one component at a time
4. Test each component before moving to next
5. Keep both board configurations working in parallel

---

## Documentation Requirements

### 📋 Phase 13: Documentation Updates

- [ ] **Hardware documentation**
  - [ ] Update `CLAUDE.md` with YB-AMP specifications
  - [ ] Create `docs/hardware/YB-ESP32-S3-AMP-Pinout.md`
  - [ ] Update `README.md` with board options
  - [ ] Add photos of YB-AMP board to `docs/hardware/images/`

- [ ] **Build instructions**
  - [ ] Document how to select board in build system
  - [ ] Update `docs/technical/ota-build-process.md` with board variants
  - [ ] Create separate build commands for each board

- [ ] **User guide**
  - [ ] Update setup instructions for YB-AMP
  - [ ] Document speaker connection
  - [ ] Document SD card usage
  - [ ] Update troubleshooting section

### 📋 Phase 14: Testing Documentation

- [ ] **Create test reports**
  - [ ] GPIO test results spreadsheet
  - [ ] I2C bus scan results
  - [ ] Audio quality measurements
  - [ ] SD card performance benchmarks
  - [ ] Power consumption measurements

- [ ] **Create comparison matrix**
  - [ ] DevKitC vs YB-AMP feature comparison
  - [ ] Performance comparison
  - [ ] Cost comparison
  - [ ] Ease of use comparison

---

## Risk Assessment & Mitigation

### 🔴 High Risk Items

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| GPIO conflicts prevent I2C from working | **CRITICAL** | Medium | Identify alternative pins before arrival; prepare rework plan |
| Audio pins incompatible with current code | High | Low | Review YB-AMP datasheet thoroughly; prepare for code changes |
| SD card SPI bus conflicts with other peripherals | High | Low | Use different SPI bus; verify bus availability |
| Power consumption exceeds USB capability | Medium | Medium | Prepare external 5V power supply; measure current draw |

### 🟡 Medium Risk Items

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Board has different PSRAM/Flash configuration | Medium | Low | Verify specifications; adjust sdkconfig if needed |
| Speaker impedance mismatch | Medium | Medium | Research recommended speaker specs; purchase compatible speakers |
| Audio quality issues (noise, distortion) | Medium | Medium | Test with scope; add filtering if needed |
| SD card compatibility issues | Low | Low | Test multiple SD card brands; use known-good cards |

### 🟢 Low Risk Items

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| USB-C connection issues | Low | Low | Use known-good cable; test multiple cables |
| Bootloader/flashing problems | Low | Low | ESP32-S3 module should be identical; use esptool.py |
| Minor pin remapping needed | Low | High | Already planned for in HAL approach |

---

## Success Criteria

### Minimum Viable Migration (Phase 1)

- [ ] Board powers up and enumerates on USB
- [ ] Can flash firmware successfully
- [ ] WiFi and MQTT connectivity work
- [ ] I2C devices (TLC59116, DS3231, BH1750) detected and functional
- [ ] LED matrix displays time correctly

### Full Feature Migration (Phase 2)

- [ ] Audio system plays test tones through speakers
- [ ] Audio quality meets acceptable standards (no distortion)
- [ ] SD card mounts and files can be read/written
- [ ] Westminster chimes play from SD card
- [ ] All Home Assistant integrations work
- [ ] OTA updates work reliably

### Performance Targets

- [ ] Audio latency < 100ms
- [ ] SD card read speed > 1 MB/s
- [ ] Power consumption within USB 2.0 limits (500mA max)
- [ ] No I2C communication errors
- [ ] WiFi connection stability maintained

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Pre-arrival prep (Phases 1-5) | 2-3 days | None - can start now |
| Initial testing (Phases 6-8) | 1 day | Board arrival |
| Audio testing (Phase 9) | 1-2 days | Phase 8 complete, speakers available |
| SD card testing (Phase 10) | 0.5 day | Phase 8 complete, SD card available |
| Code refactoring (Phase 11-12) | 2-3 days | Pin mapping confirmed |
| Documentation (Phase 13-14) | 1 day | All testing complete |
| **Total estimated time** | **7-10 days** | Board arrival + accessories |

---

## Rollback Plan

If migration encounters critical issues:

1. **Keep DevKitC-1 board operational** - Do not disassemble until YB-AMP is proven
2. **Maintain separate git branches** - `main` (DevKitC), `feature/yb-amp-support` (YB-AMP)
3. **Separate firmware builds** - Both board types remain buildable
4. **Document breaking changes** - List anything that prevents easy rollback
5. **Test before committing** - Verify each change thoroughly before merging

---

## Next Steps (Immediate Actions)

### Before Board Arrives:

1. ✅ **Create this migration checklist** (COMPLETE)
2. [ ] **Research YB-ESP32-S3-AMP online**
   - Search for datasheet, schematic, user manual
   - Check manufacturer website
   - Look for user reviews, forum posts
3. [ ] **Prepare development environment**
   - Ensure ESP-IDF 5.4.2 is up to date
   - Test build system with current board
4. [ ] **Order accessories** (if not already available)
   - Speakers (4Ω or 8Ω, verify from datasheet)
   - microSD card (2-32GB, Class 10 recommended)
   - External 5V power supply (2-3A recommended for safety margin)

### When Board Arrives:

1. [ ] **Complete Phase 6: Visual Inspection**
2. [ ] **Complete Phase 7: Power-Up Testing**
3. [ ] **Update this checklist** with actual findings
4. [ ] **Proceed methodically** through remaining phases

---

## Notes & Observations

### Board Receipt Date: [TBD]

**Initial observations:**
- [To be filled when board arrives]

**Pin mapping discoveries:**
- [To be filled during testing]

**Issues encountered:**
- [To be filled if any issues arise]

**Performance notes:**
- [To be filled during testing]

---

## References

### Datasheets & Documentation

- [ ] YB-ESP32-S3-AMP schematic (TBD - add link when found)
- [ ] YB-ESP32-S3-AMP user manual (TBD - add link when found)
- [ ] ESP32-S3 Technical Reference Manual (already have)
- [ ] MAX98357A datasheet (already referenced in Phase 2.1)

### Related Project Documents

- [CLAUDE.md](../../CLAUDE.md) - Current hardware configuration
- [OTA Build Process](../technical/ota-build-process.md) - Build system details
- [Audio System Implementation](../implementation/audio/) - Phase 2.1 audio details
- [SD Card Implementation](../implementation/sdcard/) - Phase 2.2 SD card details

---

**Document Status**: ✅ Ready for use
**Maintained by**: Development team
**Review frequency**: Update as migration progresses
