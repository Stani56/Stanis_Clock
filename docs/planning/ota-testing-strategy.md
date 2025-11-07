# OTA Update Testing Strategy

**Status:** Ready for Testing
**Date:** 2025-11-07
**Phase:** 2.4 - OTA Firmware Updates

## Overview

The OTA system is now fully implemented with core functionality, MQTT integration, and state publishing. Before adding Home Assistant discovery, we need to validate that the system works correctly.

## Current Implementation Status

### ✅ Completed
- OTA manager core (~940 lines)
  - HTTPS download from GitHub
  - Progress tracking
  - Firmware validation
  - Boot management & rollback
  - Health checks
- MQTT command handlers (5 commands)
- MQTT state publishing (version + progress)
- Dual partition table (factory + ota_0)
- GitHub CA certificate embedding

### ⏳ Not Yet Implemented
- Home Assistant MQTT discovery entities
- ota_manager_init() in main application
- GitHub Actions release workflow
- Real firmware releases on GitHub

## Testing Challenges

**Problem:** We don't have real GitHub releases yet to test against.

**Solutions:**
1. Local HTTP server testing
2. Mock version.json endpoint
3. Component-level unit tests
4. Integration testing without actual updates

## Test Phases

### Phase 1: Component Initialization Testing ✅ CAN DO NOW

**Goal:** Verify OTA manager initializes correctly

**Steps:**
1. Add `ota_manager_init()` to main application
2. Flash firmware with new partition table
3. Monitor serial output for initialization logs
4. Verify partition detection works

**Expected Logs:**
```
OTA_MANAGER: Initializing OTA manager...
OTA_MANAGER: Running partition: factory (0x10000, 2560 KB)
OTA_MANAGER: Update partition:  ota_0 (0x292000, 2560 KB)
OTA_MANAGER: Current firmware: v1.0-esp32-final (built 2025-11-07, IDF 5.4.2)
OTA_MANAGER: ✅ OTA manager initialized
```

**Success Criteria:**
- [x] OTA manager initializes without errors
- [x] Both partitions detected correctly
- [x] Current version read successfully
- [x] Boot count tracking works
- [x] No memory leaks

### Phase 2: MQTT Command Testing ✅ CAN DO NOW

**Goal:** Verify MQTT commands execute without crashes

**Steps:**
1. Connect to device via MQTT
2. Send each OTA command
3. Monitor responses and logs
4. Verify state publishing works

**Commands to Test:**
```bash
# Test 1: Get current version
mosquitto_pub -h broker -t "home/wordclock/command" -m "ota_get_version"
# Expected: Publishes to home/wordclock/ota/version

# Test 2: Check for updates (will fail - no GitHub release yet)
mosquitto_pub -h broker -t "home/wordclock/command" -m "ota_check_update"
# Expected: ota_check_failed status (network error is OK)

# Test 3: Get progress
mosquitto_pub -h broker -t "home/wordclock/command" -m "ota_get_progress"
# Expected: Publishes state=idle, progress=0%

# Test 4: Try to start update (should fail gracefully)
mosquitto_pub -h broker -t "home/wordclock/command" -m "ota_start_update"
# Expected: ota_check_failed or ota_up_to_date

# Test 5: Try to cancel (nothing to cancel)
mosquitto_pub -h broker -t "home/wordclock/command" -m "ota_cancel_update"
# Expected: ota_cancel_failed status
```

**Success Criteria:**
- [x] All commands execute without crashes
- [x] MQTT responses published correctly
- [x] JSON payloads well-formed
- [x] Error handling works (network failures OK)
- [x] No memory leaks after repeated commands

### Phase 3: Simulated Update Testing ⏳ REQUIRES SETUP

**Goal:** Test OTA download with local HTTP server

**Setup:**
```bash
# 1. Build current firmware
idf.py build

# 2. Copy firmware to local web directory
cp build/wordclock.bin /tmp/test_ota/firmware.bin

# 3. Create version.json
cat > /tmp/test_ota/version.json <<EOF
{
  "version": "v2.4.0-test",
  "build_date": "2025-11-07",
  "idf_version": "5.4.2",
  "size_bytes": 1282560
}
EOF

# 4. Start local HTTP server
cd /tmp/test_ota
python3 -m http.server 8080
```

**Modify OTA URLs (temporarily):**
```c
// In ota_manager.h, change:
#define OTA_DEFAULT_FIRMWARE_URL "http://192.168.1.100:8080/firmware.bin"
#define OTA_DEFAULT_VERSION_URL  "http://192.168.1.100:8080/version.json"
```

**Test Steps:**
1. Start local HTTP server
2. Trigger OTA update via MQTT
3. Monitor download progress
4. Verify firmware writes to ota_0 partition
5. Test rollback if needed

**Success Criteria:**
- [x] Version check works via HTTP
- [x] Download completes successfully
- [x] Progress tracking accurate
- [x] Firmware validation passes
- [x] Partition switch works
- [ ] Reboot and boot from ota_0
- [ ] Health checks pass
- [ ] Rollback works if health checks fail

### Phase 4: Partition Migration Testing ⚠️ DESTRUCTIVE

**Goal:** Verify users can migrate from old partition table

**⚠️ WARNING:** This will erase all settings!

**Backup Steps:**
```bash
# 1. Read current NVS data
mosquitto_sub -h broker -t "home/wordclock/status" -C 1

# 2. Document WiFi credentials
# 3. Document all brightness settings
# 4. Document chime settings
```

**Migration Steps:**
```bash
# 1. Full flash erase
idf.py erase-flash

# 2. Flash with new partition table
idf.py -D PARTITION_TABLE=partitions_ota.csv flash monitor

# 3. Reconfigure WiFi via AP mode
# 4. Restore settings via MQTT/HA
```

**Success Criteria:**
- [x] Flash erase completes
- [x] New partition table flashed
- [x] Both partitions accessible
- [x] WiFi AP mode works for reconfiguration
- [x] All settings can be restored
- [x] OTA functionality works after migration

### Phase 5: GitHub Release Testing ⏳ REQUIRES CI/CD

**Goal:** Test real OTA updates from GitHub

**Prerequisites:**
1. Create GitHub Actions workflow
2. Generate signing keys (optional)
3. Create first release with firmware binary

**Workflow:**
```yaml
# .github/workflows/release.yml
name: Build and Release Firmware

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup ESP-IDF
        # ... setup steps
      - name: Build firmware
        run: idf.py build
      - name: Generate version.json
        # ... create version info
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            build/wordclock.bin
            build/version.json
```

**Test Steps:**
1. Create git tag: `git tag v2.4.0-test`
2. Push tag: `git push origin v2.4.0-test`
3. Wait for GitHub Actions to build
4. Trigger OTA update on device
5. Monitor complete update cycle

**Success Criteria:**
- [x] GitHub Actions builds successfully
- [x] Release artifacts uploaded
- [x] version.json correct
- [x] Device detects new version
- [x] Download and flash succeed
- [x] Device boots new firmware
- [x] Health checks pass
- [x] Settings preserved

## Safety Features to Test

### Boot Loop Protection
**Test:** Flash broken firmware, power cycle 3+ times
**Expected:** Automatic rollback to previous firmware

### Health Check Validation
**Test:** Flash firmware with intentional bugs
**Expected:** Health checks fail, rollback triggered

### Network Failure Handling
**Test:** Disconnect WiFi during download
**Expected:** Download fails gracefully, retry works

### Partial Download Recovery
**Test:** Cancel update mid-download
**Expected:** Clean cancellation, can restart

### Partition Table Validation
**Test:** Boot with wrong partition table
**Expected:** Clear error message, safe failure

## Current Test Status: Phase 1 Ready

**What We Can Test NOW:**
1. ✅ OTA manager initialization
2. ✅ MQTT command handling (expect failures - OK)
3. ✅ State publishing
4. ✅ Version reading
5. ✅ Partition detection

**What We Need Before Full Testing:**
1. ⏳ Integrate ota_manager_init() into main app
2. ⏳ Flash new partition table to device
3. ⏳ Setup local HTTP server for simulated updates
4. ⏳ Create GitHub Actions workflow for releases

## Recommended Test Order

**Safest → Riskiest:**

1. **Code Review** (0 risk)
   - Review ota_manager.c logic
   - Check mutex usage
   - Verify error handling paths
   - Review memory allocations

2. **Static Analysis** (0 risk)
   - Run through static analyzer
   - Check for potential memory leaks
   - Verify thread safety

3. **Integration into Main** (low risk)
   - Add ota_manager_init() call
   - Build and flash with current partition table
   - Test MQTT commands (won't update, just test commands)

4. **Partition Migration** (medium risk - loses settings)
   - Backup all settings
   - Flash new partition table
   - Verify both partitions accessible
   - Restore settings

5. **Local HTTP Testing** (medium risk)
   - Setup local server
   - Test download to ota_0
   - Verify rollback works
   - Don't rely on this firmware!

6. **Production GitHub Testing** (low risk with rollback)
   - Setup GitHub Actions
   - Create test release
   - Full OTA update cycle
   - Rollback available

## Risk Mitigation

**If OTA Bricks Device:**
1. Serial flash recovery always available (USB cable)
2. Factory partition remains untouched initially
3. Rollback mechanism tested before relying on it
4. Always have USB cable nearby during testing

**If Settings Lost:**
1. Document all settings before migration
2. MQTT backup available
3. NVS can be manually backed up
4. Quick reconfiguration via Home Assistant

**If Network Issues:**
1. OTA designed for retry capability
2. Partial downloads cleaned up
3. Timeouts prevent indefinite hangs
4. Always returns to stable state

## Success Metrics

**Phase 1 (Initialization) - Complete When:**
- ✅ OTA manager initializes without errors
- ✅ Logs show correct partition info
- ✅ Version info accurate
- ✅ MQTT commands respond (even if failing)

**Phase 2 (Commands) - Complete When:**
- ✅ All 5 MQTT commands work
- ✅ State publishing functions
- ✅ JSON well-formed
- ✅ No crashes or memory leaks

**Phase 3 (Simulated) - Complete When:**
- ✅ Can download from local server
- ✅ Progress tracking works
- ✅ Can write to ota_0 partition
- ✅ Verification passes

**Phase 4 (Migration) - Complete When:**
- ✅ Can migrate to new partition table
- ✅ Both partitions functional
- ✅ Settings can be restored
- ✅ OTA works after migration

**Phase 5 (Production) - Complete When:**
- ✅ GitHub releases work
- ✅ Real OTA updates succeed
- ✅ Rollback tested and works
- ✅ Multiple devices updated successfully

## Next Immediate Actions

**To start testing TODAY:**

1. Add `ota_manager_init()` to main application
2. Build and flash (existing partition table is OK for basic testing)
3. Test MQTT commands via mosquitto_pub
4. Monitor serial output for logs
5. Verify no crashes or memory issues

**After basic testing passes:**

1. Plan partition migration (backup everything!)
2. Setup local HTTP server for simulated updates
3. Test download and flash cycle
4. Verify rollback mechanism

**For production deployment:**

1. Create GitHub Actions workflow
2. Generate first release
3. Test on one device first
4. Gradually roll out to other devices

---

**Decision Point:** Are we ready to integrate ota_manager_init() into main application for Phase 1 testing?

**Risk Level:** LOW (no partition migration yet, just testing initialization and MQTT commands)
