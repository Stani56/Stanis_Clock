# External Memory Options for Extended Audio Storage
## ESP32 Word Clock - When Internal Flash Isn't Enough

---

## When Do You Need External Memory?

**Current Situation:**
- ✅ Internal flash: 4 MB total
- ✅ Available for audio: 1.4 MB (or up to 2.5 MB if you sacrifice storage partition)
- ✅ Westminster chimes at 16 kHz: 608 KB

**You need external memory if:**
- ❌ Multiple chime styles (Westminster + Big Ben + church bells)
- ❌ Voice announcements (time, date, weather = hundreds of phrases)
- ❌ Music playback (songs, melodies)
- ❌ Long-form audio (podcasts, recordings)
- ❌ High sample rates (44.1 kHz = 2.76× larger files)

**Rule of thumb:** If you need **more than 2 MB of audio**, consider external memory.

---

## Option Comparison Table

| Option | Cost | Capacity | Speed | Complexity | Reliability | Best For |
|--------|------|----------|-------|------------|-------------|----------|
| **External SPI Flash** | $2-5 | 4-16 MB | Fast | Medium | High | **RECOMMENDED** ✅ |
| **SD Card** | $8-15 | Unlimited | Medium | High | Medium | User-replaceable files |
| **PSRAM** | $1-3 | 4-8 MB | Very Fast | Low | High | ❌ Volatile (loses data) |
| **Increase Internal** | $0 | +1.4 MB | Very Fast | Low | Very High | If <2 MB total needed |

---

## RECOMMENDED: External SPI Flash

### ✅ Best Choice for Extended Audio Storage

**Why:**
- ✅ Non-volatile (persists after power-off)
- ✅ Fast SPI interface (same speed as internal flash)
- ✅ Reliable (no moving parts like SD cards)
- ✅ Low cost ($2-5)
- ✅ Easy to integrate (uses existing SPI bus)
- ✅ 4-16 MB capacity (enough for extensive audio)

---

### Hardware: Winbond W25Q Flash Chips

**Recommended Models:**

| Model | Capacity | Cost | Part Number | Notes |
|-------|----------|------|-------------|-------|
| **W25Q32** | 4 MB | $2 | W25Q32JVSSIQ | Good starting point |
| **W25Q64** | 8 MB | $3 | W25Q64JVSSIQ | **BEST VALUE** ✅ |
| **W25Q128** | 16 MB | $5 | W25Q128JVSIQ | Maximum capacity |

**Where to Buy:**
- Adafruit: https://www.adafruit.com/product/4763 (W25Q32 breakout)
- Amazon: Search "W25Q64 SPI Flash Module"
- Digikey/Mouser: W25Q64JVSSIQ (bare chip)

---

### Wiring: SPI Flash to ESP32

**Connection Diagram:**

```
ESP32 GPIO          W25Q Flash Module
────────────────    ──────────────────
GPIO 23 (MOSI)  →   DI (Data In)
GPIO 19 (MISO)  ←   DO (Data Out)
GPIO 18 (SCK)   →   CLK (Clock)
GPIO 15 (CS)    →   CS (Chip Select)
3.3V            →   VCC (Power)
GND             →   GND (Ground)
```

**⚠️ IMPORTANT: GPIO Conflicts**

Current GPIO usage:
```
GPIO 18/19: I2C (Sensors/LEDs) ← Already used!
GPIO 23: Available ✅
```

**SOLUTION: Use HSPI (alternate SPI bus)**

ESP32 has **two SPI buses**: VSPI (default) and HSPI.

**HSPI Pin Assignment (ESP32-PICO-KIT Verified):**
```
ESP32 GPIO          W25Q Flash Module
────────────────    ──────────────────
GPIO 14 (MOSI)  →   DI (Data In)       ← HSPI MOSI ✅
GPIO 12 (MISO)  ←   DO (Data Out)      ← HSPI MISO ✅
GPIO 13 (SCK)   →   CLK (Clock)        ← HSPI SCK  ✅
GPIO 15 (CS)    →   CS (Chip Select)   ← No resistor needed!
3.3V            →   VCC
GND             →   GND
```

**Important Notes:**
- ✅ No conflicts with existing I2C on GPIO 18/19
- ✅ Verified available on ESP32-PICO-KIT V4.1
- ✅ GPIO 15 has internal ~50kΩ pull-up (no external resistor needed!)
- ✅ Internal pull-up keeps CS HIGH during boot automatically
- ❌ GPIO 6-11 AND 16-17 are reserved for internal flash (never use!)

**No external components required!** Just direct wiring.

See detailed GPIO analysis: [docs/hardware/esp32-pico-kit-gpio-analysis.md](../hardware/esp32-pico-kit-gpio-analysis.md)

---

### Software Implementation

#### Step 1: Add External Flash Component

**Create: `components/external_flash/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS "external_flash.c"
    INCLUDE_DIRS "include"
    REQUIRES "spi_flash" "driver"
)
```

**Create: `components/external_flash/include/external_flash.h`**
```c
#ifndef EXTERNAL_FLASH_H
#define EXTERNAL_FLASH_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Initialize external SPI flash
esp_err_t external_flash_init(void);

// Read data from external flash
esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size);

// Write data to external flash (with erase)
esp_err_t external_flash_write(uint32_t address, const void *data, size_t size);

// Erase sector (4 KB) at address
esp_err_t external_flash_erase_sector(uint32_t address);

// Get flash size
uint32_t external_flash_get_size(void);

#endif // EXTERNAL_FLASH_H
```

**Create: `components/external_flash/external_flash.c`**
```c
#include "external_flash.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ext_flash";

// HSPI pins (no conflict with I2C, verified for ESP32-PICO-KIT V4.1)
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 14
#define PIN_NUM_CLK  13
#define PIN_NUM_CS   15  // GPIO 15 (has internal ~50kΩ pull-up, no external resistor needed)

#define SPI_FLASH_SIZE (8 * 1024 * 1024)  // 8 MB (W25Q64)

static spi_device_handle_t spi_handle;

// W25Q command codes
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS     0x05
#define CMD_JEDEC_ID        0x9F

esp_err_t external_flash_init(void) {
    esp_err_t ret;

    // Configure SPI bus (HSPI)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attach flash device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz (conservative)
        .mode = 0,                            // SPI mode 0
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
        .flags = 0,
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read JEDEC ID to verify chip
    uint8_t tx_data[4] = {CMD_JEDEC_ID, 0, 0, 0};
    uint8_t rx_data[4];

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    ret = spi_device_transmit(spi_handle, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read JEDEC ID: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "External flash detected - JEDEC ID: %02X %02X %02X",
             rx_data[1], rx_data[2], rx_data[3]);

    // Expected for W25Q64: 0xEF 0x40 0x17
    // Expected for W25Q32: 0xEF 0x40 0x16

    return ESP_OK;
}

esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size) {
    if (address + size > SPI_FLASH_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = cmd,
    };

    spi_transaction_t t2 = {
        .length = size * 8,
        .rx_buffer = buffer,
        .flags = 0,
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &t);
    if (ret == ESP_OK) {
        ret = spi_device_transmit(spi_handle, &t2);
    }

    return ret;
}

// Additional write/erase functions omitted for brevity
// See ESP-IDF SPI flash examples for complete implementation

uint32_t external_flash_get_size(void) {
    return SPI_FLASH_SIZE;
}
```

---

#### Step 2: Modify Chime Library to Support External Flash

**Update: `components/chime_library/chime_library.h`**
```c
typedef enum {
    CHIME_STORAGE_INTERNAL,  // Embedded in firmware (current)
    CHIME_STORAGE_EXTERNAL,  // External SPI flash (new!)
    CHIME_STORAGE_SPIFFS,    // Internal SPIFFS partition
} chime_storage_t;

typedef struct {
    const char *name;
    chime_storage_t storage;
    union {
        struct {
            const uint8_t *data;  // Pointer to embedded data
            size_t length;
        } internal;
        struct {
            uint32_t address;     // Address in external flash
            size_t length;
        } external;
        struct {
            const char *path;     // Path in SPIFFS
        } spiffs;
    };
} chime_definition_t;
```

**Example Chime Definitions:**
```c
// Internal flash (embedded, for critical chimes)
static const chime_definition_t chime_quarter = {
    .name = "quarter",
    .storage = CHIME_STORAGE_INTERNAL,
    .internal = {
        .data = westminster_quarter,
        .length = westminster_quarter_len
    }
};

// External flash (for additional chimes)
static const chime_definition_t chime_bigben_hour = {
    .name = "bigben_hour",
    .storage = CHIME_STORAGE_EXTERNAL,
    .external = {
        .address = 0x000000,  // Start of external flash
        .length = 512000      // 500 KB
    }
};
```

---

#### Step 3: Update Audio Manager to Stream from External Flash

**Modify: `components/audio_manager/audio_manager.c`**
```c
esp_err_t audio_manager_play_chime(const chime_definition_t *chime) {
    switch (chime->storage) {
        case CHIME_STORAGE_INTERNAL:
            // Direct playback from internal flash (current method)
            return i2s_write(I2S_NUM_0,
                             chime->internal.data,
                             chime->internal.length,
                             &bytes_written,
                             portMAX_DELAY);

        case CHIME_STORAGE_EXTERNAL:
            // Stream from external flash in chunks
            return audio_stream_from_external_flash(
                chime->external.address,
                chime->external.length
            );

        case CHIME_STORAGE_SPIFFS:
            return audio_stream_from_file(chime->spiffs.path);
    }
}

static esp_err_t audio_stream_from_external_flash(uint32_t address, size_t length) {
    uint8_t buffer[4096];  // 4 KB chunk buffer
    size_t remaining = length;
    size_t bytes_written;

    while (remaining > 0) {
        size_t chunk_size = (remaining > 4096) ? 4096 : remaining;

        // Read chunk from external flash
        esp_err_t ret = external_flash_read(address, buffer, chunk_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read from external flash");
            return ret;
        }

        // Write chunk to I2S (speaker)
        ret = i2s_write(I2S_NUM_0, buffer, chunk_size, &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK || bytes_written != chunk_size) {
            ESP_LOGE(TAG, "Failed to write to I2S");
            return ret;
        }

        address += chunk_size;
        remaining -= chunk_size;
    }

    return ESP_OK;
}
```

---

### Storage Layout on External Flash

**8 MB External Flash Organization:**

```
External Flash (8 MB):
┌────────────────────────────────────────────────────┐
│ Address      Size        Content                   │
├────────────────────────────────────────────────────┤
│ 0x000000     512 KB      Big Ben Full Chimes       │ ← Hour strikes
├────────────────────────────────────────────────────┤
│ 0x080000     512 KB      Church Bell Chimes        │ ← Alternative style
├────────────────────────────────────────────────────┤
│ 0x100000     1 MB        Voice Announcements       │ ← "It's 3 o'clock"
│                          (English phrases)         │
├────────────────────────────────────────────────────┤
│ 0x200000     1 MB        Voice Announcements       │ ← "Es ist 3 Uhr"
│                          (German phrases)          │
├────────────────────────────────────────────────────┤
│ 0x300000     2 MB        Music/Melodies            │ ← Birthday song, etc.
├────────────────────────────────────────────────────┤
│ 0x500000     3 MB        Reserved / Future Use     │
└────────────────────────────────────────────────────┘
```

**Chime Address Map (`chime_map.h`):**
```c
// External flash addresses (8 MB W25Q64)
#define EXT_FLASH_BIGBEN_FULL       0x000000
#define EXT_FLASH_BIGBEN_QUARTER    0x040000
#define EXT_FLASH_CHURCH_FULL       0x080000
#define EXT_FLASH_VOICE_EN_START    0x100000
#define EXT_FLASH_VOICE_DE_START    0x200000
#define EXT_FLASH_MUSIC_START       0x300000
```

---

### Performance Analysis

**Read Speed:**
```
SPI Clock: 10 MHz (conservative)
Theoretical speed: 10 Mbps / 8 = 1.25 MB/s
Actual speed: ~1 MB/s (with overhead)

Audio data rate (16 kHz): 32 KB/s

Speed margin: 1000 KB/s ÷ 32 KB/s = 31×
→ Plenty of headroom! No buffer underruns.
```

**Latency:**
```
First byte: ~5 ms (SPI transaction setup)
Streaming: Continuous (no gaps)
```

**CPU Usage:**
```
SPI reads: <1% (DMA transfer)
I2S writes: <0.5% (DMA transfer)
Total: ~1.5% CPU overhead
```

---

### Programming External Flash

#### Method 1: Upload via MQTT (Recommended)

**User-friendly approach:**

1. User downloads chime WAV file
2. User sends via MQTT (base64 encoded)
3. ESP32 writes to external flash

**MQTT Topic:**
```
home/wordclock/external_flash/upload
```

**Payload:**
```json
{
  "address": "0x080000",
  "data": "base64_encoded_audio_data...",
  "length": 512000,
  "name": "church_bells_full"
}
```

---

#### Method 2: Flash Programmer (Factory Setup)

**One-time programming:**

```bash
# Convert audio to binary
ffmpeg -i bigben_full.wav -f s16le bigben_full.pcm

# Flash to external chip using esptool
esptool.py --port /dev/ttyUSB0 --baud 115200 \
    write_flash 0x080000 bigben_full.pcm
```

**Note:** Requires custom bootloader modification (advanced).

---

#### Method 3: Pre-program Before Soldering

**Best for production:**

1. Program W25Q chip using standalone programmer
2. Solder pre-programmed chip to board
3. ESP32 reads on first boot

**Programmer:** CH341A USB Programmer (~$5 on Amazon)

---

### Cost Analysis

| Component | Cost | Notes |
|-----------|------|-------|
| W25Q64 chip (8 MB) | $3 | Bare chip from Digikey |
| OR W25Q64 module | $5 | Pre-soldered breakout board |
| Jumper wires (6×) | $1 | If using module |
| **Total** | **$4-6** | One-time cost |

**Cost per MB:** $0.50/MB (vs $0 for internal, but limited)

---

### Advantages of External SPI Flash

✅ **Large capacity** - 4-16 MB (8-32× more than available internal)
✅ **Fast** - Same speed as internal flash (~1 MB/s)
✅ **Reliable** - No moving parts, no file system corruption
✅ **Non-volatile** - Persists after power-off
✅ **Low cost** - $3-5 for 8 MB
✅ **Easy to program** - Via MQTT or USB programmer
✅ **DMA-friendly** - Zero-copy streaming possible
✅ **No GPIO conflicts** - Uses HSPI bus (GPIO 12-15)

---

### Disadvantages

❌ **Extra hardware** - Need W25Q chip/module
❌ **Soldering** - If using bare chip (or buy module)
❌ **More complex code** - SPI driver + streaming logic
❌ **4 GPIO pins** - Uses GPIO 12, 13, 14, 15
❌ **Not user-serviceable** - Can't swap like SD card

---

## Alternative: SD Card (Less Recommended)

### When to Use SD Card

**Good for:**
- ✅ User wants to swap audio files easily (no computer needed)
- ✅ Very large libraries (100+ MB of audio)
- ✅ Frequent audio updates

**Bad for:**
- ❌ Permanent installation (SD cards can pop out)
- ❌ Harsh environments (corrosion, vibration)
- ❌ Production devices (too fragile)

---

### Hardware: SD Card Module

**Connection:**
```
ESP32 GPIO          SD Card Module
────────────────    ──────────────
GPIO 14 (MOSI)  →   MOSI (DI)
GPIO 2  (MISO)  ←   MISO (DO)
GPIO 15 (SCK)   →   SCK (CLK)
GPIO 13 (CS)    →   CS (CD)
5V              →   VCC (some modules need 5V)
GND             →   GND
```

**Cost:** $8-15 for module + SD card

---

### Software: SD Card Support

ESP-IDF has built-in SD card support:

```c
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

esp_err_t init_sd_card(void) {
    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host,
                                             &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", mount_point);
    return ESP_OK;
}

// Play audio from SD card
void play_from_sd(const char *filename) {
    FILE *f = fopen("/sdcard/chimes/bigben.pcm", "rb");
    uint8_t buffer[4096];

    while (fread(buffer, 1, 4096, f) > 0) {
        i2s_write(I2S_NUM_0, buffer, 4096, &bytes_written, portMAX_DELAY);
    }

    fclose(f);
}
```

---

### SD Card Reliability Issues

**Common problems:**
- ❌ File system corruption (power loss during write)
- ❌ Card pops out (mechanical failure)
- ❌ Counterfeit cards (fail after months)
- ❌ Slower than SPI flash (~500 KB/s vs 1 MB/s)
- ❌ Higher power consumption (SD protocol overhead)

**Mitigation:**
- Use high-quality cards (SanDisk, Samsung)
- Mount read-only (prevent corruption)
- Add card detect pin (know when card removed)

---

## Comparison: External Flash vs SD Card

| Feature | External SPI Flash | SD Card |
|---------|-------------------|---------|
| **Capacity** | 4-16 MB | Unlimited |
| **Speed** | 1 MB/s | 0.5 MB/s |
| **Reliability** | High | Medium |
| **User-replaceable** | No | Yes |
| **Cost** | $3-5 | $8-15 |
| **Complexity** | Medium | High |
| **File system** | No (raw binary) | Yes (FAT32) |
| **Power loss safety** | High | Low (corruption risk) |
| **Best for** | **Production** ✅ | Development/hobbyist |

---

## PSRAM (Not Suitable for Audio Storage)

### Why Not PSRAM?

**PSRAM (Pseudo-Static RAM):**
- ✅ 4-8 MB capacity
- ✅ Very fast (80 MHz)
- ❌ **VOLATILE** - Loses data when power off!

**Use case:** Runtime buffers only
- Decompress audio into PSRAM
- Mix multiple audio streams
- Audio effects processing

**NOT for persistent storage!**

---

## Implementation Timeline

### Phase 1: External SPI Flash Setup (1-2 days)
- Order W25Q64 module ($5)
- Wire to ESP32 (HSPI pins)
- Initialize SPI driver
- Test read/write

### Phase 2: Upload Tool (1 day)
- Create MQTT upload handler
- Base64 decode and write to flash
- Verify written data

### Phase 3: Streaming Playback (1 day)
- Implement chunked reading
- Integrate with I2S audio
- Test playback quality

### Phase 4: Chime Library Integration (1 day)
- Add external storage support
- Create address map
- Update MQTT Discovery

**Total time:** 4-5 days part-time

---

## Recommendation Summary

### For Your Word Clock Project:

**If you need <2 MB audio:**
→ ✅ **Use internal flash** (increase app partition to 3 MB)

**If you need 2-16 MB audio:**
→ ✅ **Use external SPI flash (W25Q64)** - Best balance

**If you need >16 MB or user-replaceable:**
→ ⚠️ **Use SD card** - More complex, less reliable

**Current situation (Westminster chimes only):**
→ ✅ **Stick with internal flash** (608 KB fits perfectly)

---

## Next Steps

1. **Assess your needs:**
   - How many chime styles? (1-2 styles = internal, 3+ = external)
   - Voice announcements? (Yes = external)
   - Music playback? (Yes = external or SD)

2. **Choose hardware:**
   - W25Q64 module from Amazon ($5)
   - Or W25Q128 for 16 MB ($5)

3. **Follow implementation plan** (detailed above)

4. **Test thoroughly:**
   - Write test audio to flash
   - Verify playback quality
   - Check for buffer underruns

---

## Conclusion

**Best option for extended audio: External SPI Flash (W25Q64)**

✅ 8 MB capacity (13× more than internal)
✅ Fast and reliable
✅ Low cost ($3-5)
✅ Production-ready
✅ Easy to integrate (HSPI bus, no GPIO conflicts)

**Perfect for:**
- Multiple chime styles
- Voice announcements
- Music/melody playback
- Future expansion

Let me know if you want me to create the detailed implementation plan for adding external SPI flash! 🔊
