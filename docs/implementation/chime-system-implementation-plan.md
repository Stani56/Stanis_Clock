# Westminster Chime System - Detailed Implementation Plan
## ESP32 Word Clock with MAX98357A I2S Amplifier

**Hardware:** MAX98357A I2S DAC + 8Ω 3W Speaker
**Approach:** Micro-step implementation with testing at every stage
**Timeline:** 2-3 weeks part-time (1-2 hours/day)
**Philosophy:** Small steps, test everything, commit frequently

---

## Phase 0: Preparation & Hardware Setup
**Duration:** 1-2 days
**Goal:** Get hardware ready and verify connections

---

### STEP 0.1: Order Hardware (Week 0, Day 1)
**Time:** 15 minutes
**What:** Purchase components

**Shopping List:**
- [ ] MAX98357A I2S Amplifier Module (~$5)
  - Adafruit: https://www.adafruit.com/product/3006
  - Amazon: Search "MAX98357A I2S"
- [ ] 8Ω 3W Speaker (~$3)
  - Diameter: 40-57mm (fits in enclosure)
  - Wire length: 15cm minimum
- [ ] Jumper wires (if needed) - 3× female-to-female

**Total Cost:** ~$8-10
**Shipping:** 3-7 days (while you prepare software)

**Testing:** None yet
**Commit:** None yet
**Success:** Components ordered ✅

---

### STEP 0.2: Study MAX98357A Datasheet (Day 1)
**Time:** 30 minutes
**What:** Understand hardware requirements

**Read:**
- [ ] MAX98357A datasheet (key sections):
  - Pin configuration
  - I2S timing requirements
  - Sample rate support (8kHz - 96kHz)
  - Power requirements (2.5V - 5.5V)
- [ ] Adafruit MAX98357A guide:
  - https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp

**Key Points to Note:**
```
I2S Requirements:
- BCLK (Bit Clock): 16× sample rate minimum
- LRCLK (Left/Right): Sample rate
- DIN (Data): Serial audio data
- GAIN: Configurable (9dB, 12dB, 15dB via pin)
- SD_MODE: Shutdown control (optional)
```

**Testing:** None
**Commit:** None
**Success:** Understand pinout and timing ✅

---

### STEP 0.3: Plan GPIO Pin Assignment (Day 1)
**Time:** 20 minutes
**What:** Choose available GPIO pins for I2S

**Current GPIO Usage (Check!):**
```
Used GPIO Pins:
├─ GPIO 18/19: I2C (Sensors/LEDs)
├─ GPIO 25/26: I2C (LEDs)
├─ GPIO 34: Potentiometer (ADC)
├─ GPIO 21/22: Status LEDs
└─ GPIO 5: Reset button
```

**Available GPIO for I2S:**
```
Recommended I2S Pin Assignment:
├─ GPIO 27: I2S BCLK (Bit Clock)    ← NEW
├─ GPIO 26: I2S LRCLK (LR Clock)    ⚠️ CONFLICT! (currently I2C)
└─ GPIO 25: I2S DIN (Data)          ⚠️ CONFLICT! (currently I2C)
```

**⚠️ PROBLEM DETECTED!** GPIO 25/26 are already used for I2C LEDs!

**Solution: Use Different GPIOs:**
```
Alternative I2S Pin Assignment:
├─ GPIO 32: I2S BCLK (Bit Clock)    ← Better choice
├─ GPIO 33: I2S LRCLK (LR Clock)    ← Better choice
└─ GPIO 25: I2S DIN (Data)          ← Keep this, or use GPIO 27
```

**Final Recommendation:**
```
I2S Pins (No conflicts):
├─ GPIO 32: BCLK
├─ GPIO 33: LRCLK
└─ GPIO 27: DIN
```

**Testing:** Document in code comments
**Commit:** Update CLAUDE.md with GPIO assignments
**Success:** Pin conflicts resolved ✅

---

### STEP 0.4: Hardware Wiring (Day 2 - After components arrive)
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

**Photos/Documentation:**
- [ ] Take photo of wiring
- [ ] Verify connections with multimeter
- [ ] Check for shorts (VIN to GND)

**Testing:**
```bash
# Check GPIO are readable
idf.py monitor
# In serial output, verify GPIOs not showing errors
```

**Commit:** None yet (just hardware)
**Success:** MAX98357A connected, no shorts ✅

---

## Phase 1: Basic I2S Audio Output
**Duration:** 2-3 days
**Goal:** Get any sound playing through the speaker

---

### STEP 1.1: Create audio_manager Component Stub (Day 3)
**Time:** 20 minutes
**What:** Set up component structure

**Actions:**
```bash
# Create directory structure
mkdir -p components/audio_manager/include
touch components/audio_manager/CMakeLists.txt
touch components/audio_manager/audio_manager.c
touch components/audio_manager/include/audio_manager.h
```

**CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "audio_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_common freertos
)
```

**audio_manager.h (Minimal API):**
```c
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Initialize I2S audio output
esp_err_t audio_manager_init(void);

// Play raw PCM data
esp_err_t audio_manager_play_pcm(const uint8_t *data, size_t len);

// Stop playback
esp_err_t audio_manager_stop(void);

#endif
```

**audio_manager.c (Stub):**
```c
#include "audio_manager.h"
#include "esp_log.h"

static const char *TAG = "audio_manager";

esp_err_t audio_manager_init(void) {
    ESP_LOGI(TAG, "Audio manager init (stub)");
    return ESP_OK;
}

esp_err_t audio_manager_play_pcm(const uint8_t *data, size_t len) {
    ESP_LOGI(TAG, "Play PCM: %zu bytes (stub)", len);
    return ESP_OK;
}

esp_err_t audio_manager_stop(void) {
    ESP_LOGI(TAG, "Stop playback (stub)");
    return ESP_OK;
}
```

**Testing:**
```bash
idf.py build
# Should compile without errors
```

**Commit:**
```bash
git add components/audio_manager/
git commit -m "Add audio_manager component stub structure"
git push
```

**Success:** Component compiles ✅

---

### STEP 1.2: Initialize I2S Driver (Day 3)
**Time:** 45 minutes
**What:** Configure ESP32 I2S peripheral

**Modify audio_manager.c:**
Add I2S configuration and initialization:

```c
#include "driver/i2s_std.h"

// I2S configuration
// 16 kHz chosen for optimal storage/quality balance:
// - 44.1 kHz would require 1.68 MB (doesn't fit in 1.4 MB available)
// - 16 kHz requires only 608 KB (45.9 seconds available)
// - Westminster chimes are low-frequency bells (16 kHz preserves tones well)
// - Source audio downsampled from 44.1 kHz via prepare_chimes.sh script
#define I2S_SAMPLE_RATE     16000
#define I2S_BITS_PER_SAMPLE 16
#define I2S_DMA_BUF_COUNT   2
#define I2S_DMA_BUF_LEN     1024

// GPIO pins
#define I2S_BCLK_PIN        32
#define I2S_LRCLK_PIN       33
#define I2S_DIN_PIN         27

static i2s_chan_handle_t tx_handle = NULL;

esp_err_t audio_manager_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_LRCLK_PIN,
            .dout = I2S_DIN_PIN,
            .din = I2S_GPIO_UNUSED,
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "I2S initialized: %d Hz, 16-bit mono", I2S_SAMPLE_RATE);
    return ESP_OK;
}
```

**Testing:**
```bash
idf.py build flash monitor
# Look for: "I2S initialized: 16000 Hz, 16-bit mono"
```

**Commit:**
```bash
git commit -am "Initialize I2S driver with MAX98357A configuration"
git push
```

**Success:** I2S driver initializes without errors ✅

---

### STEP 1.3: Generate Test Tone (Day 3-4)
**Time:** 30 minutes
**What:** Create a simple sine wave in memory

**Add to audio_manager.c:**
```c
#include <math.h>

// Generate a 440Hz sine wave (A note)
static void generate_sine_wave(int16_t *buffer, size_t samples, uint32_t sample_rate, float frequency) {
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        float value = sinf(2.0f * M_PI * frequency * t);
        buffer[i] = (int16_t)(value * 16000);  // 50% volume
    }
}

// Test function (for debugging)
esp_err_t audio_manager_play_test_tone(void) {
    const size_t samples = I2S_SAMPLE_RATE * 2;  // 2 seconds
    int16_t *buffer = malloc(samples * sizeof(int16_t));
    if (!buffer) return ESP_ERR_NO_MEM;

    generate_sine_wave(buffer, samples, I2S_SAMPLE_RATE, 440.0f);

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);

    free(buffer);
    ESP_LOGI(TAG, "Test tone played: %zu bytes", bytes_written);
    return ret;
}
```

**Add to audio_manager.h:**
```c
esp_err_t audio_manager_play_test_tone(void);
```

**Testing:**
```c
// In wordclock.c app_main(), after init:
audio_manager_init();
audio_manager_play_test_tone();  // Should hear 440Hz tone for 2 seconds!
```

```bash
idf.py build flash
# Put speaker near ear - you should hear a 2-second tone!
```

**Commit:**
```bash
git commit -am "Add test tone generation (440Hz sine wave)"
git push
```

**Success:** You hear a tone from the speaker! 🔊✅

---

### STEP 1.4: Implement PCM Playback (Day 4)
**Time:** 30 minutes
**What:** Play arbitrary PCM data

**Modify audio_manager.c:**
```c
esp_err_t audio_manager_play_pcm(const uint8_t *data, size_t len) {
    if (!tx_handle) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) {
        ESP_LOGE(TAG, "Invalid PCM data");
        return ESP_ERR_INVALID_ARG;
    }

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, data, len, &bytes_written, portMAX_DELAY);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PCM played: %zu/%zu bytes", bytes_written, len);
    } else {
        ESP_LOGE(TAG, "PCM playback failed: %s", esp_err_to_name(ret));
    }

    return ret;
}
```

**Testing:**
Create a small test PCM file:
```c
// Test with test tone buffer
int16_t test_data[1000];
generate_sine_wave(test_data, 1000, I2S_SAMPLE_RATE, 880.0f);  // Higher pitch
audio_manager_play_pcm((uint8_t*)test_data, sizeof(test_data));
```

```bash
idf.py build flash
# Should hear short beep
```

**Commit:**
```bash
git commit -am "Implement PCM playback function"
git push
```

**Success:** Can play arbitrary PCM data ✅

---

### STEP 1.5: Add Volume Control (Day 4)
**Time:** 30 minutes
**What:** Adjust playback volume

**Add to audio_manager.c:**
```c
static uint8_t current_volume = 50;  // 0-100

void audio_manager_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    current_volume = volume;
    ESP_LOGI(TAG, "Volume set to: %d%%", volume);
}

uint8_t audio_manager_get_volume(void) {
    return current_volume;
}

// Modify play_pcm to apply volume:
esp_err_t audio_manager_play_pcm(const uint8_t *data, size_t len) {
    // Create volume-adjusted buffer
    int16_t *adjusted = malloc(len);
    if (!adjusted) return ESP_ERR_NO_MEM;

    // Apply volume scaling
    int16_t *src = (int16_t*)data;
    size_t samples = len / sizeof(int16_t);
    for (size_t i = 0; i < samples; i++) {
        adjusted[i] = (src[i] * current_volume) / 100;
    }

    // Play adjusted audio
    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, adjusted, len, &bytes_written, portMAX_DELAY);

    free(adjusted);
    return ret;
}
```

**Add to header:**
```c
void audio_manager_set_volume(uint8_t volume);  // 0-100
uint8_t audio_manager_get_volume(void);
```

**Testing:**
```c
audio_manager_set_volume(10);   // Quiet
audio_manager_play_test_tone();
vTaskDelay(pdMS_TO_TICKS(3000));

audio_manager_set_volume(100);  // Loud
audio_manager_play_test_tone();
```

**Commit:**
```bash
git commit -am "Add volume control (0-100%)"
git push
```

**Success:** Volume adjustable ✅

---

### **PHASE 1 CHECKPOINT:** Basic Audio Working ✅
```
What works now:
✅ I2S driver initialized
✅ MAX98357A outputting sound
✅ Can play test tones
✅ Can play PCM data
✅ Volume control works

What to test:
├─ Hear 440Hz tone
├─ Hear 880Hz tone
├─ Volume changes work
└─ No distortion/clicks

Build size: ~1.12 MB (+20 KB for audio manager)
Flash time: ~10 seconds
```

---

## Phase 2: Westminster Chime Audio Files
**Duration:** 2-3 days
**Goal:** Embed Westminster chimes in firmware

---

### STEP 2.1: Download Westminster Chime Audio (Day 5)
**Time:** 30 minutes
**What:** Get royalty-free chime files

**Sources (Choose One):**

**Option A: ZapSplat (Recommended)**
1. Create free account: https://www.zapsplat.com
2. Search: "Westminster chime"
3. Download:
   - `clock_westminster_quarter.wav`
   - `clock_westminster_half.wav`
   - `clock_westminster_three_quarter.wav`
   - `clock_westminster_full.wav`
   - `clock_big_ben_strike.wav`

**Option B: Freesound.org**
1. Search: "Westminster quarter chime CC0"
2. Download first good quality result

**Option C: Generate with Audacity**
- Use synthesis guide from `docs/resources/audio-sources-guide.md`

**Place files in:**
```bash
mkdir -p audio_source
# Put WAV files here
```

**Testing:** Play files on your computer
**Commit:** None (audio files not committed)
**Success:** Have 5 WAV files ✅

---

### STEP 2.2: Process Audio Files (Day 5)
**Time:** 20 minutes
**What:** Convert to ESP32-compatible format

**Run processing script:**
```bash
cd Stanis_Clock
chmod +x tools/prepare_chimes.sh

# Place WAV files in source_audio/
cp ~/Downloads/westminster*.wav source_audio/
cp ~/Downloads/bigben*.wav source_audio/

# Run conversion
./tools/prepare_chimes.sh
```

**Output:**
```
source_audio/         ← Your WAV files
processed_audio/      ← Intermediate files
c_arrays/            ← C headers ready to use!
  ├─ westminster_quarter.h
  ├─ westminster_half.h
  ├─ westminster_three_quarter.h
  ├─ westminster_full.h
  └─ bigben_strike.h
```

**Testing:** Check file sizes
```bash
ls -lh c_arrays/
# Should see ~50-250 KB per file
```

**Commit:** None yet (audio too large for git)
**Success:** Have C header files ✅

---

### STEP 2.3: Create chime_library Component (Day 5)
**Time:** 30 minutes
**What:** Set up component for chime data

**Create structure:**
```bash
mkdir -p components/chime_library/include
mkdir -p components/chime_library/chimes
touch components/chime_library/CMakeLists.txt
touch components/chime_library/chime_library.c
touch components/chime_library/include/chime_library.h
```

**CMakeLists.txt:**
```cmake
idf_component_register(
    SRCS "chime_library.c"
    INCLUDE_DIRS "include"
    REQUIRES audio_manager
)
```

**chime_library.h:**
```c
#ifndef CHIME_LIBRARY_H
#define CHIME_LIBRARY_H

#include "esp_err.h"

typedef enum {
    CHIME_QUARTER,
    CHIME_HALF,
    CHIME_THREE_QUARTER,
    CHIME_FULL,
    CHIME_HOUR_STRIKE,
} chime_type_t;

// Initialize chime library
esp_err_t chime_library_init(void);

// Play a specific chime
esp_err_t chime_library_play(chime_type_t type);

#endif
```

**chime_library.c (stub):**
```c
#include "chime_library.h"
#include "esp_log.h"

static const char *TAG = "chime_library";

esp_err_t chime_library_init(void) {
    ESP_LOGI(TAG, "Chime library init");
    return ESP_OK;
}

esp_err_t chime_library_play(chime_type_t type) {
    ESP_LOGI(TAG, "Play chime type: %d (stub)", type);
    return ESP_OK;
}
```

**Testing:**
```bash
idf.py build
# Should compile
```

**Commit:**
```bash
git add components/chime_library/
git commit -m "Add chime_library component stub"
git push
```

**Success:** Component compiles ✅

---

### STEP 2.4: Embed ONE Chime First (Day 6)
**Time:** 30 minutes
**What:** Start with just quarter chime (smallest)

**Copy header file:**
```bash
cp c_arrays/westminster_quarter.h components/chime_library/chimes/
```

**Modify chime_library.c:**
```c
#include "chimes/westminster_quarter.h"
#include "audio_manager.h"

esp_err_t chime_library_play(chime_type_t type) {
    switch (type) {
        case CHIME_QUARTER:
            ESP_LOGI(TAG, "Playing quarter chime (%u bytes)", westminster_quarter_len);
            return audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);

        default:
            ESP_LOGW(TAG, "Chime type %d not implemented yet", type);
            return ESP_ERR_NOT_SUPPORTED;
    }
}
```

**Testing:**
```c
// In wordclock.c
chime_library_init();
chime_library_play(CHIME_QUARTER);  // Should hear Westminster quarter!
```

```bash
idf.py build flash
# Listen for Westminster chime!
```

**Commit:**
```bash
git add components/chime_library/chimes/westminster_quarter.h
git commit -m "Embed Westminster quarter chime (first chime working!)"
git push
```

**Success:** Hear Westminster quarter chime! 🔔✅

---

### STEP 2.5: Add Remaining Chimes (Day 6)
**Time:** 45 minutes
**What:** Add all other chime files

**Copy remaining headers:**
```bash
cp c_arrays/westminster_half.h components/chime_library/chimes/
cp c_arrays/westminster_three_quarter.h components/chime_library/chimes/
cp c_arrays/westminster_full.h components/chime_library/chimes/
cp c_arrays/bigben_strike.h components/chime_library/chimes/
```

**Update chime_library.c:**
```c
#include "chimes/westminster_quarter.h"
#include "chimes/westminster_half.h"
#include "chimes/westminster_three_quarter.h"
#include "chimes/westminster_full.h"
#include "chimes/bigben_strike.h"

esp_err_t chime_library_play(chime_type_t type) {
    const uint8_t *data;
    unsigned int len;

    switch (type) {
        case CHIME_QUARTER:
            data = westminster_quarter;
            len = westminster_quarter_len;
            break;
        case CHIME_HALF:
            data = westminster_half;
            len = westminster_half_len;
            break;
        case CHIME_THREE_QUARTER:
            data = westminster_three_quarter;
            len = westminster_three_quarter_len;
            break;
        case CHIME_FULL:
            data = westminster_full;
            len = westminster_full_len;
            break;
        case CHIME_HOUR_STRIKE:
            data = bigben_strike;
            len = bigben_strike_len;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Playing chime type %d (%u bytes)", type, len);
    return audio_manager_play_pcm(data, len);
}
```

**Testing:**
```c
// Test all chimes in sequence
for (int i = 0; i < 5; i++) {
    chime_library_play((chime_type_t)i);
    vTaskDelay(pdMS_TO_TICKS(5000));  // 5 second gap
}
```

**Commit:**
```bash
git add components/chime_library/chimes/*.h
git commit -m "Add all Westminster chimes (quarter, half, 3/4, full, hour strike)"
git push
```

**Success:** All 5 chimes working! 🔔✅

---

### **PHASE 2 CHECKPOINT:** All Chimes Embedded ✅
```
What works now:
✅ Westminster quarter chime
✅ Westminster half chime
✅ Westminster 3/4 chime
✅ Westminster full chime
✅ Big Ben hour strike

What to test:
├─ Play each chime manually
├─ Check audio quality
├─ Verify volume control
└─ Check app size (<2.5 MB)

Build size: ~1.7 MB (code + chimes)
Flash time: ~15 seconds
Free space: ~800 KB still available ✅
```

---

## Phase 3: Chime Scheduler
**Duration:** 2-3 days
**Goal:** Automatic chiming on schedule

---

### STEP 3.1: Create chime_scheduler Component (Day 7)
**Time:** 30 minutes
**What:** Set up scheduler structure

**Create:**
```bash
mkdir -p components/chime_scheduler/include
touch components/chime_scheduler/CMakeLists.txt
touch components/chime_scheduler/chime_scheduler.c
touch components/chime_scheduler/include/chime_scheduler.h
```

**chime_scheduler.h:**
```c
#ifndef CHIME_SCHEDULER_H
#define CHIME_SCHEDULER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SCHEDULE_OFF,           // No chimes
    SCHEDULE_QUARTER,       // Every 15 minutes
    SCHEDULE_HALF,          // Every 30 minutes
    SCHEDULE_HOUR,          // Every hour
} chime_schedule_t;

typedef struct {
    bool enabled;
    chime_schedule_t schedule;
    uint8_t volume;
    uint8_t quiet_start_hour;   // e.g., 22 (10 PM)
    uint8_t quiet_end_hour;     // e.g., 7 (7 AM)
} chime_config_t;

// Initialize scheduler
esp_err_t chime_scheduler_init(void);

// Configure chimes
esp_err_t chime_scheduler_set_config(const chime_config_t *config);
esp_err_t chime_scheduler_get_config(chime_config_t *config);

// Manual trigger (for testing)
esp_err_t chime_scheduler_trigger_now(void);

#endif
```

**Stub implementation:**
```c
#include "chime_scheduler.h"
#include "esp_log.h"

static const char *TAG = "chime_scheduler";
static chime_config_t g_config = {0};

esp_err_t chime_scheduler_init(void) {
    // Default config
    g_config.enabled = false;
    g_config.schedule = SCHEDULE_QUARTER;
    g_config.volume = 50;
    g_config.quiet_start_hour = 22;
    g_config.quiet_end_hour = 7;

    ESP_LOGI(TAG, "Chime scheduler init");
    return ESP_OK;
}

esp_err_t chime_scheduler_set_config(const chime_config_t *config) {
    g_config = *config;
    ESP_LOGI(TAG, "Config updated: enabled=%d, schedule=%d",
             config->enabled, config->schedule);
    return ESP_OK;
}

esp_err_t chime_scheduler_get_config(chime_config_t *config) {
    *config = g_config;
    return ESP_OK;
}

esp_err_t chime_scheduler_trigger_now(void) {
    ESP_LOGI(TAG, "Manual trigger (stub)");
    return ESP_OK;
}
```

**Testing:**
```bash
idf.py build
```

**Commit:**
```bash
git add components/chime_scheduler/
git commit -m "Add chime_scheduler component stub"
git push
```

**Success:** Component compiles ✅

---

### STEP 3.2: Add NVS Configuration Storage (Day 7)
**Time:** 45 minutes
**What:** Save chime config to NVS

**Modify chime_scheduler.c:**
```c
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "chime_config"

esp_err_t chime_scheduler_save_to_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    nvs_set_u8(nvs_handle, "enabled", g_config.enabled ? 1 : 0);
    nvs_set_u8(nvs_handle, "schedule", g_config.schedule);
    nvs_set_u8(nvs_handle, "volume", g_config.volume);
    nvs_set_u8(nvs_handle, "quiet_start", g_config.quiet_start_hour);
    nvs_set_u8(nvs_handle, "quiet_end", g_config.quiet_end_hour);

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Config saved to NVS");
    return ret;
}

esp_err_t chime_scheduler_load_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return ESP_OK;
    }

    uint8_t val;
    if (nvs_get_u8(nvs_handle, "enabled", &val) == ESP_OK)
        g_config.enabled = (val != 0);
    if (nvs_get_u8(nvs_handle, "schedule", &val) == ESP_OK)
        g_config.schedule = val;
    if (nvs_get_u8(nvs_handle, "volume", &val) == ESP_OK)
        g_config.volume = val;
    if (nvs_get_u8(nvs_handle, "quiet_start", &val) == ESP_OK)
        g_config.quiet_start_hour = val;
    if (nvs_get_u8(nvs_handle, "quiet_end", &val) == ESP_OK)
        g_config.quiet_end_hour = val;

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Config loaded from NVS");
    return ESP_OK;
}

// Update init to load config
esp_err_t chime_scheduler_init(void) {
    // Set defaults
    g_config.enabled = false;
    g_config.schedule = SCHEDULE_QUARTER;
    g_config.volume = 50;
    g_config.quiet_start_hour = 22;
    g_config.quiet_end_hour = 7;

    // Try to load from NVS
    chime_scheduler_load_from_nvs();

    ESP_LOGI(TAG, "Chime scheduler init: enabled=%d", g_config.enabled);
    return ESP_OK;
}

// Update set_config to save
esp_err_t chime_scheduler_set_config(const chime_config_t *config) {
    g_config = *config;
    chime_scheduler_save_to_nvs();
    return ESP_OK;
}
```

**Add to header:**
```c
esp_err_t chime_scheduler_save_to_nvs(void);
esp_err_t chime_scheduler_load_from_nvs(void);
```

**Testing:**
```c
// Test persistence
chime_config_t cfg = {
    .enabled = true,
    .schedule = SCHEDULE_QUARTER,
    .volume = 75,
    .quiet_start_hour = 23,
    .quiet_end_hour = 6,
};
chime_scheduler_set_config(&cfg);

// Restart ESP32
esp_restart();

// After restart, check:
chime_config_t loaded;
chime_scheduler_get_config(&loaded);
// loaded should match cfg!
```

**Commit:**
```bash
git commit -am "Add NVS storage for chime configuration"
git push
```

**Success:** Config persists across reboots ✅

---

### STEP 3.3: Implement Quiet Hours Logic (Day 8)
**Time:** 30 minutes
**What:** Check if current time is in quiet period

**Add to chime_scheduler.c:**
```c
#include <time.h>
#include "i2c_devices.h"  // For DS3231 RTC

static bool is_quiet_hours(void) {
    if (!g_config.enabled) return true;  // Disabled = always quiet

    struct tm timeinfo;
    if (ds3231_get_time(&timeinfo) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get time, assuming not quiet hours");
        return false;
    }

    uint8_t hour = timeinfo.tm_hour;
    uint8_t start = g_config.quiet_start_hour;
    uint8_t end = g_config.quiet_end_hour;

    // Handle wrap-around (e.g., 22:00 to 07:00)
    if (start > end) {
        // Quiet hours cross midnight
        return (hour >= start || hour < end);
    } else {
        // Normal range (e.g., 01:00 to 05:00)
        return (hour >= start && hour < end);
    }
}

// Test function
bool chime_scheduler_is_quiet_now(void) {
    bool quiet = is_quiet_hours();
    ESP_LOGI(TAG, "Quiet hours check: %s", quiet ? "YES (quiet)" : "NO (active)");
    return quiet;
}
```

**Add to header:**
```c
bool chime_scheduler_is_quiet_now(void);
```

**Testing:**
```c
// Test at different times
struct tm test_time = {0};

// Test 23:00 (should be quiet if quiet_start=22)
test_time.tm_hour = 23;
ds3231_set_time(&test_time);
bool quiet = chime_scheduler_is_quiet_now();
// Should be true

// Test 10:00 (should be active)
test_time.tm_hour = 10;
ds3231_set_time(&test_time);
quiet = chime_scheduler_is_quiet_now();
// Should be false
```

**Commit:**
```bash
git commit -am "Implement quiet hours logic with RTC integration"
git push
```

**Success:** Quiet hours detection works ✅

---

### STEP 3.4: Implement Schedule Logic (Day 8)
**Time:** 45 minutes
**What:** Determine which chime to play based on time

**Add to chime_scheduler.c:**
```c
#include "chime_library.h"

static chime_type_t get_chime_for_minute(uint8_t minute) {
    if (minute == 0) return CHIME_FULL;
    if (minute == 15) return CHIME_QUARTER;
    if (minute == 30) return CHIME_HALF;
    if (minute == 45) return CHIME_THREE_QUARTER;
    return -1;  // Not a chime minute
}

static bool should_chime_now(uint8_t minute) {
    switch (g_config.schedule) {
        case SCHEDULE_OFF:
            return false;
        case SCHEDULE_HOUR:
            return (minute == 0);
        case SCHEDULE_HALF:
            return (minute == 0 || minute == 30);
        case SCHEDULE_QUARTER:
            return (minute == 0 || minute == 15 || minute == 30 || minute == 45);
        default:
            return false;
    }
}

esp_err_t chime_scheduler_check_and_play(void) {
    if (!g_config.enabled) {
        return ESP_OK;  // Disabled
    }

    if (is_quiet_hours()) {
        ESP_LOGI(TAG, "In quiet hours, skipping chime");
        return ESP_OK;
    }

    struct tm timeinfo;
    if (ds3231_get_time(&timeinfo) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t minute = timeinfo.tm_min;

    if (!should_chime_now(minute)) {
        return ESP_OK;  // Not a chime time
    }

    chime_type_t type = get_chime_for_minute(minute);
    if (type < 0) return ESP_OK;

    ESP_LOGI(TAG, "Playing chime: %02d:%02d", timeinfo.tm_hour, minute);

    audio_manager_set_volume(g_config.volume);
    return chime_library_play(type);
}
```

**Add to header:**
```c
esp_err_t chime_scheduler_check_and_play(void);
```

**Testing:**
```c
// Test schedule logic
chime_config_t cfg = {
    .enabled = true,
    .schedule = SCHEDULE_QUARTER,
    .volume = 60,
    .quiet_start_hour = 22,
    .quiet_end_hour = 7,
};
chime_scheduler_set_config(&cfg);

// Simulate different times
struct tm test;
ds3231_get_time(&test);

test.tm_min = 0;
ds3231_set_time(&test);
chime_scheduler_check_and_play();  // Should play full chime

test.tm_min = 15;
ds3231_set_time(&test);
chime_scheduler_check_and_play();  // Should play quarter

test.tm_min = 17;
ds3231_set_time(&test);
chime_scheduler_check_and_play();  // Should NOT play
```

**Commit:**
```bash
git commit -am "Implement chime scheduling logic (quarter/half/hour)"
git push
```

**Success:** Scheduler logic works ✅

---

### STEP 3.5: Integrate with Main Loop (Day 8-9)
**Time:** 30 minutes
**What:** Call scheduler on minute changes

**Modify wordclock.c:**
```c
#include "chime_scheduler.h"

// In app_main() after initialization:
chime_scheduler_init();

// In your existing main loop (where you check time every second):
static uint8_t last_minute = 255;

while (1) {
    wordclock_time_t current_time;
    if (ds3231_get_time_wordclock(&current_time) == ESP_OK) {

        // Existing display update logic...
        display_german_time_with_transitions(&current_time);

        // NEW: Check for chimes on minute change
        if (current_time.minutes != last_minute) {
            last_minute = current_time.minutes;
            chime_scheduler_check_and_play();  // Play chime if needed
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

**Testing:**
```bash
idf.py build flash monitor

# Wait for minute changes (:00, :15, :30, :45)
# Should hear Westminster chimes automatically!
```

**Commit:**
```bash
git commit -am "Integrate chime scheduler with main loop"
git push
```

**Success:** Automatic chiming works! 🔔✅

---

### **PHASE 3 CHECKPOINT:** Automatic Chiming ✅
```
What works now:
✅ Chimes play automatically on schedule
✅ Quarter-hour schedule (:00, :15, :30, :45)
✅ Half-hour schedule (:00, :30)
✅ Hour-only schedule (:00)
✅ Quiet hours respected
✅ Volume control
✅ Config persists in NVS

What to test:
├─ Wait for :15 - quarter chime plays
├─ Wait for :30 - half chime plays
├─ Wait for :45 - 3/4 chime plays
├─ Wait for :00 - full chime + hour strikes (TODO)
├─ Change to quiet hours - chimes stop
├─ Restart ESP32 - config survives

Build size: ~1.75 MB
Flash time: ~15 seconds
```

---

## Phase 4: MQTT Integration
**Duration:** 2-3 days
**Goal:** Home Assistant control

---

### STEP 4.1: Add MQTT Command Handler (Day 9)
**Time:** 45 minutes
**What:** Handle chime control commands

**Modify mqtt_manager.c:**
Add new handler function:

```c
#include "chime_scheduler.h"

// Add to existing handlers
static esp_err_t mqtt_handle_chime_command(const char *payload, int payload_len) {
    ESP_LOGI(TAG, "=== CHIME COMMAND RECEIVED ===");

    char json_str[256];
    int copy_len = (payload_len < sizeof(json_str) - 1) ? payload_len : sizeof(json_str) - 1;
    strncpy(json_str, payload, copy_len);
    json_str[copy_len] = '\0';

    cJSON *json = cJSON_Parse(json_str);
    if (!json) return ESP_ERR_INVALID_ARG;

    chime_config_t config;
    chime_scheduler_get_config(&config);

    // Parse JSON fields
    cJSON *enabled = cJSON_GetObjectItem(json, "enabled");
    if (enabled && cJSON_IsBool(enabled)) {
        config.enabled = cJSON_IsTrue(enabled);
    }

    cJSON *schedule = cJSON_GetObjectItem(json, "schedule");
    if (schedule && cJSON_IsString(schedule)) {
        const char *sched = cJSON_GetStringValue(schedule);
        if (strcmp(sched, "off") == 0) config.schedule = SCHEDULE_OFF;
        else if (strcmp(sched, "quarter") == 0) config.schedule = SCHEDULE_QUARTER;
        else if (strcmp(sched, "half") == 0) config.schedule = SCHEDULE_HALF;
        else if (strcmp(sched, "hour") == 0) config.schedule = SCHEDULE_HOUR;
    }

    cJSON *volume = cJSON_GetObjectItem(json, "volume");
    if (volume && cJSON_IsNumber(volume)) {
        config.volume = (uint8_t)cJSON_GetNumberValue(volume);
    }

    cJSON *quiet_start = cJSON_GetObjectItem(json, "quiet_start");
    if (quiet_start && cJSON_IsNumber(quiet_start)) {
        config.quiet_start_hour = (uint8_t)cJSON_GetNumberValue(quiet_start);
    }

    cJSON *quiet_end = cJSON_GetObjectItem(json, "quiet_end");
    if (quiet_end && cJSON_IsNumber(quiet_end)) {
        config.quiet_end_hour = (uint8_t)cJSON_GetNumberValue(quiet_end);
    }

    cJSON_Delete(json);

    // Apply config
    chime_scheduler_set_config(&config);
    ESP_LOGI(TAG, "Chime config updated");

    return ESP_OK;
}
```

**Add subscription in mqtt_event_handler:**
```c
// In MQTT_EVENT_CONNECTED:
esp_mqtt_client_subscribe(client, "home/wordclock/chime/set", 1);

// In MQTT_EVENT_DATA:
if (strncmp(event->topic, "home/wordclock/chime/set", event->topic_len) == 0) {
    mqtt_handle_chime_command(event->data, event->data_len);
}
```

**Testing:**
```bash
# Via MQTT
mosquitto_pub -t "home/wordclock/chime/set" -m '{"enabled":true,"schedule":"quarter","volume":70}'

# Check serial monitor for "Chime config updated"
```

**Commit:**
```bash
git commit -am "Add MQTT command handler for chime control"
git push
```

**Success:** MQTT commands work ✅

---

### STEP 4.2: Add MQTT Status Publisher (Day 9)
**Time:** 30 minutes
**What:** Publish current chime config

**Add to mqtt_manager.c:**
```c
esp_err_t mqtt_publish_chime_status(void) {
    if (!thread_safe_get_mqtt_connected()) return ESP_ERR_INVALID_STATE;

    chime_config_t config;
    chime_scheduler_get_config(&config);

    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;

    cJSON_AddBoolToObject(json, "enabled", config.enabled);

    const char *sched_str = "off";
    switch (config.schedule) {
        case SCHEDULE_QUARTER: sched_str = "quarter"; break;
        case SCHEDULE_HALF: sched_str = "half"; break;
        case SCHEDULE_HOUR: sched_str = "hour"; break;
        default: break;
    }
    cJSON_AddStringToObject(json, "schedule", sched_str);

    cJSON_AddNumberToObject(json, "volume", config.volume);
    cJSON_AddNumberToObject(json, "quiet_start", config.quiet_start_hour);
    cJSON_AddNumberToObject(json, "quiet_end", config.quiet_end_hour);

    char *json_string = cJSON_Print(json);
    esp_err_t ret = ESP_FAIL;

    if (json_string) {
        int msg_id = esp_mqtt_client_publish(mqtt_client,
            "home/wordclock/chime/status",
            json_string, 0, 1, true);
        ret = (msg_id != -1) ? ESP_OK : ESP_FAIL;
        cJSON_free(json_string);
    }

    cJSON_Delete(json);
    return ret;
}
```

**Call on connect and after config changes:**
```c
// In MQTT_EVENT_CONNECTED:
mqtt_publish_chime_status();

// In mqtt_handle_chime_command (after set_config):
mqtt_publish_chime_status();
```

**Testing:**
```bash
# Subscribe to status
mosquitto_sub -t "home/wordclock/chime/status"

# Should see JSON with current config
```

**Commit:**
```bash
git commit -am "Add MQTT status publisher for chime config"
git push
```

**Success:** Status publishing works ✅

---

### STEP 4.3: Add Home Assistant MQTT Discovery (Day 10)
**Time:** 60 minutes
**What:** Auto-configure in Home Assistant

**Modify mqtt_discovery.c:**
Add new function:

```c
esp_err_t mqtt_discovery_publish_chime_entities(void) {
    esp_err_t ret = ESP_OK;
    char unique_id[64];

    // 1. Chimes Enabled Switch
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Chimes Enabled");
            snprintf(unique_id, sizeof(unique_id), "%s_chimes_enabled", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/chime/status");
            cJSON_AddStringToObject(config, "command_topic", "~/chime/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.enabled }}");
            cJSON_AddStringToObject(config, "payload_on", "{\"enabled\": true}");
            cJSON_AddStringToObject(config, "payload_off", "{\"enabled\": false}");
            cJSON_AddBoolToObject(config, "state_on", true);
            cJSON_AddBoolToObject(config, "state_off", false);
            cJSON_AddStringToObject(config, "icon", "mdi:bell");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("switch", "chimes_enabled", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // 2. Chime Schedule Select
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Chime Schedule");
            snprintf(unique_id, sizeof(unique_id), "%s_chime_schedule", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/chime/status");
            cJSON_AddStringToObject(config, "command_topic", "~/chime/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.schedule }}");
            cJSON_AddStringToObject(config, "command_template", "{\"schedule\": \"{{ value }}\"}");

            cJSON *options = cJSON_CreateArray();
            if (options) {
                cJSON_AddItemToArray(options, cJSON_CreateString("off"));
                cJSON_AddItemToArray(options, cJSON_CreateString("hour"));
                cJSON_AddItemToArray(options, cJSON_CreateString("half"));
                cJSON_AddItemToArray(options, cJSON_CreateString("quarter"));
                cJSON_AddItemToObject(config, "options", options);
            }

            cJSON_AddStringToObject(config, "icon", "mdi:clock-outline");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("select", "chime_schedule", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // 3. Chime Volume Number
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Chime Volume");
            snprintf(unique_id, sizeof(unique_id), "%s_chime_volume", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/chime/status");
            cJSON_AddStringToObject(config, "command_topic", "~/chime/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.volume }}");
            cJSON_AddStringToObject(config, "command_template", "{\"volume\": {{ value }}}");

            cJSON_AddNumberToObject(config, "min", 0);
            cJSON_AddNumberToObject(config, "max", 100);
            cJSON_AddNumberToObject(config, "step", 5);
            cJSON_AddStringToObject(config, "unit_of_measurement", "%");
            cJSON_AddStringToObject(config, "icon", "mdi:volume-high");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("number", "chime_volume", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // 4. Quiet Hours Start Number
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Quiet Hours Start");
            snprintf(unique_id, sizeof(unique_id), "%s_quiet_start", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/chime/status");
            cJSON_AddStringToObject(config, "command_topic", "~/chime/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.quiet_start }}");
            cJSON_AddStringToObject(config, "command_template", "{\"quiet_start\": {{ value }}}");

            cJSON_AddNumberToObject(config, "min", 0);
            cJSON_AddNumberToObject(config, "max", 23);
            cJSON_AddNumberToObject(config, "step", 1);
            cJSON_AddStringToObject(config, "unit_of_measurement", "hour");
            cJSON_AddStringToObject(config, "icon", "mdi:sleep");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("number", "quiet_start", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // 5. Quiet Hours End Number
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Quiet Hours End");
            snprintf(unique_id, sizeof(unique_id), "%s_quiet_end", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "state_topic", "~/chime/status");
            cJSON_AddStringToObject(config, "command_topic", "~/chime/set");
            cJSON_AddStringToObject(config, "value_template", "{{ value_json.quiet_end }}");
            cJSON_AddStringToObject(config, "command_template", "{\"quiet_end\": {{ value }}}");

            cJSON_AddNumberToObject(config, "min", 0);
            cJSON_AddNumberToObject(config, "max", 23);
            cJSON_AddNumberToObject(config, "step", 1);
            cJSON_AddStringToObject(config, "unit_of_measurement", "hour");
            cJSON_AddStringToObject(config, "icon", "mdi:alarm");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("number", "quiet_end", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    // 6. Test Chime Button
    {
        cJSON *config = cJSON_CreateObject();
        if (config) {
            add_base_topic(config);
            cJSON_AddStringToObject(config, "name", "Test Chime");
            snprintf(unique_id, sizeof(unique_id), "%s_test_chime", discovery_config.device_id);
            cJSON_AddStringToObject(config, "unique_id", unique_id);
            cJSON_AddStringToObject(config, "command_topic", "~/chime/test");
            cJSON_AddStringToObject(config, "payload_press", "test");
            cJSON_AddStringToObject(config, "icon", "mdi:bell-ring");

            cJSON *device = create_device_json();
            if (device) cJSON_AddItemToObject(config, "device", device);

            ret = publish_discovery_config("button", "test_chime", config, true);
            cJSON_Delete(config);
            if (ret != ESP_OK) return ret;
        }
    }

    return ret;
}
```

**Call in main discovery function:**
```c
// In mqtt_discovery_publish_all():
ret = mqtt_discovery_publish_chime_entities();
if (ret != ESP_OK) return ret;
```

**Testing:**
```bash
idf.py build flash
# Restart ESP32
# Open Home Assistant
# Should see 6 new entities under word clock device!
```

**Commit:**
```bash
git commit -am "Add Home Assistant MQTT Discovery for chime entities (6 new entities)"
git push
```

**Success:** HA entities appear automatically! ✅

---

### **PHASE 4 CHECKPOINT:** Home Assistant Integration ✅
```
What works now:
✅ MQTT control of chimes
✅ MQTT status publishing
✅ Home Assistant auto-discovery
✅ 6 new HA entities:
   1. Switch: Chimes Enabled
   2. Select: Chime Schedule
   3. Number: Volume (0-100%)
   4. Number: Quiet Start Hour
   5. Number: Quiet End Hour
   6. Button: Test Chime

Total HA entities: 44 (37 existing + 7 new)

What to test:
├─ Toggle "Chimes Enabled" in HA
├─ Change schedule (quarter/half/hour)
├─ Adjust volume slider
├─ Set quiet hours
├─ Press "Test Chime" button
└─ Restart ESP32 - settings persist

Build size: ~1.8 MB
Flash time: ~16 seconds
```

---

## Phase 5: Testing & Documentation
**Duration:** 2-3 days
**Goal:** Production-ready system

---

### STEP 5.1: Comprehensive Testing (Day 11)
**Time:** 2 hours
**What:** Test all features systematically

**Test Checklist:**

**Basic Audio:**
- [ ] Test tone plays (440Hz)
- [ ] All 5 chimes play correctly
- [ ] Volume control works (0%, 50%, 100%)
- [ ] No clicks/pops/distortion
- [ ] Speaker clear and audible

**Scheduling:**
- [ ] Quarter-hour schedule works (:00, :15, :30, :45)
- [ ] Half-hour schedule works (:00, :30)
- [ ] Hour-only schedule works (:00)
- [ ] Quiet hours suppress chimes
- [ ] Schedule changes take effect immediately

**MQTT:**
- [ ] Enable/disable via HA switch
- [ ] Schedule select updates
- [ ] Volume slider updates
- [ ] Quiet hours set correctly
- [ ] Test button triggers chime
- [ ] Status updates after changes

**Persistence:**
- [ ] Config survives reboot
- [ ] NVS storage working
- [ ] Settings restored correctly

**Integration:**
- [ ] Chimes don't affect display
- [ ] LED transitions smooth during chime
- [ ] WiFi stable during playback
- [ ] MQTT publishes during chime
- [ ] No system lag or stuttering

**Commit:** Document test results
**Success:** All tests pass ✅

---

### STEP 5.2: Update Documentation (Day 12)
**Time:** 60 minutes
**What:** Update all docs

**Update CLAUDE.md:**
```markdown
## Audio System (Westminster Chimes)

### Hardware
- MAX98357A I2S Amplifier (GPIO 32, 33, 27)
- 8Ω 3W Speaker

### Components
- audio_manager: I2S playback
- chime_library: Westminster chime audio data
- chime_scheduler: Automatic chiming logic

### MQTT Topics
- home/wordclock/chime/set - Control chimes
- home/wordclock/chime/status - Current config
- home/wordclock/chime/test - Manual trigger

### Home Assistant Entities (7 new)
1. Switch: Chimes Enabled
2. Select: Schedule (off/hour/half/quarter)
3. Number: Volume (0-100%)
4. Number: Quiet Start Hour (0-23)
5. Number: Quiet End Hour (0-23)
6. Button: Test Chime

Total entities: 44
```

**Create User Guide:**
```bash
touch docs/user/chime-system-guide.md
# Document how to use chimes from HA
```

**Update README.md:**
Add chime system to features list

**Commit:**
```bash
git commit -am "Update documentation for chime system"
git push
```

**Success:** Documentation complete ✅

---

### STEP 5.3: Performance Verification (Day 12)
**Time:** 30 minutes
**What:** Verify resource usage

**Monitor:**
```c
// Add to wordclock.c (temporary)
void print_system_stats(void) {
    printf("Free heap: %d bytes\n", esp_get_free_heap_size());
    printf("Min free heap: %d bytes\n", esp_get_minimum_free_heap_size());

    // Task stats
    char buf[1024];
    vTaskList(buf);
    printf("Tasks:\n%s\n", buf);
}

// Call periodically
print_system_stats();
```

**Check:**
- [ ] Free heap >150 KB
- [ ] CPU usage <15%
- [ ] No memory leaks (heap stable)
- [ ] Flash usage <2.5 MB
- [ ] I2C LEDs not affected

**Commit:** Remove debug code
**Success:** Performance acceptable ✅

---

### STEP 5.4: Final Cleanup (Day 13)
**Time:** 30 minutes
**What:** Clean up code

**Tasks:**
- [ ] Remove debug prints
- [ ] Remove test functions
- [ ] Fix compiler warnings
- [ ] Format code consistently
- [ ] Update version numbers

**Run:**
```bash
idf.py build
# Check for warnings
```

**Commit:**
```bash
git commit -am "Final cleanup: remove debug code, fix warnings"
git push
```

**Success:** Clean build ✅

---

### STEP 5.5: Create Release Tag (Day 13)
**Time:** 15 minutes
**What:** Tag stable version

**Tag release:**
```bash
git tag -a v1.0.0-chimes -m "Westminster Chime System Release

Features:
- MAX98357A I2S audio output
- Westminster quarter/half/3-quarter/full chimes
- Big Ben hour strikes
- Automatic scheduling (quarter/half/hour)
- Quiet hours (configurable)
- Volume control (0-100%)
- Full Home Assistant integration (7 new entities)
- NVS persistence

Hardware: MAX98357A + 8Ω speaker
Flash usage: 1.8 MB (700 KB free)
Total HA entities: 44
"

git push origin v1.0.0-chimes
```

**Success:** Release tagged ✅

---

## Summary Timeline

| Phase | Duration | What | Deliverable |
|-------|----------|------|-------------|
| **Phase 0** | 1-2 days | Hardware setup | MAX98357A wired, pins assigned |
| **Phase 1** | 2-3 days | Basic audio | I2S working, test tones playing |
| **Phase 2** | 2-3 days | Chime audio | All Westminster chimes embedded |
| **Phase 3** | 2-3 days | Scheduler | Automatic chiming on schedule |
| **Phase 4** | 2-3 days | MQTT/HA | Full Home Assistant control |
| **Phase 5** | 2-3 days | Testing/docs | Production-ready system |
| **Total** | **11-18 days** | **~1-2 hrs/day** | **Complete chime system** |

---

## Rollback Plan

At any checkpoint, you can rollback:

```bash
# See commits
git log --oneline

# Rollback to specific commit
git reset --hard <commit-hash>

# Or rollback by phase
git reset --hard v1.0.0-halb-centric  # Before chimes
```

**Safe points:**
- After Phase 1: Audio works, no chimes yet
- After Phase 2: Chimes embedded, no auto-play
- After Phase 3: Auto-play works, no MQTT
- After Phase 4: Everything works!

---

## Success Criteria

### MVP Complete When:
✅ Westminster chimes play on schedule
✅ Volume adjustable (0-100%)
✅ Quiet hours work
✅ Home Assistant control works
✅ Config persists across reboots
✅ Display unaffected by audio
✅ System stable (no crashes)

### Nice-to-Have (Future):
🎯 Multiple chime styles
🎯 Per-day quiet hours
🎯 Custom chime upload via web
🎯 Voice announcements

---

## Estimated Costs

| Item | Cost | Notes |
|------|------|-------|
| MAX98357A module | $5 | Adafruit or Amazon |
| 8Ω 3W Speaker | $3 | 40-57mm diameter |
| Jumper wires | $1 | If needed |
| **Total** | **~$9** | One-time hardware cost |

**Time investment:** 11-18 days × 1-2 hrs = 20-35 hours total

---

**Ready to start? Order the hardware and begin Phase 0!** 🔔
