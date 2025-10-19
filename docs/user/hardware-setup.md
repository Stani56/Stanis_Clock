# 🔌 Hardware Setup Guide

Complete guide for assembling and connecting your ESP32 German Word Clock hardware.

## 📋 Required Components

### Core Components
- **ESP32 Development Board** (minimum 4MB flash)
- **DS3231 Real-Time Clock Module**
- **BH1750 Digital Light Sensor**
- **Potentiometer** (for brightness control)
- **Reset Button** (momentary switch)
- **2× Status LEDs** (WiFi and NTP indicators)

### Display System
- **LED Matrix**: 10 rows × 16 columns (160 LEDs total)
- **10× TLC59116 LED Controllers** (one per row)
- **Power Supply**: 5V DC (adequate for LED load)

### Additional Components
- **Resistors**: For LED current limiting and pull-ups
- **Capacitors**: Power supply filtering
- **Breadboard/PCB**: For connections
- **Jumper Wires**: For GPIO connections

### Optional: Chime System Expansion
- **W25Q64 8MB SPI Flash Module** (for audio storage)
  - Enables Westminster chime sounds
  - Supports voice announcements and custom audio
  - Connects via SPI interface (HSPI)

## 🔧 GPIO Pin Assignments

```
ESP32 GPIO Connections:
├── I2C Buses
│   ├── GPIO 18 → I2C SDA (Sensors Bus)    → DS3231, BH1750
│   ├── GPIO 19 → I2C SCL (Sensors Bus)    → DS3231, BH1750
│   ├── GPIO 25 → I2C SDA (LEDs Bus)       → TLC59116 controllers
│   └── GPIO 26 → I2C SCL (LEDs Bus)       → TLC59116 controllers
├── SPI Bus (Optional - for W25Q64 external flash)
│   ├── GPIO 12 → SPI MISO                 → W25Q64 DO/SO
│   ├── GPIO 13 → SPI SCK                  → W25Q64 CLK/SCK
│   ├── GPIO 14 → SPI MOSI                 → W25Q64 DI/SI
│   └── GPIO 15 → SPI CS                   → W25Q64 /CS (chip select)
├── Analog Input
│   └── GPIO 34 → ADC Input                → Potentiometer (brightness)
├── Digital Outputs
│   ├── GPIO 21 → WiFi Status LED
│   └── GPIO 22 → NTP Status LED
└── Digital Input
    └── GPIO 5  → Reset Button (with pull-up)
```

**Note on GPIO 15:** Has internal pull-up resistor - no external pull-up needed for SPI CS.

## 🔗 I2C Device Addresses

### LED Controllers (LEDs Bus)
```
TLC59116 Controllers:
├── Row 0: 0x60    ├── Row 5: 0x65
├── Row 1: 0x61    ├── Row 6: 0x66
├── Row 2: 0x62    ├── Row 7: 0x67
├── Row 3: 0x63    ├── Row 8: 0x69
├── Row 4: 0x64    └── Row 9: 0x6A
└── Call-All Address: 0x6B (for broadcast commands)
```

### Sensors (Sensors Bus)
```
DS3231 RTC: 0x68
BH1750 Light Sensor: 0x23
```

## 🎛️ LED Matrix Layout

```
German Word Clock Layout (10×16 matrix):
R0 │ E S • I S T • F Ü N F • • • • • • │
R1 │ Z E H N Z W A N Z I G • • • • • │
R2 │ D R E I V I E R T E L • • • • • │
R3 │ V O R • • • • N A C H • • • • • │
R4 │ H A L B • E L F Ü N F • • • • • │
R5 │ E I N S • • • Z W E I • • • • • │
R6 │ D R E I • • • V I E R • • • • • │
R7 │ S E C H S • • A C H T • • • • • │
R8 │ S I E B E N Z W Ö L F • • • • • │
R9 │ Z E H N E U N • U H R • [●][●][●][●] │
```

### Important Layout Notes
- **Active Display**: Columns 0-10 (time words)
- **Minute Indicators**: Row 9, Columns 11-14 (4 LEDs)
- **Overlapping Words**: Some words share letters (e.g., ZEHN/NEUN share 'N')

## ⚡ Power Supply Requirements

### Voltage Requirements
- **ESP32**: 3.3V (regulated on dev board)
- **I2C Devices**: 3.3V (DS3231, BH1750, TLC59116)
- **LEDs**: 3.3V logic, current depends on brightness

### Current Estimates
```
Component Current Draw:
├── ESP32 (active): ~240mA
├── DS3231 RTC: ~1.5mA
├── BH1750 Sensor: ~0.12mA
├── TLC59116 (×10): ~10mA each
└── LEDs: Variable (depends on brightness and active count)
```

### Recommended Supply
- **5V DC Supply**: 2A minimum capacity
- **Onboard Regulators**: ESP32 dev board provides 3.3V
- **Current Limiting**: Built into TLC59116 controllers

## 🔌 Wiring Instructions

### Step 1: I2C Bus Connections
```bash
# Sensors Bus (100kHz)
ESP32 GPIO 18 → DS3231 SDA, BH1750 SDA
ESP32 GPIO 19 → DS3231 SCL, BH1750 SCL
3.3V → DS3231 VCC, BH1750 VCC
GND → DS3231 GND, BH1750 GND

# LEDs Bus (100kHz)  
ESP32 GPIO 25 → All TLC59116 SDA pins
ESP32 GPIO 26 → All TLC59116 SCL pins
3.3V → All TLC59116 VCC pins
GND → All TLC59116 GND pins
```

### Step 2: Status LEDs
```bash
# WiFi Status LED
ESP32 GPIO 21 → LED Anode (via 220Ω resistor)
GND → LED Cathode

# NTP Status LED
ESP32 GPIO 22 → LED Anode (via 220Ω resistor)
GND → LED Cathode
```

### Step 3: User Controls
```bash
# Brightness Potentiometer
ESP32 GPIO 34 → Potentiometer Wiper
3.3V → Potentiometer High
GND → Potentiometer Low

# Reset Button
ESP32 GPIO 5 → Button (one side)
GND → Button (other side)
ESP32 GPIO 5 → 10kΩ Pull-up to 3.3V
```

### Step 4: TLC59116 Address Configuration
Each TLC59116 needs unique address configuration via ADDR pins:
```
Row 0 (0x60): ADDR1=L, ADDR2=L, ADDR3=L, ADDR4=L
Row 1 (0x61): ADDR1=H, ADDR2=L, ADDR3=L, ADDR4=L
Row 2 (0x62): ADDR1=L, ADDR2=H, ADDR3=L, ADDR4=L
... (see I2C Device Addresses section for complete list)
```

### Step 5: External Flash (Optional - for Chime System)
```bash
# W25Q64 8MB SPI Flash Module
ESP32 GPIO 14 → W25Q64 DI/SI (MOSI)
ESP32 GPIO 12 → W25Q64 DO/SO (MISO)
ESP32 GPIO 13 → W25Q64 CLK/SCK
ESP32 GPIO 15 → W25Q64 /CS (Chip Select)
3.3V → W25Q64 VCC
GND → W25Q64 GND

# Optional but recommended:
# - 100nF (0.1µF) capacitor between VCC and GND (close to chip)
```

**Testing External Flash:**
After wiring, test with firmware commands:
```c
external_flash_init();           // Initialize and verify JEDEC ID
external_flash_quick_test();     // Quick hardware verification
```

See [external-flash-quick-start.md](../technical/external-flash-quick-start.md) for detailed testing guide.

## 🧪 Hardware Testing

### Basic Connectivity Test
1. **Power On**: Verify 3.3V on all device VCC pins
2. **I2C Scan**: Use firmware to scan for expected devices
3. **LED Test**: Individual LED control verification
4. **Sensor Test**: RTC time reading and light sensor values

### Expected Device Detection
```
LEDs Bus (Port 0): 10 devices found
- TLC59116 Controllers: 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6A

Sensors Bus (Port 1): 2 devices found
- DS3231 RTC: 0x68
- BH1750 Light Sensor: 0x23

External Flash (Optional):
- W25Q64 SPI Flash: JEDEC ID 0xEF 0x40 0x17 (Winbond 8MB)
```

## ⚠️ Safety and Troubleshooting

### Safety Considerations
- **Voltage Levels**: Ensure all devices operate at 3.3V
- **Current Limits**: TLC59116 provides built-in LED current limiting
- **ESD Protection**: Handle ESP32 with anti-static precautions
- **Power Supply**: Use stable, filtered 5V DC supply

### Common Issues
| Problem | Cause | Solution |
|---------|-------|----------|
| Device not detected | Wiring error | Check SDA/SCL connections |
| LEDs not lighting | Address conflict | Verify TLC59116 address pins |
| Erratic behavior | Power supply noise | Add filtering capacitors |
| I2C timeouts | Bus speed too high | Firmware uses 100kHz (conservative) |
| External flash not detected | SPI wiring | Check GPIO 12/13/14/15 connections |
| Wrong JEDEC ID | Incorrect chip | Verify W25Q64 chip marking |

### Troubleshooting Tools
- **Multimeter**: Verify voltage levels and continuity
- **Logic Analyzer**: Debug I2C communication (optional)
- **Firmware Debug**: Built-in I2C scanning and device tests

---
🔧 **Difficulty**: Intermediate electronics  
⏱️ **Assembly Time**: 2-4 hours  
🛠️ **Tools Needed**: Soldering iron, multimeter, basic hand tools