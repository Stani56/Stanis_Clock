# YB-ESP32-S3-AMP Hardware Specifications

**Board:** YelloByte YB-ESP32-S3-AMP Audio Development Board
**Manufacturer:** YelloByte
**Repository:** https://github.com/yellobyte/YB-ESP32-S3-AMP
**Documentation:** `/doc` folder in repository

---

## Overview

The YB-ESP32-S3-AMP is an audio-focused development board featuring:
- ESP32-S3 WiFi/BLE MCU with PSRAM
- Dual MAX98357A 3W audio amplifiers (integrated)
- MicroSD card slot (integrated)
- USB-C connectivity
- Breadboard-compatible design

---

## MCU Module Specifications

| Specification | Value |
|---------------|-------|
| **Module** | ESP32-S3-WROOM-1-N8R2 |
| **Flash** | 8MB (N8) |
| **PSRAM** | 2MB (R2) - Octal SPI |
| **CPU** | Dual-core Xtensa LX7 @ 240MHz |
| **WiFi** | 802.11 b/g/n |
| **Bluetooth** | BLE 5.0 |
| **Operating Voltage** | 3.3V (logic) |
| **Input Voltage** | 5V via USB-C or VIN pin |

---

## GPIO Pin Assignments

### I2S Audio (Integrated MAX98357A Amplifiers)

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO 5 | BCLK (Bit Clock) | Dedicated to audio, not exposed |
| GPIO 6 | LRCLK (Frame Clock) | Dedicated to audio, not exposed |
| GPIO 7 | DIN (Data Input) | Dedicated to audio, not exposed |

**Important:** These pins are **hard-wired to the amplifiers** and cannot be used for other purposes.

**✅ USER VERIFIED (2025-11-09):** Pin assignment identical to current ESP32-S3-DevKitC-1 external MAX98357A setup (BCLK=5, LRCLK=6, DIN=7). Zero code changes required for I2S audio migration.

### MicroSD Card (SPI/FSPI)

| GPIO | Function | Alt Function | Notes |
|------|----------|--------------|-------|
| GPIO 10 | SCS (Chip Select) | SD_CS | Can be used for other purposes if SD_CS solder bridge is open |
| GPIO 11 | MOSI/CMD | Data out | Dedicated to microSD, not exposed |
| GPIO 12 | SCK (Clock) | Clock | Dedicated to microSD, not exposed |
| GPIO 13 | MISO/D0 | Data in | Dedicated to microSD, not exposed |

**Important:** GPIOs 11/12/13 are **hard-wired to microSD slot** and not available on board connectors.

### Status LED

| GPIO | Function | Alt Function | Notes |
|------|----------|--------------|-------|
| GPIO 47 | Status LED | DAC_MUTE | Can mute amplifiers if DAC_MUTE solder bridge is closed |

### Available GPIOs (Exposed on Board)

The following GPIOs are **available on board connectors** for user applications:

| GPIO | ADC | Notes |
|------|-----|-------|
| GPIO 1 | ADC1_CH0 | Safe for use with WiFi |
| GPIO 2 | ADC1_CH1 | Safe for use with WiFi |
| GPIO 3 | ADC1_CH2 | Safe for use with WiFi |
| GPIO 4 | ADC1_CH3 | Safe for use with WiFi |
| GPIO 8 | - | Digital GPIO |
| GPIO 9 | - | Digital GPIO |
| GPIO 14 | ADC2_CH3 | **Avoid with WiFi** |
| GPIO 15 | ADC2_CH4 | **Avoid with WiFi** |
| GPIO 16 | ADC2_CH5 | **Avoid with WiFi** |
| GPIO 17 | ADC2_CH6 | **Avoid with WiFi** |
| GPIO 18 | ADC2_CH7 | **Avoid with WiFi** |
| GPIO 21 | - | Digital GPIO |
| GPIO 38 | - | Digital GPIO |
| GPIO 39 | - | Digital GPIO |
| GPIO 40 | - | Digital GPIO |
| GPIO 41 | - | Digital GPIO |
| GPIO 42 | - | Digital GPIO |

**Note:** GPIO 0 is connected to 'B' button (boot mode selection).

---

## Power Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Input Voltage (USB-C)** | 5V DC | USB-C connector |
| **Input Voltage (VIN)** | 5V DC (max 5.5V) | Pin header |
| **Logic Voltage** | 3.3V | ESP32-S3 I/O |
| **Amplifier Supply** | 5V | Direct from USB/VIN |
| **Idle Current** | ~45mA | WiFi disabled, no audio |
| **Active Current** | ~100mA | WiFi enabled, no audio |
| **Max Audio Output** | 3.2W per channel | @ 4Ω load |

---

## Integrated Peripherals

### MAX98357A Audio Amplifiers (2x)

| Feature | Specification |
|---------|---------------|
| **Type** | Class D Amplifier |
| **Channels** | 2 (Stereo) |
| **Output Power** | 3.2W per channel @ 4Ω |
| **Interface** | I2S Digital Audio |
| **Sample Rate** | Up to 96kHz |
| **Bit Depth** | 16/24-bit |
| **THD+N** | <0.1% @ 1W |

**Wiring:**
- **Hardwired** to GPIOs 5, 6, 7
- **Cannot be reassigned**
- Power supplied directly from 5V rail

### MicroSD Card Slot

| Feature | Specification |
|---------|---------------|
| **Interface** | SPI (FSPI) |
| **Supported Cards** | microSD, microSDHC |
| **Max Capacity** | 32GB (FAT32) |
| **Speed** | Up to 20MHz SPI clock |

**Wiring:**
- **Hardwired** to GPIOs 10, 11, 12, 13
- **Cannot be reassigned** (except CS via solder bridge)

---

## Comparison with Current Hardware

### Current Setup: ESP32-S3-DevKitC-1 N16R8

| Feature | DevKitC-1 | YB-AMP |
|---------|-----------|--------|
| **Flash** | 16MB | 8MB |
| **PSRAM** | 8MB | 2MB |
| **Audio** | External MAX98357A | Integrated (2x) |
| **microSD** | External via SPI | Integrated |
| **I2S Pins** | GPIO 5/6/7 (external) | GPIO 5/6/7 (integrated) |
| **SD Pins** | GPIO 10/11/12/13 (external) | GPIO 10/11/12/13 (integrated) |
| **Available GPIOs** | All exposed | Some dedicated |

**Key Difference:** YB-AMP has **identical I2S and SD pin assignments** but they're hardwired to integrated peripherals.

---

## Critical Migration Notes

### ✅ Good News: Pin Compatibility

The YB-AMP board uses **exactly the same GPIOs** as your current external setup:
- **I2S:** GPIO 5, 6, 7 (same as current external MAX98357A)
- **SD Card:** GPIO 10, 11, 12, 13 (same as current external SD card)

**Result:** Minimal code changes required! Only configuration flags need updating.

### ⚠️ Reduced Flash/PSRAM

| Resource | Current | YB-AMP | Impact |
|----------|---------|--------|--------|
| **Flash** | 16MB | 8MB | **50% reduction** |
| **PSRAM** | 8MB | 2MB | **75% reduction** |

**Current firmware:** 1.3MB (8% of 16MB)
**YB-AMP margin:** 1.3MB (16% of 8MB) - Still plenty of headroom

**Action Required:** Verify current PSRAM usage to ensure 2MB is sufficient.

### ⚠️ Dedicated Peripherals

**Cannot use GPIO 5/6/7 for anything except audio** - hardwired to amplifiers
**Cannot use GPIO 11/12/13 for anything except SD card** - hardwired to slot
**GPIO 10 can be repurposed** if SD_CS solder bridge is opened

---

## Solder Bridge Options

| Bridge | Default | Function |
|--------|---------|----------|
| **SD_CS** | Closed | GPIO10 → microSD chip select |
| **DAC_MUTE** | Open | GPIO47 → Amplifier mute control |

**If opened:** GPIOs become available for other uses, but peripherals disabled.

---

## References

- **GitHub Repository:** https://github.com/yellobyte/YB-ESP32-S3-AMP
- **Schematic:** `/doc` folder in repository
- **ESP32-S3 Datasheet:** https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- **MAX98357A Datasheet:** Available in `/doc` folder

---

**Last Updated:** 2025-11-09
**Document Version:** 1.0
**Status:** Pre-migration research phase
