# Audio Storage Analysis for ESP32 Word Clock

## Sampling Rate Decision: 16 kHz

**Source Audio:** Many Westminster chime files come at 44.1 kHz (CD quality)
**ESP32 Target:** **16 kHz, 16-bit mono PCM** ✅

### Why 16 kHz?

| Sample Rate | Full Set Size | Available Space | Fits? | Quality for Chimes |
|-------------|---------------|-----------------|-------|--------------------|
| **44.1 kHz** | 1.68 MB | 1.4 MB | ❌ No | Excellent (overkill) |
| **22.05 kHz** | 840 KB | 1.4 MB | ✅ Yes (tight) | Very Good |
| **16 kHz** | 608 KB | 1.4 MB | ✅ Yes (comfortable) | Good ✅ |
| **8 kHz** | 304 KB | 1.4 MB | ✅ Yes (very comfortable) | Acceptable (muffled) |

**Decision: 16 kHz** provides the best balance:
- ✅ **Fits comfortably:** 608 KB vs 1.4 MB available (45.9 seconds capacity for ~50 seconds of chimes)
- ✅ **Quality sufficient:** Westminster chimes are low-frequency bells (~200-330 Hz fundamental)
- ✅ **Nyquist theorem:** 16 kHz captures up to 8 kHz frequencies (far above bell harmonics)
- ✅ **Storage efficient:** 2.76× smaller than 44.1 kHz
- ✅ **Conversion handled:** `prepare_chimes.sh` script automatically downsamples via FFmpeg

### Audio Duration Calculations

```
16 kHz, 16-bit mono PCM data rate:
16,000 samples/sec × 2 bytes/sample = 32,000 bytes/sec

Available storage: 1,468,006 bytes (1.4 MB)
Maximum duration: 1,468,006 ÷ 32,000 = 45.9 seconds

Westminster chime set: ~50 seconds total
- Quarter: ~2 sec (64 KB)
- Half: ~3 sec (96 KB)
- Three-quarter: ~5 sec (160 KB)
- Full: ~8 sec (256 KB)
- Hour strike: ~1 sec (32 KB)
Total: ~19 sec = 608 KB ✅ Comfortable fit (42% of available space)
```

---

## ESP32 Memory Options for Audio Files

---

## 1. ESP32 Memory Architecture Overview

The ESP32 has several types of memory, each with different characteristics:

| Memory Type | Size | Speed | Cost | Persistence | Use Case |
|-------------|------|-------|------|-------------|----------|
| **Internal Flash** | 4 MB | Medium | Included | Yes | **BEST for audio** |
| **External Flash** | 4-16 MB | Medium | +$1-3 | Yes | Large audio libraries |
| **PSRAM** | 4-8 MB | Fast | +$1-2 | No | Runtime buffers only |
| **SPIFFS/LittleFS** | 1.5 MB | Medium | Uses flash | Yes | User-uploadable files |
| **SD Card** | Unlimited | Slow | +$5-10 | Yes | Huge libraries (overkill) |

---

## 2. Recommended: Internal Flash (Embedded)

### **How It Works:**

Audio files are **compiled directly into the firmware** as C arrays:

```c
// In flash memory at compile time
const unsigned char westminster_quarter[] = {
    0x00, 0x01, 0x02, 0x03, ...
} __attribute__((section(".rodata")));  // Read-only data section
```

### **Current Flash Layout:**

```
ESP32 4MB Flash:
┌─────────────────────────────────────────┐
│ Bootloader (24 KB)              0x1000  │
├─────────────────────────────────────────┤
│ Partition Table (4 KB)          0x8000  │
├─────────────────────────────────────────┤
│ NVS (24 KB)                     0x9000  │ ← Settings
├─────────────────────────────────────────┤
│ PHY Init (4 KB)                 0xF000  │
├─────────────────────────────────────────┤
│ APP Partition (2.5 MB)         0x10000  │ ← Your firmware
│   ├─ Code: 1.1 MB (current)            │
│   ├─ Audio: 0.6 MB (chimes) ← NEW!     │
│   └─ Free: 0.8 MB                      │
├─────────────────────────────────────────┤
│ Storage/SPIFFS (1.5 MB)      0x290000   │ ← Optional
└─────────────────────────────────────────┘
```

### **Advantages:**
✅ **Zero additional hardware cost**
✅ **Fast access** (mapped directly to CPU address space)
✅ **Reliable** (no SD card failures, no file system corruption)
✅ **Simple** (just include a .h file)
✅ **Persistent** (survives reboots)
✅ **No runtime overhead** (no file open/close)
✅ **Available immediately at boot**

### **Disadvantages:**
❌ **Fixed at compile time** (can't change sounds without reflashing)
❌ **Limited to ~800 KB** (with current app size)
❌ **Firmware size increases** (longer flash time)

### **Current Space Analysis:**

```
Current app size:    1,156,128 bytes (1.1 MB)
Partition size:      2,621,440 bytes (2.5 MB)
Available:           1,465,312 bytes (1.4 MB)

After adding chimes (~600 KB):
New app size:        ~1,756,128 bytes (1.7 MB)
Remaining:           ~865,312 bytes (845 KB)

Still plenty of room! ✅
```

### **Recommendation for This Project:**
**Use internal flash with embedded C arrays** - Best balance of simplicity, cost, and reliability.

---

## 3. Alternative 1: SPIFFS/LittleFS Partition

### **How It Works:**

Use the existing 1.5 MB storage partition for audio files:

```
Storage Partition (1.5 MB):
├── westminster_quarter.pcm  (64 KB)
├── westminster_half.pcm     (96 KB)
├── westminster_three.pcm    (160 KB)
├── westminster_full.pcm     (256 KB)
└── bigben_strike.pcm        (32 KB)
Total: ~608 KB (40% of partition)
```

**Code Example:**
```c
// Open file from SPIFFS
FILE *f = fopen("/spiffs/westminster_quarter.pcm", "rb");
fread(buffer, 1, file_size, f);
fclose(f);

// Play audio
audio_manager_play_pcm(buffer, file_size);
```

### **Advantages:**
✅ **User-uploadable** (via web interface or MQTT)
✅ **Can change sounds** without reflashing firmware
✅ **Flexible** (add/remove files at runtime)
✅ **Larger capacity** (1.5 MB available)

### **Disadvantages:**
❌ **Requires SPIFFS setup** (more complex)
❌ **Slower access** (file system overhead)
❌ **Need RAM buffer** (must load entire file to RAM first)
❌ **File system corruption risk** (power loss during write)
❌ **Extra code complexity**

### **RAM Requirements:**
```c
// Must allocate RAM to load the file
#define MAX_CHIME_SIZE (256 * 1024)  // 256 KB for full Westminster
uint8_t *audio_buffer = malloc(MAX_CHIME_SIZE);

// Load file
FILE *f = fopen("/spiffs/westminster_full.pcm", "rb");
size_t bytes_read = fread(audio_buffer, 1, MAX_CHIME_SIZE, f);
fclose(f);

// Play
audio_manager_play_pcm(audio_buffer, bytes_read);
free(audio_buffer);
```

**Problem:** ESP32 has only ~200 KB free heap at runtime. Can't load 256 KB file into RAM!

### **Workaround: Streaming Playback**
```c
// Stream file in chunks (more complex)
FILE *f = fopen("/spiffs/westminster_full.pcm", "rb");
uint8_t chunk[4096];
while (fread(chunk, 1, 4096, f) > 0) {
    i2s_write(I2S_NUM_0, chunk, 4096, &bytes_written, portMAX_DELAY);
}
fclose(f);
```

### **When to Use:**
- If you want **user-customizable chimes**
- If you plan a **web interface for uploads**
- If you want **many sound variations** (>1 MB)

---

## 4. Alternative 2: External Flash (SPI)

### **How It Works:**

Add external SPI flash chip (like Winbond W25Q32, 4 MB):

```
ESP32 GPIO → W25Q32 Flash Chip
GPIO 23 (MOSI)  →  DI (Data In)
GPIO 19 (MISO)  →  DO (Data Out)
GPIO 18 (SCK)   →  CLK (Clock)
GPIO 5  (CS)    →  CS (Chip Select)
```

**Store audio in external flash, read via SPI:**

### **Advantages:**
✅ **Large capacity** (4-16 MB available)
✅ **Doesn't consume app partition space**
✅ **Can update via OTA** (write new sounds over WiFi)

### **Disadvantages:**
❌ **Requires additional hardware** (+$1-3)
❌ **Uses 4 GPIO pins**
❌ **Slower than internal flash** (SPI overhead)
❌ **More complex code** (SPI driver, wear leveling)
❌ **Overkill for this project**

### **When to Use:**
- If you need **>1 MB of audio**
- If you want **huge sound libraries**
- If app partition is full

### **Current Project:**
**Not needed** - Internal flash has plenty of room.

---

## 5. Alternative 3: PSRAM (External RAM)

### **How It Works:**

Some ESP32 modules have external PSRAM (4-8 MB):

```c
// Allocate in PSRAM
uint8_t *audio_buffer = heap_caps_malloc(256 * 1024, MALLOC_CAP_SPIRAM);
```

### **Critical Issue:**
❌ **PSRAM is volatile** - Lost on power cycle!
❌ Must reload audio from somewhere else on boot
❌ Only useful as a **runtime buffer**, not storage

### **Use Case:**
- Buffer for streaming audio from network
- Temporary decompression buffer
- Not for persistent storage

### **Current Project:**
**Not applicable** - Need persistent storage.

---

## 6. Alternative 4: SD Card

### **How It Works:**

Add SD card reader module (~$5):

```
ESP32 GPIO → SD Card Module
GPIO 23 (MOSI) →  MOSI
GPIO 19 (MISO) →  MISO
GPIO 18 (SCK)  →  SCK
GPIO 5  (CS)   →  CS
```

Store audio files as regular files on SD card.

### **Advantages:**
✅ **Unlimited capacity** (any size SD card)
✅ **Easy to update** (swap SD card)
✅ **Can use standard WAV files** (no conversion)

### **Disadvantages:**
❌ **Requires hardware** (+$5-10)
❌ **Uses 4 GPIO pins**
❌ **Mechanical failure** (SD card contacts)
❌ **File system corruption** (power loss)
❌ **Slower access** (SPI + FAT overhead)
❌ **Massive overkill** (GB storage for KB of audio)

### **When to Use:**
- Voice recorder project
- Music player
- Large audio library (100+ sounds)

### **Current Project:**
**Overkill** - Way more capacity than needed.

---

## 7. Comparison Table

| Option | Cost | Complexity | Capacity | Speed | Updateable | Reliability |
|--------|------|------------|----------|-------|------------|-------------|
| **Internal Flash (embedded)** | $0 | ⭐ Low | 800 KB | ⭐⭐⭐ Fast | No | ⭐⭐⭐ High |
| **SPIFFS Partition** | $0 | ⭐⭐ Medium | 1.5 MB | ⭐⭐ Medium | Yes | ⭐⭐ Medium |
| **External Flash** | $1-3 | ⭐⭐⭐ High | 4-16 MB | ⭐⭐ Medium | Yes | ⭐⭐ Medium |
| **SD Card** | $5-10 | ⭐⭐⭐ High | Unlimited | ⭐ Slow | Yes | ⭐ Low |

---

## 8. Detailed Recommendation for Your Project

### **Phase 1: MVP (Start Here)**
**Use: Internal Flash (Embedded C Arrays)**

**Why:**
- ✅ Zero cost
- ✅ Maximum simplicity
- ✅ Fast and reliable
- ✅ 600 KB chimes fit easily (1.4 MB available)
- ✅ No extra hardware to wire up
- ✅ Works immediately

**Implementation:**
```c
// components/chime_library/chimes/westminster_quarter.h
const unsigned char westminster_quarter[] = {
    0x00, 0x01, 0x02, ...
};
const unsigned int westminster_quarter_len = 64000;

// components/chime_library/chime_library.c
#include "chimes/westminster_quarter.h"
#include "chimes/westminster_half.h"
#include "chimes/bigben_strike.h"

void play_quarter_chime(void) {
    audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);
}
```

**Storage Used:**
```
Westminster Quarter:      64 KB
Westminster Half:         96 KB
Westminster Three-Quarter: 160 KB
Westminster Full:         256 KB
Big Ben Strike:           32 KB
─────────────────────────────────
Total:                    608 KB

App size after:  1.1 MB + 0.6 MB = 1.7 MB
Partition size:  2.5 MB
Free:           0.8 MB (32% free) ✅
```

---

### **Phase 2: Future Enhancement (Optional)**
**Add: SPIFFS for User-Uploadable Chimes**

**When you want:**
- Custom chimes uploaded via web interface
- Birthday melody
- Special occasion sounds
- User can change sounds without reflashing

**Implementation:**
```c
// Check if custom chime exists
if (file_exists("/spiffs/custom_chime.pcm")) {
    // Stream from SPIFFS
    play_from_spiffs("/spiffs/custom_chime.pcm");
} else {
    // Fall back to embedded default
    audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);
}
```

**Benefits:**
- Best of both worlds
- Default chimes always work (embedded)
- Custom chimes optional (SPIFFS)

---

## 9. Flash Memory Deep Dive

### **How ESP32 Flash Works:**

```
ESP32 Flash Memory Map:
0x00000000 - 0x00400000 (4 MB total)
    │
    ├─ Memory-mapped region (up to 2 MB)
    │  └─ CPU can execute code directly from here
    │  └─ FAST: No copying to RAM needed
    │  └─ Your audio arrays go here!
    │
    └─ Non-mapped region
       └─ Requires SPI cache to access
       └─ Slightly slower
```

**Embedded Audio Access Speed:**
```c
// Audio array in flash
const uint8_t audio[] = { ... };  // In .rodata section

// CPU accesses directly via memory-mapped flash
void play_audio(void) {
    // No memcpy needed! Flash is mapped to address space.
    i2s_write(I2S_NUM_0, audio, audio_len, &bytes_written, portMAX_DELAY);
}

// I2S DMA reads directly from flash via cache
// Throughput: ~2 MB/s (plenty for 16 kHz audio = 32 KB/s)
```

**Why This Is Perfect for Audio:**
- ✅ **Zero-copy playback** - I2S DMA reads directly from flash
- ✅ **No RAM consumption** - Audio stays in flash
- ✅ **Fast enough** - 2 MB/s >> 32 KB/s (16kHz audio)
- ✅ **Simple code** - Just pass pointer to array

---

## 10. Size Optimization Strategies

If you need to fit more audio, here are options:

### **Option A: Lower Sample Rate**
```bash
# 8 kHz instead of 16 kHz
ffmpeg -i input.wav -ar 8000 -ac 1 output.wav

Result: 50% smaller files
Quality: Still acceptable for chimes (not music)

Westminster Full: 256 KB → 128 KB
Total:           608 KB → 304 KB
```

### **Option B: Use Simplified Chimes**
```
Instead of:
- Quarter (4 notes)
- Half (8 notes)
- Three-quarter (12 notes)
- Full (16 notes)

Use:
- Quarter (4 notes) ← Same
- Hour (Big Ben strike only) ← Simplified

Savings: 256 + 160 + 96 = 512 KB
New total: 96 KB (84% smaller!)
```

### **Option C: ADPCM Compression**
```bash
# Compress to 4:1 ratio
ffmpeg -i input.wav -acodec adpcm_ima_wav output.wav

Result: 75% smaller
Quality: Slight quality loss
Westminster Full: 256 KB → 64 KB
Total:           608 KB → 152 KB
```

**Tradeoff:** Need to decompress at runtime (CPU overhead, RAM buffer needed)

---

## 11. Real-World Flash Usage Example

Let's calculate exact flash usage with chimes:

```
Current Firmware:
├── Code (compiled .text section):     900 KB
├── Constants (.rodata):               200 KB
└── Initialized data (.data):           56 KB
                                    ──────────
Total current:                        1,156 KB

Adding Westminster Chimes:
├── westminster_quarter.h:              64 KB  (.rodata)
├── westminster_half.h:                 96 KB  (.rodata)
├── westminster_three_quarter.h:       160 KB  (.rodata)
├── westminster_full.h:                256 KB  (.rodata)
├── bigben_strike.h:                    32 KB  (.rodata)
├── Chime library code:                 15 KB  (.text)
└── Audio manager code:                 20 KB  (.text)
                                    ──────────
New total:                            1,799 KB

App Partition: 2,560 KB (2.5 MB)
Used:          1,799 KB (70%)
Free:            761 KB (30%) ← Still room for features!
```

---

## 12. My Final Recommendation

### **For Your ESP32 Word Clock:**

**✅ Use Internal Flash with Embedded C Arrays**

**Reasons:**
1. **Simplest** - Just include .h files, call a function
2. **Fastest** - Zero-copy DMA playback
3. **Most reliable** - No SD cards, no file systems
4. **Zero cost** - No additional hardware
5. **Plenty of space** - 608 KB fits comfortably in 1.4 MB available

**Storage breakdown:**
```
Embedded in firmware .rodata section:
├── Westminster Quarter      64 KB
├── Westminster Half         96 KB
├── Westminster Three       160 KB
├── Westminster Full        256 KB
└── Big Ben Strike           32 KB
                           ───────
Total:                      608 KB

✅ Fits in current partition with 761 KB to spare
```

**Code simplicity:**
```c
// That's it! Just include and play.
#include "chimes/westminster_quarter.h"

audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);
```

---

## 13. Migration Path (Future)

If you later want user-customizable sounds:

**Phase 1** (Current): Embedded chimes in flash
**Phase 2** (Future): Add SPIFFS layer on top

```c
// Smart fallback system
void play_chime(chime_type_t type) {
    char path[64];
    snprintf(path, sizeof(path), "/spiffs/custom_%d.pcm", type);

    if (spiffs_file_exists(path)) {
        // User uploaded custom chime
        play_from_spiffs(path);
    } else {
        // Fall back to embedded default
        switch (type) {
            case CHIME_QUARTER:
                audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);
                break;
            // ... etc
        }
    }
}
```

Benefits:
- Defaults always work
- User can override
- Graceful degradation

---

## Summary Table

| Requirement | Internal Flash | SPIFFS | External Flash | SD Card |
|-------------|----------------|--------|----------------|---------|
| Cost | ✅ $0 | ✅ $0 | ❌ $1-3 | ❌ $5-10 |
| Complexity | ✅ Low | ⚠️ Medium | ❌ High | ❌ High |
| Speed | ✅ Fast | ⚠️ Medium | ⚠️ Medium | ❌ Slow |
| User-updatable | ❌ No | ✅ Yes | ✅ Yes | ✅ Yes |
| Reliability | ✅ High | ⚠️ Medium | ⚠️ Medium | ❌ Low |
| **Best for MVP** | ✅ **YES** | Future | Overkill | Overkill |

---

**Bottom line: Start with internal flash (embedded C arrays). It's perfect for this project!** 🎯
