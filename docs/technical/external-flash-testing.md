# External Flash Testing Guide

## Overview

The W25Q64 external flash driver includes comprehensive testing functions to verify hardware connectivity and functionality.

## Test Functions

### Quick Test (Recommended for Initial Verification)

```c
#include "external_flash.h"

// In your app_main() or initialization code
esp_err_t ret = external_flash_init();
if (ret == ESP_OK) {
    if (external_flash_quick_test()) {
        ESP_LOGI(TAG, "Flash hardware verified!");
    } else {
        ESP_LOGE(TAG, "Flash test failed - check wiring");
    }
}
```

**Quick test performs:**
- Initialization check
- JEDEC ID verification (0xEF 0x40 0x17)
- Basic write/read operation
- **Duration:** <1 second

### Full Test Suite (Comprehensive Validation)

```c
#include "external_flash.h"

// Run complete test suite
int failed_tests = external_flash_run_all_tests();

if (failed_tests == 0) {
    ESP_LOGI(TAG, "All tests passed!");
} else {
    ESP_LOGE(TAG, "%d tests failed", failed_tests);
}
```

**Full test suite includes 9 tests:**
1. Initialization and availability
2. JEDEC ID verification
3. Sector erase (4KB)
4. Basic write/read (256 bytes)
5. Page boundary handling (512 bytes across pages)
6. Multi-sector erase range (12KB / 3 sectors)
7. Alternating patterns (0x55, 0xAA, incrementing)
8. Large block read/write (4KB full sector)
9. Statistics tracking

**Duration:** 5-10 seconds

## Integration Example

### Option 1: Run on Startup (Development Mode)

Add to `main/wordclock.c`:

```c
#include "external_flash.h"

void app_main(void)
{
    // ... existing initialization ...

    // Initialize external flash
    ESP_LOGI(TAG, "Initializing external flash...");
    esp_err_t ret = external_flash_init();

    if (ret == ESP_OK) {
        // Quick test to verify hardware
        ESP_LOGI(TAG, "Running flash verification...");
        if (external_flash_quick_test()) {
            ESP_LOGI(TAG, "✅ W25Q64 flash verified and ready");
        } else {
            ESP_LOGE(TAG, "❌ Flash test failed - check GPIO wiring:");
            ESP_LOGE(TAG, "   GPIO 14 (MOSI), GPIO 12 (MISO)");
            ESP_LOGE(TAG, "   GPIO 13 (SCK),  GPIO 15 (CS)");
        }
    } else {
        ESP_LOGE(TAG, "Flash initialization failed: %s", esp_err_to_name(ret));
    }

    // ... rest of application ...
}
```

### Option 2: MQTT Command Trigger

Add test command handler to `main/wordclock_mqtt_handlers.c`:

```c
#include "external_flash.h"

// Add to command handler
if (strcmp(command, "test_flash") == 0) {
    ESP_LOGI(TAG, "Running flash test via MQTT...");

    int failed = external_flash_run_all_tests();

    char response[128];
    if (failed == 0) {
        snprintf(response, sizeof(response), "✅ All flash tests passed!");
    } else {
        snprintf(response, sizeof(response), "❌ %d flash tests failed", failed);
    }

    mqtt_publish_status(response);
}
```

### Option 3: GPIO Button Trigger

Add to button handler:

```c
#include "external_flash.h"

// Triple-press button to run flash test
if (button_press_count == 3) {
    ESP_LOGI(TAG, "Running flash diagnostic...");
    if (external_flash_quick_test()) {
        // Blink status LED green
        status_led_flash_success();
    } else {
        // Blink status LED red
        status_led_flash_error();
    }
}
```

## Test Coverage

### Hardware Verification
- ✅ SPI bus initialization
- ✅ GPIO pin connectivity (MISO, MOSI, SCK, CS)
- ✅ Chip detection via JEDEC ID
- ✅ SPI communication integrity

### Functional Testing
- ✅ Read operations (single byte to 4KB)
- ✅ Write operations (page programming)
- ✅ Erase operations (sector and range)
- ✅ Page boundary crossing (256-byte pages)
- ✅ Sector boundary alignment (4KB sectors)

### Data Integrity
- ✅ Pattern verification (0x55, 0xAA, 0xFF)
- ✅ Incrementing sequence validation
- ✅ Large block integrity (4KB)
- ✅ Erase state verification (all 0xFF)

### Performance Metrics
- ✅ Statistics tracking (reads, writes, erases)
- ✅ Timeout handling
- ✅ Error reporting

## Expected Test Output

```
I (1234) flash_test:
I (1234) flash_test: ========================================
I (1234) flash_test:   W25Q64 EXTERNAL FLASH TEST SUITE
I (1234) flash_test: ========================================
I (1234) flash_test:
I (1250) flash_test: Test 1: Initialization and availability
I (1251) flash_test: Flash is available
I (1252) flash_test: Flash size: 8388608 bytes (8 MB)
I (1253) flash_test: ✅ PASS: Test 1: Initialization
I (1260) flash_test: Test 2: JEDEC ID verification
I (1261) flash_test: JEDEC ID: 0xEF 0x40 0x17
I (1262) flash_test: ✅ PASS: Test 2: JEDEC ID
...
I (5678) flash_test:
I (5678) flash_test: ========================================
I (5678) flash_test:   TEST SUMMARY
I (5678) flash_test: ========================================
I (5678) flash_test: Total tests: 9
I (5678) flash_test: Passed: 9
I (5678) flash_test: Failed: 0
I (5678) flash_test:
I (5679) flash_test: ✅ ALL TESTS PASSED!
I (5679) flash_test:
```

## Troubleshooting Test Failures

### Test 1 Failed (Initialization)
**Symptom:** "Flash not available"

**Possible causes:**
- W25Q64 module not connected
- GPIO pins miswired
- Power not connected to flash module (VCC/GND)

**Check:**
```
GPIO 14 → MOSI (SI/DI on flash module)
GPIO 12 → MISO (SO/DO on flash module)
GPIO 13 → CLK  (SCK/CLK on flash module)
GPIO 15 → CS   (/CS chip select)
3.3V    → VCC
GND     → GND
```

### Test 2 Failed (JEDEC ID)
**Symptom:** "JEDEC ID mismatch" or "Chip verification failed"

**Possible causes:**
- Wrong flash chip (not W25Q64)
- SPI communication issues
- Incorrect wiring

**Check:**
1. Read JEDEC ID manually in logs
2. Expected: `0xEF 0x40 0x17` (Winbond W25Q64)
3. If different chip detected, update expected values in code

### Test 3-8 Failed (Read/Write/Erase)
**Symptom:** Data verification failures

**Possible causes:**
- Intermittent SPI connection
- Power supply issues (flash needs stable 3.3V)
- Counterfeit flash chip
- Bad sectors on flash

**Actions:**
1. Check power supply voltage (should be 3.0-3.6V)
2. Add decoupling capacitor (100nF) near flash VCC pin
3. Reduce SPI clock speed (change `SPI_CLOCK_SPEED_HZ` to 5 MHz)
4. Check for loose connections

### Test 9 Failed (Statistics)
**Symptom:** Counter mismatch

**Possible causes:**
- Previous tests partially failed
- Code logic error

**Action:** This test failure indicates a driver bug, not hardware issue

## Hardware Connection Diagram

```
ESP32-PICO-KIT V4.1          W25Q64 Module
┌─────────────────┐          ┌─────────────┐
│                 │          │             │
│  GPIO 14 (MOSI) ├─────────►│ DI/SI       │
│  GPIO 12 (MISO) │◄─────────┤ DO/SO       │
│  GPIO 13 (SCK)  ├─────────►│ CLK/SCK     │
│  GPIO 15 (CS)   ├─────────►│ /CS         │
│                 │          │             │
│  3.3V           ├─────────►│ VCC         │
│  GND            ├──────────┤ GND         │
└─────────────────┘          └─────────────┘

Note: Add 100nF capacitor between VCC and GND (close to chip)
```

## Test Data Locations

All tests use the **reserved area** at the end of flash:
- **Test Sector 1:** 0x700000 (7MB mark)
- **Test Sector 2:** 0x701000 (7MB + 4KB)
- **Test Sector 3:** 0x702000 (7MB + 8KB)

**Total test area:** 12KB (0x700000 - 0x703000)

These addresses are safely outside the chime data regions and will not interfere with audio storage.

## Production vs Development

### Development Mode
- Run full test suite on every boot
- Log all test details
- Useful during hardware bring-up

### Production Mode
- Run quick test only (or skip entirely)
- Log minimal info
- Rely on JEDEC ID verification in `external_flash_init()`

## Next Steps After Successful Testing

Once tests pass:
1. ✅ Hardware verified and working
2. ✅ Driver validated
3. ➡️ Ready to integrate chime audio storage
4. ➡️ Implement `chime_storage` component (Phase 2.5)
5. ➡️ Upload chime audio files to flash

See: [chime-system-implementation-plan-w25q64.md](../implementation/chime-system-implementation-plan-w25q64.md)
