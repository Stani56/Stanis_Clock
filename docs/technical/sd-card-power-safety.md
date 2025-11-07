# SD Card Power Safety Guide

**For ESP32-S3 Word Clock with SD Card Storage**

## Overview

This document explains SD card data safety during power loss and provides strategies to minimize risk.

## Current Risk Assessment

### Risk Level: **LOW** ✅

**Why it's currently safe:**
- SD card is used **read-only** during normal operation
- Chime files are written externally (via computer, ESP32 powered off)
- No runtime file creation or modification in Phase 2.2/2.3
- File system corruption only possible during file upload/write operations

**When risk increases:**
- Future features that write logs at runtime
- MQTT file upload functionality
- Chime customization with file writes
- Configuration file updates

## SD Card Failure Modes

### 1. FAT Filesystem Corruption (Most Common)

**Symptoms:**
- SD card not mounting: "Failed to mount filesystem"
- Missing files that were previously visible
- "File not found" errors for existing files
- Incorrect file sizes or dates

**Cause:**
- Power loss during FAT table update
- Incomplete directory entry write
- Buffer not flushed to physical media

**Recovery:**
```bash
# On computer (Windows):
chkdsk E: /F

# On computer (Linux):
sudo fsck.vfat /dev/sdX1

# On computer (Mac):
diskutil verifyVolume /dev/diskN
diskutil repairVolume /dev/diskN
```

**Prevention:**
- Graceful shutdown before power-off
- Periodic filesystem sync during writes
- Use high-quality SD cards (SanDisk, Samsung)

### 2. File Corruption (During Write)

**Symptoms:**
- File present but wrong size
- Audio playback fails midway through
- Partial/garbled audio

**Cause:**
- Power loss during `fwrite()` operation
- File handle not closed (`fclose()`)
- Write buffer not flushed

**Recovery:**
- Delete corrupted file
- Re-upload from backup

**Prevention:**
- Always `fclose()` files immediately after write
- Call `sync()` after critical writes
- Use atomic write patterns (write to temp file, rename)

### 3. SD Card Physical Damage (Rare)

**Symptoms:**
- Card not detected at all
- JEDEC ID errors
- Repeated timeout errors

**Cause:**
- Voltage spike during power loss
- Flash cell damage from incomplete erase cycle
- Wear-out from excessive write cycles

**Recovery:**
- Usually unrecoverable
- Try low-level format if card still detected
- Replace card

**Prevention:**
- Use quality power supply with stable voltage
- Avoid frequent power cycles during writes
- Use SD cards rated for industrial/endurance use

## Current System Safety

### Phase 2.2 Implementation (Read-Only)

```c
// Current usage pattern - SAFE ✅
esp_err_t play_chime(void) {
    // 1. Open file (read-only)
    FILE *f = fopen("/sdcard/chimes/quarter_past.pcm", "rb");

    // 2. Read data (power loss here is safe - no corruption)
    fread(buffer, 1, size, f);

    // 3. Close file
    fclose(f);

    // 4. Play audio
    audio_play_buffer(buffer, size);

    // No writes = No corruption risk
    return ESP_OK;
}
```

**Power loss scenarios:**
| Scenario | Safe? | Why |
|----------|-------|-----|
| During boot (mounting SD) | ✅ Yes | Mount is read-only operation |
| During file open | ✅ Yes | Read-only, no FAT modification |
| During file read | ✅ Yes | No writes to card |
| During audio playback | ✅ Yes | SD card idle, file already closed |
| During MQTT command | ✅ Yes | No SD card operations |

### Future Risk Scenarios (Phase 2.3+)

**If you add these features:**

❌ **Log files written at runtime**
```c
// RISKY - power loss during write corrupts file/FAT
FILE *f = fopen("/sdcard/log.txt", "a");
fprintf(f, "Chime played at %s\n", timestamp);
fclose(f);  // If power lost before this, corruption possible
```

❌ **MQTT file upload**
```c
// RISKY - power loss during upload corrupts file
FILE *f = fopen("/sdcard/chimes/new_chime.pcm", "wb");
while (mqtt_data_available()) {
    fwrite(buffer, 1, size, f);  // Power loss here = corruption
}
fclose(f);
```

❌ **Configuration file updates**
```c
// RISKY - power loss during config save
FILE *f = fopen("/sdcard/config.json", "w");
fprintf(f, "{\"enabled\": true}");
fclose(f);  // Power loss before this = lost config
```

## Mitigation Strategies

### Strategy 1: Graceful Shutdown (Recommended)

Add MQTT shutdown command to safely unmount SD card before power-off.

**Implementation:**

```c
// In sdcard_manager.c - add sync function
esp_err_t sdcard_sync(void) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Force all buffers to physical SD card
    sync();  // POSIX sync() - flushes all write buffers

    ESP_LOGI(TAG, "💾 SD card filesystem synced to physical media");
    return ESP_OK;
}
```

```c
// In mqtt_manager.c - add shutdown command
else if (strcmp(command, "shutdown") == 0) {
    ESP_LOGI(TAG, "🛑 Graceful shutdown requested via MQTT");

    // Stop any audio playback
    if (audio_is_playing()) {
        audio_stop();
        ESP_LOGI(TAG, "Audio playback stopped");
    }

    // Sync filesystem to SD card
    esp_err_t ret = sdcard_sync();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ SD card synced successfully");
    }

    // Unmount SD card cleanly
    ret = sdcard_manager_deinit();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ SD card unmounted successfully");
    }

    // Notify user it's safe to power off
    mqtt_publish_status("ready_for_poweroff");
    ESP_LOGI(TAG, "🔌 System ready for power-off");

    // Optional: Enter deep sleep instead of waiting
    // esp_deep_sleep_start();
}
```

**Usage:**
```bash
# Before unplugging power:
mosquitto_pub -h YOUR_BROKER -p 8883 --capath /etc/ssl/certs/ \
  -u esp32_led_device -P 'YOUR_PASSWORD' \
  -t 'home/Clock_Stani_1/command' -m 'shutdown'

# Wait 2-3 seconds for "ready_for_poweroff" status
# Then safe to unplug power
```

**Home Assistant Automation:**
```yaml
# automation.yaml
- alias: "Word Clock - Safe Shutdown"
  trigger:
    - platform: state
      entity_id: switch.word_clock_power  # Your smart plug
      to: 'off'
  action:
    - service: mqtt.publish
      data:
        topic: "home/Clock_Stani_1/command"
        payload: "shutdown"
    - delay: '00:00:03'  # Wait for clean unmount
```

### Strategy 2: Write-Through Caching

Force immediate writes without buffering.

```c
// When creating files
FILE *f = fopen("/sdcard/log.txt", "w");
setvbuf(f, NULL, _IONBF, 0);  // Disable buffering - write immediately

fprintf(f, "Data\n");  // Written immediately to SD card
fclose(f);
```

**Pros:**
- Data written immediately
- Lower corruption risk

**Cons:**
- Slower writes
- More SD card wear
- Higher power consumption

### Strategy 3: Atomic File Writes

Write to temporary file, then rename when complete.

```c
esp_err_t safe_file_write(const char *filepath, const char *data, size_t len) {
    char temp_path[128];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", filepath);

    // Write to temporary file
    FILE *f = fopen(temp_path, "wb");
    if (!f) return ESP_FAIL;

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        remove(temp_path);
        return ESP_FAIL;
    }

    // Sync to physical media
    sync();

    // Atomic rename (very fast, unlikely to be interrupted)
    if (rename(temp_path, filepath) != 0) {
        remove(temp_path);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

**Pros:**
- Original file never corrupted
- Power loss during write only loses temp file
- Temp file can be cleaned up on next boot

**Cons:**
- Slightly more complex
- Uses 2× disk space during write
- Rename still has tiny window of vulnerability

### Strategy 4: Periodic Sync (For Long Writes)

Flush buffers during lengthy operations.

```c
// During MQTT file upload
FILE *f = fopen("/sdcard/large_file.pcm", "wb");

size_t total = 0;
while (mqtt_data_available()) {
    size_t written = fwrite(buffer, 1, chunk_size, f);
    total += written;

    // Sync every 64KB
    if (total % 65536 == 0) {
        fflush(f);  // Flush stdio buffer
        sync();     // Flush filesystem cache
        ESP_LOGI(TAG, "Progress: %zu bytes written, synced", total);
    }
}

fclose(f);
sync();  // Final sync
```

**Pros:**
- Limits data loss to last 64KB
- Progress visible in logs
- Good for large file uploads

**Cons:**
- Slower (sync is expensive)
- More write cycles

### Strategy 5: Wear Leveling SD Cards

Use industrial-grade SD cards with wear leveling.

**Recommended SD Cards:**
- **SanDisk High Endurance** - Designed for dashcams, surveillance
- **Samsung PRO Endurance** - Rated for 140,000 hours continuous recording
- **Transcend High Endurance** - Industrial-grade
- **Delkin Devices Industrial** - -40°C to +85°C operating temp

**Avoid:**
- Cheap no-name cards
- Very old cards (SD/SDHC from before 2010)
- Cards from unreliable vendors

## Best Practices

### DO ✅

1. **Use high-quality SD cards** (SanDisk, Samsung, Transcend)
2. **Close files immediately** after read/write
3. **Implement shutdown command** for planned power-off
4. **Test SD card recovery** - simulate power loss, verify files intact
5. **Keep backup chime files** on computer
6. **Use read-only mode** when possible (current Phase 2.2 approach)
7. **Monitor SD card health** - replace if errors increase

### DON'T ❌

1. **Don't write files during normal operation** (unless necessary)
2. **Don't leave files open** for extended periods
3. **Don't power cycle rapidly** during file writes
4. **Don't use ultra-cheap SD cards**
5. **Don't assume files survive power loss** during writes
6. **Don't skip error handling** for SD operations
7. **Don't write logs continuously** - batch writes, use RAM buffer

## Recovery Procedures

### Corrupted Filesystem

1. **Remove SD card** from ESP32
2. **Insert into computer**
3. **Run filesystem check:**
   - Windows: `chkdsk E: /F`
   - Linux: `sudo fsck.vfat -a /dev/sdX1`
   - Mac: `diskutil verifyVolume /Volumes/SDCARD`
4. **Restore chime files** from backup if needed
5. **Reinsert into ESP32**
6. **Test:** Send `test_audio_file` MQTT command

### Unrecoverable SD Card

1. **Format SD card** in computer (FAT32)
2. **Copy chime files** from backup
3. **Verify directory structure:**
   ```
   /sdcard/
   ├── chimes/
   │   └── westminster/
   │       ├── quarter_past.pcm
   │       ├── half_past.pcm
   │       ├── quarter_to.pcm
   │       ├── hour.pcm
   │       └── strike.pcm
   └── test.pcm
   ```
4. **Reinsert into ESP32**
5. **Restart ESP32**
6. **Verify mount:** Check serial logs for "SD card mounted successfully"

### Emergency Backup

Keep chime files backed up on computer:

```bash
# Backup from SD card
cp -r /media/sdcard/chimes ~/word_clock_backup/
cp /media/sdcard/test.pcm ~/word_clock_backup/

# Restore to SD card
cp -r ~/word_clock_backup/* /media/sdcard/
```

## Monitoring SD Card Health

### Check for Warning Signs

**Serial Log Indicators:**
```
⚠️ Warning signs:
E (xxxx) sdmmc_cmd: sdmmc_read_sectors_dma: timeout
E (xxxx) diskio_sdmmc: sdmmc_read_blocks failed
W (xxxx) sdcard_mgr: File read slow (>100ms)

✅ Healthy signs:
I (xxxx) sdcard_mgr: SD card mounted successfully
I (xxxx) audio_mgr: ✅ File playback complete
```

### MQTT Health Monitoring (Future)

Add SD card health reporting:

```c
// Publish SD card stats every hour
{
  "total_bytes": 15262000000,
  "free_bytes": 14000000000,
  "files_read_ok": 1234,
  "files_read_errors": 2,
  "health_score": 99
}
```

## Conclusion

### Current System: **SAFE** ✅

Your Phase 2.2 implementation is **already safe** for normal operation because:
- SD card used read-only
- No runtime file writes
- Files copied externally while ESP32 powered off

### Future Enhancements

If adding write operations in Phase 2.3+:
1. Implement graceful shutdown command (highest priority)
2. Use atomic file writes for critical data
3. Add periodic sync for long writes
4. Consider industrial-grade SD cards
5. Keep backups of chime files

### Risk vs. Convenience

| Approach | Safety | Convenience | Recommended For |
|----------|--------|-------------|-----------------|
| Current (read-only) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Phase 2.2/2.3 |
| Graceful shutdown | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | Phase 2.3+ |
| Atomic writes | ⭐⭐⭐⭐ | ⭐⭐⭐ | File uploads |
| Periodic sync | ⭐⭐⭐⭐ | ⭐⭐ | Large writes |
| Industrial SD | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Production |

---

**Document Version:** 1.0
**Last Updated:** November 6, 2025
**Related:** Phase 2.2 (SD Card Integration), Phase 2.3 (Westminster Chimes)
