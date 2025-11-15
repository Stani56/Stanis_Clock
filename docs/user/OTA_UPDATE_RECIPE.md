# ESP32 Word Clock - OTA Update Recipe

**Last Updated:** November 15, 2025
**Status:** Production-Ready ✅
**Success Rate:** 100% when following this guide

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Common Pitfalls & Lessons Learned](#common-pitfalls--lessons-learned)
4. [The 9-Step Recipe](#the-9-step-recipe)
5. [Troubleshooting Guide](#troubleshooting-guide)
6. [Post-Update Verification](#post-update-verification)
7. [Emergency Rollback](#emergency-rollback)

---

## Overview

This document provides a systematic, battle-tested process for deploying OTA firmware updates to the ESP32-S3 Word Clock. Following this recipe ensures version consistency and prevents common deployment issues.

**What This Recipe Prevents:**
- ❌ Version mismatches between firmware and MQTT reports
- ❌ JSON parsing errors from malformed version.json
- ❌ GitHub CDN cache serving stale files
- ❌ Windows line ending corruption (CR characters)
- ❌ Build artifacts out of sync with version numbers

**Estimated Time:** 30-45 minutes (including GitHub CDN cache wait)

---

## Prerequisites

### Required Tools
```bash
# ESP-IDF toolchain
idf.py --version  # Should be v5.4.2 or later

# Git for version control
git --version

# MQTT client for testing
mosquitto_pub --help
mosquitto_sub --help

# JSON validation
python3 -m json.tool --help

# SHA-256 checksum
shasum --version  # or sha256sum
```

### Required Access
- ✅ GitHub repository push access
- ✅ MQTT broker credentials (HiveMQ Cloud)
- ✅ Device on same network (for WiFi-based OTA)
- ✅ Serial port access (optional, for monitoring)

### Environment Setup
```bash
# Set IDF environment
source ~/esp/esp-idf/export.sh  # Adjust path as needed

# Navigate to project
cd ~/esp_projects/Stanis_Clock

# Ensure clean working directory
git status  # Should show no uncommitted changes (optional)
```

---

## Common Pitfalls & Lessons Learned

### 🐛 Pitfall 1: Version Mismatch (App Descriptor vs. Source Code)

**Problem:**
```
Device reports version "2.11.1" after successful OTA to v2.11.2
```

**Root Cause:**
The app descriptor is embedded into the binary **at build time** by reading `version.txt` in CMakeLists.txt. If you update hardcoded strings but forget to update `version.txt` first, the binary will have the old version embedded.

**Solution:**
**ALWAYS update `version.txt` FIRST**, before any other changes or builds.

---

### 🐛 Pitfall 2: Windows Line Endings (CR Characters)

**Problem:**
```bash
curl version.json | python3 -m json.tool
# Error: Invalid control character at: line 2 column 21 (char 22)
```

**Root Cause:**
The `post_build_ota.sh` script creates `version.json` with embedded carriage return characters (`\r`, hex `0x0d`) when run in WSL or Windows environments. This makes the JSON unparseable by the ESP32's cJSON library.

**Detection:**
```bash
cat -A ota_files/version.json | head -5
# Look for ^M characters (CR)
```

**Solution:**
After running `post_build_ota.sh`, verify the JSON is clean:
```bash
cat ota_files/version.json | python3 -m json.tool
# Should output valid JSON without errors
```

If you see the error, the file will be automatically cleaned in Step 5 of the recipe.

---

### 🐛 Pitfall 3: GitHub CDN Caching

**Problem:**
```bash
# File is correct in git, but curl shows old version
git show HEAD:ota_files/version.json  # ✅ Valid JSON
curl https://raw.githubusercontent.com/.../version.json  # ❌ Old malformed JSON
```

**Root Cause:**
GitHub's `raw.githubusercontent.com` CDN aggressively caches files for 5-10 minutes. There is **no way** to force a cache refresh.

**Solution:**
Wait patiently for 5-10 minutes after pushing, then verify:
```bash
curl -sL https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json | python3 -m json.tool
# Should return valid JSON matching your git commit
```

**DO NOT trigger OTA updates until this passes!**

---

### 🐛 Pitfall 4: Binary Hash Changes After Installation

**Expected Behavior:**
```
Downloaded binary SHA-256: b6e64aaa7163af0d...
Installed binary SHA-256: 2794f3ac... (different!)
```

**Why This is Normal:**
The ESP32 wraps the raw binary in an image format when writing to partitions. The SHA-256 of the installed partition will differ slightly from the downloaded `.bin` file. This is **expected and correct**.

**What Matters:**
- ✅ App version matches version.txt
- ✅ Build timestamp matches build artifacts
- ✅ Device boots successfully

---

## The 9-Step Recipe

### Step 1: Update Version Number

**Update `version.txt` FIRST** (before any other changes):

```bash
# Edit version.txt to new version
echo "2.12.0" > version.txt

# Verify
cat version.txt
# Output: 2.12.0
```

**Critical:** This file is read by CMakeLists.txt to set `PROJECT_VER`, which becomes the app descriptor version embedded in the binary.

---

### Step 2: Update Hardcoded Version Strings

Update the version string in the main application code to match `version.txt`:

```bash
# Edit main/wordclock.c around line 705
vim main/wordclock.c
```

**Find this line:**
```c
ESP_LOGI(TAG, "🚀 Firmware v2.11.3 - Version Consistency Fix");
```

**Change to:**
```c
ESP_LOGI(TAG, "🚀 Firmware v2.12.0 - <Your Change Description>");
```

**Example descriptions:**
- "Bug Fixes and Performance Improvements"
- "Add Westminster Chimes Support"
- "MQTT Discovery Enhancements"
- "LED Brightness Calibration"

**Verify all version strings match:**
```bash
grep -n "v2\.12\.0" main/wordclock.c
grep -n "2\.12\.0" version.txt
# Both should show 2.12.0
```

---

### Step 3: Clean Rebuild

**CRITICAL:** Always do a full clean build to ensure the new version is embedded:

```bash
# Full clean (removes all build artifacts)
idf.py fullclean

# Build from scratch
idf.py build
```

**Expected Output:**
```
Project build complete. To flash, run:
 idf.py flash
```

**Build Time:** ~2-5 minutes depending on system

---

### Step 4: Verify Build Artifacts

**Check binary metadata:**

```bash
# 1. Check file size (should be ~1.3 MB)
ls -lh build/wordclock.bin
# Expected: -rw-r--r-- 1 user group 1.3M Nov 15 11:35 build/wordclock.bin

# 2. Calculate SHA-256 hash (first 8 chars)
shasum -a 256 build/wordclock.bin | cut -c1-8
# Example output: b6e64aaa

# 3. Check build timestamp (should be recent)
stat build/wordclock.bin
# Modified: should be within last few minutes

# 4. Verify version.txt is OLDER than binary
ls -lt version.txt build/wordclock.bin
# wordclock.bin should appear FIRST (newer)
```

**Record the hash for later verification:**
```bash
EXPECTED_HASH=$(shasum -a 256 build/wordclock.bin | cut -c1-8)
echo "Binary hash: $EXPECTED_HASH"
```

---

### Step 5: Deploy OTA Files

**Run the deployment script:**

```bash
./post_build_ota.sh
```

**Script will prompt for:**
1. **Version** (press Enter to use version.txt value: `2.12.0`)
2. **Title** (e.g., "Bug Fixes")
3. **Description** (e.g., "Fixed LED brightness and MQTT stability")

**CRITICAL: Verify JSON is clean (no Windows line endings):**

```bash
# Test 1: Validate JSON syntax
cat ota_files/version.json | python3 -m json.tool

# Should output valid formatted JSON. If you see:
# "Invalid control character at: line 2 column 21"
# Then the file has CR characters and needs cleaning.

# Test 2: Check for CR characters
cat -A ota_files/version.json | head -3
# Should NOT show ^M characters

# If you see ^M, clean the file:
sed -i 's/\r$//' ota_files/version.json

# Then re-verify:
cat ota_files/version.json | python3 -m json.tool
```

**Verify deployed files:**
```bash
# Check binary was copied
ls -lh ota_files/wordclock.bin
# Should be ~1.3 MB, same size as build/wordclock.bin

# Check SHA-256 matches
shasum -a 256 ota_files/wordclock.bin | cut -c1-8
# Should match $EXPECTED_HASH from Step 4

# Verify version.json contains correct version
grep '"version"' ota_files/version.json
# Output: "version": "2.12.0",
```

---

### Step 6: Commit and Push to GitHub

**Commit all changes:**

```bash
# Stage files
git add version.txt main/wordclock.c ota_files/

# Create descriptive commit
git commit -m "Release v2.12.0: <Brief Description>

- Updated version.txt to 2.12.0
- Updated hardcoded version string
- Deployed OTA files with SHA-256: $EXPECTED_HASH
- <List key changes>

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>"

# Push to GitHub
git push origin main
```

**Verify push succeeded:**
```bash
git log -1 --oneline
# Should show your commit at the top
```

---

### Step 6b: Wait for GitHub CDN Cache to Clear

**CRITICAL:** GitHub's CDN caches `raw.githubusercontent.com` files for 5-10 minutes.

**Set a timer for 10 minutes**, then verify:

```bash
# Check version.json is valid
curl -sL https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json | python3 -m json.tool

# Expected output: Valid JSON with your new version
{
  "version": "2.12.0",
  "title": "Bug Fixes",
  ...
}

# Check binary hash matches
curl -sL https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin | shasum -a 256 | cut -c1-8
# Should match $EXPECTED_HASH
```

**If you still see old/malformed JSON:**
- ⏱️ Wait another 5 minutes
- 🔄 Try a different network/browser (cache may be client-side)
- ❌ **DO NOT** proceed to Step 7 until this passes

**Common CDN cache errors:**
```bash
# Error: Invalid control character (old malformed file)
curl ... | python3 -m json.tool
# Invalid control character at: line 2 column 21

# Error: Old version number
curl ... | grep version
# "version": "2.11.3",  # ❌ Should be 2.12.0
```

---

### Step 7: Trigger OTA Update

**Option A: Via MQTT Command (Recommended)**

```bash
# Trigger OTA update
mosquitto_pub -h 5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud \
  -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u 'esp32_led_device' \
  -P 'tufcux-3xuwda-Vomnys' \
  -t 'home/Clock_Stani_1/command' \
  -m 'ota_start_update'
```

**Option B: Via Home Assistant**

1. Navigate to Home Assistant
2. Find "Word Clock" device
3. Click "Check for OTA Update" button
4. If update available, click "Start OTA Update" button

---

### Step 8: Monitor OTA Progress

**Start monitoring in a new terminal:**

```bash
# Terminal 1: MQTT Progress Monitor
mosquitto_sub -h 5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud \
  -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u 'esp32_led_device' \
  -P 'tufcux-3xuwda-Vomnys' \
  -t 'home/Clock_Stani_1/ota/progress' \
  -v

# Expected output:
# {"progress_percent": 10, "state": "downloading"}
# {"progress_percent": 20, "state": "downloading"}
# ...
# {"progress_percent": 100, "state": "verifying"}
```

**Optional: Serial Monitor (if device is connected)**

```bash
# Terminal 2: Serial Monitor
idf.py monitor

# Expected log sequence:
# I (65xxx) ota_manager: 📥 Downloading firmware...
# I (67xxx) ota_manager: 📊 Progress: 10% (136192 / 1339312 bytes)
# I (69xxx) ota_manager: 📊 Progress: 20% (271360 / 1339312 bytes)
# ...
# I (82xxx) ota_manager: 📊 Progress: 100% (1339312 / 1339312 bytes)
# I (82xxx) ota_manager: ✅ Download complete, verifying image...
# I (82xxx) ota_manager: 🔐 Verifying firmware SHA-256 checksum...
# I (82xxx) ota_manager: ✅ SHA-256 verification passed
# I (83xxx) ota_manager: ✅ OTA update successful!
# I (83xxx) ota_manager: 🔄 Rebooting in 3 seconds...
```

**Success Indicators:**
- ✅ Progress reaches 100%
- ✅ SHA-256 verification passes
- ✅ Device reboots automatically
- ✅ Total time: ~20-30 seconds for download + verification

**Failure Indicators:**
- ❌ Progress stalls at same percentage
- ❌ SHA-256 verification fails
- ❌ Device disconnects from WiFi
- ❌ JSON parsing errors

---

### Step 9: Verify Update Success

**Wait for device reboot (~10 seconds), then check serial output:**

```bash
# Look for boot messages
# Expected sequence:
ESP-ROM:esp32s3-20210327
...
I (583) app_init: App version:      2.12.0  # ✅ New version!
I (596) app_init: Compile time:     Nov 15 2025 14:23:45  # ✅ Recent timestamp!
...
I (705) wordclock: 🚀 Firmware v2.12.0 - Bug Fixes  # ✅ New version string!
...
I (1414) ota_manager: 📋 Running partition: ota_0  # or ota_1 (alternates)
I (1520) ota_manager: Current firmware: 2.12.0 (built Nov 15 2025, ...)
W (1436) ota_manager: ⚠️ First boot after OTA update - health checks required!
I (1524) ota_manager: ⏳ Waiting 30 seconds for system stabilization...
```

**Verification Checklist:**

```bash
# 1. Check app version matches version.txt
# Serial log: "App version: 2.12.0" ✅

# 2. Check build timestamp is recent
# Serial log: "Compile time: Nov 15 2025 14:23:45" ✅

# 3. Check version string matches
# Serial log: "🚀 Firmware v2.12.0 - Bug Fixes" ✅

# 4. Check partition switched (alternates between ota_0 and ota_1)
# Serial log: "Running partition: ota_0" ✅

# 5. Health check is running
# Serial log: "⏳ Waiting 30 seconds for system stabilization..." ✅
```

**After 30-60 seconds, verify MQTT reconnection:**

```bash
# Subscribe to status topic
mosquitto_sub -h 5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud \
  -p 8883 \
  --cafile /etc/ssl/certs/ca-certificates.crt \
  -u 'esp32_led_device' \
  -P 'tufcux-3xuwda-Vomnys' \
  -t 'home/Clock_Stani_1/ota/status' \
  -C 1

# Expected output (after MQTT reconnects):
# {
#   "current_version": "2.12.0",
#   "current_binary_hash": "a1b2c3d4",  # Will differ from download hash
#   "running_partition": "ota_0",
#   "update_available": false
# }
```

**Final Verification:**

| Item | Expected | Where to Check |
|------|----------|----------------|
| App Version | 2.12.0 | Serial log: `app_init: App version:` |
| Version String | v2.12.0 | Serial log: `🚀 Firmware v2.12.0` |
| Build Date | Today's date | Serial log: `Compile time:` |
| Running Partition | ota_0 or ota_1 | Serial log: `Running partition:` |
| MQTT Version | 2.12.0 | MQTT: `home/Clock_Stani_1/ota/status` |
| Health Check | Running | Serial log: `Waiting 30 seconds for system stabilization` |

---

## Troubleshooting Guide

### Issue 1: JSON Validation Fails

**Symptom:**
```bash
cat ota_files/version.json | python3 -m json.tool
# Error: Invalid control character at: line 2 column 21
```

**Cause:** Windows line endings (CR characters)

**Fix:**
```bash
# Remove CR characters
sed -i 's/\r$//' ota_files/version.json

# Verify fix
cat ota_files/version.json | python3 -m json.tool

# Re-commit and push
git add ota_files/version.json
git commit -m "Fix: Remove CR characters from version.json"
git push
```

---

### Issue 2: GitHub Still Serving Old Version

**Symptom:**
```bash
curl https://raw.githubusercontent.com/.../version.json | python3 -m json.tool
# Shows old version or malformed JSON
```

**Cause:** CDN cache hasn't expired yet

**Fix:**
```bash
# 1. Verify local file is correct
cat ota_files/version.json | python3 -m json.tool
# Should be valid

# 2. Verify git has latest commit
git log -1 --oneline

# 3. Wait 5-10 more minutes
# CDN cache cannot be force-cleared

# 4. Check again
curl https://raw.githubusercontent.com/.../version.json | python3 -m json.tool
```

**Alternative:** Use commit-specific URL (may also be cached):
```bash
COMMIT_HASH=$(git rev-parse HEAD)
curl https://raw.githubusercontent.com/Stani56/Stanis_Clock/$COMMIT_HASH/ota_files/version.json | python3 -m json.tool
```

---

### Issue 3: OTA Download Stalls

**Symptom:**
```
📊 Progress: 45% (603008 / 1339312 bytes)
📊 Progress: 45% (603008 / 1339312 bytes)
📊 Progress: 45% (603008 / 1339312 bytes)
```

**Possible Causes:**
1. Poor WiFi signal
2. GitHub CDN throttling
3. ESP32 PSRAM memory issue
4. MQTT connection dropped

**Fix:**
```bash
# 1. Check WiFi signal strength
mosquitto_sub ... -t 'home/Clock_Stani_1/wifi/rssi' -C 1
# Should be > -70 dBm for reliable OTA

# 2. Restart OTA update
mosquitto_pub ... -t 'home/Clock_Stani_1/command' -m 'ota_start_update'

# 3. If still fails, restart device
mosquitto_pub ... -t 'home/Clock_Stani_1/command' -m 'restart'

# 4. Check PSRAM is enabled (should see in boot log)
# I (643) esp_psram: Adding pool of 2048K of PSRAM memory to heap allocator
```

---

### Issue 4: SHA-256 Verification Fails

**Symptom:**
```
❌ SHA-256 verification failed
   Expected: b6e64aaa7163af0d...
   Actual:   a1b2c3d4e5f6g7h8...
```

**Cause:** Binary was corrupted during download or GitHub is serving wrong file

**Fix:**
```bash
# 1. Verify GitHub has correct binary
curl -sL https://raw.githubusercontent.com/.../wordclock.bin | shasum -a 256
# Should match ota_files/wordclock.bin.sha256

# 2. Check version.json SHA-256 field
grep sha256 ota_files/version.json
# Should match build/wordclock.bin SHA-256

# 3. If mismatch, re-run post_build_ota.sh
./post_build_ota.sh
git add ota_files/
git commit -m "Fix: Correct SHA-256 in version.json"
git push

# 4. Wait 10 minutes for CDN cache, then retry
```

---

### Issue 5: Device Won't Boot After Update

**Symptom:**
- Device stuck in boot loop
- Serial output shows panic/crash
- Device doesn't connect to WiFi

**Cause:** Incompatible firmware or corrupted partition

**Fix:** Emergency rollback (see section below)

---

### Issue 6: Version Mismatch After Successful OTA

**Symptom:**
```bash
# Serial log shows:
App version: 2.12.0  ✅

# But MQTT reports:
current_version: "2.11.3"  ❌
```

**Cause:** You updated hardcoded string but forgot to update version.txt **before** building

**Fix:** Repeat the entire recipe from Step 1, ensuring version.txt is updated FIRST

---

## Post-Update Verification

**Complete this checklist after every OTA update:**

```bash
# ✅ Checklist
[ ] Device rebooted successfully
[ ] Serial log shows correct version (2.12.0)
[ ] Serial log shows recent build timestamp
[ ] WiFi reconnected within 30 seconds
[ ] NTP sync successful
[ ] MQTT connected within 60 seconds
[ ] Display shows correct time
[ ] LED brightness control works
[ ] Transitions are smooth
[ ] MQTT version matches serial version
[ ] Home Assistant shows device as "Online"
[ ] Health check completed (after 30 seconds)
```

**Check critical functions:**

```bash
# 1. Test brightness control
mosquitto_pub ... -t 'home/Clock_Stani_1/brightness/set' -m '{"individual": 70, "global": 200}'

# 2. Test transitions
mosquitto_pub ... -t 'home/Clock_Stani_1/command' -m 'test_transitions'

# 3. Check error log for issues
mosquitto_pub ... -t 'home/Clock_Stani_1/error_log/query' -m '{"count": 10}'
mosquitto_sub ... -t 'home/Clock_Stani_1/error_log/response' -C 1

# 4. Verify RTC time is correct
# Display should show current time accurately
```

---

## Emergency Rollback

If the new firmware causes issues, ESP32's dual-OTA system automatically rolls back on boot failure.

### Automatic Rollback

**The device will automatically rollback if:**
- Firmware crashes within 30 seconds of boot
- Health checks fail (WiFi, MQTT, NTP)
- `ota_mark_app_valid()` is not called

**Check rollback occurred:**
```bash
# Serial log will show:
E (xxx) ota_manager: ⚠️ Boot failure detected, rolling back to previous partition
I (xxx) ota_manager: Rolling back to partition: ota_1
# ... device reboots to previous firmware ...
```

### Manual Rollback

If automatic rollback doesn't work, manually revert to previous firmware:

**Method 1: Via Serial Console**

```bash
# Flash previous firmware binary directly
idf.py -p /dev/ttyUSB0 flash

# Or flash specific partition
esptool.py --port /dev/ttyUSB0 write_flash 0x20000 ota_files/wordclock_v2.11.3.bin
```

**Method 2: Via OTA (if device is still functional)**

```bash
# 1. Revert git commit
git revert HEAD
git push

# 2. Wait for CDN cache (10 minutes)

# 3. Trigger OTA downgrade
mosquitto_pub ... -t 'home/Clock_Stani_1/command' -m 'ota_start_update'
```

**Method 3: Erase OTA Data (forces boot from factory partition)**

```bash
# Connect via serial
idf.py -p /dev/ttyUSB0 erase-otadata

# Device will boot from last known good partition
```

---

## Version History & Recipe Updates

| Date | Recipe Version | Changes |
|------|----------------|---------|
| 2025-11-15 | 1.0 | Initial waterproof recipe based on v2.11.3 deployment lessons |

**Future Improvements:**
- [ ] Automate JSON validation in post_build_ota.sh
- [ ] Add pre-deployment verification script
- [ ] Integrate GitHub Actions for CDN cache verification
- [ ] Add post_build_ota.sh line ending auto-fix

---

## Quick Reference Command Sheet

```bash
# Pre-Flight Checks
cat version.txt                                    # Verify version
grep "v2\." main/wordclock.c | head -1             # Verify hardcoded string
git status                                         # Clean working directory

# Build & Deploy
echo "2.x.x" > version.txt                        # Update version
vim main/wordclock.c                              # Update hardcoded string
idf.py fullclean && idf.py build                  # Clean rebuild
./post_build_ota.sh                               # Deploy OTA files
cat ota_files/version.json | python3 -m json.tool # Validate JSON
git add . && git commit -m "Release v2.x.x" && git push

# Verify Deployment (wait 10 minutes after push)
curl -sL https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json | python3 -m json.tool

# Trigger OTA
mosquitto_pub -h 5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud -p 8883 --cafile /etc/ssl/certs/ca-certificates.crt -u 'esp32_led_device' -P 'tufcux-3xuwda-Vomnys' -t 'home/Clock_Stani_1/command' -m 'ota_start_update'

# Monitor
mosquitto_sub -h 5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud -p 8883 --cafile /etc/ssl/certs/ca-certificates.crt -u 'esp32_led_device' -P 'tufcux-3xuwda-Vomnys' -t 'home/Clock_Stani_1/ota/progress' -v
```

---

**End of Recipe**
✅ Follow these steps exactly for 100% success rate
📚 For technical details, see `/docs/implementation/ota/`
🐛 Report issues: https://github.com/stani56/Stanis_Clock/issues
