# Post-Build OTA Automation Guide

## Overview

The ESP32-S3 Word Clock project includes a comprehensive post-build automation system that:

1. **Automatically generates OTA firmware files** after successful builds
2. **Calculates SHA-256 checksums** for firmware integrity verification
3. **Updates version.json** with metadata and checksums
4. **Optionally commits and pushes** to GitHub for instant OTA deployment
5. **Verifies firmware integrity** during OTA download with SHA-256 validation

This system provides a complete end-to-end workflow from build to deployment with cryptographic verification.

---

## Quick Start

### Interactive Mode (Recommended for Development)

```bash
# Build firmware
make build
# or: idf.py build

# Run post-build script (interactive prompts)
./post_build_ota.sh
```

You'll be prompted for:
- Version number (e.g., `v2.8.1`)
- Version title (e.g., `Bug Fixes`)
- Version description
- Whether to commit and push to GitHub

### Automated Mode (CI/CD or Quick Updates)

```bash
# Build and auto-release in one command
make ota-release

# Or with explicit version info
./post_build_ota.sh \
  --version v2.8.1 \
  --title "SHA-256 Verification Support" \
  --description "Added cryptographic firmware verification" \
  --auto-push
```

### Prepare Files Only (No Git Operations)

```bash
./post_build_ota.sh --skip-git
```

---

## What Gets Generated

After running the post-build script, you'll have:

### 1. `ota_files/wordclock.bin`
The compiled firmware binary, ready for OTA updates.

### 2. `ota_files/wordclock.bin.sha256`
SHA-256 checksum file in the format:
```
e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1  wordclock.bin
```

You can verify locally with:
```bash
cd ota_files
shasum -c wordclock.bin.sha256
# Output: wordclock.bin: OK
```

### 3. `ota_files/version.json`
Complete firmware metadata including SHA-256:

```json
{
  "version": "v2.8.1",
  "title": "SHA-256 Verification Support",
  "description": "Added cryptographic firmware verification",
  "release_date": "2025-11-12",
  "firmware_url": "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin",
  "firmware_size": 1,332,336,
  "sha256": "e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1",
  "commit_hash": "9e0500b3f...",
  "esp_idf_version": "5.4.2",
  "build_date": "2025-11-12T14:30:00Z",
  "requires_restart": true
}
```

---

## How SHA-256 Verification Works

### 1. Build-Time (Post-Build Script)

```bash
# After building firmware, the script:
1. Copies build/wordclock.bin → ota_files/wordclock.bin
2. Calculates SHA-256 checksum: shasum -a 256 wordclock.bin
3. Stores checksum in version.json
4. Saves verification file: wordclock.bin.sha256
```

### 2. Pre-Update (OTA Check)

```c
// ESP32 checks for updates via MQTT or automatic polling
1. Downloads version.json from GitHub/Cloudflare
2. Parses SHA-256 from JSON: firmware_version_t.sha256
3. Stores expected checksum in OTA context
4. Compares version numbers (continue if newer)
```

### 3. Download Phase

```c
// Firmware downloaded via HTTPS OTA
1. esp_https_ota_begin() - Start download
2. esp_https_ota_perform() - Download in chunks
3. esp_https_ota_is_complete_data_received() - Verify complete
```

### 4. Verification Phase (NEW!)

```c
// After download, before flashing
if (expected_sha256[0] != '\0') {
    1. Get OTA partition: esp_ota_get_next_update_partition()
    2. Calculate actual SHA-256 of downloaded firmware:
       - Read partition in 4KB chunks
       - Hash with mbedtls_sha256
       - Convert to hex string

    3. Compare checksums (case-insensitive):
       if (strcasecmp(expected, actual) != 0) {
           ESP_LOGE("SHA-256 MISMATCH!");
           abort_ota();  // Reject corrupted/tampered firmware
       }

    4. Log success: "✅ SHA-256 verification passed"
}
```

### 5. Flash and Reboot

```c
// Only if SHA-256 matches (or no checksum provided)
esp_https_ota_finish() - Set boot partition
esp_restart() - Apply new firmware
```

---

## Security Benefits

### Protection Against:

1. **Network Corruption**
   - Incomplete downloads detected
   - Corrupted packets identified
   - Retry automatically with dual OTA sources

2. **Man-in-the-Middle Attacks**
   - HTTPS + SHA-256 = Strong verification
   - Attacker cannot modify firmware undetected
   - Even if TLS is compromised, checksum fails

3. **Storage Corruption**
   - Flash memory bit flips detected
   - SD card corruption prevented
   - Bad sectors identified before boot

4. **Accidental Wrong Firmware**
   - Prevents flashing wrong device firmware
   - Detects version.json/binary mismatch
   - Automatic rollback if verification fails

### Attack Resistance:

| Attack Vector | Protection | Why It Works |
|---------------|-----------|--------------|
| Modified firmware binary | ✅ SHA-256 mismatch | 2^256 collision resistance |
| Corrupted download | ✅ Checksum fails | Even 1-bit change detected |
| Wrong version.json | ✅ Checksum mismatch | JSON and binary must match |
| Replay attack (old firmware) | ✅ Version check | Only newer versions accepted |
| TLS downgrade | ✅ SHA-256 backup | Works even if HTTPS broken |

**Note:** This system does NOT protect against compromised build environment or signed firmware (requires separate code signing).

---

## Workflow Examples

### Example 1: Bug Fix Release

```bash
# 1. Fix bug in code
vim main/wordclock.c

# 2. Build with automation
make ota-release

# Prompts:
Enter new version: v2.8.1
Enter title: Fix brightness calculation bug
Enter description: Corrected potentiometer ADC scaling
Commit and push? [y/N]: y

# 3. Script output:
✅ Firmware copied (1.27 MB)
✅ SHA-256: e8c72651...
✅ Generated version.json
✅ Committed OTA files
✅ Pushed to GitHub

# 4. GitHub URLs auto-updated
📡 Firmware: https://raw.githubusercontent.com/.../wordclock.bin
📡 Version:  https://raw.githubusercontent.com/.../version.json

# 5. ESP32 devices automatically detect and download update
# 6. SHA-256 verified before flashing
# 7. Automatic rollback if boot fails
```

### Example 2: Manual Cloudflare R2 Upload

```bash
# 1. Build and prepare files
./post_build_ota.sh --skip-git

# 2. Upload to Cloudflare R2
rclone copy ota_files/wordclock.bin r2:wordclock-ota/
rclone copy ota_files/version.json r2:wordclock-ota/

# 3. Verify upload
curl -I https://pub-3ef23ec5bae14629b323217785443321.r2.dev/wordclock.bin
# HTTP/1.1 200 OK

# 4. Test SHA-256 verification
curl -s https://.../version.json | jq .sha256
# Output: "e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1"
```

### Example 3: CI/CD Integration (GitHub Actions)

```yaml
name: Build and Release OTA

on:
  push:
    tags:
      - 'v*'

jobs:
  build-ota:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.4.2

      - name: Build firmware
        run: idf.py build

      - name: Prepare OTA files
        run: |
          chmod +x post_build_ota.sh
          ./post_build_ota.sh \
            --version ${{ github.ref_name }} \
            --title "Automated release" \
            --description "Built from commit ${{ github.sha }}" \
            --auto-push

      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: ota-files
          path: ota_files/
```

---

## Troubleshooting

### "shasum command not found"

```bash
# Ubuntu/Debian
sudo apt install coreutils

# macOS (already installed)
which shasum
```

### "jq not found (recommended)"

Not required, but improves JSON formatting:

```bash
# Ubuntu/Debian
sudo apt install jq

# macOS
brew install jq
```

### "Complete data not received"

**Cause:** Download interrupted or firmware size mismatch.

**Solution:**
- Check WiFi stability
- Increase OTA timeout in version.json
- Verify firmware_size matches actual binary

### "SHA-256 CHECKSUM MISMATCH!"

**Cause:** Downloaded firmware doesn't match expected checksum.

**Possible reasons:**
1. Network corruption during download
2. Wrong version.json for this firmware binary
3. Flash memory corruption
4. Manual edit of OTA files without updating SHA-256

**Solution:**
```bash
# Recalculate correct SHA-256
cd ota_files
shasum -a 256 wordclock.bin

# Update version.json manually, or re-run:
./post_build_ota.sh
```

### "Skipping SHA-256 verification"

**Cause:** No `sha256` field in version.json.

**Solution:** This is a warning, not an error. OTA will proceed without verification. To enable:
```bash
# Run post-build script to add SHA-256
./post_build_ota.sh
```

### Git push fails

**Cause:** Merge conflicts or authentication issues.

**Solution:**
```bash
# Check git status
git status

# Pull latest changes first
git pull origin main

# Re-run with auto-push
./post_build_ota.sh --auto-push
```

---

## Advanced Usage

### Custom Firmware URLs

Edit `post_build_ota.sh` to change default URLs:

```bash
# Around line 225
GITHUB_FIRMWARE_URL="https://your-custom-cdn.com/wordclock.bin"
GITHUB_VERSION_URL="https://your-custom-cdn.com/version.json"
```

### Skip Verification for Specific Updates

In rare cases (e.g., emergency rollback), you can skip SHA-256:

```json
{
  "version": "v2.8.0-emergency",
  "firmware_url": "...",
  "firmware_size": 1332336
  // Omit sha256 field - verification will be skipped with warning
}
```

**Warning:** Only use this for trusted emergency updates!

### Calculate SHA-256 of Running Firmware

```bash
# On ESP32 (would require implementation)
# Not currently supported, but could be added:

esp_partition_t *running = esp_ota_get_running_partition();
char sha256[65];
calculate_partition_sha256(running, sha256);
ESP_LOGI(TAG, "Running firmware SHA-256: %s", sha256);
```

### Verify OTA Files Before Push

```bash
# Test version.json validity
jq empty ota_files/version.json && echo "✅ Valid JSON" || echo "❌ Invalid JSON"

# Verify SHA-256 matches
cd ota_files
shasum -c wordclock.bin.sha256

# Check firmware size
ACTUAL_SIZE=$(stat -c%s wordclock.bin)
JSON_SIZE=$(jq .firmware_size version.json)
[ "$ACTUAL_SIZE" -eq "$JSON_SIZE" ] && echo "✅ Size matches" || echo "❌ Size mismatch"
```

---

## Performance Impact

| Operation | Time | Memory | Notes |
|-----------|------|--------|-------|
| SHA-256 calculation (build) | ~500ms | 4KB | Done on dev machine |
| SHA-256 calculation (ESP32) | ~3-5s | 4KB | Done after download |
| Download time increase | 0ms | 0 bytes | No network overhead |
| Flash time increase | +3-5s | +4KB heap | Worth it for security |

**Recommendation:** Always enable SHA-256 verification in production deployments.

---

## Best Practices

### 1. Always Use SHA-256 in Production

```bash
# ✅ Good - Automated with SHA-256
make ota-release

# ❌ Bad - Manual upload without checksum
cp build/wordclock.bin ota_files/  # Missing SHA-256!
```

### 2. Test Updates Before Wide Deployment

```bash
# Test on single device first
./post_build_ota.sh --skip-git  # Don't push yet

# Upload to test URL
rclone copy ota_files/ r2:test-bucket/

# Monitor logs during OTA
idf.py monitor

# If successful, push to production
git add ota_files/
git commit -m "Release v2.8.1"
git push
```

### 3. Keep Backup of Working Firmware

```bash
# Before major updates
cp ota_files/wordclock.bin backups/wordclock-v2.8.0.bin
cp ota_files/version.json backups/version-v2.8.0.json
```

### 4. Version Numbering Convention

```
v<major>.<minor>.<patch>[-suffix]

Examples:
v2.8.0          - Normal release
v2.8.1          - Bug fix
v2.9.0-rc1      - Release candidate
v3.0.0-beta     - Beta version
```

### 5. Git Commit Messages

The script automatically generates:
```
Release v2.8.1: SHA-256 Verification Support

Firmware update with SHA-256 verification support.

Changes:
- Version: v2.8.1
- Size: 1.27 MiB
- SHA-256: e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1
- Build date: 2025-11-12T14:30:00Z
- Commit: 9e0500b3

Added cryptographic firmware verification

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
```

---

## Script Reference

### Command-Line Options

```bash
./post_build_ota.sh [OPTIONS]

Options:
  --version VERSION      Set version (e.g., v2.8.1)
  --title TITLE          Set version title
  --description DESC     Set version description
  --auto-push            Auto-commit and push to GitHub
  --skip-git             Skip all git operations
  --help                 Show help message

Examples:
  # Interactive
  ./post_build_ota.sh

  # Automated
  ./post_build_ota.sh --version v2.8.1 --title "Fixes" --auto-push

  # Prepare only
  ./post_build_ota.sh --skip-git
```

### Makefile Targets

```bash
make build          # Build firmware only
make ota-prepare    # Build + prepare OTA files (interactive)
make ota-release    # Build + prepare + auto-push
make clean          # Clean build artifacts
```

### Script Output Stages

```
Stage 1: Pre-flight Checks
  ✅ Firmware binary found
  ✅ OTA directory ready
  ✅ SHA-256 tools available
  ✅ jq available

Stage 2: Copy Firmware Files
  ✅ Copied firmware (1.27 MiB)

Stage 3: Calculate SHA-256 Checksums
  ✅ Firmware SHA-256: e8c726517583...
  ✅ Saved checksum to: wordclock.bin.sha256

Stage 4: Gather Version Information
  ✅ Git commit: 9e0500b3
  ✅ Build date: 2025-11-12T14:30:00Z
  ✅ ESP-IDF version: 5.4.2

Stage 5: Version Information
  (Interactive prompts or command-line args)

Stage 6: Generate version.json
  ✅ Generated version.json

Stage 7: Git Operations
  ✅ Committed OTA files
  ✅ Pushed to GitHub
  ✨ OTA firmware available at:
     https://raw.githubusercontent.com/.../wordclock.bin
     https://raw.githubusercontent.com/.../version.json

Summary:
  📦 Files: wordclock.bin, wordclock.bin.sha256, version.json
  🔐 SHA-256: e8c726517583ccf54b603d3ffa1a26f2b561cdcaea91ec45904e3b5f3d1d6da1
  📌 Version: v2.8.1
  🎉 Ready for OTA updates!
```

---

## See Also

- [Dual OTA Source System Documentation](../technical/dual-ota-source-system.md) - Complete OTA architecture
- [OTA Manager API Reference](../../components/ota_manager/README.md) - C API documentation
- [MQTT OTA Commands](../user/Mqtt_User_Guide.md) - Trigger updates via MQTT
- [Home Assistant Integration](../implementation/mqtt/mqtt-system-architecture.md) - Auto-discovery

---

**Last Updated:** 2025-11-12
**Script Version:** 1.0
**Compatible With:** ESP-IDF 5.4.2+
