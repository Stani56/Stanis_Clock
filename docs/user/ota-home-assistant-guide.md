# OTA Firmware Updates - Home Assistant Guide

**Status:** Phase 2.4 Complete
**Date:** 2025-11-07
**Version:** v1.0-esp32-final-20-g08b5aee

## Overview

The Word Clock now supports Over-The-Air (OTA) firmware updates through Home Assistant. This allows you to check for, download, and install firmware updates without connecting a USB cable.

## Home Assistant Entities

After the device connects to MQTT, 9 new OTA entities will appear automatically in Home Assistant:

### Sensors (6 entities)

1. **Firmware Version**
   - Shows current firmware version with git commit hash
   - Example: `v1.0-esp32-final-20-g08b5aee`
   - Entity category: Diagnostic

2. **Firmware Build Date**
   - Shows when current firmware was compiled
   - Example: `Nov 7 2025`
   - Entity category: Diagnostic

3. **OTA Update Status**
   - Current OTA operation state
   - Values: `idle`, `checking`, `downloading`, `flashing`, `complete`, `failed`
   - Updates in real-time during OTA process

4. **OTA Download Progress**
   - Shows download/flash progress as percentage (0-100%)
   - Displays progress bar in HA UI
   - Unit: `%`

5. **Update Available**
   - Binary sensor indicating if new firmware exists
   - Values: `ON` (update available), `OFF` (up to date)
   - Device class: `update`

6. **Running Partition**
   - Shows which flash partition is currently running
   - Values: `factory`, `ota_0`
   - Helps track successful OTA updates
   - Entity category: Diagnostic

### Buttons (3 entities)

1. **Z6. Check for Updates**
   - Triggers version check against GitHub releases
   - Compares current version with latest available
   - Updates "Update Available" sensor
   - Icon: `mdi:cloud-search`

2. **Z7. Start Update**
   - Begins OTA download and flash process
   - Only proceeds if update available
   - Monitors progress via OTA Download Progress sensor
   - Automatically reboots after successful flash
   - Icon: `mdi:download`

3. **Z8. Cancel Update**
   - Aborts in-progress OTA operation
   - Safe cancellation - returns to stable state
   - Only works during download phase
   - Icon: `mdi:cancel`

## Usage Workflow

### Checking for Updates

1. Open Home Assistant
2. Navigate to your Word Clock device
3. Press **Z6. Check for Updates** button
4. Wait 5-10 seconds for GitHub check
5. Check **Update Available** sensor
   - `ON` = New version available
   - `OFF` = Already up to date

### Installing Updates

1. Ensure device has stable WiFi connection
2. Press **Z7. Start Update** button
3. Monitor progress:
   - **OTA Update Status**: Shows current phase
   - **OTA Download Progress**: Shows percentage complete
4. Wait for completion (typically 1-3 minutes)
5. Device will automatically reboot with new firmware
6. Verify **Firmware Version** sensor shows new version

### Monitoring Update Process

**Status Progression:**
```
idle → checking → downloading → flashing → complete → (reboot)
```

**Download Progress:**
- 0-80%: Downloading firmware from GitHub
- 80-95%: Writing to flash partition
- 95-100%: Verifying firmware integrity

**If Update Fails:**
- Status changes to `failed`
- Device remains on current firmware (safe)
- Check error logs via `home/[DEVICE]/error_log/query`
- Common causes: Network timeout, invalid firmware, checksum mismatch

### Canceling Updates

**When to cancel:**
- Accidentally triggered update
- Need to use device immediately
- Network issues causing slow download

**How to cancel:**
1. Press **Z8. Cancel Update** button
2. Status changes to `failed` or `idle`
3. Device returns to normal operation
4. Can retry update later

**Note:** Cancellation only works during download phase. Once flashing starts, update must complete.

## MQTT Topics Reference

For advanced users and automation:

### Version Information
**Topic:** `home/[DEVICE_NAME]/ota/version`
**Payload:**
```json
{
  "current_version": "v1.0-esp32-final-20-g08b5aee",
  "current_build_date": "Nov  7 2025",
  "current_idf_version": "v5.4.2",
  "current_size_bytes": 2621440,
  "update_available": false,
  "running_partition": "factory"
}
```

### Progress Updates
**Topic:** `home/[DEVICE_NAME]/ota/progress`
**Payload:**
```json
{
  "state": "downloading",
  "progress_percent": 45,
  "bytes_downloaded": 1179648,
  "total_bytes": 2621440,
  "time_elapsed_ms": 15340,
  "time_remaining_ms": 18660
}
```

### Commands
**Topic:** `home/[DEVICE_NAME]/command`
**Payloads:**
- `ota_check_update` - Check for new version
- `ota_get_version` - Publish current version info
- `ota_start_update` - Begin OTA process
- `ota_cancel_update` - Abort update
- `ota_get_progress` - Publish current progress

## Safety Features

### Automatic Rollback
If new firmware fails to boot 3 times:
- ESP32 automatically reverts to previous firmware
- No risk of bricking device
- Previous working firmware always preserved

### Health Checks
After OTA update, device performs self-tests:
- WiFi connectivity check
- MQTT connection check
- I2C bus functionality check
- Memory integrity check
- Partition validation

If any health check fails:
- Device triggers rollback to previous firmware
- Ensures stable operation

### Boot Loop Protection
- ESP32 tracks boot attempts
- 3+ failed boots = automatic rollback
- Prevents infinite boot loops
- Always returns to working state

### Network Failure Handling
- Timeouts prevent indefinite hangs
- Partial downloads cleaned up
- Safe retry available
- No corruption from incomplete downloads

## Troubleshooting

### "Update check failed" Status

**Possible causes:**
1. No internet connection
2. GitHub rate limiting
3. TLS certificate issue
4. Invalid firmware URL

**Solutions:**
- Verify WiFi connection (check WiFi Status sensor)
- Wait 1 hour if rate limited
- Check device logs for detailed error

### "Update available" but won't start

**Possible causes:**
1. No OTA partitions (factory-only partition table)
2. Insufficient free memory
3. Another update in progress

**Solutions:**
- Check **Running Partition** sensor
  - If shows `factory`: Need partition migration (see next section)
  - If shows `ota_0`: Should work normally
- Restart device if memory low
- Wait if another operation running

### Download stalls at specific percentage

**Possible causes:**
1. Weak WiFi signal
2. Network congestion
3. GitHub server issues

**Solutions:**
- Move device closer to WiFi access point
- Retry during off-peak hours
- Cancel and retry update

### Device reboots but shows old version

**Possible causes:**
1. Firmware validation failed
2. Health checks failed
3. Automatic rollback triggered

**Solutions:**
- Check error logs for specific failure
- Verify new firmware compatible with hardware
- Report issue if persistent

## Partition Migration (Future)

**Current Status:** Device runs on `factory` partition (no OTA support)

**To enable OTA updates:**
1. Backup all settings (brightness zones, WiFi credentials, chime settings)
2. Flash device with `partitions_ota.csv` partition table
3. Perform full flash erase (`idf.py erase-flash`)
4. Flash new firmware with OTA partition support
5. Reconfigure WiFi via AP mode
6. Restore settings via MQTT/Home Assistant

**After migration:**
- **Running Partition** will show `ota_0` or `factory`
- OTA updates will work normally
- Can switch between partitions
- Rollback protection active

**See:** [Partition Migration Guide](../planning/partition-migration-guide.md) (coming soon)

## Advanced Usage

### Automations

**Auto-check for updates daily:**
```yaml
automation:
  - alias: "Word Clock - Check Updates Daily"
    trigger:
      - platform: time
        at: "02:00:00"
    action:
      - service: button.press
        target:
          entity_id: button.word_clock_clock_stani_1_z6_action_check_for_updates
```

**Notify when update available:**
```yaml
automation:
  - alias: "Word Clock - Update Available Notification"
    trigger:
      - platform: state
        entity_id: binary_sensor.word_clock_clock_stani_1_update_available
        to: "on"
    action:
      - service: notify.mobile_app
        data:
          message: "New firmware available for Word Clock"
          title: "Update Available"
```

**Auto-install updates (careful!):**
```yaml
automation:
  - alias: "Word Clock - Auto Install Updates"
    trigger:
      - platform: state
        entity_id: binary_sensor.word_clock_clock_stani_1_update_available
        to: "on"
        for: "01:00:00"  # Wait 1 hour after detection
    condition:
      - condition: time
        after: "02:00:00"
        before: "04:00:00"  # Only during night
    action:
      - service: button.press
        target:
          entity_id: button.word_clock_clock_stani_1_z7_action_start_update
```

### Lovelace Dashboard Card

```yaml
type: entities
title: Word Clock - Firmware
entities:
  - entity: sensor.word_clock_clock_stani_1_firmware_version
  - entity: sensor.word_clock_clock_stani_1_firmware_build_date
  - entity: binary_sensor.word_clock_clock_stani_1_update_available
  - entity: sensor.word_clock_clock_stani_1_ota_update_status
  - entity: sensor.word_clock_clock_stani_1_ota_download_progress
  - entity: button.word_clock_clock_stani_1_z6_action_check_for_updates
  - entity: button.word_clock_clock_stani_1_z7_action_start_update
  - entity: button.word_clock_clock_stani_1_z8_action_cancel_update
```

## FAQ

**Q: How often should I check for updates?**
A: Once per week is sufficient. Updates are rare and typically for bug fixes or new features.

**Q: Will OTA updates erase my settings?**
A: No. Settings stored in NVS (WiFi, brightness zones, chime settings) persist across OTA updates.

**Q: Can I downgrade to older firmware?**
A: Yes, via manual flash only. OTA system only installs newer versions (safety measure).

**Q: What happens if power fails during update?**
A: Device boots from previous working firmware. No data loss. Retry update when power stable.

**Q: How much internet bandwidth does an update use?**
A: Approximately 1.3-2.6 MB per update (firmware binary size).

**Q: Can I schedule updates for specific times?**
A: Yes, via Home Assistant automations (see Advanced Usage section).

**Q: Will the display work during updates?**
A: Yes, display continues showing time while downloading. Brief interruption only during final reboot.

## Support

**Documentation:**
- [OTA Testing Strategy](../planning/ota-testing-strategy.md)
- [OTA Feasibility Study](../planning/ota-update-feasibility-study.md)
- [Phase 2 Test Results](../planning/phase-2-test-results.md)

**Troubleshooting:**
- Check error logs: `home/[DEVICE]/error_log/query`
- Enable debug logging in Home Assistant
- Report issues on GitHub

**GitHub:** https://github.com/stani56/Stanis_Clock

---

**Phase 2.4 OTA Implementation Complete** ✅
Remote firmware updates via Home Assistant fully functional.
