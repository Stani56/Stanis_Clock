# Partition Migration Guide - Enable OTA Updates

**Status:** Required for OTA Functionality
**Date:** 2025-11-07
**Risk Level:** ⚠️ HIGH - Data Loss (all settings erased)

## Overview

The Word Clock currently runs on a `factory` partition table that does not support Over-The-Air (OTA) firmware updates. To enable OTA functionality, you must migrate to the `partitions_ota.csv` partition table.

**⚠️ CRITICAL WARNING:**
- This process **erases all flash memory**
- **All settings will be lost**: WiFi credentials, brightness zones, chime settings, etc.
- You must manually reconfigure the device after migration
- Have physical access to the device during migration
- Keep USB cable connected throughout process

## Prerequisites

### Required Tools
- ESP-IDF 5.4+ installed and configured
- USB cable for serial connection
- Terminal access (Linux/WSL/macOS)
- Backup of current settings (see below)

### Before You Begin
1. Document all current settings
2. Have WiFi credentials ready
3. Allocate 30-60 minutes for complete process
4. Ensure stable power supply

## Backup Current Settings

### Step 1: Document Configuration

**Via Home Assistant:**
1. Open Word Clock device in Home Assistant
2. Screenshot or note down:
   - All brightness zone values (5 zones)
   - Light sensor thresholds (4 values)
   - Transition duration
   - Fade in/out curves
   - Potentiometer settings
   - Chime status and volume
   - Any custom settings

**Via MQTT (optional):**
```bash
# Subscribe to all status topics
mosquitto_sub -h YOUR_BROKER -t "home/Clock_Stani_1/#" -v > backup.txt

# Trigger status publications
mosquitto_pub -h YOUR_BROKER -t "home/Clock_Stani_1/command" -m "status"
mosquitto_pub -h YOUR_BROKER -t "home/Clock_Stani_1/command" -m "refresh_sensors"
```

### Step 2: Save WiFi Credentials

**IMPORTANT:** Write down:
- WiFi SSID (network name)
- WiFi password
- MQTT broker address (if custom)
- MQTT credentials (if custom)

## Partition Table Comparison

### Current: factory (no OTA)
```csv
nvs,      data, nvs,     0x9000,  0x6000,   # 24KB settings
phy_init, data, phy,     0xf000,  0x1000,   # 4KB WiFi calibration
factory,  app,  factory, 0x10000, 0x280000, # 2.5MB firmware
storage,  data, fat,     0x290000,0x160000, # 1.5MB storage
```

**Characteristics:**
- Single firmware partition
- No OTA support
- No rollback capability
- Firmware updates require USB cable

### New: partitions_ota.csv (OTA-enabled)
```csv
nvs,      data, nvs,     0x9000,  0x6000,   # 24KB settings
otadata,  data, ota,     0xf000,  0x2000,   # 8KB OTA state
phy_init, data, phy,     0x11000, 0x1000,   # 4KB WiFi calibration
factory,  app,  factory, 0x12000, 0x280000, # 2.5MB primary firmware
ota_0,    app,  ota_0,   0x292000,0x280000, # 2.5MB update partition
storage,  data, fat,     0x512000,0xAEE000, # 11MB storage (increased!)
```

**Characteristics:**
- Dual firmware partitions (factory + ota_0)
- OTA updates supported
- Automatic rollback on boot failure
- 7.5× more storage space (11MB vs 1.5MB)
- Remote firmware updates via WiFi

**Benefits:**
- Update firmware remotely via Home Assistant
- Automatic rollback protection
- More storage for audio files (chimes)
- Future-proof for additional features

## Migration Process

### Step 1: Build Firmware with OTA Partition Table

```bash
cd /home/tanihp/esp_projects/Stanis_Clock

# Clean previous build
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py fullclean

# Build with OTA partition table
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py \
  -D PARTITION_TABLE_CSV_PATH=partitions_ota.csv \
  build
```

**Expected output:**
```
Project build complete. To flash, run:
 idf.py flash
Binary size: ~1.3MB
Partition table: partitions_ota.csv
```

### Step 2: Erase Flash Memory

**⚠️ POINT OF NO RETURN - All data will be erased**

```bash
# Full flash erase (removes all settings)
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py \
  -p /dev/ttyUSB0 \
  erase-flash
```

**Expected output:**
```
Chip erase completed successfully in X.Xs
Hard resetting via RTS pin...
```

**What gets erased:**
- All NVS data (WiFi, MQTT, brightness settings)
- All filesystem data (audio files, logs)
- Factory calibration preserved (phy_init partition)
- Hardware remains functional

### Step 3: Flash New Firmware

```bash
# Flash bootloader, partition table, and firmware
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py \
  -p /dev/ttyUSB0 \
  -D PARTITION_TABLE_CSV_PATH=partitions_ota.csv \
  flash monitor
```

**Expected serial output:**
```
I (XXX) boot: ESP-IDF v5.4.2
I (XXX) boot: chip revision: v0.2
I (XXX) boot: Loaded app from partition at offset 0x12000
I (XXX) boot: Running partition: factory
I (XXX) wordclock: ESP32-S3 Word Clock - Fresh Start
I (XXX) wordclock: No WiFi credentials found - starting AP mode
I (XXX) wifi_manager: Starting WiFi AP mode
I (XXX) wifi_manager: SSID: ESP32-LED-Config
I (XXX) wifi_manager: Password: 12345678
I (XXX) wifi_manager: Connect and visit: http://192.168.4.1
```

### Step 4: Verify Partition Table

**In serial monitor, look for:**
```
I (XXX) ota_manager: Initializing OTA manager...
I (XXX) ota_manager: Running partition: factory (0x12000, 2560 KB)
I (XXX) ota_manager: Update partition:  ota_0 (0x292000, 2560 KB)
I (XXX) ota_manager: ✅ OTA manager initialized
```

**If you see:**
```
E (XXX) ota_manager: ❌ OTA partitions not found - OTA not available
```
→ Flash failed. Repeat Step 3 with `erase-flash` first.

### Step 5: Reconfigure WiFi

**Method 1: Web Interface (Recommended)**
1. Connect to WiFi network: `ESP32-LED-Config`
2. Password: `12345678`
3. Open browser: `http://192.168.4.1`
4. Enter your WiFi credentials
5. Click "Connect"
6. Wait for device to restart

**Method 2: USB Serial (Advanced)**
```bash
# Connect via serial
screen /dev/ttyUSB0 115200

# Device will prompt for WiFi setup
```

### Step 6: Verify MQTT Connection

**Serial output should show:**
```
I (XXX) wifi_manager: WiFi connected
I (XXX) wifi_manager: IP Address: 192.168.1.XXX
I (XXX) ntp_manager: NTP time synchronized
I (XXX) mqtt_manager: Connecting to MQTT broker...
I (XXX) mqtt_manager: === SECURE MQTT CONNECTION ESTABLISHED ===
I (XXX) mqtt_discovery: Publishing all discovery configurations...
I (XXX) mqtt_discovery: ✅ Published OTA control entities (6 sensors + 3 buttons)
```

### Step 7: Restore Settings via Home Assistant

**Brightness Zones (5 zones):**
1. Open Word Clock device in Home Assistant
2. Navigate to brightness configuration entities
3. Set each zone brightness to backed-up values:
   - 1. Brightness: Very Dark Zone
   - 2. Brightness: Dim Zone
   - 3. Brightness: Normal Zone
   - 4. Brightness: Bright Zone
   - 5. Brightness: Very Bright Zone

**Light Sensor Thresholds (4 thresholds):**
1. Set threshold values:
   - 6. Threshold: Dark→Dim
   - 7. Threshold: Dim→Normal
   - 8. Threshold: Normal→Bright
   - 9. Threshold: Bright→Very Bright

**Transition Settings:**
1. Set transition duration (200-5000ms)
2. Set fade in curve (linear/ease_in/ease_out/etc.)
3. Set fade out curve
4. Enable/disable smooth transitions

**Chime Settings:**
1. Enable/disable Westminster chimes
2. Set chime volume (0-100%)

**Other Settings:**
- Potentiometer curve (linear/logarithmic/exponential)
- Safety PWM limit (if changed from default 80)

### Step 8: Verify OTA Functionality

**Test OTA system:**
1. In Home Assistant, find Word Clock device
2. Locate "Running Partition" sensor
   - Should show: `factory`
3. Press "Z6. Check for Updates" button
4. Observe "OTA Update Status" sensor
   - Should show: `checking` → `idle` or `failed`
5. Check "Firmware Version" sensor
   - Should show current version with git commit

**Expected behavior:**
- Status sensor updates in real-time
- No crashes or reboots
- System remains stable

## Verification Checklist

After migration, verify:

### System Status
- [ ] Device boots successfully
- [ ] WiFi connects automatically
- [ ] MQTT connection established
- [ ] Home Assistant entities visible

### OTA Entities (9 new entities)
- [ ] Firmware Version sensor shows version
- [ ] Firmware Build Date sensor shows date
- [ ] Running Partition sensor shows "factory"
- [ ] Update Available sensor shows OFF
- [ ] OTA Update Status sensor shows "idle"
- [ ] OTA Download Progress sensor shows 0%
- [ ] Check for Updates button (Z6) works
- [ ] Start Update button (Z7) present
- [ ] Cancel Update button (Z8) present

### Display Function
- [ ] LED matrix displays time correctly
- [ ] German words show properly
- [ ] Minute indicator LEDs work
- [ ] Display updates every minute

### Sensors and Controls
- [ ] Light sensor working (lux values)
- [ ] Potentiometer working (brightness control)
- [ ] Brightness zones applied correctly
- [ ] Transitions working (if enabled)

### Settings Restored
- [ ] All brightness zones configured
- [ ] Light sensor thresholds set
- [ ] Transition settings restored
- [ ] Chime settings configured
- [ ] Safety limits set

## Troubleshooting

### Flash Erase Fails

**Error:** `Failed to connect to ESP32`

**Solutions:**
1. Check USB cable connection
2. Try different USB port
3. Press and hold BOOT button during flash
4. Verify device in bootloader mode:
   ```bash
   ls /dev/ttyUSB*
   # Should show: /dev/ttyUSB0
   ```

### Device Won't Boot After Flash

**Symptoms:**
- No serial output
- Device appears dead
- LEDs not lit

**Recovery:**
1. Connect USB cable
2. Press and hold BOOT button
3. Press and release RESET button
4. Release BOOT button after 2 seconds
5. Repeat Step 3 (flash firmware)

### WiFi AP Mode Not Starting

**Error:** No WiFi network visible

**Solutions:**
1. Wait 30 seconds after boot
2. Check serial output for errors:
   ```bash
   screen /dev/ttyUSB0 115200
   ```
3. Look for WiFi AP messages
4. If not found, reflash firmware

### OTA Partitions Not Detected

**Error:** `❌ OTA partitions not found - OTA not available`

**Cause:** Old partition table still in flash

**Solutions:**
1. Perform full erase again:
   ```bash
   idf.py -p /dev/ttyUSB0 erase-flash
   ```
2. Flash with explicit partition table:
   ```bash
   idf.py -p /dev/ttyUSB0 \
     -D PARTITION_TABLE_CSV_PATH=partitions_ota.csv \
     flash
   ```
3. Verify partition table flashed:
   ```bash
   esptool.py -p /dev/ttyUSB0 read_flash 0x8000 0x1000 partition_table.bin
   ```

### Settings Won't Save

**Symptoms:**
- Settings reset after reboot
- Brightness zones don't persist
- WiFi credentials lost

**Cause:** NVS partition corrupted or not initialized

**Solutions:**
1. Erase NVS only:
   ```bash
   idf.py -p /dev/ttyUSB0 erase-nvs
   ```
2. Reflash firmware
3. Reconfigure settings

### Home Assistant Entities Missing

**Symptoms:**
- OTA entities not visible
- Device shows offline

**Solutions:**
1. Check MQTT connection in serial output
2. Restart Home Assistant
3. Delete device from HA and let it rediscover:
   - Settings → Devices & Services
   - MQTT → Devices
   - Find Word Clock → Delete
   - Wait 1 minute for rediscovery
4. Check MQTT broker logs

## Post-Migration Best Practices

### Regular Backups

**Create automated backup:**
```yaml
# Home Assistant automation
automation:
  - alias: "Word Clock - Daily Settings Backup"
    trigger:
      - platform: time
        at: "03:00:00"
    action:
      - service: notify.persistent_notification
        data:
          title: "Word Clock Backup"
          message: >
            Current settings:
            Zones: {{ states('number.brightness_very_dark') }}, ...
            Transitions: {{ states('number.transition_duration') }}ms
```

### Test OTA Updates

**Before relying on OTA:**
1. Create test GitHub release (or use local HTTP server)
2. Perform test OTA update
3. Verify rollback works (power cycle 3× during boot)
4. Confirm settings persist across updates

### Monitor System Health

**Watch for issues:**
- Check "Running Partition" sensor (should alternate factory ↔ ota_0)
- Monitor free heap (should stay >100KB)
- Review error logs periodically
- Test rollback mechanism annually

## Rollback to Factory Partition Table

**If you need to revert:**

⚠️ **Warning:** Also requires full flash erase

```bash
# 1. Build with factory partition table
idf.py -D PARTITION_TABLE_CSV_PATH=partitions.csv fullclean build

# 2. Erase flash
idf.py -p /dev/ttyUSB0 erase-flash

# 3. Flash old partition table
idf.py -p /dev/ttyUSB0 \
  -D PARTITION_TABLE_CSV_PATH=partitions.csv \
  flash monitor
```

**You will lose:**
- All OTA functionality
- Storage space reduced (11MB → 1.5MB)
- Must use USB for future updates

## FAQ

**Q: Can I migrate without losing settings?**
A: No. Flash erase is required to change partition tables. Always backup first.

**Q: How long does migration take?**
A: 30-60 minutes including setup and reconfiguration.

**Q: Can I migrate back later?**
A: Yes, but requires another full erase. Settings lost again.

**Q: What if power fails during migration?**
A: Device recoverable via USB. No permanent damage. Repeat migration process.

**Q: Do I need to migrate immediately?**
A: No. Current functionality works fine. Migrate when ready for OTA convenience.

**Q: Will future updates require migration again?**
A: No. Once migrated, all future updates via OTA (no USB needed).

**Q: Can I use OTA without Home Assistant?**
A: Yes. MQTT commands work directly. HA just provides nice UI.

**Q: What happens to audio files?**
A: Erased. Must re-upload after migration. More storage available after (11MB).

**Q: Can I test OTA before migration?**
A: Partially. MQTT commands work but report "no partitions". Full testing requires migration.

## Support

**Documentation:**
- [OTA Home Assistant Guide](ota-home-assistant-guide.md) - Using OTA after migration
- [OTA Testing Strategy](../planning/ota-testing-strategy.md) - Technical details
- [OTA Feasibility Study](../planning/ota-update-feasibility-study.md) - Background

**Troubleshooting:**
- Check error logs: Send MQTT command `error_log_query`
- Serial output: Use `screen /dev/ttyUSB0 115200`
- GitHub issues: https://github.com/stani56/Stanis_Clock/issues

**Emergency Recovery:**
- Always have USB cable available
- Keep backup of last working firmware binary
- Document all custom settings

---

**Partition Migration Guide Complete**
Follow steps carefully. Backup settings first. Have USB cable ready.
