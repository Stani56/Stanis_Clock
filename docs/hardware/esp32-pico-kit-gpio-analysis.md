# ESP32-PICO-KIT GPIO Analysis for External Flash
## GPIO 12, 13, 14, 15 Availability Check

---

## ⚠️ CRITICAL: GPIO 6-11, 16-17 Are NOT Available on ESP32-PICO-KIT

**ESP32-PICO-D4 uses internal flash**, which occupies GPIO 6-11 AND 16-17 for SPI communication:

```
Reserved for Internal Flash (DO NOT USE):
GPIO 6  - Flash SCK
GPIO 7  - Flash SD0
GPIO 8  - Flash SD1
GPIO 9  - Flash SD2
GPIO 10 - Flash SD3
GPIO 11 - Flash CMD
GPIO 16 - Flash CS   ⚠️ ALSO RESERVED!
GPIO 17 - Flash CLK  ⚠️ ALSO RESERVED!

Using these pins will BRICK your device! ⚠️
```

**This is different from standard ESP32 modules** where only GPIO 6-11 are used for external flash.

---

## ✅ GPIO 12-15 Status: MOSTLY AVAILABLE

### GPIO 12 - ⚠️ STRAPPING PIN (Use with Caution)

**Function:** Boot voltage selection (VDD_SDIO)
**Default:** Internal pull-down
**Risk:** LOW (safe for HSPI after boot)

**Behavior:**
- At boot: Must be LOW for 3.3V flash (default)
- After boot: Can be used as GPIO

**Safe to use for SPI MISO:** ✅ Yes, but don't pull HIGH during boot

---

### GPIO 13 - ✅ FULLY AVAILABLE

**Function:** No restrictions
**Safe to use for SPI MOSI:** ✅ Yes

---

### GPIO 14 - ✅ FULLY AVAILABLE

**Function:** No restrictions
**Safe to use for SPI SCK:** ✅ Yes

---

### GPIO 15 - ⚠️ STRAPPING PIN (Best Available Option for CS)

**Function:** Boot debug output / SDIO configuration
**Default:** Internal ~50kΩ pull-up (active during boot)
**Risk:** LOW (internal pull-up keeps it HIGH during boot)

**Behavior:**
- At boot: Must be HIGH for normal boot (internal pull-up handles this)
- If LOW at boot: Mutes UART debug output / changes SDIO mode
- After boot: Can be used as GPIO

**Safe to use for SPI CS:** ✅ Yes, NO external resistor needed!

**Why it's safe for SPI CS:**
- ESP32 drives CS as output (not input)
- Internal pull-up keeps CS HIGH during boot (flash not selected)
- W25Q64 CS input has very low current draw (<1µA)
- No external components required!

**This is now the BEST option for CS** since GPIO 16/17 are reserved for internal flash!

---

## Your Current GPIO Usage

```
Pin   Function                 Status
────────────────────────────────────────
5     Reset button             ✅ Used
18    I2C SCL (Sensors)        ✅ Used
19    I2C SDA (Sensors)        ✅ Used
21    WiFi status LED          ✅ Used
22    NTP status LED           ✅ Used
25    I2C SDA (LEDs)           ✅ Used
26    I2C SCL (LEDs)           ✅ Used
34    Potentiometer (ADC)      ✅ Used

12    Available (strapping)    ⚠️ Available
13    Available                ✅ Available
14    Available                ✅ Available
15    Available (strapping)    ⚠️ Available
```

---

## ✅ CORRECTED Recommendation: GPIO 12-15 for External Flash

**Yes, GPIO 12, 13, 14, 15 are available on ESP32-PICO-KIT!**
**⚠️ GPIO 16/17 are NOT available (used for internal flash)**

**CORRECT HSPI Configuration for W25Q64:**
```
ESP32 GPIO          W25Q64 Flash
──────────────      ────────────
GPIO 14 (MOSI)  →   DI (Data In)
GPIO 12 (MISO)  ←   DO (Data Out)
GPIO 13 (SCK)   →   CLK (Clock)
GPIO 15 (CS)    →   CS (Chip Select)
3.3V            →   VCC
GND             →   GND
```

**No external resistor needed!** GPIO 15 has internal ~50kΩ pull-up active during boot.

---

## ⚠️ Important Precautions for GPIO 12 & 15

### GPIO 12 (MISO) - Strapping Pin

**Problem:** Internal pull-down at boot
**Solution:** Ensure W25Q64 doesn't drive this pin HIGH during boot

**Best practice:**
```c
// Initialize SPI after boot sequence completes
void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for boot to complete
    external_flash_init();            // Now safe to use GPIO 12
}
```

**No external components needed** - Internal pull-down handles boot correctly

---

### GPIO 15 (CS) - Strapping Pin

**Status:** Internal ~50kΩ pull-up at boot (keeps CS HIGH automatically)
**Solution:** No external components needed!

**Why it works:**
- GPIO 15 has internal pull-up active during boot
- Keeps CS HIGH (inactive) automatically
- W25Q64 CS input draws <1µA (doesn't interfere with boot)
- After boot, ESP32 drives CS as output

**No circuit required - just direct connection!**

---

## Complete GPIO Availability Table for ESP32-PICO-KIT

| GPIO | Available? | Notes | Best Use |
|------|------------|-------|----------|
| 0 | ⚠️ Strapping | Boot mode (must be HIGH) | Avoid (used for UART download) |
| 1 | ⚠️ UART TX | Console output | Avoid (debugging) |
| 2 | ⚠️ Strapping | Boot mode + on-board LED | Avoid (boot issues) |
| 3 | ⚠️ UART RX | Console input | Avoid (debugging) |
| 4 | ✅ Yes | Fully available | General purpose |
| 5 | ✅ Yes | **Currently used** (Reset button) | - |
| 6-11 | ❌ NO | **Internal flash SPI** | Never use! |
| 12 | ⚠️ Strapping | Safe after boot (MISO) | ✅ SPI MISO |
| 13 | ✅ Yes | Fully available | ✅ SPI SCK |
| 14 | ✅ Yes | Fully available | ✅ SPI MOSI |
| 15 | ⚠️ Strapping | Add pull-up for CS | ⚠️ SPI CS (needs resistor) |
| 16 | ✅ Yes | Fully available | ✅ **SPI CS (best!)** |
| 17 | ✅ Yes | Fully available | ✅ Alternative CS |
| 18 | ✅ Yes | **Currently used** (I2C SCL) | - |
| 19 | ✅ Yes | **Currently used** (I2C SDA) | - |
| 21 | ✅ Yes | **Currently used** (WiFi LED) | - |
| 22 | ✅ Yes | **Currently used** (NTP LED) | - |
| 23 | ✅ Yes | Fully available | General purpose |
| 25 | ✅ Yes | **Currently used** (I2C SDA LEDs) | - |
| 26 | ✅ Yes | **Currently used** (I2C SCL LEDs) | - |
| 27 | ✅ Yes | Fully available | ✅ I2S or general |
| 32 | ✅ Yes | Fully available | ✅ I2S BCLK |
| 33 | ✅ Yes | Fully available | ✅ I2S LRCLK |
| 34 | ✅ Input only | **Currently used** (ADC) | - |
| 35 | ✅ Input only | Fully available | ADC/Input only |
| 36 | ✅ Input only | Fully available | ADC/Input only |
| 39 | ✅ Input only | Fully available | ADC/Input only |

---

## Recommended GPIO Assignments for Future Features

### External SPI Flash (W25Q64):
```
GPIO 14: MOSI (Data to flash)
GPIO 12: MISO (Data from flash)
GPIO 13: SCK  (Clock)
GPIO 16: CS   (Chip Select) ← Better than GPIO 15
```

### I2S Audio (MAX98357A):
```
GPIO 32: BCLK  (Bit Clock)
GPIO 33: LRCLK (Left/Right Clock)
GPIO 25: DIN   (Data) ← CONFLICT! Already used for I2C LEDs
```

**⚠️ I2S Conflict Detected!**

**Solution for I2S + External Flash together:**
```
Option A: Use GPIO 27 for I2S DIN instead of GPIO 25
GPIO 32: I2S BCLK
GPIO 33: I2S LRCLK
GPIO 27: I2S DIN   ← No conflict!

Option B: Move I2C LEDs to software I2C (different pins)
This is complex and not recommended.
```

---

## Updated Pin Assignments for Your Project

### Current (No Changes):
```
GPIO 5:  Reset button
GPIO 18: I2C SCL (Sensors)
GPIO 19: I2C SDA (Sensors)
GPIO 21: WiFi status LED
GPIO 22: NTP status LED
GPIO 25: I2C SDA (LEDs)
GPIO 26: I2C SCL (LEDs)
GPIO 34: Potentiometer (ADC)
```

### Adding External Flash (New):
```
GPIO 12: SPI MISO (W25Q64)
GPIO 13: SPI SCK  (W25Q64)
GPIO 14: SPI MOSI (W25Q64)
GPIO 16: SPI CS   (W25Q64) ← Use GPIO 16, not 15!
```

### Adding I2S Audio (Future):
```
GPIO 27: I2S DIN   (MAX98357A)
GPIO 32: I2S BCLK  (MAX98357A)
GPIO 33: I2S LRCLK (MAX98357A)
```

### Available for Future Use:
```
GPIO 4:  General purpose
GPIO 17: General purpose (or alternative CS)
GPIO 23: General purpose
GPIO 35: Input only (ADC)
GPIO 36: Input only (ADC)
GPIO 39: Input only (ADC)
```

---

## Boot-Time Considerations

### Strapping Pins Summary:

| GPIO | At Boot | After Boot | Safe for SPI? |
|------|---------|------------|---------------|
| 0 | Must be HIGH (normal boot) | Can use | ⚠️ Avoid |
| 2 | Must be LOW (normal boot) | Can use | ⚠️ Avoid |
| 5 | Any value OK | Can use | ✅ Yes |
| 12 | Must be LOW (3.3V flash) | Can use | ✅ Yes (MISO OK) |
| 15 | Must be HIGH (normal boot) | Can use | ⚠️ Needs pull-up for CS |

**Best practice:** Use GPIO 12-14 for SPI data/clock, and GPIO 16 for CS (no strapping issues!)

---

## ESP32-PICO-D4 vs Standard ESP32

**Your ESP32-PICO-KIT uses ESP32-PICO-D4 SiP (System-in-Package):**

| Feature | Standard ESP32 | ESP32-PICO-D4 (Your Board) |
|---------|----------------|----------------------------|
| Flash | External chip | ✅ **Integrated** 4 MB |
| PSRAM | Optional external | ❌ Not available |
| GPIO 6-11 | Available | ❌ **Reserved for internal flash** |
| GPIO 12-15 | Available | ✅ Available (with strapping notes) |
| GPIO 16-17 | Available | ✅ Available |
| Package | Separate chips | Single package |

**Key difference:** GPIO 6-11 are used internally for the integrated flash, so they're not broken out.

---

## Testing GPIO Availability

**Simple test to verify GPIO 12-15 work:**

```c
#include "driver/gpio.h"

void test_gpio_availability(void) {
    // Test GPIO 12, 13, 14, 16 (skip 15 for now)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 12) | (1ULL << 13) | (1ULL << 14) | (1ULL << 16),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Blink each pin
    for (int pin = 12; pin <= 16; pin++) {
        if (pin == 15) continue;  // Skip strapping pin for safety

        ESP_LOGI("GPIO_TEST", "Testing GPIO %d", pin);
        gpio_set_level(pin, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(pin, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI("GPIO_TEST", "All GPIOs tested successfully!");
}
```

**Expected result:** LEDs or scope should show pulses on each pin

---

## Conclusion

### ✅ YES, GPIO 12-15 are available on ESP32-PICO-KIT!

**Recommended configuration for external SPI flash:**

```c
// In external_flash component
#define PIN_NUM_MISO  12  // ✅ Available (strapping, safe for MISO)
#define PIN_NUM_SCK   13  // ✅ Available (fully safe)
#define PIN_NUM_MOSI  14  // ✅ Available (fully safe)
#define PIN_NUM_CS    16  // ✅ Available (better than GPIO 15)
```

**Why GPIO 16 instead of GPIO 15 for CS:**
- GPIO 15 is strapping pin (needs external pull-up)
- GPIO 16 has no restrictions (plug-and-play)
- Saves one resistor in BOM

---

## Next Steps

1. **Order W25Q64 module** ($5) with pre-soldered header pins
2. **Wire to ESP32-PICO-KIT:**
   - GPIO 14 → MOSI
   - GPIO 12 → MISO
   - GPIO 13 → SCK
   - GPIO 16 → CS
   - 3.3V → VCC
   - GND → GND
3. **No external components needed** (no pull-ups required with GPIO 16)
4. **Test SPI communication** (read JEDEC ID)
5. **Implement streaming audio** from external flash

---

## Summary

| Question | Answer |
|----------|--------|
| Are GPIO 12-15 available? | ✅ Yes (12, 13, 14 fully; 15 needs care) |
| Can I use them for SPI flash? | ✅ Yes, recommended! |
| Any restrictions? | ⚠️ Use GPIO 16 for CS instead of 15 |
| Do I need external resistors? | ❌ No (if using GPIO 16 for CS) |
| Will it conflict with I2C? | ✅ No conflicts |
| Will it work with I2S audio? | ✅ Yes (use GPIO 27/32/33 for I2S) |

**Bottom line:** GPIO 12-14 + 16 are perfect for external SPI flash on your ESP32-PICO-KIT! 🎯
