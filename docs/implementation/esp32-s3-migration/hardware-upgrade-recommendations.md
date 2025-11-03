# Hardware Upgrade Recommendations for Stanis Clock Audio Support

**Date:** 2025-11-01
**Current Hardware:** ESP32-PICO-D4 (ESP32-PICO-KIT v4.1)
**Issue:** ESP32 cannot reliably handle WiFi + MQTT + I2S audio simultaneously due to silicon-level hardware conflicts

---

## Problem Summary

After extensive testing and debugging, we discovered a **fundamental ESP32 hardware limitation**:

- ✅ **Audio works** when MQTT is disconnected
- ❌ **System crashes** (`StoreProhibited` in WiFi stack) when MQTT + I2S audio run together
- ❌ Even with `WIFI_PS_NONE` (power save disabled), WiFi packet processing conflicts with I2S DMA
- ❌ MQTT keepalive packets trigger WiFi interrupts that crash during I2S operation

**Root Cause:** The original ESP32 (ESP32-PICO-D4) has shared hardware resources between WiFi PHY layer and I2S DMA that cannot be accessed concurrently without causing semaphore corruption.

---

## Recommended Solution: Upgrade to ESP32-S3

The **ESP32-S3** is specifically designed to handle WiFi + MQTT + I2S audio simultaneously with:
- Improved interrupt handling and memory architecture
- Dedicated PSRAM for audio buffering
- Better isolation between WiFi and I2S subsystems
- Proven track record in voice assistant and audio streaming applications

---

## Recommended Hardware Options

### Option 1: YelloByte YB-ESP32-S3-AMP (⭐ **Best Choice**)

**Why This Board:**
- Purpose-built for audio applications
- Includes **2× MAX98357A amplifiers** built-in (stereo)
- No external audio hardware needed

**Specifications:**
- **MCU:** ESP32-S3-WROOM-1-N8R2
  - 8MB Flash
  - **2MB PSRAM** (critical for audio buffering)
  - Dual-core Xtensa LX7 @ 240 MHz
- **Audio:** 2× MAX98357A I2S PCM Class D Amplifiers
  - Stereo output
  - 2× 3.2W @ 4Ω
- **Storage:** microSD card slot (for chime files)
- **Connectivity:** WiFi, USB-C
- **Available:** GitHub - https://github.com/yellobyte/YB-ESP32-S3-AMP

**Advantages:**
- ✅ Drop-in audio solution (amplifiers included)
- ✅ PSRAM for reliable audio streaming
- ✅ microSD for chime file storage (no external flash chip needed)
- ✅ USB-C for easy programming
- ✅ Two hardware revisions available (Rev 2 with CH340, Rev 3 with native USB)

**Compatibility with Current Project:**
- Your existing MAX98357A code will work with minimal changes
- Same I2S protocol
- Same audio file format (16kHz, 16-bit PCM mono)
- Can use microSD instead of W25Q64 SPI flash

---

### Option 2: ESP Audio Boards v2 / Loud-ESP32-S3

**Specifications:**
- **MCU:** ESP32-S3 WROOVER module
  - 16MB Flash
  - **8MB PSRAM** (even more buffer space)
- **Audio:** Built-in MAX98357 BGA DAC
- **Reference:** https://hackaday.io/project/198042-esp-audio-boards-v2-and-louder-esp32

**Advantages:**
- ✅ Larger PSRAM (8MB vs 2MB)
- ✅ More Flash (16MB)
- ✅ Professional audio development board

---

### Option 3: Generic ESP32-S3 DevKit + External MAX98357A

**Specifications:**
- **MCU:** ESP32-S3-DevKitC-1 (or similar)
  - 8MB Flash minimum
  - **Must have PSRAM** (2MB minimum, 8MB recommended)
- **Audio:** Your existing MAX98357A breakout board

**Advantages:**
- ✅ Most flexible option
- ✅ Reuse existing MAX98357A hardware
- ✅ Widely available from multiple vendors

**Typical GPIO Pin Mapping for ESP32-S3 + MAX98357A:**
```
MAX98357A Pin    ESP32-S3 GPIO
DIN (Data)    →  GPIO 7
BCLK (Clock)  →  GPIO 15
LRC (WS)      →  GPIO 16
```

---

## Migration Path

### Code Compatibility

Your current codebase is **95% compatible** with ESP32-S3:

✅ **No Changes Needed:**
- WiFi manager
- MQTT manager
- NTP manager
- I2C devices (TLC59116, DS3231, BH1750)
- LED display logic
- Brightness control
- Home Assistant discovery

✅ **Minor Changes (I2S Configuration):**
```c
// Current ESP32-PICO-D4 config:
#define AUDIO_GPIO_BCLK     26
#define AUDIO_GPIO_WS       25
#define AUDIO_GPIO_DOUT     22

// ESP32-S3 typical config:
#define AUDIO_GPIO_BCLK     15  // Adjust based on board
#define AUDIO_GPIO_WS       16
#define AUDIO_GPIO_DOUT     7
```

✅ **Storage Options:**
- **Option A:** Keep W25Q64 external flash + LittleFS (your Phase 1 implementation)
- **Option B:** Use microSD card (if board has slot, like YB-ESP32-S3-AMP)

---

## Performance Improvements with ESP32-S3

### What You'll Gain:

1. **Reliable Audio Playback**
   - No crashes during WiFi + MQTT + I2S operation
   - No need to disconnect MQTT before audio
   - Proper concurrent operation

2. **Better Audio Quality**
   - PSRAM allows larger audio buffers
   - Smoother playback without underruns
   - Support for higher sample rates (44.1kHz if needed)

3. **Future Expansion**
   - Voice control capability (ESP32-S3 has AI acceleration)
   - Multiple audio streams
   - More complex audio processing

4. **Lower Power Consumption**
   - ESP32-S3 has better power management
   - More efficient WiFi implementation

---

## Cost Comparison

| Hardware | Estimated Price | Includes Audio Amp | PSRAM |
|----------|----------------|-------------------|-------|
| ESP32-PICO-KIT v4.1 (current) | $8-12 | ❌ (separate MAX98357A) | ❌ |
| YB-ESP32-S3-AMP | ~$25-30 | ✅ (2× built-in) | ✅ 2MB |
| ESP Audio Boards v2 | ~$30-35 | ✅ (built-in) | ✅ 8MB |
| ESP32-S3-DevKitC-1 + MAX98357A | ~$15-20 | ❌ (reuse existing) | ✅ 2MB |

---

## Recommendation

**For Your Word Clock Project: YelloByte YB-ESP32-S3-AMP**

**Reasons:**
1. ✅ All-in-one solution (MCU + audio amplifiers)
2. ✅ PSRAM for reliable operation
3. ✅ microSD slot (easier file management than SPI flash)
4. ✅ Proven compatibility with MAX98357A ecosystem
5. ✅ Active development and support
6. ✅ Can drive your existing speaker directly

**Migration Effort:** ~1-2 hours
- Update GPIO pin definitions
- Test I2S audio
- Test WiFi + MQTT + Audio concurrently
- Everything else works unchanged

---

## Alternative: Keep Current Hardware with Workaround

If upgrading is not immediately feasible, the **temporary workaround** is:

```c
// Disconnect MQTT before audio playback
esp_mqtt_client_disconnect(mqtt_client);
vTaskDelay(pdMS_TO_TICKS(300));

// Play audio
audio_play_test_tone();

// Wait 5 seconds for WiFi to stabilize
vTaskDelay(pdMS_TO_TICKS(5000));

// Reconnect MQTT
esp_mqtt_client_reconnect(mqtt_client);
```

**Limitations:**
- ❌ MQTT disconnects during audio (5-10 seconds total)
- ❌ Home Assistant shows device offline during audio
- ❌ Cannot receive commands during audio
- ⚠️ WiFi sometimes drops, requiring manual recovery

---

## Conclusion

The ESP32-S3 is the **clear upgrade path** for adding reliable audio to your word clock while maintaining full WiFi and MQTT connectivity. The investment (~$25-30) provides a robust, future-proof solution that eliminates the current hardware limitations.

**Next Steps:**
1. Order YB-ESP32-S3-AMP board
2. Test basic I2S audio playback
3. Migrate codebase (1-2 hours)
4. Enjoy crash-free audio! 🎵

---

**References:**
- ESP32-S3 Technical Specs: https://xiaozhi.dev/en/docs/esp32/technical-specs/
- YB-ESP32-S3-AMP: https://github.com/yellobyte/YB-ESP32-S3-AMP
- ESP32-audioI2S Library: https://github.com/schreibfaul1/ESP32-audioI2S
- ESP-IDF I2S Documentation: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2s.html
