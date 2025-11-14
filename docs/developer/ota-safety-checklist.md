# OTA Safety Checklist - Preventing SHA-256 Verification Failures

**Last Updated:** November 2025
**Context:** Multi-layered prevention after v2.10.0 SHA-256 bug

## Quick Reference

**Before every OTA release:**
```bash
# 1. Build firmware
idf.py build

# 2. Test SHA-256 calculation
make ota-test

# 3. If test passes, deploy
make ota-release
```

**If anything fails → STOP and investigate!**

## Layer 1: Automated Testing (Primary Defense)

### Local SHA-256 Verification Test

**Script:** `scripts/test_ota_locally.sh`

**What it does:**
- Simulates device partition with residual data
- Calculates SHA-256 using firmware_size (like device does)
- Compares with SHA-256 of actual binary file
- **Blocks deployment if mismatch detected**

**Integrated into Makefile:**
```makefile
ota-release: build ota-test
    ./post_build_ota.sh --auto-push
```

**Result:** Cannot deploy without passing SHA-256 test

### Unit Tests

**File:** `components/ota_manager/test/test_sha256_calculation.c`

**Tests:**
1. SHA-256 with firmware smaller than partition (main bug scenario)
2. Chunked reading matches direct reading (4KB chunks)
3. Zero-size firmware handling
4. Known SHA-256 test vectors
5. **Regression test for v2.10.0 bug**

**Run tests:**
```bash
cd components/ota_manager
idf.py test
```

## Layer 2: Build System Integration

### Makefile Safety Checks

**File:** `Makefile`

**Key targets:**
- `make ota-test` - Run SHA-256 verification test
- `make ota-prepare` - Prepare OTA files (interactive)
- `make ota-release` - Build + test + deploy (automated with safety check)

**Safety mechanism:**
```makefile
ota-release: build ota-test  # ← Depends on ota-test passing
    ./post_build_ota.sh --auto-push
```

If `ota-test` fails, `ota-release` aborts before deployment.

### Post-Build Script Warning

**File:** `post_build_ota.sh`

**Added warning banner:**
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
⚠️  IMPORTANT: SHA-256 Verification
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Before deploying, ALWAYS test SHA-256 calculation:
  make ota-test
This prevents OTA failures like the v2.10.0 bug.
See: docs/archive/bug-reports/v2.10.0-sha256-mismatch.md
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Reminds developer to run test before deploying.

## Layer 3: Documentation

### OTA Testing Guide

**File:** `docs/developer/ota-testing-guide.md`

**Contents:**
- Critical lesson learned from v2.10.0 bug
- Pre-deployment testing workflow
- Safe OTA release workflow
- Test device OTA update procedure
- Unit testing instructions
- Deployment checklist
- Common issues and solutions
- Testing best practices

### Bug Report

**File:** `docs/archive/bug-reports/v2.10.0-sha256-mismatch.md`

**Contents:**
- Detailed root cause analysis
- Technical explanation of partition behavior
- Before/after code comparison
- Testing results
- Migration path for affected devices
- Lessons learned
- Prevention measures
- Timeline

### This Safety Checklist

Provides quick reference for all safety layers.

## Layer 4: Code Quality

### Fixed Code (v2.10.1+)

**File:** `components/ota_manager/ota_manager.c`

**Critical function:**
```c
static esp_err_t calculate_partition_sha256(const esp_partition_t *partition,
                                           size_t firmware_size,  // ⭐ Essential parameter
                                           char *sha256_hex)
{
    // Use firmware_size if provided, otherwise fall back to partition_size
    size_t bytes_to_hash = (firmware_size > 0) ? firmware_size : partition->size;

    ESP_LOGI(TAG, "Calculating SHA-256 checksum of %lu bytes (partition size: %lu bytes)...",
             (unsigned long)bytes_to_hash, (unsigned long)partition->size);

    // Hash only bytes_to_hash (not full partition)
    while (offset < bytes_to_hash) {
        // ...
    }
}
```

**Caller:**
```c
// Get firmware size from OTA context
uint32_t firmware_size = ota_context.total_bytes;

if (firmware_size == 0) {
    ESP_LOGE(TAG, "❌ Firmware size is 0, cannot verify");
    // Abort OTA update
}

// Pass firmware_size to hash function
ret = calculate_partition_sha256(update_partition, firmware_size, actual_sha256);
```

### Logging

**Added detailed logging:**
```
I (xxx) ota_manager: Firmware size: 1336816 bytes
I (xxx) ota_manager: Calculating SHA-256 checksum of 1336816 bytes (partition size: 2621440 bytes)...
I (xxx) ota_manager: SHA-256 checksum calculated: c5f8086e...
I (xxx) ota_manager: Expected SHA-256: c5f8086e...
I (xxx) ota_manager: ✅ SHA-256 checksum verified successfully
```

Helps diagnose any future issues.

## Deployment Checklist

### Pre-Deployment (Local)

- [ ] Code changes committed
- [ ] `idf.py build` succeeds
- [ ] `make ota-test` passes (SHA-256 verification)
- [ ] Unit tests pass (if applicable)
- [ ] Version string updated in `main/wordclock.c`
- [ ] Git tag created (e.g., `v2.10.3`)

### Deployment

- [ ] `make ota-release` succeeds
- [ ] `version.json` contains correct SHA-256
- [ ] `wordclock.bin` pushed to GitHub
- [ ] Git tag pushed to GitHub

### Post-Deployment (Test Device)

- [ ] MQTT OTA check shows update available
- [ ] Start OTA update via MQTT
- [ ] Monitor serial logs for SHA-256 verification
- [ ] Device reboots with correct version banner
- [ ] Device passes health checks
- [ ] MQTT version topic shows correct version

### Production Rollout

- [ ] Test device confirms successful update
- [ ] Deploy to remaining production devices
- [ ] Monitor for any OTA failures
- [ ] Document any issues in bug reports

## Failure Response Plan

### If `make ota-test` Fails

1. **DO NOT DEPLOY** - Fix the issue first
2. Check if firmware binary changed since build
3. Verify `calculate_partition_sha256()` logic
4. Review recent code changes to OTA manager
5. Run unit tests: `cd components/ota_manager && idf.py test`
6. Document issue in `docs/archive/bug-reports/`

### If OTA Update Fails on Device

1. Check serial monitor for error message
2. If SHA-256 mismatch:
   - Verify `version.json` SHA-256 matches `wordclock.bin.sha256`
   - Check if firmware was modified after SHA-256 calculation
   - Verify GitHub has correct binary
3. Document failure scenario
4. USB flash device to known-good version
5. Investigate root cause before next deployment

### If Multiple Devices Report Failures

1. **STOP ALL OTA UPDATES** immediately
2. Rollback GitHub ota_files to last known-good version
3. Document failure pattern
4. Fix and test locally
5. Deploy to single test device first
6. Gradual rollout after confirmation

## Success Indicators

### Local Testing
```
✅ SHA-256 MATCH - OTA verification will succeed
```

### Device OTA Update
```
I (xxx) ota_manager: ✅ SHA-256 checksum verified successfully
I (xxx) ota_manager: Download complete, verification successful
I (xxx) ota_manager: OTA update successful, restarting...
```

### Post-Reboot
```
I (xxx) wordclock: 🚀 Firmware vX.Y.Z - [Description]
```

## References

- [OTA Testing Guide](ota-testing-guide.md) - Detailed testing procedures
- [v2.10.0 Bug Report](../archive/bug-reports/v2.10.0-sha256-mismatch.md) - Root cause analysis
- [OTA Manager README](../../components/ota_manager/README.md) - Component documentation
- [Post-Build Automation Guide](post-build-automation-guide.md) - Deployment workflow
- [Dual OTA Source System](../technical/dual-ota-source-system.md) - Architecture

## Summary of Prevention Layers

| Layer | Mechanism | Effectiveness | Automation |
|-------|-----------|---------------|------------|
| **1. Local Testing** | `make ota-test` | High | Semi-automated |
| **2. Build System** | Makefile dependency | High | Fully automated |
| **3. Unit Tests** | Regression tests | Medium | Manual trigger |
| **4. Documentation** | Guides + checklists | Medium | N/A |
| **5. Code Review** | Fixed SHA-256 logic | High | N/A |
| **6. Logging** | Detailed diagnostics | Medium | Automatic |

**Defense in Depth:** Multiple independent layers catch errors before deployment.

---

**Key Principle:** Never deploy firmware without passing `make ota-test` first. The 30 seconds spent testing can prevent hours of debugging failed OTA updates.
