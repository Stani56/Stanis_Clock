# Correct ESP32-S3 Flashing Procedure - Avoiding Partition Conflicts

**Date:** November 14, 2025
**Context:** Permanent solution to prevent ESP_ERR_OTA_PARTITION_CONFLICT errors
**Related Issues:** v2.10.5 OTA partition conflict debugging

---

## Problem: Manual Partition Address Specification

### What Goes Wrong

When flashing firmware manually with specific partition addresses like this:

```bash
# ❌ WRONG - Manual partition address specification
esptool.py write_flash 0x2a0000 build/wordclock.bin
```

**Problems:**
1. **No otadata update** - The otadata partition (0xf000) is not written
2. **ESP-IDF gets confused** - System doesn't know which partition is active
3. **OTA conflicts** - Next OTA tries to write to the running partition
4. **Error:** `ESP_ERR_OTA_PARTITION_CONFLICT`

### Why This Happens

ESP32-S3 uses **dual OTA partitions** for safe firmware updates:

```csv
# partitions.csv
ota_0,    app,  ota_0,   0x20000,  0x280000,  # Partition A
ota_1,    app,  ota_1,   0x2a0000, 0x280000,  # Partition B
otadata,  data, ota,     0xf000,   0x2000,    # Tracks active partition
```

The **otadata partition** contains a state table that tracks:
- Which partition is currently active (running firmware)
- Which partition is inactive (safe to overwrite)
- Boot sequence number for each partition

When you manually flash to 0x2a0000 without updating otadata:
- Device runs from ota_1 (0x2a0000)
- But otadata still points to ota_0 as active
- ESP-IDF calculates inactive partition = 0x2a0000
- OTA tries to write to 0x2a0000 → **CONFLICT!**

---

## Solution: Always Use ESP-IDF Automatic Flashing

### ✅ CORRECT Method - idf.py flash

```bash
# Build and flash (recommended)
idf.py build flash

# Or separate steps
idf.py build
idf.py flash
```

**What this does automatically:**
1. ✅ Reads partition table from `partitions.csv`
2. ✅ Flashes firmware to correct partition (usually ota_0 at 0x20000)
3. ✅ Writes bootloader to 0x0
4. ✅ Writes partition table to 0x8000
5. ✅ **Initializes otadata partition at 0xf000** (critical!)
6. ✅ Sets correct boot partition in otadata

### What Gets Flashed

```bash
# Example output from idf.py flash:
0x0      bootloader/bootloader.bin
0x20000  wordclock.bin                    # ← Firmware to ota_0
0x8000   partition_table/partition-table.bin
0xf000   ota_data_initial.bin             # ← Critical for OTA!
```

The `ota_data_initial.bin` file is auto-generated and contains the proper state table.

---

## When You Might Need Manual Flashing

### Scenario: Recovering from Bad OTA State

If OTA system is completely broken and device won't boot:

```bash
# Step 1: Erase flash completely (nuclear option)
idf.py erase-flash

# Step 2: Flash everything fresh
idf.py flash
```

### Scenario: Flashing Only Firmware (Advanced)

If you **really** need to flash just the firmware binary:

```bash
# Still let ESP-IDF handle the addresses!
idf.py app-flash
```

This flashes only the app partition but still uses correct addresses from partition table.

---

## Verification After Flashing

### Check Boot Logs

Look for these indicators of correct partition setup:

```
I (xxx) boot: Loaded app from partition at offset 0x20000
I (xxx) boot: OTA data partition at offset 0xf000 marked valid
I (xxx) esp_image: segment 0: paddr=00020020 vaddr=3c0b0020 size=2e994h
```

**Key indicators:**
- ✅ "offset 0x20000" - Running from ota_0
- ✅ "OTA data partition...marked valid" - otadata initialized
- ✅ "paddr=00020020" - Physical address matches ota_0

### Test OTA Update

After correct flashing, OTA should work:

```bash
# Trigger OTA via MQTT
mosquitto_pub -t "home/Clock_Stani_1/command" -m "ota_start_update"

# Watch for success logs:
# I (xxx) esp_https_ota: Writing to <ota_1> partition at offset 0x2a0000
# I (xxx) ota_manager: ✅ SHA-256 checksum verified successfully
# I (xxx) ota_manager: OTA update successful, restarting...
```

**Expected behavior:**
- First OTA after flash: Writes to ota_1 (0x2a0000)
- Device reboots from ota_1
- Next OTA: Writes to ota_0 (0x20000)
- Device reboots from ota_0
- Continues alternating between partitions

---

## Common Mistakes and Fixes

### Mistake 1: Using Manual write_flash Commands

```bash
# ❌ WRONG
esptool.py write_flash 0x20000 build/wordclock.bin
esptool.py write_flash 0x2a0000 build/wordclock.bin

# ✅ CORRECT
idf.py flash
```

### Mistake 2: Flashing to Both Partitions

Some guides suggest flashing identical firmware to both ota_0 and ota_1. **Don't do this!**

```bash
# ❌ WRONG - Confuses OTA system
esptool.py write_flash 0x20000 build/wordclock.bin
esptool.py write_flash 0x2a0000 build/wordclock.bin

# ✅ CORRECT - Let OTA handle the second partition
idf.py flash  # Flashes to ota_0 only
# Then use OTA to update to ota_1 when needed
```

### Mistake 3: Forgetting to Update otadata

If you absolutely must use manual flashing for debugging:

```bash
# If you manually flash firmware, you MUST also update otadata
# But this is complex and error-prone - use idf.py flash instead!
```

There's no simple manual command to update otadata correctly. The state table format is binary and version-specific. **This is why you should always use `idf.py flash`.**

---

## Troubleshooting Partition Conflicts

### Symptom: ESP_ERR_OTA_PARTITION_CONFLICT

```
E (xxx) esp_https_ota: esp_ota_begin failed (ESP_ERR_OTA_PARTITION_CONFLICT)
E (xxx) ota_manager: ❌ OTA download failed: ESP_ERR_OTA_PARTITION_CONFLICT
```

**Diagnosis:**

1. Check what partition is currently running:
```
I (xxx) boot: Loaded app from partition at offset 0xXXXXXX
```

2. Check where OTA tried to write:
```
I (xxx) esp_https_ota: Writing to <ota_X> partition at offset 0xXXXXXX
```

3. If both offsets are the same → **otadata is corrupted**

**Fix:**

```bash
# Option 1: Clean reflash (recommended)
idf.py flash

# Option 2: Erase and reflash (nuclear option)
idf.py erase-flash
idf.py flash
```

---

## Summary: The Golden Rule

### Never Specify Partition Addresses Manually

**Always use:**
```bash
idf.py flash        # Full flash (bootloader + app + partitions + otadata)
idf.py app-flash    # App only (still uses correct addresses)
```

**Never use:**
```bash
esptool.py write_flash 0x20000 ...   # ❌ Manual addresses
esptool.py write_flash 0x2a0000 ...  # ❌ Manual addresses
```

### Why This Matters

ESP-IDF's OTA system is sophisticated and handles:
- Partition table parsing
- Address calculation
- otadata state management
- Boot sequence tracking
- Rollback protection

When you bypass this with manual addressing:
- You break the state machine
- OTA updates fail with conflicts
- Recovery requires USB reflashing

**Bottom line:** Let ESP-IDF handle partition management. It knows what it's doing.

---

## References

- [ESP-IDF OTA Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/ota.html)
- [Partition Tables Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html)
- [OTA Safety Checklist](ota-safety-checklist.md)
- [Dual OTA Source System](../technical/dual-ota-source-system.md)

---

**Key Takeaway:** "How can we avoid flashing to the wrong partition once and for ever?" → **Always use `idf.py flash`**
