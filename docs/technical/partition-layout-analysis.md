# ESP32 Partition Layout Analysis

## Current Partition Table Analysis

---

## 1. Your Current ESP32 Flash Configuration

### **Flash Size: 4 MB (32 Mbit)**

```
ESP32 4MB Flash Memory Map:
┌────────────────────────────────────────────────────┐
│ Address      Size        Name         Type         │
├────────────────────────────────────────────────────┤
│ 0x0000       24 KB      (Reserved)   Bootloader   │ ← ESP32 ROM bootloader
├────────────────────────────────────────────────────┤
│ 0x8000       4 KB       Partition    Table        │ ← This file (partitions.csv)
├────────────────────────────────────────────────────┤
│ 0x9000       24 KB      NVS          Data         │ ← Settings storage
├────────────────────────────────────────────────────┤
│ 0xF000       4 KB       PHY Init     Data         │ ← WiFi calibration
├────────────────────────────────────────────────────┤
│ 0x10000      2.5 MB     Factory      App          │ ← YOUR FIRMWARE HERE
│              (2,621,440 bytes)                     │   Current: 1.1 MB
│                                                    │   After chimes: 1.7 MB
│                                                    │   Free: 0.8 MB
├────────────────────────────────────────────────────┤
│ 0x290000     1.375 MB   Storage      FAT          │ ← Optional data storage
│              (1,441,792 bytes)                     │   Currently unused
└────────────────────────────────────────────────────┘
Total: 4,194,304 bytes (4 MB exactly)
```

---

## 2. Current Partition Breakdown

### **From Your `partitions.csv`:**

| Name | Type | SubType | Offset | Size (Hex) | Size (Dec) | Size (MB) | Purpose |
|------|------|---------|--------|------------|------------|-----------|---------|
| nvs | data | nvs | 0x9000 | 0x6000 | 24,576 | 0.023 MB | Settings storage |
| phy_init | data | phy | 0xf000 | 0x1000 | 4,096 | 0.004 MB | WiFi calibration |
| **factory** | **app** | **factory** | **0x10000** | **0x280000** | **2,621,440** | **2.5 MB** | **Your firmware** |
| storage | data | fat | 0x290000 | 0x160000 | 1,441,792 | 1.375 MB | Optional storage |

**Total used:** 4,091,904 bytes (3.9 MB)
**Overhead:** ~102 KB (bootloader, partition table, alignment)

---

## 3. Maximum Partition Sizes

### **Absolute Maximum for APP Partition:**

The APP partition can theoretically be as large as:

```
Total Flash:           4,194,304 bytes (4 MB)
- Bootloader space:       24,576 bytes (at 0x1000)
- Partition table:         4,096 bytes (at 0x8000)
- NVS minimum:            12,288 bytes (3 sectors minimum)
- PHY init:                4,096 bytes (required)
────────────────────────────────────────────────────
Available for APP:     4,149,248 bytes (~3.96 MB)
```

**But there's a critical constraint:**

### **ESP32 Memory-Mapped Flash Limit: 3.25 MB**

The ESP32 can only **memory-map** (execute code directly from) the first **3.25 MB** of flash:

```
Memory-Mapped Region:
0x00000000 - 0x00340000 (3.25 MB)
└─ CPU can execute code directly
└─ Fastest access (no cache misses)

Above 3.25 MB:
└─ Can only be read via SPI (slower)
└─ Cannot execute code from here
└─ Suitable for data storage only
```

---

## 4. Practical Maximum APP Partition Size

### **Recommended Maximum: 3.125 MB (0x320000)**

```
Recommended Layout for Maximum APP Size:
┌────────────────────────────────────────────────────┐
│ 0x0000       24 KB      (Reserved)                 │
├────────────────────────────────────────────────────┤
│ 0x8000       4 KB       Partition Table            │
├────────────────────────────────────────────────────┤
│ 0x9000       16 KB      NVS (reduced to minimum)   │
├────────────────────────────────────────────────────┤
│ 0xD000       4 KB       PHY Init                   │
├────────────────────────────────────────────────────┤
│ 0x10000      3.125 MB   Factory APP (MAXIMUM!)     │
│              (3,276,800 bytes)                     │
├────────────────────────────────────────────────────┤
│ 0x330000     ~844 KB    Storage (remaining)        │
└────────────────────────────────────────────────────┘
```

**Maximum APP Partition Table Entry:**
```csv
factory,  app,  factory, 0x10000, 0x320000,
```

**Size:** 0x320000 = 3,276,800 bytes = **3.125 MB**

---

## 5. Your Current vs Maximum Comparison

| Configuration | APP Size | Storage Size | Total | Notes |
|---------------|----------|--------------|-------|-------|
| **Current** | 2.5 MB | 1.375 MB | 3.875 MB | Balanced, plenty of room |
| **Maximum APP** | 3.125 MB | 0.844 MB | 3.969 MB | If you need huge firmware |
| **Maximum Storage** | 1.5 MB | 2.375 MB | 3.875 MB | If you need lots of data files |

---

## 6. Should You Increase Your APP Partition?

### **Current Situation:**
```
Current APP size:    1,156,128 bytes (1.1 MB)
Current partition:   2,621,440 bytes (2.5 MB)
Free space:          1,465,312 bytes (1.4 MB)

After adding chimes (~600 KB):
New APP size:        ~1,756,128 bytes (1.7 MB)
Free space:          ~865,312 bytes (845 KB)
Percentage free:     33%
```

### **Recommendation: DON'T CHANGE IT!**

**Why your current 2.5 MB partition is perfect:**

✅ **Plenty of headroom** - 845 KB free after chimes (33% unused)
✅ **Room for growth** - Can add more features later
✅ **Conservative** - Leaves space for OTA updates (future)
✅ **Storage partition available** - 1.375 MB for custom chimes/sounds (future)

**When to increase to 3.125 MB:**
- ❌ If app size exceeds 2.5 MB (not happening anytime soon)
- ❌ If you add voice announcements (hundreds of phrases)
- ❌ If you add video playback (you're not!)

---

## 7. Partition Size Constraints

### **Alignment Requirements:**

All partitions must be aligned to **4 KB (0x1000)** boundaries:

```
Valid sizes:
0x1000   = 4 KB    ✅
0x10000  = 64 KB   ✅
0x100000 = 1 MB    ✅
0x123000 = ?       ✅ (multiple of 0x1000)
0x123456 = ?       ❌ NOT aligned!
```

### **Minimum Partition Sizes:**

| Partition Type | Minimum Size | Reason |
|----------------|--------------|--------|
| NVS | 12 KB (0x3000) | 3 sectors × 4 KB |
| PHY Init | 4 KB (0x1000) | Fixed data |
| APP | 64 KB (0x10000) | Practical minimum for bootloader |
| FAT/SPIFFS | 64 KB (0x10000) | File system overhead |

---

## 8. Alternative Partition Schemes

### **Option 1: Current (Balanced) - RECOMMENDED ✅**
```csv
# Good balance of app and storage
nvs,      data, nvs,     0x9000,  0x6000,    # 24 KB
phy_init, data, phy,     0xf000,  0x1000,    # 4 KB
factory,  app,  factory, 0x10000, 0x280000,  # 2.5 MB ← Current
storage,  data, fat,     0x290000, 0x160000, # 1.375 MB
```
**Best for:** General purpose, room for growth

---

### **Option 2: Maximum APP (If You Need It)**
```csv
# Maximize firmware space
nvs,      data, nvs,     0x9000,  0x4000,    # 16 KB (reduced)
phy_init, data, phy,     0xd000,  0x1000,    # 4 KB
factory,  app,  factory, 0x10000, 0x320000,  # 3.125 MB ← MAXIMUM!
storage,  data, fat,     0x330000, 0xD0000,  # 832 KB
```
**Best for:** Huge firmware with voice/video

---

### **Option 3: Maximum Storage**
```csv
# Maximize data storage space
nvs,      data, nvs,     0x9000,  0x4000,    # 16 KB
phy_init, data, phy,     0xd000,  0x1000,    # 4 KB
factory,  app,  factory, 0x10000, 0x180000,  # 1.5 MB
storage,  data, fat,     0x190000, 0x260000, # 2.375 MB ← MAXIMUM!
```
**Best for:** Lots of audio files, web assets

---

### **Option 4: OTA (Dual APP Partitions)**
```csv
# For Over-The-Air updates
nvs,      data, nvs,     0x9000,  0x4000,    # 16 KB
otadata,  data, ota,     0xd000,  0x2000,    # 8 KB (OTA selection)
phy_init, data, phy,     0xf000,  0x1000,    # 4 KB
factory,  app,  ota_0,   0x10000, 0x180000,  # 1.5 MB (APP slot 1)
ota_1,    app,  ota_1,   0x190000, 0x180000, # 1.5 MB (APP slot 2)
storage,  data, fat,     0x310000, 0x80000,  # 512 KB
```
**Best for:** Remote firmware updates (future feature)

---

## 9. Flash Size Detection

### **How to Check Your ESP32 Flash Size:**

```bash
# Using esptool
esptool.py flash_id

# Output will show something like:
# Detected flash size: 4MB
```

Or in your ESP32 code:
```c
#include "esp_flash.h"

uint32_t flash_size;
esp_flash_get_size(NULL, &flash_size);
printf("Flash size: %lu MB\n", flash_size / (1024 * 1024));
```

### **Common ESP32 Flash Sizes:**

| Flash Size | Total | Typical APP | Typical Storage | Use Case |
|------------|-------|-------------|-----------------|----------|
| 2 MB | 2,097,152 | 1 MB | 512 KB | Budget projects |
| **4 MB** | **4,194,304** | **1.5-2.5 MB** | **1-2 MB** | **Most common** ← You |
| 8 MB | 8,388,608 | 3-4 MB | 3-4 MB | Feature-rich |
| 16 MB | 16,777,216 | 4-8 MB | 8-12 MB | Advanced (rare) |

---

## 10. Upgrading Flash Size (If Needed)

### **Can You Upgrade to 8 MB or 16 MB?**

**Physically:** Yes, but requires:
1. Desoldering current flash chip
2. Soldering new 8/16 MB chip
3. Updating menuconfig: `idf.py menuconfig` → Flash size

**Practically:** Not worth it for this project!

**Why:**
- ✅ Your 4 MB is plenty (only using 1.7 MB after chimes)
- ❌ SMD soldering is difficult (0.5mm pitch)
- ❌ Risk of breaking ESP32
- ❌ $5-10 for chip + tools
- ❌ Better to just use external storage if needed

**Alternative:** Add SD card or external flash via SPI (easier, safer)

---

## 11. Partition Size Calculator

### **Formula:**

```
Maximum APP Size = Total Flash - Fixed Overhead

Where:
Fixed Overhead = Bootloader (32 KB)
               + Partition Table (4 KB)
               + NVS (12-24 KB minimum)
               + PHY Init (4 KB)
               + Storage (if needed)
               + Alignment padding

For 4 MB Flash:
Maximum APP ≈ 4 MB - 60 KB - Storage Size
            ≈ 3.94 MB - Storage Size
```

### **Your Calculations:**

| Storage Size | Maximum APP Size | Notes |
|--------------|------------------|-------|
| 0 MB | ~3.94 MB | No storage partition |
| 500 KB | ~3.44 MB | Minimal storage |
| 1 MB | ~2.94 MB | Balanced |
| 1.375 MB | **2.5 MB** | **← Your current setup** |
| 2 MB | ~1.94 MB | Lots of storage |

---

## 12. Current Space Analysis for Chimes

### **With Current 2.5 MB Partition:**

```
Space Breakdown After Adding Westminster Chimes:

Code (current):              1,100 KB
Westminster chimes:            608 KB
Audio manager code:             20 KB
Chime scheduler code:           15 KB
MQTT additions:                 10 KB
                             ─────────
Total used:                  1,753 KB (1.7 MB)

Partition size:              2,560 KB (2.5 MB)
Free space:                    807 KB (0.8 MB)
Percentage free:                31.5%

Verdict: EXCELLENT! ✅
```

### **Room for More Features:**

With 807 KB free, you could still add:

| Feature | Size | Would it fit? |
|---------|------|---------------|
| More chime styles | 200 KB | ✅ Yes (607 KB left) |
| Simple voice announcements | 300 KB | ✅ Yes (507 KB left) |
| Web interface assets | 150 KB | ✅ Yes (657 KB left) |
| Custom fonts/graphics | 100 KB | ✅ Yes (707 KB left) |
| All of the above | 750 KB | ✅ BARELY! (57 KB left) |

---

## 13. Recommendation

### **Keep Your Current 2.5 MB APP Partition**

**Why:**
1. ✅ **Plenty of space** - 807 KB free after chimes (31.5%)
2. ✅ **Room to grow** - Can add more features
3. ✅ **Balanced** - 1.375 MB storage partition available for future use
4. ✅ **Safe** - Not pushing flash limits
5. ✅ **OTA-ready** - Could convert to dual-partition scheme later if needed

**Don't increase unless:**
- App size exceeds 2.3 MB (you're at 1.7 MB)
- You add massive voice libraries (100+ phrases)
- You add video/images (unlikely for a word clock!)

---

## 14. How to Modify Partition Table (If Needed)

### **Step 1: Edit `partitions.csv`**
```csv
# Example: Increase APP to 3 MB
factory,  app,  factory, 0x10000, 0x300000,  # 3 MB instead of 2.5 MB
storage,  data, fat,     0x310000, 0xE0000,  # Reduce storage to 896 KB
```

### **Step 2: Clean build**
```bash
idf.py fullclean
idf.py build
```

### **Step 3: Flash with erase**
```bash
idf.py erase-flash  # ⚠️ Erases ALL data (including NVS settings!)
idf.py flash
```

### **⚠️ WARNING:**
Changing partition sizes **erases all data**! You'll need to:
- Reconfigure WiFi credentials
- Reset brightness settings
- Re-enter MQTT credentials

**Better approach:** Keep current layout unless absolutely necessary.

---

## 15. Summary Table

| Metric | Value | Notes |
|--------|-------|-------|
| **Total Flash** | 4 MB | ESP32 chip capacity |
| **Current APP Partition** | 2.5 MB | Your setting |
| **Maximum APP Partition** | 3.125 MB | Theoretical maximum |
| **Current APP Size** | 1.1 MB | Before chimes |
| **After Chimes** | 1.7 MB | With Westminster chimes |
| **Free Space** | 0.8 MB | Room for growth |
| **Storage Partition** | 1.375 MB | Available for data files |
| **Recommendation** | **Keep current!** | ✅ Perfect balance |

---

## Conclusion

**Your current 2.5 MB APP partition is perfectly sized!**

✅ Plenty of room for Westminster chimes (608 KB)
✅ Still 807 KB free for future features (31.5%)
✅ Storage partition available (1.375 MB) for custom sounds later
✅ No need to change anything

**Maximum possible APP partition: 3.125 MB**
(But you don't need it - you're only using 1.7 MB!)

**Keep your current `partitions.csv` as-is.** 👍
