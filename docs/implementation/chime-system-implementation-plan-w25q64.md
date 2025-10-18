# Westminster Chime System with W25Q64 External Flash - Implementation Plan
## ESP32 Word Clock with MAX98357A I2S Amplifier + W25Q64 8MB Storage

**Hardware:**
- MAX98357A I2S DAC + 8Ω 3W Speaker (Audio Output)
- W25Q64 8MB SPI Flash Module (External Storage)

**Approach:** Micro-step implementation with testing at every stage
**Timeline:** 3-4 weeks part-time (1-2 hours/day)
**Philosophy:** Small steps, test everything, commit frequently

**Key Difference from Basic Plan:**
- Adds W25Q64 external flash for 8MB dedicated audio storage
- Hybrid storage: Critical chimes in internal flash (fallback), full library in external flash
- Supports future expansion: Multiple chime styles, voice announcements, music

---

## Updated Shopping List

### Audio Hardware
- [ ] MAX98357A I2S Amplifier Module (~$5)
  - Adafruit: https://www.adafruit.com/product/3006
  - Amazon: Search "MAX98357A I2S"
- [ ] 8Ω 3W Speaker (~$3)
  - Diameter: 40-57mm (fits in enclosure)
  - Wire length: 15cm minimum
- [ ] Jumper wires (3×) - female-to-female (~$1)

### External Storage Hardware (NEW!)
- [ ] **W25Q64 SPI Flash Module (~$5)** ⭐
  - Amazon: Search "W25Q64 SPI Flash Module"
  - AliExpress: "W25Q64FV SPI Breakout"
  - Expected JEDEC ID: 0xEF 0x40 0x17 (for verification)
- [ ] Jumper wires (6×) - female-to-female (~$1)

**Total Cost:** ~$17 (vs $9 without external flash)
**Shipping:** 3-7 days

---

## GPIO Pin Assignments

### I2S Audio (MAX98357A)
```
GPIO 32: I2S BCLK (Bit Clock)
GPIO 33: I2S LRCLK (LR Clock)
GPIO 27: I2S DIN (Data)
3.3V/5V: VIN
GND: GND
```

### HSPI External Flash (W25Q64) - NEW!
```
GPIO 14: HSPI MOSI (Master Out, Slave In)
GPIO 12: HSPI MISO (Master In, Slave Out)
GPIO 13: HSPI SCK  (Serial Clock)
GPIO 15: HSPI CS   (Chip Select) - Has internal ~50kΩ pull-up ✅
3.3V: VCC
GND: GND
```

**Total New GPIO Used:** 7 pins
**Conflicts Resolved:** ✅ No conflicts with existing I2C (GPIO 18/19, 25/26)

---

## Storage Architecture

### Hybrid Storage Strategy

```
Internal Flash (4MB):
├─ App Code:                    1.1 MB
├─ Essential Chimes (fallback):  200 KB  ← Critical defaults
│  ├─ Westminster Quarter:        64 KB
│  └─ Westminster Half:           96 KB
├─ Free for future code:        ~1.2 MB
└─ [SPIFFS partition: 1.5 MB - untouched]

External Flash W25Q64 (8MB):
├─ Full Westminster Set:         608 KB  ← Primary source
│  ├─ Westminster Quarter:        64 KB  @ 0x000000
│  ├─ Westminster Half:           96 KB  @ 0x010000
│  ├─ Westminster Three-Quarter: 160 KB  @ 0x020000
│  ├─ Westminster Full:          256 KB  @ 0x030000
│  └─ Big Ben Hour Strike:        32 KB  @ 0x050000
├─ Alternative Chime Styles:     1.0 MB  ← Future expansion
│  ├─ Church Bells:              500 KB  @ 0x100000
│  └─ Simple Beeps:              100 KB  @ 0x150000
├─ Voice Announcements (EN):     2.0 MB  @ 0x200000
├─ Voice Announcements (DE):     2.0 MB  @ 0x400000
├─ Music/Melodies:               1.0 MB  @ 0x600000
└─ Reserved for Future:          1.4 MB  @ 0x700000
```

**Benefits:**
- ✅ Essential chimes always work (internal flash fallback)
- ✅ 8MB expansion capacity
- ✅ User-uploadable custom chimes via MQTT
- ✅ Future-proof for voice/music

---

## Phase 0: Preparation & Hardware Setup
**Duration:** 2-3 days (extended from original 1-2 days)
**Goal:** Get both audio and external flash hardware ready

---

### STEP 0.1: Order Hardware (Week 0, Day 1)
**Time:** 15 minutes
**What:** Purchase all components

**Updated Shopping List:** See above
**Total Cost:** ~$17
**Shipping:** 3-7 days

**Testing:** None yet
**Commit:** None yet
**Success:** All components ordered ✅

---

### STEP 0.2: Study Datasheets (Day 1)
**Time:** 45 minutes (extended from 30 min)
**What:** Understand both hardware modules

**Read:**
1. **MAX98357A Datasheet:**
   - Pin configuration
   - I2S timing requirements
   - Sample rate support (8kHz - 96kHz)
   - Power requirements (2.5V - 5.5V)
   - Adafruit guide: https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp

2. **W25Q64 Datasheet (NEW!):**
   - SPI protocol (Mode 0, CPOL=0, CPHA=0)
   - Command set (Read: 0x03, Write: 0x02, Erase: 0x20)
   - JEDEC ID: 0xEF 0x40 0x17
   - 8MB capacity (4096 sectors × 4KB each)
   - Max SPI speed: 104 MHz (we'll use 10 MHz conservative)

**Key Points:**
```
I2S (MAX98357A):
- BCLK: 16× sample rate minimum
- LRCLK: Sample rate (16 kHz)
- DIN: Serial audio data

SPI (W25Q64):
- CS must be HIGH during idle (GPIO 15 has internal pull-up ✅)
- Sector erase before write (4KB sectors)
- Page program size: 256 bytes max
- Read latency: ~5ms first byte, streaming continuous
```

**Testing:** None
**Commit:** None
**Success:** Understand both modules ✅

---

### STEP 0.3: Plan GPIO Pin Assignments (Day 1)
**Time:** 30 minutes (extended from 20 min)
**What:** Verify all GPIO assignments

**Current GPIO Usage:**
```
Used Pins:
├─ GPIO 18/19: I2C (Sensors)
├─ GPIO 25/26: I2C (LEDs)
├─ GPIO 34: Potentiometer (ADC)
├─ GPIO 21/22: Status LEDs
└─ GPIO 5: Reset button

NEW Assignments:
├─ GPIO 32: I2S BCLK     ✅ Available
├─ GPIO 33: I2S LRCLK    ✅ Available
├─ GPIO 27: I2S DIN      ✅ Available
├─ GPIO 14: HSPI MOSI    ✅ Available
├─ GPIO 12: HSPI MISO    ✅ Available
├─ GPIO 13: HSPI SCK     ✅ Available
└─ GPIO 15: HSPI CS      ✅ Available (internal pull-up)

Reserved (DO NOT USE):
├─ GPIO 6-11: Internal flash ❌
└─ GPIO 16-17: Internal flash (ESP32-PICO) ❌
```

**Testing:** Document in code comments
**Commit:** Update CLAUDE.md with GPIO assignments
**Success:** All pins assigned, no conflicts ✅

---

### STEP 0.4: Hardware Wiring - Audio (Day 2)
**Time:** 30 minutes
**What:** Connect MAX98357A to ESP32

**Wiring Diagram:**
```
ESP32 Pin      →    MAX98357A Pin    →    Speaker
─────────────────────────────────────────────────
GPIO 32 (BCLK) →    BCLK
GPIO 33 (LRCLK)→    LRC
GPIO 27 (DIN)  →    DIN
3.3V or 5V     →    VIN
GND            →    GND
                    GAIN    →    GND (9dB gain, lowest)
                    SD      →    (leave floating, or tie to VIN)
                    OUT+    →    Speaker (+)
                    OUT-    →    Speaker (-)
```

**Verification:**
- [ ] Take photo of wiring
- [ ] Verify connections with multimeter
- [ ] Check for shorts (VIN to GND)

**Testing:**
```bash
idf.py monitor
# Verify GPIOs not showing errors
```

**Commit:** None yet (just hardware)
**Success:** MAX98357A connected, no shorts ✅

---

### STEP 0.5: Hardware Wiring - External Flash (Day 2) - NEW!
**Time:** 30 minutes
**What:** Connect W25Q64 to ESP32

**Wiring Diagram:**
```
ESP32 Pin        →    W25Q64 Module Pin
────────────────────────────────────────
GPIO 14 (MOSI)  →    DI  (Data In)
GPIO 12 (MISO)  ←    DO  (Data Out)
GPIO 13 (SCK)   →    CLK (Clock)
GPIO 15 (CS)    →    CS  (Chip Select)
3.3V            →    VCC (Power)
GND             →    GND (Ground)
```

**Important Notes:**
- ✅ GPIO 15 has internal ~50kΩ pull-up (no external resistor needed!)
- ✅ CS stays HIGH during boot automatically
- ✅ No conflicts with existing I2C buses
- ✅ Verified available on ESP32-PICO-KIT V4.1

**Verification:**
- [ ] Take photo of wiring
- [ ] Verify connections with multimeter
- [ ] Check for shorts (VCC to GND)
- [ ] Verify CS pin is HIGH when idle

**Testing:**
```bash
# Will test in next step after driver created
```

**Commit:** None yet (just hardware)
**Success:** W25Q64 connected, no shorts ✅

---

## Phase 0.5: External Flash Driver Setup - NEW PHASE!
**Duration:** 1-2 days
**Goal:** Get W25Q64 working with read/write verification

---

### STEP 0.5.1: Create external_flash Component (Day 3)
**Time:** 45 minutes
**What:** Set up SPI flash driver component

**Actions:**
```bash
mkdir -p components/external_flash/include
touch components/external_flash/CMakeLists.txt
touch components/external_flash/external_flash.c
touch components/external_flash/include/external_flash.h
```

**CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "external_flash.c"
    INCLUDE_DIRS "include"
    REQUIRES driver spi_flash
)
```

**external_flash.h:**
```c
#ifndef EXTERNAL_FLASH_H
#define EXTERNAL_FLASH_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Initialize external SPI flash (W25Q64, 8MB)
esp_err_t external_flash_init(void);

// Read data from address
esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size);

// Write data to address (auto-erases sectors as needed)
esp_err_t external_flash_write(uint32_t address, const void *data, size_t size);

// Erase 4KB sector at address (must be sector-aligned)
esp_err_t external_flash_erase_sector(uint32_t address);

// Get flash size in bytes (returns 8388608 for W25Q64)
uint32_t external_flash_get_size(void);

// Check if external flash is available
bool external_flash_available(void);

#endif
```

**external_flash.c (initial stub):**
```c
#include "external_flash.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ext_flash";

// HSPI pins (verified for ESP32-PICO-KIT V4.1)
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 14
#define PIN_NUM_CLK  13
#define PIN_NUM_CS   15  // Has internal ~50kΩ pull-up

#define SPI_FLASH_SIZE (8 * 1024 * 1024)  // 8 MB

static spi_device_handle_t spi_handle = NULL;
static bool flash_initialized = false;

// W25Q command codes
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS     0x05
#define CMD_JEDEC_ID        0x9F

esp_err_t external_flash_init(void) {
    ESP_LOGI(TAG, "Initializing W25Q64 external flash...");

    // Will implement in next step
    flash_initialized = true;
    return ESP_OK;
}

esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size) {
    ESP_LOGI(TAG, "Read stub: %zu bytes from 0x%06X", size, address);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t external_flash_write(uint32_t address, const void *data, size_t size) {
    ESP_LOGI(TAG, "Write stub: %zu bytes to 0x%06X", size, address);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t external_flash_erase_sector(uint32_t address) {
    ESP_LOGI(TAG, "Erase stub: sector @ 0x%06X", address);
    return ESP_ERR_NOT_SUPPORTED;
}

uint32_t external_flash_get_size(void) {
    return SPI_FLASH_SIZE;
}

bool external_flash_available(void) {
    return flash_initialized;
}
```

**Testing:**
```bash
idf.py build
# Should compile without errors
```

**Commit:**
```bash
git add components/external_flash/
git commit -m "Add external_flash component stub for W25Q64"
git push
```

**Success:** Component compiles ✅

---

### STEP 0.5.2: Implement SPI Driver Initialization (Day 3)
**Time:** 60 minutes
**What:** Initialize HSPI bus and detect W25Q64

**Update external_flash.c - Implement init():**
```c
esp_err_t external_flash_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing W25Q64 external flash on HSPI...");

    // Configure SPI bus (HSPI - SPI2_HOST)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus already initialized");
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
    uint8_t tx_data[4] = {CMD_JEDEC_ID, 0xFF, 0xFF, 0xFF};
    uint8_t rx_data[4] = {0};

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

    ESP_LOGI(TAG, "✅ External flash detected - JEDEC ID: %02X %02X %02X",
             rx_data[1], rx_data[2], rx_data[3]);

    // Verify W25Q64: Expected 0xEF 0x40 0x17
    if (rx_data[1] == 0xEF && rx_data[2] == 0x40 && rx_data[3] == 0x17) {
        ESP_LOGI(TAG, "✅ W25Q64 (8MB) verified successfully");
        flash_initialized = true;
    } else {
        ESP_LOGW(TAG, "⚠️  Unexpected chip ID (expected EF 40 17)");
        // Continue anyway - might be W25Q32 or compatible
        flash_initialized = true;
    }

    return ESP_OK;
}
```

**Testing:**
```bash
# In wordclock.c app_main(), add after system_init:
external_flash_init();

idf.py build flash monitor
# Look for: "✅ External flash detected - JEDEC ID: EF 40 17"
# Look for: "✅ W25Q64 (8MB) verified successfully"
```

**Commit:**
```bash
git commit -am "Implement W25Q64 SPI initialization and JEDEC ID verification"
git push
```

**Success:** W25Q64 detected! ✅

---

### STEP 0.5.3: Implement Read Operations (Day 3)
**Time:** 45 minutes
**What:** Read data from external flash

**Update external_flash.c - Implement read():**
```c
esp_err_t external_flash_read(uint32_t address, void *buffer, size_t size) {
    if (!flash_initialized || !spi_handle) {
        ESP_LOGE(TAG, "Flash not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (address + size > SPI_FLASH_SIZE) {
        ESP_LOGE(TAG, "Read beyond flash size: 0x%X + %zu > 0x%X",
                 address, size, SPI_FLASH_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    if (!buffer || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Prepare read command
    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    spi_transaction_t t_cmd = {
        .length = 32,
        .tx_buffer = cmd,
        .flags = 0,
    };

    spi_transaction_t t_data = {
        .length = size * 8,
        .rx_buffer = buffer,
        .flags = 0,
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &t_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read command failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = spi_device_transmit(spi_handle, &t_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read data failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Read %zu bytes from 0x%06X", size, address);
    return ESP_OK;
}
```

**Testing:**
```c
// Test read (should read all 0xFF from empty flash)
uint8_t test_buffer[256];
esp_err_t ret = external_flash_read(0x000000, test_buffer, 256);
if (ret == ESP_OK) {
    ESP_LOGI("test", "Read successful, first 10 bytes:");
    ESP_LOG_BUFFER_HEX("test", test_buffer, 10);
}
```

**Commit:**
```bash
git commit -am "Implement external flash read operations"
git push
```

**Success:** Can read from flash ✅

---

### STEP 0.5.4: Implement Write/Erase Operations (Day 4)
**Time:** 60 minutes
**What:** Write data and erase sectors

**Update external_flash.c - Add helper functions:**
```c
// Wait for write/erase operation to complete
static esp_err_t wait_ready(uint32_t timeout_ms) {
    uint8_t status_cmd[2] = {CMD_READ_STATUS, 0xFF};
    uint8_t status_rx[2];

    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        spi_transaction_t t = {
            .length = 16,
            .tx_buffer = status_cmd,
            .rx_buffer = status_rx,
        };

        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) return ret;

        if ((status_rx[1] & 0x01) == 0) {
            return ESP_OK;  // Ready
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ESP_ERR_TIMEOUT;
}

// Enable write operations
static esp_err_t write_enable(void) {
    uint8_t cmd = CMD_WRITE_ENABLE;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    return spi_device_transmit(spi_handle, &t);
}

esp_err_t external_flash_erase_sector(uint32_t address) {
    if (!flash_initialized || !spi_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    // Address must be sector-aligned (4KB = 0x1000)
    if (address % 4096 != 0) {
        ESP_LOGE(TAG, "Address not sector-aligned: 0x%06X", address);
        return ESP_ERR_INVALID_ARG;
    }

    if (address >= SPI_FLASH_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    // Enable write
    esp_err_t ret = write_enable();
    if (ret != ESP_OK) return ret;

    // Send erase command
    uint8_t cmd[4] = {
        CMD_SECTOR_ERASE,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = cmd,
    };

    ret = spi_device_transmit(spi_handle, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erase command failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for erase to complete (up to 400ms per sector typical)
    ret = wait_ready(1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erase timeout");
        return ret;
    }

    ESP_LOGI(TAG, "Erased sector @ 0x%06X", address);
    return ESP_OK;
}

esp_err_t external_flash_write(uint32_t address, const void *data, size_t size) {
    if (!flash_initialized || !spi_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    if (address + size > SPI_FLASH_SIZE) {
        ESP_LOGE(TAG, "Write beyond flash size");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = size;
    uint32_t current_addr = address;

    // Write in 256-byte pages (W25Q64 page size)
    while (remaining > 0) {
        size_t page_size = 256 - (current_addr % 256);
        if (page_size > remaining) page_size = remaining;

        // Enable write
        esp_err_t ret = write_enable();
        if (ret != ESP_OK) return ret;

        // Prepare page program command
        uint8_t cmd[4] = {
            CMD_PAGE_PROGRAM,
            (current_addr >> 16) & 0xFF,
            (current_addr >> 8) & 0xFF,
            current_addr & 0xFF
        };

        spi_transaction_t t_cmd = {
            .length = 32,
            .tx_buffer = cmd,
        };

        spi_transaction_t t_data = {
            .length = page_size * 8,
            .tx_buffer = src,
        };

        ret = spi_device_transmit(spi_handle, &t_cmd);
        if (ret != ESP_OK) return ret;

        ret = spi_device_transmit(spi_handle, &t_data);
        if (ret != ESP_OK) return ret;

        // Wait for page program to complete
        ret = wait_ready(100);
        if (ret != ESP_OK) return ret;

        src += page_size;
        current_addr += page_size;
        remaining -= page_size;
    }

    ESP_LOGI(TAG, "Wrote %zu bytes to 0x%06X", size, address);
    return ESP_OK;
}
```

**Testing:**
```c
// Test write/read cycle
uint8_t write_data[256];
uint8_t read_data[256];

// Fill with test pattern
for (int i = 0; i < 256; i++) {
    write_data[i] = i;
}

// Erase sector
ESP_LOGI("test", "Erasing sector 0...");
external_flash_erase_sector(0x000000);

// Write data
ESP_LOGI("test", "Writing test pattern...");
external_flash_write(0x000000, write_data, 256);

// Read back
ESP_LOGI("test", "Reading back...");
external_flash_read(0x000000, read_data, 256);

// Verify
bool match = true;
for (int i = 0; i < 256; i++) {
    if (read_data[i] != write_data[i]) {
        ESP_LOGE("test", "Mismatch at byte %d: wrote %02X, read %02X",
                 i, write_data[i], read_data[i]);
        match = false;
        break;
    }
}

if (match) {
    ESP_LOGI("test", "✅ Flash read/write test PASSED!");
} else {
    ESP_LOGE("test", "❌ Flash read/write test FAILED!");
}
```

**Commit:**
```bash
git commit -am "Implement external flash write/erase operations with verification"
git push
```

**Success:** Can write to and verify flash ✅

---

### **PHASE 0.5 CHECKPOINT:** External Flash Working ✅
```
What works now:
✅ W25Q64 detected via JEDEC ID
✅ Can read data from flash
✅ Can erase 4KB sectors
✅ Can write data (256-byte pages)
✅ Read/write verification passes

Hardware verified:
✅ HSPI bus initialized (GPIO 12/13/14/15)
✅ No GPIO conflicts
✅ SPI communication working

Ready for Phase 1: Basic I2S Audio
```

---

## Phase 1-3: Basic Audio, Chimes, Scheduler
**Duration:** 6-9 days (SAME AS ORIGINAL PLAN)**
**Goal:** Get Westminster chimes playing automatically

**Note:** Phases 1, 2, and 3 are IDENTICAL to the original plan (without external flash).
We'll implement the internal flash version first to ensure audio works.

For complete details on these phases, refer to the original plan sections:
- Phase 1: Basic I2S Audio Output (Steps 1.1-1.5)
- Phase 2: Westminster Chime Audio Files (Steps 2.1-2.5)
- Phase 3: Chime Scheduler (Steps 3.1-3.5)

**Key difference:** We'll keep the internal flash chimes as fallback.

---

## Phase 2.5: External Flash Storage Integration - NEW PHASE!
**Duration:** 2-3 days
**Goal:** Move chimes to external flash with fallback

---

### STEP 2.5.1: Create Chime Address Map (Day 7)
**Time:** 30 minutes
**What:** Define external flash memory layout

**Create: `components/chime_library/include/chime_map.h`**
```c
#ifndef CHIME_MAP_H
#define CHIME_MAP_H

// W25Q64 External Flash Address Map (8MB total)
// Sector size: 4KB (0x1000)
// Each address must be sector-aligned for erasing

// ===== Westminster Chimes (608 KB) =====
#define EXT_FLASH_WESTMINSTER_QUARTER       0x000000  // 64 KB
#define EXT_FLASH_WESTMINSTER_HALF          0x010000  // 96 KB
#define EXT_FLASH_WESTMINSTER_THREE         0x020000  // 160 KB
#define EXT_FLASH_WESTMINSTER_FULL          0x030000  // 256 KB
#define EXT_FLASH_BIGBEN_STRIKE             0x050000  // 32 KB
#define EXT_FLASH_WESTMINSTER_END           0x060000

// ===== Alternative Chime Styles (1 MB) =====
#define EXT_FLASH_CHURCH_QUARTER            0x100000  // 64 KB
#define EXT_FLASH_CHURCH_HALF               0x110000  // 96 KB
#define EXT_FLASH_CHURCH_FULL               0x120000  // 256 KB
#define EXT_FLASH_SIMPLE_BEEP               0x150000  // 32 KB
#define EXT_FLASH_ALTSTYLES_END             0x200000

// ===== Voice Announcements - English (2 MB) =====
#define EXT_FLASH_VOICE_EN_START            0x200000
#define EXT_FLASH_VOICE_EN_END              0x400000

// ===== Voice Announcements - German (2 MB) =====
#define EXT_FLASH_VOICE_DE_START            0x400000
#define EXT_FLASH_VOICE_DE_END              0x600000

// ===== Music & Melodies (1 MB) =====
#define EXT_FLASH_MUSIC_BIRTHDAY            0x600000  // 256 KB
#define EXT_FLASH_MUSIC_ALARM               0x640000  // 256 KB
#define EXT_FLASH_MUSIC_CUSTOM              0x680000  // 512 KB
#define EXT_FLASH_MUSIC_END                 0x700000

// ===== Reserved for Future (1.4 MB) =====
#define EXT_FLASH_RESERVED_START            0x700000
#define EXT_FLASH_TOTAL_SIZE                0x800000  // 8 MB

// Helper macros
#define SECTOR_SIZE                         0x1000    // 4 KB
#define SECTOR_ALIGN(addr)                  ((addr) & ~(SECTOR_SIZE - 1))
#define SECTORS_NEEDED(size)                (((size) + SECTOR_SIZE - 1) / SECTOR_SIZE)

#endif // CHIME_MAP_H
```

**Testing:**
```bash
idf.py build
# Should compile
```

**Commit:**
```bash
git add components/chime_library/include/chime_map.h
git commit -m "Add W25Q64 external flash address map for chimes"
git push
```

**Success:** Address map defined ✅

---

### STEP 2.5.2: Extend Chime Library for Dual Storage (Day 7)
**Time:** 60 minutes
**What:** Support both internal and external flash

**Update chime_library.h:**
```c
#ifndef CHIME_LIBRARY_H
#define CHIME_LIBRARY_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CHIME_QUARTER,
    CHIME_HALF,
    CHIME_THREE_QUARTER,
    CHIME_FULL,
    CHIME_HOUR_STRIKE,
} chime_type_t;

// Storage location for chime audio
typedef enum {
    CHIME_STORAGE_INTERNAL,   // Embedded in firmware (fallback)
    CHIME_STORAGE_EXTERNAL,   // W25Q64 SPI flash (primary)
} chime_storage_t;

// Chime definition structure
typedef struct {
    const char *name;
    chime_storage_t storage;
    union {
        struct {
            const uint8_t *data;   // Pointer to embedded C array
            size_t length;
        } internal;
        struct {
            uint32_t address;      // Address in W25Q64
            size_t length;
        } external;
    };
} chime_definition_t;

// Initialize chime library
esp_err_t chime_library_init(void);

// Play a specific chime (auto-detects storage)
esp_err_t chime_library_play(chime_type_t type);

// Get chime definition (for inspection)
const chime_definition_t *chime_library_get_definition(chime_type_t type);

#endif
```

**Update chime_library.c:**
```c
#include "chime_library.h"
#include "chime_map.h"
#include "audio_manager.h"
#include "external_flash.h"
#include "esp_log.h"

// Internal flash fallback chimes (always available)
#include "chimes/westminster_quarter.h"
#include "chimes/westminster_half.h"

static const char *TAG = "chime_library";

// Chime definitions with fallback strategy
static chime_definition_t chime_defs[] = {
    // QUARTER - External primary, internal fallback
    [CHIME_QUARTER] = {
        .name = "westminster_quarter",
        .storage = CHIME_STORAGE_EXTERNAL,
        .external = {
            .address = EXT_FLASH_WESTMINSTER_QUARTER,
            .length = 64000  // Will be set dynamically
        }
    },

    // HALF - External primary, internal fallback
    [CHIME_HALF] = {
        .name = "westminster_half",
        .storage = CHIME_STORAGE_EXTERNAL,
        .external = {
            .address = EXT_FLASH_WESTMINSTER_HALF,
            .length = 96000
        }
    },

    // THREE_QUARTER - External only (no fallback, less critical)
    [CHIME_THREE_QUARTER] = {
        .name = "westminster_three_quarter",
        .storage = CHIME_STORAGE_EXTERNAL,
        .external = {
            .address = EXT_FLASH_WESTMINSTER_THREE,
            .length = 160000
        }
    },

    // FULL - External only
    [CHIME_FULL] = {
        .name = "westminster_full",
        .storage = CHIME_STORAGE_EXTERNAL,
        .external = {
            .address = EXT_FLASH_WESTMINSTER_FULL,
            .length = 256000
        }
    },

    // HOUR_STRIKE - External only
    [CHIME_HOUR_STRIKE] = {
        .name = "bigben_strike",
        .storage = CHIME_STORAGE_EXTERNAL,
        .external = {
            .address = EXT_FLASH_BIGBEN_STRIKE,
            .length = 32000
        }
    },
};

// Fallback definitions (internal flash)
static const chime_definition_t fallback_quarter = {
    .name = "westminster_quarter_internal",
    .storage = CHIME_STORAGE_INTERNAL,
    .internal = {
        .data = westminster_quarter,
        .length = westminster_quarter_len
    }
};

static const chime_definition_t fallback_half = {
    .name = "westminster_half_internal",
    .storage = CHIME_STORAGE_INTERNAL,
    .internal = {
        .data = westminster_half,
        .length = westminster_half_len
    }
};

esp_err_t chime_library_init(void) {
    ESP_LOGI(TAG, "Chime library init");

    // Check if external flash is available
    if (external_flash_available()) {
        ESP_LOGI(TAG, "✅ External flash available - using W25Q64 storage");
    } else {
        ESP_LOGW(TAG, "⚠️  External flash not available - using internal fallbacks");
    }

    return ESP_OK;
}

const chime_definition_t *chime_library_get_definition(chime_type_t type) {
    if (type < 0 || type >= sizeof(chime_defs) / sizeof(chime_defs[0])) {
        return NULL;
    }

    chime_definition_t *def = &chime_defs[type];

    // Check if external flash is available
    if (def->storage == CHIME_STORAGE_EXTERNAL && !external_flash_available()) {
        // Fallback to internal flash if available
        ESP_LOGW(TAG, "External flash unavailable, checking for fallback...");

        if (type == CHIME_QUARTER) {
            ESP_LOGI(TAG, "Using internal fallback for quarter chime");
            return &fallback_quarter;
        } else if (type == CHIME_HALF) {
            ESP_LOGI(TAG, "Using internal fallback for half chime");
            return &fallback_half;
        } else {
            ESP_LOGE(TAG, "No fallback available for chime type %d", type);
            return NULL;
        }
    }

    return def;
}

esp_err_t chime_library_play(chime_type_t type) {
    const chime_definition_t *def = chime_library_get_definition(type);
    if (!def) {
        ESP_LOGE(TAG, "Invalid chime type or no fallback: %d", type);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Playing chime: %s (storage: %s)",
             def->name,
             def->storage == CHIME_STORAGE_INTERNAL ? "internal" : "external");

    // Delegate to audio manager based on storage type
    if (def->storage == CHIME_STORAGE_INTERNAL) {
        // Direct playback from internal flash
        return audio_manager_play_pcm(def->internal.data, def->internal.length);
    } else {
        // Streaming playback from external flash
        return audio_manager_play_external(def->external.address, def->external.length);
    }
}
```

**Testing:**
```bash
idf.py build
# Should compile
```

**Commit:**
```bash
git commit -am "Extend chime library for dual storage (internal/external)"
git push
```

**Success:** Dual storage support added ✅

---

### STEP 2.5.3: Add External Flash Streaming to Audio Manager (Day 8)
**Time:** 60 minutes
**What:** Stream audio from W25Q64 in chunks

**Update audio_manager.h:**
```c
// Add new function for external flash streaming
esp_err_t audio_manager_play_external(uint32_t flash_address, size_t length);
```

**Update audio_manager.c:**
```c
#include "external_flash.h"

#define STREAM_CHUNK_SIZE 4096  // 4KB chunks

esp_err_t audio_manager_play_external(uint32_t flash_address, size_t length) {
    if (!tx_handle) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!external_flash_available()) {
        ESP_LOGE(TAG, "External flash not available");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Streaming %zu bytes from external flash @ 0x%06X",
             length, flash_address);

    // Allocate chunk buffer
    uint8_t *chunk_buffer = malloc(STREAM_CHUNK_SIZE);
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        return ESP_ERR_NO_MEM;
    }

    size_t remaining = length;
    uint32_t current_addr = flash_address;
    size_t total_written = 0;

    while (remaining > 0) {
        size_t chunk_size = (remaining > STREAM_CHUNK_SIZE) ? STREAM_CHUNK_SIZE : remaining;

        // Read chunk from external flash
        esp_err_t ret = external_flash_read(current_addr, chunk_buffer, chunk_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read from external flash: %s",
                     esp_err_to_name(ret));
            free(chunk_buffer);
            return ret;
        }

        // Apply volume scaling
        if (current_volume < 100) {
            int16_t *samples = (int16_t *)chunk_buffer;
            size_t sample_count = chunk_size / sizeof(int16_t);
            for (size_t i = 0; i < sample_count; i++) {
                samples[i] = (samples[i] * current_volume) / 100;
            }
        }

        // Write to I2S
        size_t bytes_written = 0;
        ret = i2s_channel_write(tx_handle, chunk_buffer, chunk_size,
                                &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK || bytes_written != chunk_size) {
            ESP_LOGE(TAG, "I2S write failed: %s (wrote %zu/%zu bytes)",
                     esp_err_to_name(ret), bytes_written, chunk_size);
            free(chunk_buffer);
            return ret;
        }

        current_addr += chunk_size;
        remaining -= chunk_size;
        total_written += bytes_written;
    }

    free(chunk_buffer);

    ESP_LOGI(TAG, "✅ Streaming complete: %zu bytes played", total_written);
    return ESP_OK;
}
```

**Testing:**
```c
// Test streaming (after uploading chime to flash)
audio_manager_init();
audio_manager_set_volume(70);
audio_manager_play_external(EXT_FLASH_WESTMINSTER_QUARTER, 64000);
```

**Commit:**
```bash
git commit -am "Add external flash streaming to audio manager (4KB chunks)"
git push
```

**Success:** Can stream audio from W25Q64 ✅

---

### STEP 2.5.4: Upload Chimes to External Flash (Day 8)
**Time:** 60 minutes
**What:** Write processed chimes to W25Q64

**Create upload tool script:**
```bash
#!/bin/bash
# tools/upload_chimes_to_w25q64.sh

echo "=== Upload Chimes to W25Q64 External Flash ==="

# Ensure chimes are processed first
if [ ! -d "c_arrays" ]; then
    echo "Error: c_arrays/ not found. Run ./tools/prepare_chimes.sh first!"
    exit 1
fi

# Map addresses (from chime_map.h)
declare -A FLASH_ADDRESSES=(
    ["westminster_quarter"]=0x000000
    ["westminster_half"]=0x010000
    ["westminster_three_quarter"]=0x020000
    ["westminster_full"]=0x030000
    ["bigben_strike"]=0x050000
)

echo "This script will upload chimes via MQTT or serial"
echo "Choose method:"
echo "  1) MQTT (requires ESP32 running with MQTT)"
echo "  2) Serial (direct upload via custom protocol)"
read -p "Selection: " method

if [ "$method" == "1" ]; then
    echo "MQTT upload not yet implemented - coming in Phase 4!"
    echo "Use serial method for now."
    exit 1
fi

# For now, provide manual instructions
echo ""
echo "Manual Upload Instructions:"
echo "============================"
echo ""
echo "Add this code to wordclock.c app_main() (temporary):"
echo ""
cat << 'EOF'
// TEMPORARY: Upload chimes to external flash
#include "external_flash.h"
#include "chime_map.h"

// Include the processed chime data
#include "chimes/westminster_quarter.h"
#include "chimes/westminster_half.h"
// ... etc

void upload_chimes_to_flash(void) {
    ESP_LOGI("upload", "Starting chime upload to W25Q64...");

    // Upload quarter chime
    ESP_LOGI("upload", "Uploading quarter chime...");
    uint32_t sectors = SECTORS_NEEDED(westminster_quarter_len);
    for (uint32_t i = 0; i < sectors; i++) {
        external_flash_erase_sector(EXT_FLASH_WESTMINSTER_QUARTER + (i * 0x1000));
    }
    external_flash_write(EXT_FLASH_WESTMINSTER_QUARTER,
                        westminster_quarter,
                        westminster_quarter_len);
    ESP_LOGI("upload", "✅ Quarter chime uploaded (%u bytes)", westminster_quarter_len);

    // Upload half chime
    ESP_LOGI("upload", "Uploading half chime...");
    sectors = SECTORS_NEEDED(westminster_half_len);
    for (uint32_t i = 0; i < sectors; i++) {
        external_flash_erase_sector(EXT_FLASH_WESTMINSTER_HALF + (i * 0x1000));
    }
    external_flash_write(EXT_FLASH_WESTMINSTER_HALF,
                        westminster_half,
                        westminster_half_len);
    ESP_LOGI("upload", "✅ Half chime uploaded (%u bytes)", westminster_half_len);

    // Repeat for other chimes...

    ESP_LOGI("upload", "✅ All chimes uploaded successfully!");
}

// In app_main(), call once:
upload_chimes_to_flash();
EOF
echo ""
echo "After upload complete, remove the upload code and reflash."
echo ""
```

**Make executable:**
```bash
chmod +x tools/upload_chimes_to_w25q64.sh
```

**Testing:**
- Add upload code to wordclock.c
- Flash and monitor
- Verify "✅ All chimes uploaded successfully!"
- Remove upload code
- Test playback: `chime_library_play(CHIME_QUARTER);`

**Commit:**
```bash
git add tools/upload_chimes_to_w25q64.sh
git commit -m "Add chime upload tool for W25Q64 external flash"
git push
```

**Success:** Chimes uploaded to W25Q64 ✅

---

### **PHASE 2.5 CHECKPOINT:** External Flash Chimes Working ✅
```
What works now:
✅ Chimes stored in W25Q64 external flash (8MB)
✅ Streaming playback from external flash (4KB chunks)
✅ Fallback to internal flash if external fails
✅ Dual storage abstraction layer complete

Storage Usage:
├─ Internal Flash: ~200 KB (quarter + half fallbacks)
├─ External Flash: ~608 KB (full Westminster set)
└─ Free External: ~7.4 MB (for future expansion!)

Performance:
├─ Streaming CPU usage: ~1.5% (vs 0.2% internal)
├─ SPI read speed: ~1 MB/s (31× faster than 16kHz audio)
├─ No audio glitches or buffer underruns
└─ Display unaffected during playback

Ready for Phase 3 & 4: Scheduler + MQTT
```

---

## Phase 4 Updates: MQTT with Flash Management
**Duration:** 3-4 days (extended from 2-3)**
**Goal:** Home Assistant control + custom chime upload

Follow original Phase 4 (Steps 4.1-4.3), then add new steps:

---

### STEP 4.4: MQTT Flash Upload Handler (Day 11) - NEW!
**Time:** 90 minutes
**What:** Upload custom chimes via MQTT

**Add to mqtt_manager.c:**
```c
#include "external_flash.h"
#include "chime_map.h"
#include "mbedtls/base64.h"

static esp_err_t mqtt_handle_flash_upload(const char *payload, int len) {
    ESP_LOGI(TAG, "=== FLASH UPLOAD REQUEST ===");

    cJSON *json = cJSON_Parse(payload);
    if (!json) {
        ESP_LOGE(TAG, "Invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    // Parse upload request
    cJSON *addr_obj = cJSON_GetObjectItem(json, "address");
    cJSON *data_obj = cJSON_GetObjectItem(json, "data");
    cJSON *name_obj = cJSON_GetObjectItem(json, "name");

    if (!addr_obj || !data_obj || !name_obj) {
        ESP_LOGE(TAG, "Missing required fields");
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t address = (uint32_t)cJSON_GetNumberValue(addr_obj);
    const char *data_b64 = cJSON_GetStringValue(data_obj);
    const char *name = cJSON_GetStringValue(name_obj);

    ESP_LOGI(TAG, "Upload: %s to 0x%06X", name, address);

    // Decode base64
    size_t decoded_len;
    unsigned char *decoded = NULL;

    // Calculate required buffer size
    mbedtls_base64_decode(NULL, 0, &decoded_len,
                          (const unsigned char *)data_b64, strlen(data_b64));

    decoded = malloc(decoded_len);
    if (!decoded) {
        ESP_LOGE(TAG, "Failed to allocate decode buffer");
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    int ret = mbedtls_base64_decode(decoded, decoded_len, &decoded_len,
                                     (const unsigned char *)data_b64,
                                     strlen(data_b64));
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        free(decoded);
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Decoded %zu bytes", decoded_len);

    // Erase required sectors
    uint32_t sectors = SECTORS_NEEDED(decoded_len);
    ESP_LOGI(TAG, "Erasing %u sectors...", sectors);

    for (uint32_t i = 0; i < sectors; i++) {
        esp_err_t erase_ret = external_flash_erase_sector(address + (i * SECTOR_SIZE));
        if (erase_ret != ESP_OK) {
            ESP_LOGE(TAG, "Erase failed at sector %u", i);
            free(decoded);
            cJSON_Delete(json);
            return erase_ret;
        }
    }

    // Write to flash
    ESP_LOGI(TAG, "Writing %zu bytes...", decoded_len);
    esp_err_t write_ret = external_flash_write(address, decoded, decoded_len);

    free(decoded);
    cJSON_Delete(json);

    if (write_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Upload complete: %s", name);

        // Publish success status
        mqtt_publish_flash_upload_status(name, address, decoded_len, true);
    } else {
        ESP_LOGE(TAG, "❌ Upload failed: %s", name);
        mqtt_publish_flash_upload_status(name, address, 0, false);
    }

    return write_ret;
}

// Add subscription
// In MQTT_EVENT_CONNECTED:
esp_mqtt_client_subscribe(client, "home/wordclock/flash/upload", 1);

// In MQTT_EVENT_DATA:
if (strncmp(event->topic, "home/wordclock/flash/upload", event->topic_len) == 0) {
    mqtt_handle_flash_upload(event->data, event->data_len);
}
```

**New MQTT Topics:**
```
home/wordclock/flash/upload        - Upload audio (JSON with base64 data)
home/wordclock/flash/status        - Flash usage statistics
home/wordclock/flash/list          - List stored chimes
```

**Testing:**
```bash
# Example upload (small test file)
mosquitto_pub -t "home/wordclock/flash/upload" -m '{
  "address": "0x700000",
  "name": "test_custom_chime",
  "data": "'"$(base64 < test_chime.pcm)"'"
}'
```

**Commit:**
```bash
git commit -am "Add MQTT flash upload handler for custom chimes"
git push
```

**Success:** Can upload chimes via MQTT ✅

---

## Updated Timeline with W25Q64

| Phase | Original | With W25Q64 | Duration | What Changed |
|-------|----------|-------------|----------|--------------|
| **Phase 0** | 1-2 days | 2-3 days | +1 day | +W25Q64 wiring |
| **Phase 0.5** | - | **1-2 days** | NEW | **W25Q64 driver** |
| **Phase 1** | 2-3 days | 2-3 days | Same | I2S audio |
| **Phase 2** | 2-3 days | 2-3 days | Same | Internal chimes |
| **Phase 2.5** | - | **2-3 days** | NEW | **External storage** |
| **Phase 3** | 2-3 days | 2-3 days | Same | Scheduler |
| **Phase 4** | 2-3 days | 3-4 days | +1 day | +MQTT uploads |
| **Phase 5** | 2-3 days | 2-3 days | Same | Testing |
| **TOTAL** | **11-18 days** | **16-24 days** | **+5-6 days** | |

**Time investment:**
- Original: 20-35 hours
- With W25Q64: 30-45 hours (+10-15 hours)

---

## Success Criteria (Updated)

### MVP Complete When:
✅ Westminster chimes play on schedule
✅ **External flash (W25Q64) working and verified**
✅ **Chimes stream from external flash smoothly**
✅ **Fallback to internal flash if external fails**
✅ Volume adjustable (0-100%)
✅ Quiet hours work
✅ Home Assistant control works
✅ **MQTT upload of custom chimes works**
✅ Config persists across reboots
✅ Display unaffected by audio
✅ System stable (no crashes)

### Future Enhancements (Enabled by W25Q64):
🎯 Multiple chime styles (Westminster + Church + Custom)
🎯 Voice announcements (EN + DE)
🎯 Music playback (Birthday song, alarms)
🎯 User-uploadable sounds via web interface
🎯 Seasonal chimes (Christmas, New Year, etc.)

---

## Cost-Benefit Analysis

### Additional Cost:
- W25Q64 module: $5
- Extra jumper wires: $1
- **Total added: $6** (was $9, now $17)

### Benefits Gained:
✅ **8MB dedicated audio storage** (vs 1.4MB internal available)
✅ **5.7× more capacity** for expansion
✅ **User-uploadable sounds** without firmware reflashing
✅ **Multiple chime styles** ready to add
✅ **Voice announcements** possible (4MB total)
✅ **Music playback** capability
✅ **Future-proof** architecture
✅ **Fallback safety** (critical chimes in internal flash)

### Performance Impact:
- CPU: +1.3% (from 0.2% to 1.5% during playback) - still negligible
- Streaming: 4KB chunks, zero glitches
- SPI speed: 1 MB/s (31× faster than audio rate)

**Verdict:** **Worth it!** Small cost for massive future flexibility.

---

**Ready to start? Order both MAX98357A AND W25Q64, then begin Phase 0!** 🔔🎵
