# External Flash Quick Start Guide

## The Simplest Way to Test External Flash

Add these lines to your `main/wordclock.c` file:

```c
#include "external_flash.h"

void app_main(void)
{
    // ... your existing initialization code ...

    // Initialize external flash
    ESP_LOGI(TAG, "Initializing W25Q64 external flash...");
    esp_err_t ret = external_flash_init();

    if (ret == ESP_OK) {
        // Quick test (~1 second)
        ESP_LOGI(TAG, "Running flash verification test...");
        if (external_flash_quick_test()) {
            ESP_LOGI(TAG, "✅ W25Q64 flash ready for use");
        } else {
            ESP_LOGE(TAG, "❌ Flash test failed - check hardware connections");
        }
    } else {
        ESP_LOGE(TAG, "Flash init failed: %s", esp_err_to_name(ret));
    }

    // ... rest of your application ...
}
```

## Expected Output (Success)

```
I (1234) wordclock: Initializing W25Q64 external flash...
I (1235) external_flash: Initializing W25Q64 external flash...
I (1250) external_flash: JEDEC ID: Manufacturer=0xEF, MemType=0x40, Capacity=0x17
I (1251) external_flash: ✅ W25Q64 initialized successfully (8 MB)
I (1252) external_flash: GPIO: MISO=12, MOSI=14, CLK=13, CS=15
I (1260) wordclock: Running flash verification test...
I (1261) flash_test: Running quick smoke test...
I (1262) flash_test: Test 1: Initialization and availability
I (1263) flash_test: Flash is available
I (1264) flash_test: Flash size: 8388608 bytes (8 MB)
I (1270) flash_test: Test 2: JEDEC ID verification
I (1271) flash_test: JEDEC ID: 0xEF 0x40 0x17
I (1280) flash_test: Test 4: Basic write and read
I (1320) flash_test: Write/Read successful (pattern 0x55)
I (1321) flash_test: ✅ Quick test passed - flash is working!
I (1322) wordclock: ✅ W25Q64 flash ready for use
```

## Expected Output (Hardware Not Connected)

```
I (1234) wordclock: Initializing W25Q64 external flash...
I (1235) external_flash: Initializing W25Q64 external flash...
E (1250) external_flash: Failed to read JEDEC ID: ESP_ERR_TIMEOUT
E (1251) external_flash: Flash initialization may have failed
I (1260) wordclock: Running flash verification test...
I (1261) flash_test: Running quick smoke test...
E (1262) flash_test: Flash not available - initialization may have failed
E (1263) flash_test: Quick test failed: initialization
I (1264) wordclock: ❌ Flash test failed - check hardware connections
```

## Full Test Suite (Optional)

For comprehensive testing during development:

```c
#include "external_flash.h"

void app_main(void)
{
    // ... existing init ...

    if (external_flash_init() == ESP_OK) {
        // Run all 9 tests (takes ~5-10 seconds)
        ESP_LOGI(TAG, "Running comprehensive flash test suite...");
        int failed = external_flash_run_all_tests();

        if (failed == 0) {
            ESP_LOGI(TAG, "All flash tests passed!");
        } else {
            ESP_LOGE(TAG, "%d flash test(s) failed", failed);
        }
    }

    // ... rest of app ...
}
```

## Hardware Connection Checklist

Before testing, verify:

```
✅ W25Q64 VCC → ESP32 3.3V
✅ W25Q64 GND → ESP32 GND
✅ W25Q64 DI/SI → GPIO 14 (MOSI)
✅ W25Q64 DO/SO → GPIO 12 (MISO)
✅ W25Q64 CLK/SCK → GPIO 13
✅ W25Q64 /CS → GPIO 15
```

**Optional but recommended:**
- 100nF (0.1µF) capacitor between VCC and GND (close to W25Q64 chip)

## Troubleshooting

### "ESP_ERR_TIMEOUT" or "Failed to read JEDEC ID"
**Problem:** No SPI communication
**Solution:**
1. Check all 6 connections (VCC, GND, 4 GPIO pins)
2. Verify 3.3V power supply is stable
3. Check for loose breadboard connections

### "Unexpected flash chip! Expected W25Q64"
**Problem:** Wrong chip or JEDEC ID mismatch
**Solution:**
1. Verify chip marking says "W25Q64" (not W25Q32, W25Q128, etc.)
2. Check printed JEDEC ID - should be `0xEF 0x40 0x17`
3. If different chip, you may need to update expected values

### "Write/Read failed"
**Problem:** Partial connectivity or power issues
**Solution:**
1. Add 100nF capacitor if not present
2. Check power supply voltage (should be 3.0-3.6V)
3. Shorten wire lengths if using long jumper wires
4. Try reducing SPI speed in code (from 10MHz to 5MHz)

## Next Steps After Successful Test

1. ✅ **Flash working** → Ready to integrate chime storage
2. **Remove or comment out test code** for production
3. **Implement chime_storage component** (Phase 2.5 of implementation plan)
4. **Upload chime audio files** to flash memory

See: [chime-system-implementation-plan-w25q64.md](../implementation/chime-system-implementation-plan-w25q64.md)

## Production Code

For production, you typically only want the init check:

```c
void app_main(void)
{
    // ... existing init ...

    // Initialize flash (logs JEDEC ID automatically)
    esp_err_t ret = external_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "External flash not available");
        // Fall back to internal flash chimes if needed
    }

    // No need to run tests in production
    // The init function already verifies JEDEC ID

    // ... rest of app ...
}
```

The `external_flash_init()` function already:
- Verifies GPIO pins are connected
- Reads and validates JEDEC ID (0xEF 0x40 0x17)
- Reports success/failure

So for production, just checking the return value is sufficient!
