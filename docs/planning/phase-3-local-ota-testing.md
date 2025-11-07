# Phase 3: Local OTA Testing Plan

**Status:** Ready to Execute
**Date:** 2025-11-07
**Goal:** Test complete OTA update cycle using local HTTP server
**Risk Level:** MEDIUM (requires test firmware, may require revert)

## Overview

Phase 3 tests the full OTA update cycle using a local HTTP server instead of GitHub. This allows us to:
- Verify firmware download works
- Test progress tracking accuracy
- Validate partition switching
- Confirm rollback mechanism
- Document complete OTA workflow

**Why Local Testing First?**
- Faster iteration (no GitHub upload/download)
- No network dependencies
- Controlled test environment
- Can test rollback scenarios safely
- No rate limiting issues

## Prerequisites

### Current Status Check
```bash
# Check current partition table
grep "^ota_0" partitions_ota.csv
# Should exist: ota_0,app,ota_0,0x292000,0x280000

# Check device has OTA partitions (if already migrated)
# Look in serial output for:
# "Running partition: factory"
# "Update partition: ota_0"
```

### Required Setup
1. ✅ OTA manager implemented (Phase 2.4 complete)
2. ✅ MQTT commands functional
3. ✅ Home Assistant discovery working
4. ⚠️ **Device must be on OTA partition table** (migration required)
5. Local HTTP server (Python, we'll set this up)
6. Test firmware binary

## Test Strategy

### Safe Testing Approach
1. Build current firmware (v2.4.0-test)
2. Modify version string slightly
3. Flash device with OTA partition table
4. Setup local HTTP server with "new" firmware
5. Trigger OTA update via MQTT
6. Monitor complete process
7. Verify new version running
8. Test rollback (power cycle 3×)

## Step-by-Step Execution

### Step 1: Build Test Firmware

**Create test version marker:**
```bash
cd /home/tanihp/esp_projects/Stanis_Clock

# Build with current code (this will be our "old" firmware)
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py build

# Save this binary as "old" firmware
cp build/wordclock.bin /tmp/ota_test/wordclock_v1.bin
```

**Modify version for "new" firmware:**
```bash
# Edit sdkconfig or CMakeLists.txt to change version
# Or just rebuild after making a small code change
# This creates a "newer" version for testing
```

**Build "new" firmware:**
```bash
# Make a trivial change (e.g., add comment to wordclock.c)
echo "// OTA Test Build" >> main/wordclock.c

# Rebuild
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py build

# Save as "new" firmware
cp build/wordclock.bin /tmp/ota_test/wordclock_v2.bin

# Clean up the test comment
git checkout main/wordclock.c
```

### Step 2: Setup Local HTTP Server

**Create test directory:**
```bash
mkdir -p /tmp/ota_test
cd /tmp/ota_test
```

**Create version.json:**
```bash
cat > version.json <<'EOF'
{
  "version": "v2.4.0-local-test",
  "build_date": "Nov  7 2025",
  "idf_version": "5.4.2",
  "size_bytes": 1310720,
  "description": "Local OTA test firmware",
  "changelog": "Testing OTA update mechanism"
}
EOF
```

**Copy firmware binary:**
```bash
# Copy the "new" firmware to test directory
cp build/wordclock.bin /tmp/ota_test/firmware.bin

# Get actual size
ls -l firmware.bin
# Update size_bytes in version.json with actual value
```

**Start HTTP server:**
```bash
cd /tmp/ota_test
python3 -m http.server 8080 &

# Test server accessibility
curl http://localhost:8080/version.json
curl -I http://localhost:8080/firmware.bin
```

### Step 3: Modify OTA Manager for Local Testing

**Temporary URL modification:**

Edit `components/ota_manager/include/ota_manager.h`:

```c
// Original (GitHub):
// #define OTA_DEFAULT_FIRMWARE_URL "https://github.com/stani56/Stanis_Clock/releases/latest/download/wordclock.bin"
// #define OTA_DEFAULT_VERSION_URL  "https://github.com/stani56/Stanis_Clock/releases/latest/download/version.json"

// Local testing (change to your IP address):
#define OTA_DEFAULT_FIRMWARE_URL "http://192.168.1.XXX:8080/firmware.bin"
#define OTA_DEFAULT_VERSION_URL  "http://192.168.1.XXX:8080/version.json"
```

**Find your local IP:**
```bash
ip addr show | grep "inet " | grep -v 127.0.0.1
# Use the IP address shown (e.g., 192.168.1.100)
```

**Rebuild with local URLs:**
```bash
# Edit ota_manager.h with your local IP
nano components/ota_manager/include/ota_manager.h

# Rebuild
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py build
```

### Step 4: Flash Device with OTA Partition Table

**⚠️ WARNING: This step erases all settings!**

See [Partition Migration Guide](../user/partition-migration-guide.md) for detailed steps.

**Quick version:**
```bash
# Erase flash
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py \
  -p /dev/ttyUSB0 erase-flash

# Flash with OTA partition table
/home/tanihp/.espressif/python_env/idf5.4_py3.12_env/bin/python \
  /home/tanihp/esp/esp-idf/tools/idf.py \
  -p /dev/ttyUSB0 \
  -D PARTITION_TABLE_CSV_PATH=partitions_ota.csv \
  flash monitor
```

**Verify OTA partitions detected:**
```
I (XXX) ota_manager: Running partition: factory (0x12000, 2560 KB)
I (XXX) ota_manager: Update partition:  ota_0 (0x292000, 2560 KB)
I (XXX) ota_manager: ✅ OTA manager initialized
```

### Step 5: Perform OTA Update

**Via MQTT command:**
```bash
# Get IP address from device serial output
# WiFi connected, IP: 192.168.1.XXX

# Test connectivity to HTTP server from device network
curl http://192.168.1.XXX:8080/version.json

# Trigger OTA check
mosquitto_pub -h YOUR_BROKER \
  -t "home/Clock_Stani_1/command" \
  -m "ota_check_update"

# Monitor serial output for version check
# Should see: "Update available: vX.X.X → v2.4.0-local-test"

# Start OTA update
mosquitto_pub -h YOUR_BROKER \
  -t "home/Clock_Stani_1/command" \
  -m "ota_start_update"
```

**Expected serial output:**
```
I (XXX) MQTT_MANAGER: 🚀 Starting OTA firmware update...
I (XXX) ota_manager: Checking for updates...
I (XXX) ota_manager: Update available: v1.0-esp32-final-21-gXXX → v2.4.0-local-test
I (XXX) ota_manager: Starting OTA update...
I (XXX) ota_manager: Downloading from: http://192.168.1.XXX:8080/firmware.bin
I (XXX) ota_manager: Image size: 1310720 bytes
I (XXX) ota_manager: Progress: 0%
I (XXX) ota_manager: Progress: 10% (131072 bytes)
I (XXX) ota_manager: Progress: 20% (262144 bytes)
...
I (XXX) ota_manager: Progress: 100% (1310720 bytes)
I (XXX) ota_manager: ✅ Download complete
I (XXX) ota_manager: Verifying firmware...
I (XXX) ota_manager: New firmware version: v2.4.0-local-test
I (XXX) ota_manager: ✅ Image validation passed
I (XXX) ota_manager: Writing to ota_0 partition...
I (XXX) ota_manager: ✅ Firmware written successfully
I (XXX) ota_manager: Setting boot partition to ota_0
I (XXX) ota_manager: 🔄 Rebooting in 3 seconds...
```

**Monitor via Home Assistant:**
1. Watch "OTA Update Status" sensor
2. Monitor "OTA Download Progress" sensor
3. Observe progress bar filling up
4. Device will reboot automatically

### Step 6: Verify New Firmware Running

**After reboot, check serial output:**
```
I (XXX) boot: Running partition: ota_0
I (XXX) wordclock: Firmware: v2.4.0-local-test
I (XXX) ota_manager: Boot count: 1
I (XXX) ota_manager: Performing health checks...
I (XXX) ota_manager: ✅ WiFi connected
I (XXX) ota_manager: ✅ MQTT connected
I (XXX) ota_manager: ✅ I2C bus healthy
I (XXX) ota_manager: ✅ Memory healthy
I (XXX) ota_manager: ✅ Partitions valid
I (XXX) ota_manager: ✅ All health checks passed (5/5)
I (XXX) ota_manager: Marking app as valid
```

**Check Home Assistant:**
- "Firmware Version" shows: `v2.4.0-local-test`
- "Running Partition" shows: `ota_0`
- "Update Available" shows: `OFF`

### Step 7: Test Rollback Mechanism

**Simulate boot failure:**
```bash
# Power cycle device 3 times quickly (within ~10 seconds each)
# This triggers rollback protection

# Method 1: Power button
# 1. Unplug power
# 2. Wait 2 seconds
# 3. Plug back in
# 4. Wait for boot to start
# 5. Repeat 2 more times

# Method 2: Reset button (if available)
# Press reset button 3 times with 2-second intervals
```

**Expected rollback behavior:**
```
I (XXX) boot: Boot count: 3
I (XXX) ota_manager: ⚠️ Excessive boot failures detected
I (XXX) ota_manager: 🔄 Rolling back to previous firmware
I (XXX) ota_manager: Setting boot partition to: factory
I (XXX) boot: Rebooting...

[After reboot]
I (XXX) boot: Running partition: factory
I (XXX) wordclock: Firmware: v1.0-esp32-final-21-gXXX (original version)
I (XXX) ota_manager: ✅ Rollback successful
```

### Step 8: Test Successful Health Checks

**Re-perform OTA update:**
```bash
# Start update again
mosquitto_pub -h YOUR_BROKER \
  -t "home/Clock_Stani_1/command" \
  -m "ota_start_update"

# Let it complete and reboot
# This time, let device boot normally
```

**After reboot, mark app as valid:**
```bash
# Health checks should pass automatically
# OTA manager will mark app as valid
# Boot count resets to 0

# Verify no automatic rollback occurs
# Device should stay on ota_0 partition
```

**Verify stability:**
```bash
# Power cycle normally 5+ times
# Device should remain on ota_0
# No rollback should occur
```

## Success Criteria

### Phase 3 Complete When:
- [ ] Local HTTP server serving firmware
- [ ] OTA check detects available update
- [ ] Download completes successfully
- [ ] Progress tracking accurate (0-100%)
- [ ] Firmware writes to ota_0 partition
- [ ] Device boots from ota_0
- [ ] Health checks pass (5/5)
- [ ] New version displayed in HA
- [ ] Rollback triggers on 3 boot failures
- [ ] Device reverts to factory partition
- [ ] Settings preserved across updates
- [ ] No crashes or memory leaks

### Measurements to Record:
1. Download speed (KB/s)
2. Total download time (seconds)
3. Flash write time (seconds)
4. Total update time (minutes)
5. Memory usage during download
6. Boot time after update
7. Health check execution time
8. Rollback trigger time

## Troubleshooting

### Update Fails: "HTTP connection failed"

**Cause:** Device can't reach local HTTP server

**Solutions:**
1. Verify server running: `curl http://localhost:8080/version.json`
2. Check firewall: `sudo ufw allow 8080` (if using ufw)
3. Verify device and server on same network
4. Check IP address in OTA URLs matches server IP
5. Test from device: `ping 192.168.1.XXX` (if ping enabled)

### Update Fails: "Invalid firmware"

**Cause:** Firmware binary corrupted or wrong format

**Solutions:**
1. Verify binary size matches version.json
2. Check MD5: `md5sum firmware.bin`
3. Rebuild firmware: `idf.py fullclean build`
4. Ensure partition table matches (ota vs factory)

### Device Won't Boot After Update

**Cause:** New firmware has bugs or incompatible

**Recovery:**
1. Power cycle 3× to trigger rollback
2. Device should revert to factory partition
3. If stuck, reflash via USB:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

### Rollback Doesn't Trigger

**Cause:** Health checks passing, or boot count not tracked

**Solutions:**
1. Check NVS: Boot count should increment
2. Verify health checks: Look for "Health check failed" logs
3. Manually trigger rollback:
   ```bash
   mosquitto_pub -t "home/Clock_Stani_1/command" -m "ota_rollback"
   # (Need to implement this command if desired)
   ```

### Settings Lost After Update

**Cause:** NVS erased or corrupted

**Solutions:**
1. Verify NVS partition intact
2. Restore settings via Home Assistant
3. Check NVS namespace: "ota_manager" vs other settings
4. May need to separate NVS partitions for settings vs OTA state

## Cleanup After Testing

**Revert to GitHub URLs:**
```bash
# Edit ota_manager.h back to GitHub URLs
nano components/ota_manager/include/ota_manager.h

# Change:
# #define OTA_DEFAULT_FIRMWARE_URL "http://192.168.1.XXX:8080/firmware.bin"
# Back to:
# #define OTA_DEFAULT_FIRMWARE_URL "https://github.com/stani56/Stanis_Clock/releases/latest/download/wordclock.bin"

# Rebuild and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

**Stop HTTP server:**
```bash
# Find PID
ps aux | grep "python3 -m http.server"

# Kill server
kill <PID>

# Clean up test files
rm -rf /tmp/ota_test
```

## Next Steps After Phase 3

Once Phase 3 testing complete:
1. **Document results** - Update test results document
2. **Commit findings** - Version control test outcomes
3. **Revert test changes** - Restore GitHub URLs
4. **Plan Phase 4** - Partition migration guide for users
5. **Plan Phase 5** - GitHub Actions CI/CD for releases

## Risk Mitigation

**Before Starting:**
- [ ] Backup all current settings
- [ ] Document WiFi credentials
- [ ] Have USB cable available
- [ ] Test rollback on non-critical device first
- [ ] Know how to reflash via USB

**During Testing:**
- [ ] Monitor serial output continuously
- [ ] Watch free heap memory
- [ ] Check for error logs
- [ ] Verify network stability
- [ ] Document all observations

**After Testing:**
- [ ] Verify settings preserved
- [ ] Test all features working
- [ ] Monitor for 24 hours
- [ ] Check for memory leaks
- [ ] Restore production URLs

---

**Phase 3 Status:** Ready to Execute
**Estimated Time:** 2-3 hours
**Risk Level:** MEDIUM (requires partition migration)
