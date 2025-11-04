# GPIO Pin Summary - Sorted by GPIO Number

**Last Updated:** November 2025
**Current Platform:** ESP32-PICO-D4
**Future Platform:** YelloByte YB-ESP32-S3-AMP (ESP32-S3-WROOM-1-N8R2)

---

## ESP32 Current Implementation (GPIO Sorted)

| GPIO | Function | Direction | Component | Internal/External | Notes |
|------|----------|-----------|-----------|-------------------|-------|
| **0** | Not Used | - | - | - | Available |
| **1** | UART TX | Output | USB/Serial | Internal | Debug console (do not use) |
| **2** | Not Used | - | - | - | Available |
| **3** | UART RX | Input | USB/Serial | Internal | Debug console (do not use) |
| **4** | Not Used | - | - | - | Available |
| **5** | Reset Button | Input | button_manager | **External** | WiFi credentials clear (long press) |
| **6-11** | Flash SPI | - | Internal Flash | Internal | DO NOT USE (connected to flash) |
| **12** | W25Q64 MISO | Input | external_flash | **External** | SPI HSPI bus (OPTIONAL) |
| **13** | W25Q64 MOSI | Output | external_flash | **External** | SPI HSPI bus (OPTIONAL) |
| **14** | W25Q64 CLK | Output | external_flash | **External** | SPI HSPI bus (OPTIONAL) |
| **15** | W25Q64 CS | Output | external_flash | **External** | SPI HSPI bus, has pull-up (OPTIONAL) |
| **16-17** | Not Used | - | - | - | Available |
| **18** | I2C1 SDA | I/O | i2c_devices | **External** | DS3231 RTC + BH1750 sensor |
| **19** | I2C1 SCL | Output | i2c_devices | **External** | DS3231 RTC + BH1750 sensor |
| **20** | Not Used | - | - | - | Available |
| **21** | WiFi Status LED | Output | status_led_manager | **External** | WiFi connection indicator |
| **22** | NTP Status LED | Output | status_led_manager | **External** | NTP sync indicator |
| **23** | Not Used | - | - | - | Available |
| **24** | Not Used | - | - | - | Available |
| **25** | I2C0 SDA | I/O | i2c_devices | **External** | 10× TLC59116 LED controllers (0x60-0x6A) |
| **26** | I2C0 SCL | Output | i2c_devices | **External** | 10× TLC59116 LED controllers |
| **27** | Not Used (Audio) | - | audio_manager | - | I2S LRCLK (DISABLED - would be external MAX98357A) |
| **28-31** | Not Available | - | - | Internal | Reserved |
| **32** | Not Used (Audio) | - | audio_manager | - | I2S DOUT (DISABLED - would be external MAX98357A) |
| **33** | Not Used (Audio) | - | audio_manager | - | I2S BCLK (DISABLED - would be external MAX98357A) |
| **34** | Potentiometer | Input | adc_manager | **External** | ADC1_CH6, brightness control (no pull-up/down) |
| **35** | Not Available | - | - | - | Input only, no pull-up/down |
| **36** | Not Available | - | - | Internal | SENSOR_VP (input only) |
| **37** | Not Available | - | - | Internal | SENSOR_CAPP (input only) |
| **38** | Not Available | - | - | Internal | SENSOR_CAPN (input only) |
| **39** | Not Available | - | - | Internal | SENSOR_VN (input only) |

### ESP32 Summary Statistics
- **Total GPIOs Used:** 11 pins (5, 12-15, 18-19, 21-22, 25-26, 34)
- **External Connections:** 11 pins (all used pins are external)
- **Internal Only:** UART (GPIO 1, 3), Flash (GPIO 6-11)
- **Audio Disabled:** GPIO 27, 32, 33 (present in code but NOT initialized)
- **Optional Hardware:** GPIO 12-15 (W25Q64 flash - works without it)

---

## ESP32-S3 Planned Implementation (GPIO Sorted)

| GPIO | Function | Direction | Component | Internal/External | Notes |
|------|----------|-----------|-----------|-------------------|-------|
| **0** | Reset Button | Input | button_manager | **External** | Boot button (WiFi credentials clear) |
| **1** | Not Used | - | - | - | Available (ADC1_CH0) |
| **2** | I2C1 SDA | I/O | i2c_devices | **External** | DS3231 RTC + BH1750 sensor |
| **3** | Potentiometer | Input | adc_manager | **External** | ADC1_CH2, brightness control |
| **4** | I2C1 SCL | Output | i2c_devices | **External** | DS3231 RTC + BH1750 sensor |
| **5** | I2S BCLK | Output | audio_manager | Internal | **Built-in** MAX98357A #1 (not on headers) |
| **6** | I2S LRCLK | Output | audio_manager | Internal | **Built-in** MAX98357A (not on headers) |
| **7** | I2S DIN | Output | audio_manager | Internal | **Built-in** MAX98357A (not on headers) |
| **8** | I2C0 SDA | I/O | i2c_devices | **External** | 10× TLC59116 LED controllers (0x60-0x6A) |
| **9** | I2C0 SCL | Output | i2c_devices | **External** | 10× TLC59116 LED controllers |
| **10** | microSD CS | Output | sdcard_manager | Internal | **Built-in** microSD (not on headers) |
| **11** | microSD MOSI | Output | sdcard_manager | Internal | **Built-in** microSD (not on headers) |
| **12** | microSD CLK | Output | sdcard_manager | Internal | **Built-in** microSD (not on headers) |
| **13** | microSD MISO | Input | sdcard_manager | Internal | **Built-in** microSD (not on headers) |
| **14** | Not Used | - | - | - | Available (ADC2_CH3) |
| **15** | Not Used | - | - | - | Available (ADC2_CH4) |
| **16** | Not Used | - | - | - | Available (ADC2_CH5) |
| **17** | Not Used | - | - | - | Available (ADC2_CH6) |
| **18** | NTP Status LED | Output | status_led_manager | **External** | NTP sync indicator |
| **19** | USB D- | I/O | USB | Internal | Native USB (Rev.3 only - do not use for GPIO) |
| **20** | USB D+ | I/O | USB | Internal | Native USB (Rev.3 only - do not use for GPIO) |
| **21** | WiFi Status LED | Output | status_led_manager | **External** | WiFi connection indicator (no change) |
| **22-26** | Not Available | - | - | - | Reserved for flash/PSRAM (do not use) |
| **27-32** | Not Available | - | - | - | Reserved for flash/PSRAM (do not use) |
| **33-37** | Not Available | - | - | - | Reserved (do not use) |
| **38** | Not Used | - | - | - | Available |
| **39-42** | Not Available | - | - | Internal | MTCK, MTDO, MTDI, MTMS (JTAG) |
| **43** | UART TX | Output | USB/Serial | Internal | Debug console (do not use) |
| **44** | UART RX | Input | USB/Serial | Internal | Debug console (do not use) |
| **45** | Not Used | - | - | - | Available |
| **46** | Not Used | - | - | - | Available |
| **47** | Onboard LED | Output | YB Board | Internal | RGB LED on YB board (can be used) |
| **48** | Not Used | - | - | - | Available |

### ESP32-S3 Summary Statistics
- **Total GPIOs Used:** 9 external + 7 internal = 16 pins
- **External Connections:** 9 pins (0, 2, 3, 4, 8, 9, 18, 21) - reduced from ESP32's 11
- **Internal Only (Built-in):**
  - Audio: GPIO 5, 6, 7 (2× MAX98357A amplifiers - not on headers)
  - microSD: GPIO 10, 11, 12, 13 (not on headers)
  - USB: GPIO 19, 20 (native USB - Rev.3 only)
  - UART: GPIO 43, 44 (debug console)
- **Available for Expansion:** GPIO 1, 14-17, 38, 45-46, 48

---

## GPIO Migration Summary (ESP32 → ESP32-S3)

### Changes Required

| Function | ESP32 GPIO | ESP32-S3 GPIO | Change | Reason |
|----------|-----------|---------------|--------|--------|
| **I2C0 SDA (LEDs)** | 25 | **8** | ✅ CHANGE | YB board default I2C |
| **I2C0 SCL (LEDs)** | 26 | **9** | ✅ CHANGE | YB board default I2C |
| **I2C1 SDA (Sensors)** | 18 | **2** | ✅ CHANGE | Available GPIO |
| **I2C1 SCL (Sensors)** | 19 | **4** | ✅ CHANGE | Available GPIO |
| **Potentiometer (ADC)** | 34 (ADC1_CH6) | **3 (ADC1_CH2)** | ✅ CHANGE | ADC1 works with WiFi |
| **WiFi Status LED** | 21 | **21** | ❌ NO CHANGE | Same GPIO |
| **NTP Status LED** | 22 | **18** | ✅ CHANGE | GPIO 22 conflicts with audio |
| **Reset Button** | 5 | **0** | ✅ CHANGE | Boot button standard |
| **I2S BCLK (Audio)** | 33 (disabled) | **5** | ✅ CHANGE | Built-in amplifier |
| **I2S LRCLK (Audio)** | 27 (disabled) | **6** | ✅ CHANGE | Built-in amplifier |
| **I2S DOUT/DIN (Audio)** | 32 (disabled) | **7** | ✅ CHANGE | Built-in amplifier |

### External Storage Changes

| Function | ESP32 GPIO | ESP32-S3 GPIO | Status |
|----------|-----------|---------------|--------|
| **W25Q64 MISO** | 12 | **REMOVED** | Conflicted with microSD |
| **W25Q64 MOSI** | 13 | **REMOVED** | Conflicted with microSD |
| **W25Q64 CLK** | 14 | **REMOVED** | Conflicted with microSD |
| **W25Q64 CS** | 15 | **REMOVED** | Conflicted with microSD |
| **microSD MISO** | - | **13 (internal)** | Built-in on YB board |
| **microSD MOSI** | - | **11 (internal)** | Built-in on YB board |
| **microSD CLK** | - | **12 (internal)** | Built-in on YB board |
| **microSD CS** | - | **10 (internal)** | Built-in on YB board |

### Code Changes Required

**Files to Modify (8 files):**
1. `components/i2c_devices/i2c_devices.c` - I2C0 and I2C1 pin changes
2. `components/adc_manager/adc_manager.c` - ADC channel change
3. `components/button_manager/button_manager.c` - Button GPIO change
4. `components/status_led_manager/status_led_manager.c` - NTP LED change
5. `components/audio_manager/audio_manager.c` - I2S pin changes
6. `main/wordclock.c` - External flash → microSD init
7. `components/filesystem_manager/` - LittleFS → FatFS migration
8. Create new: `components/sdcard_manager/` - microSD driver

**Estimated Effort:** 4-6 hours (1-2h GPIO + 2-4h filesystem)

---

## I2C Bus Details

### ESP32 Current

**I2C Bus 0 (GPIO 25/26) - LED Controllers:**
- Address 0x60: TLC59116 #1 (Row 0)
- Address 0x61: TLC59116 #2 (Row 1)
- Address 0x62: TLC59116 #3 (Row 2)
- Address 0x63: TLC59116 #4 (Row 3)
- Address 0x64: TLC59116 #5 (Row 4)
- Address 0x65: TLC59116 #6 (Row 5)
- Address 0x66: TLC59116 #7 (Row 6)
- Address 0x67: TLC59116 #8 (Row 7)
- Address 0x68: TLC59116 #9 (Row 8) - **Conflict with DS3231!**
- Address 0x6A: TLC59116 #10 (Row 9)

**I2C Bus 1 (GPIO 18/19) - Sensors:**
- Address 0x68: DS3231 RTC (separate bus to avoid conflict)
- Address 0x23: BH1750 Light Sensor

**Speed:** 100kHz (conservative for 10-device reliability)

### ESP32-S3 Planned

**I2C Bus 0 (GPIO 8/9) - LED Controllers:**
- Same 10× TLC59116 controllers (addresses 0x60-0x6A)
- Same address conflict with DS3231 (why sensors on separate bus)

**I2C Bus 1 (GPIO 2/4) - Sensors:**
- Same DS3231 RTC @ 0x68
- Same BH1750 @ 0x23

**Speed:** 100kHz (no change)

---

## Hardware Wiring Changes Required

### External Connections to Move

| Connection | Old Pin | New Pin | Notes |
|------------|---------|---------|-------|
| **I2C0 SDA (LEDs)** | GPIO 25 | GPIO 8 | 10× TLC59116 controllers |
| **I2C0 SCL (LEDs)** | GPIO 26 | GPIO 9 | 10× TLC59116 controllers |
| **I2C1 SDA (Sensors)** | GPIO 18 | GPIO 2 | DS3231 + BH1750 |
| **I2C1 SCL (Sensors)** | GPIO 19 | GPIO 4 | DS3231 + BH1750 |
| **Potentiometer** | GPIO 34 | GPIO 3 | Analog input (ADC1_CH2) |
| **NTP Status LED** | GPIO 22 | GPIO 18 | LED + resistor |
| **Reset Button** | GPIO 5 | GPIO 0 | Button + pull-up |
| **WiFi Status LED** | GPIO 21 | GPIO 21 | No change needed ✅ |

### Internal Connections (No Wiring)

| Connection | ESP32-S3 GPIO | Notes |
|------------|---------------|-------|
| **I2S BCLK** | GPIO 5 | Pre-wired to MAX98357A #1 |
| **I2S LRCLK** | GPIO 6 | Pre-wired to both MAX98357A |
| **I2S DIN** | GPIO 7 | Pre-wired to MAX98357A #2 |
| **microSD MISO** | GPIO 13 | Pre-wired, not on headers |
| **microSD MOSI** | GPIO 11 | Pre-wired, not on headers |
| **microSD CLK** | GPIO 12 | Pre-wired, not on headers |
| **microSD CS** | GPIO 10 | Pre-wired, not on headers |

### Connections to Remove (W25Q64 External Flash)

| Connection | ESP32 GPIO | Notes |
|------------|-----------|-------|
| **W25Q64 MISO** | GPIO 12 | Remove if W25Q64 installed |
| **W25Q64 MOSI** | GPIO 13 | Remove if W25Q64 installed |
| **W25Q64 CLK** | GPIO 14 | Remove if W25Q64 installed |
| **W25Q64 CS** | GPIO 15 | Remove if W25Q64 installed |

**Note:** W25Q64 is OPTIONAL on ESP32. If not installed, no removal needed.

---

## Key Benefits of ESP32-S3 GPIO Layout

1. **Built-in Audio (GPIO 5/6/7):**
   - No external MAX98357A wiring needed
   - Pre-wired 2× amplifiers on YB board
   - Concurrent WiFi+MQTT+I2S support (PSRAM + improved DMA)

2. **Built-in microSD (GPIO 10/11/12/13):**
   - No external flash wiring needed
   - 1000× more storage (8GB+ vs 8MB W25Q64)
   - Easy file management (removable card)

3. **Same External Pin Count:**
   - Still only 11 external connections
   - WiFi Status LED unchanged (GPIO 21)
   - Same I2C device count (10× TLC59116 + 2 sensors)

4. **Native USB (GPIO 19/20):**
   - Easier debugging
   - No USB-to-serial adapter needed (Rev.3 boards)

---

**For complete migration details, see [ESP32-S3-Migration-Analysis.md](ESP32-S3-Migration-Analysis.md)**
