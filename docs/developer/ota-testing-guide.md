# OTA Testing Guide - Preventing SHA-256 Verification Failures

**Last Updated:** November 2025
**Context:** After v2.10.0 SHA-256 mismatch bug (partition size vs firmware size)

## Critical Lesson Learned

**v2.10.0 Bug:** OTA manager calculated SHA-256 on full partition size (2.5MB) instead of actual firmware size (1.3MB), causing checksum mismatch during OTA updates.

**Root Cause:** ESP32-S3 dual OTA partitions retain residual data from previous firmware, which was being included in the hash calculation.

**Fix:** Modified `calculate_partition_sha256()` to accept `firmware_size` parameter and hash only the actual firmware bytes.

## Pre-Deployment Testing Workflow

### 1. Local SHA-256 Verification Test

**ALWAYS run before deploying to GitHub:**

```bash
# After building firmware
idf.py build

# Test SHA-256 calculation
make ota-test

# Or manually:
./scripts/test_ota_locally.sh
```

**What it does:**
- Calculates SHA-256 of `build/wordclock.bin` (actual firmware)
- Simulates partition with residual data (like real OTA partition)
- Hashes only `firmware_size` bytes (mimics device behavior)
- Compares both hashes - MUST match

**Expected output:**
```
================================================
OTA SHA-256 Verification Test
================================================
📦 Firmware size: 1336608 bytes
🔐 Calculated SHA-256: c5f8086e23691ddc6c09ab7ff4dcfcd30e8d9155007d09f7281938d48e642eb1
📱 Device-simulated SHA-256: c5f8086e23691ddc6c09ab7ff4dcfcd30e8d9155007d09f7281938d48e642eb1
✅ SHA-256 MATCH - OTA verification will succeed

Safe to deploy:
  ./post_build_ota.sh --auto-push
```

**If mismatch detected:**
```
❌ SHA-256 MISMATCH - OTA verification will FAIL

Expected: c5f8086e23691ddc6c09ab7ff4dcfcd30e8d9155007d09f7281938d48e642eb1
Got:      8b58a6327197814662bed4f3b8eefb1e17352872b3314272fd51fbb0bee39971

DO NOT DEPLOY - Fix SHA-256 calculation in ota_manager.c
```

### 2. Safe OTA Release Workflow

**Option A: Automated with Safety Check**
```bash
make ota-release
```
This automatically:
1. Builds firmware (`idf.py build`)
2. Runs SHA-256 verification test (`make ota-test`)
3. If test passes, runs `post_build_ota.sh --auto-push`
4. If test fails, aborts deployment

**Option B: Manual Control**
```bash
# Build
idf.py build

# Test SHA-256 locally
make ota-test

# If test passes, prepare OTA files
make ota-prepare

# Review version.json and wordclock.bin.sha256

# Push to GitHub
git add ota_files/
git commit -m "Release vX.Y.Z"
git push
```

### 3. Test Device OTA Update

**Test on non-production device first:**

```bash
# 1. Trigger OTA check via MQTT
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P <password> \
  -t home/Clock_Stani_1/command \
  -m "ota_check_update"

# 2. Monitor response
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P <password> \
  -t "home/Clock_Stani_1/ota/#" -v

# Expected:
# home/Clock_Stani_1/ota/version {"update_available": true, ...}

# 3. Start OTA update
mosquitto_pub -h broker.hivemq.com -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u StaniWirdWild -P <password> \
  -t home/Clock_Stani_1/command \
  -m "ota_start_update"

# 4. Monitor progress
# home/Clock_Stani_1/ota/progress {"percent": 50, ...}
# home/Clock_Stani_1/ota/state "verifying"  ← CRITICAL: SHA-256 check
# home/Clock_Stani_1/ota/state "success"

# 5. Verify device rebooted with new version
# Check boot logs for version banner
```

**Watch for SHA-256 verification in serial monitor:**
```
I (xxx) ota_manager: Calculating SHA-256 checksum of 1336608 bytes...
I (xxx) ota_manager: ✅ SHA-256 checksum verified successfully
I (xxx) ota_manager: Download complete, verification successful
```

**If verification fails:**
```
E (xxx) ota_manager: ❌ SHA-256 CHECKSUM MISMATCH!
E (xxx) ota_manager:    Expected: c5f8086e23691ddc6c09ab7ff4dcfcd30e8d9155007d09f7281938d48e642eb1
E (xxx) ota_manager:    Actual:   8b58a6327197814662bed4f3b8eefb1e17352872b3314272fd51fbb0bee39971
E (xxx) ota_manager: ❌ Firmware integrity check failed, aborting OTA update
```

## Unit Testing

### Run OTA Manager Unit Tests

```bash
# Build and run component tests
cd components/ota_manager
idf.py build
idf.py test

# Expected tests:
# - SHA-256 calculation uses firmware_size not partition_size
# - SHA-256 chunked reading matches direct reading
# - SHA-256 calculation handles zero firmware size
# - SHA-256 matches known test vectors
# - Regression test: v2.10.0 partition size bug
```

### Regression Test for v2.10.0 Bug

The unit tests include a specific regression test that:
1. Creates a 2.5MB partition with 1.3MB firmware + residual data
2. Calculates SHA-256 using partition_size (WRONG - v2.10.0 bug)
3. Calculates SHA-256 using firmware_size (CORRECT - v2.10.1 fix)
4. Verifies they produce different hashes (proves bug existed)

**This test MUST pass** to prevent regression of the v2.10.0 bug.

## Checklist Before Releasing New Firmware

- [ ] Firmware builds successfully (`idf.py build`)
- [ ] Local SHA-256 test passes (`make ota-test`)
- [ ] Unit tests pass (if applicable)
- [ ] Version string updated in `main/wordclock.c`
- [ ] Git tag created for version (e.g., `v2.10.3`)
- [ ] Test OTA update on non-production device first
- [ ] Monitor SHA-256 verification in serial logs
- [ ] Verify device boots with correct version banner
- [ ] Deploy to production devices

## Migration Path for Buggy Versions

**Problem:** Devices running v2.9.0 or v2.10.0 (with buggy SHA-256 code) cannot OTA update to fixed versions.

**Solution:** USB flash required for initial upgrade to v2.10.1+

**Why:** Old firmware calculates SHA-256 incorrectly, so it will reject the correctly-calculated checksum in `version.json`.

**After USB flashing v2.10.1+:** All future OTA updates work correctly via MQTT.

## Common Issues and Solutions

### Issue 1: SHA-256 Mismatch After Code Changes

**Symptom:**
```
❌ SHA-256 CHECKSUM MISMATCH!
```

**Diagnosis:**
1. Run `make ota-test` locally
2. If local test passes but device fails → Different firmware on device vs GitHub
3. If local test fails → Bug in SHA-256 calculation code

**Solution:**
- Verify `ota_files/wordclock.bin` matches `build/wordclock.bin`
- Check `post_build_ota.sh` correctly copies binary
- Ensure no manual edits to `version.json`

### Issue 2: Firmware Size Mismatch

**Symptom:**
```
W (xxx) ota_manager: Firmware size is 0, cannot verify
```

**Diagnosis:**
`ota_context.total_bytes` not set correctly by `esp_https_ota`

**Solution:**
- Verify `esp_https_ota_get_img_desc()` called before verification
- Check firmware download completed successfully
- Ensure OTA context mutex protection

### Issue 3: Partition Read Failure

**Symptom:**
```
E (xxx) ota_manager: Failed to read partition at offset XXXXX
```

**Diagnosis:**
Partition read error during SHA-256 calculation

**Solution:**
- Check partition is valid (`esp_partition_find_first()`)
- Verify partition is not corrupted
- Ensure sufficient heap for 4KB read buffer

## Testing Best Practices

1. **Always test locally before deploying** - Use `make ota-test`
2. **Test on non-production device first** - Catch issues before production
3. **Monitor serial logs during OTA** - Watch SHA-256 verification
4. **Keep USB cable handy** - For emergency recovery if OTA fails
5. **Tag every release** - Enables version tracking via `git describe`
6. **Document version changes** - Update version banner in `wordclock.c`

## Automated Testing in CI/CD (Future)

**Planned:**
```yaml
# .github/workflows/test-ota.yml
name: OTA Testing
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
      - name: Build firmware
        run: idf.py build
      - name: Test SHA-256 calculation
        run: make ota-test
      - name: Run unit tests
        run: |
          cd components/ota_manager
          idf.py test
```

## References

- [OTA Manager Component](../../components/ota_manager/README.md)
- [Post-Build Automation Guide](post-build-automation-guide.md)
- [Dual OTA Source System](../technical/dual-ota-source-system.md)
- [SHA-256 Bug Analysis](../archive/bug-reports/v2.10.0-sha256-mismatch.md) (create this)

---

**Key Takeaway:** Always verify SHA-256 calculation locally BEFORE deploying to GitHub. The 5 minutes spent running `make ota-test` can save hours of debugging failed OTA updates.
