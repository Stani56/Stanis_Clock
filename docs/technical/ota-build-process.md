# OTA Build and Release Process

## Overview

This document describes the complete build process for creating OTA firmware releases, including binary compilation, version.json generation, and GitHub release creation.

## Table of Contents

1. [Build Environment Setup](#build-environment-setup)
2. [Partition Table Configuration](#partition-table-configuration)
3. [Firmware Binary Build](#firmware-binary-build)
4. [Version Metadata Generation](#version-metadata-generation)
5. [GitHub Release Creation](#github-release-creation)
6. [OTA Download Flow](#ota-download-flow)

---

## Build Environment Setup

### Prerequisites

```bash
# ESP-IDF 5.4.2 (or compatible)
. ~/esp/esp-idf/export.sh

# Git repository
cd /home/tanihp/esp_projects/Stanis_Clock

# Python environment (automatically activated by ESP-IDF)
# - esptool.py (v4.9.0)
# - CMake (build system)
# - Ninja (build backend)
```

### Project Structure

```
Stanis_Clock/
├── main/                       # Main application code
│   ├── wordclock.c            # Entry point
│   └── CMakeLists.txt         # Main component definition
├── components/                 # ESP-IDF components
│   ├── ota_manager/           # OTA functionality
│   ├── wifi_manager/          # WiFi connectivity
│   ├── mqtt_manager/          # MQTT client
│   └── [22 other components]
├── partitions.csv             # Partition table (OTA-enabled)
├── sdkconfig                  # ESP-IDF configuration
├── CMakeLists.txt             # Top-level build script
└── build/                     # Build output (generated)
    ├── bootloader/
    │   └── bootloader.bin     # First-stage bootloader
    ├── partition_table/
    │   └── partition-table.bin
    ├── ota_data_initial.bin   # Empty otadata partition
    └── wordclock.bin          # ⭐ Main firmware binary
```

---

## Partition Table Configuration

### File: `partitions.csv`

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
# ESP32-S3 German Word Clock - OTA-enabled partition table (16MB flash)

nvs,      data, nvs,     0x9000,  0x6000,   # NVS storage (24KB)
otadata,  data, ota,     0xf000,  0x2000,   # OTA state tracking (8KB)
phy_init, data, phy,     0x11000, 0x1000,   # PHY calibration (4KB)
ota_0,    app,  ota_0,   0x20000, 0x280000, # App partition 0 (2.5MB)
ota_1,    app,  ota_1,   0x2a0000, 0x280000, # App partition 1 (2.5MB)
storage,  data, fat,     0x520000, 0x160000, # Storage/config (1.5MB)
```

### Partition Layout Visualization

```
Flash Memory (16MB):
┌─────────────────────────────────────────────────────────┐
│ 0x0000    Bootloader (22KB)                             │
│ 0x8000    Partition Table (4KB)                         │
│ 0x9000    NVS (24KB)           ← Credentials, settings  │
│ 0xf000    otadata (8KB)        ← OTA state tracking     │
│ 0x11000   phy_init (4KB)       ← WiFi calibration       │
│ 0x20000   ota_0 (2.5MB)        ← First app partition    │
│ 0x2a0000  ota_1 (2.5MB)        ← Second app partition   │
│ 0x520000  storage (1.5MB)      ← User data              │
│ ...       [Unused] (~9MB)                               │
└─────────────────────────────────────────────────────────┘
```

### Key Design Decisions

**Why two app partitions?**
- Enables ping-pong OTA updates (ota_0 ↔ ota_1)
- Automatic rollback if new firmware fails health checks
- Can always revert to previous working version

**Why 2.5MB per partition?**
- Current firmware: 1.32MB (49% utilization)
- Headroom for future features: ~1.18MB remaining
- Must fit comfortably within 2.5MB limit

**otadata partition purpose:**
- Tracks which partition is active (ota_0 or ota_1)
- Stores partition state (PENDING_VERIFY, VALID, INVALID, ABORTED)
- Sequence numbers for rollback detection

---

## Firmware Binary Build

### Build Command

```bash
# Standard build
idf.py build

# Clean build (recommended for releases)
idf.py fullclean
idf.py build
```

### Build Process Flow

```
1. CMake Configuration Phase
   ├── Read sdkconfig (configuration)
   ├── Process CMakeLists.txt (all components)
   ├── Generate build.ninja (build instructions)
   └── Configure compiler flags

2. Component Compilation
   ├── Compile 25 custom components
   ├── Compile ESP-IDF framework components
   └── Generate static libraries (.a files)

3. Linking Phase
   ├── Link all libraries into wordclock.elf
   ├── Apply linker script (memory layout)
   └── Generate symbol map

4. Binary Generation
   ├── esptool.py: ELF → BIN conversion
   ├── Merge segments (code, data, rodata)
   ├── Add ESP32-S3 image header
   ├── Calculate SHA256 checksum
   └── Output: wordclock.bin (1,327,152 bytes)

5. Partition Table Generation
   ├── Parse partitions.csv
   ├── Generate partition-table.bin
   └── Validate offsets and sizes

6. Bootloader Build
   ├── Compile bootloader (separate build)
   ├── Generate bootloader.bin (22KB)
   └── Configure for dual-OTA support
```

### Build Output

```bash
build/
├── wordclock.bin              # Main firmware (1.32MB)
│   ├── Header: ESP32-S3 magic, segments, entry point
│   ├── Code: .text section (compiled code)
│   ├── Data: .data section (initialized variables)
│   ├── Rodata: .rodata section (constants, strings)
│   └── SHA256: Embedded checksum
│
├── wordclock.elf              # ELF file with debug symbols
├── wordclock.map              # Memory map (addresses, symbols)
├── bootloader/bootloader.bin  # First-stage bootloader
├── partition_table/partition-table.bin
└── ota_data_initial.bin       # Empty otadata (all 0xFF)
```

### Binary Structure

```
wordclock.bin format:
┌────────────────────────────────────────────────┐
│ ESP32-S3 Image Header (24 bytes)               │
│   - Magic: 0xE9 (ESP32-S3)                     │
│   - Segment count: 5                           │
│   - SPI mode: QIO                              │
│   - Flash frequency: 80MHz                     │
│   - Entry point: 0x403c8950                    │
├────────────────────────────────────────────────┤
│ Segment 0: IRAM (Code in RAM)                  │
│   - Load address: 0x3fc9d400                   │
│   - Size: 10,400 bytes                         │
├────────────────────────────────────────────────┤
│ Segment 1: Flash-mapped code                   │
│   - Virtual address: 0x42000020                │
│   - Size: 885,036 bytes (largest segment)      │
├────────────────────────────────────────────────┤
│ Segment 2: DRAM (Data in RAM)                  │
│   - Load address: 0x3fc9fca0                   │
│   - Size: 10,980 bytes                         │
├────────────────────────────────────────────────┤
│ Segment 3: Additional IRAM                     │
│   - Load address: 0x40374000                   │
│   - Size: 103,172 bytes                        │
├────────────────────────────────────────────────┤
│ Segment 4: RTC data                            │
│   - Load address: 0x600fe000                   │
│   - Size: 28 bytes                             │
├────────────────────────────────────────────────┤
│ SHA256 Checksum (32 bytes)                     │
│   - Covers all segments                        │
│   - Used for verification during OTA           │
└────────────────────────────────────────────────┘
```

### Build Verification

```bash
# Check binary size
stat -c%s build/wordclock.bin
# Output: 1327152 (1.32 MB)

# Verify partition fit
# Maximum: 2,621,440 bytes (0x280000)
# Actual:  1,327,152 bytes (49% utilization)
# Free:    1,294,288 bytes (51% headroom)

# Extract SHA256 from binary
esptool.py image_info build/wordclock.bin
# Shows: SHA256, segments, entry point

# Calculate SHA256 for version.json
sha256sum build/wordclock.bin
# Output: 2640ebd2c95fd921b9967a30199e4b3023760c627948e49af6b13a19555ca84e
```

### SHA256 Verification (Two Levels)

ESP32-S3 OTA provides **two independent layers** of SHA256 integrity checking:

#### Level 1: Automatic ESP-IDF Verification (Built-in)

**How it works:**
- During build, `idf.py` automatically appends SHA256 checksum (32 bytes) to the end of the firmware binary
- The checksum covers all segments (IRAM, Flash, DRAM, RTC data)
- `esp_https_ota_finish()` automatically verifies this embedded SHA256 before marking update as successful
- **No code changes needed** - this is standard ESP-IDF behavior

**When it runs:**
- After complete firmware download
- Before committing to new partition via `esp_ota_set_boot_partition()`
- Verification failure → automatic abort, partition not switched

**Log example:**
```
I (1234) esp_https_ota: esp_ota_write_with_offset: written 1327120 bytes
I (1235) esp_https_ota: Verifying image signature...
I (1250) esp_https_ota: ✅ Signature verified successfully
```

#### Level 2: Pre-Download SHA256 Check (Optional, Future Enhancement)

**Purpose:**
- Detect corrupted downloads **before** wasting bandwidth on full 1.3MB download
- version.json provides expected SHA256 for comparison

**Implementation strategy:**
1. Download firmware to RAM buffer or temporary partition
2. Calculate SHA256 of downloaded data
3. Compare with `firmware_sha256` from version.json
4. Only proceed with `esp_ota_write()` if match

**Trade-offs:**
- Requires additional RAM buffer (or complex streaming SHA256)
- Currently **not implemented** - relying on Level 1 automatic verification
- Future enhancement for production deployments with unreliable networks

**Current status:** Version.json SHA256 field included for future use, but not currently checked by OTA code.

---

## Version Metadata Generation

### File: `version.json`

This JSON file provides OTA clients with firmware metadata.

### Generation Script (Manual Process)

```bash
# 1. Get firmware size
FIRMWARE_SIZE=$(stat -c%s build/wordclock.bin)

# 2. Calculate SHA256 checksum
FIRMWARE_SHA256=$(sha256sum build/wordclock.bin | awk '{print $1}')

# 3. Create version.json with SHA256
cat > /tmp/github_release/version.json <<EOF
{
  "version": "2.6.3",
  "build_date": "2025-11-08",
  "description": "Phase 3 OTA - Fix fresh flash partition state handling",
  "firmware_url": "https://github.com/stani56/Stanis_Clock/releases/download/v2.6.3/wordclock.bin",
  "firmware_size": ${FIRMWARE_SIZE},
  "firmware_sha256": "${FIRMWARE_SHA256}",
  "changelog": [
    "FIX: Handle ESP_ERR_NOT_SUPPORTED in ota_mark_app_valid()",
    "FIX: Fresh flash to ota_0 partition now properly recognized",
    "FIX: Eliminates 'Validation passed but failed to mark app valid' warning",
    "ADD: ESP_OTA_IMG_UNDEFINED state handling (value -1 / 0xFFFFFFFF)",
    "ADD: Enhanced logging shows both return value and state",
    "Production-ready OTA with complete partition state management"
  ]
}
EOF
```

### JSON Schema

```json
{
  "version": "string",           // Semantic version (MAJOR.MINOR.PATCH)
  "build_date": "YYYY-MM-DD",    // ISO 8601 date
  "description": "string",       // Short description
  "firmware_url": "https://...", // Direct download URL
  "firmware_size": integer,      // Bytes (for download verification)
  "firmware_sha256": "string",   // SHA256 checksum (64 hex chars)
  "changelog": [                 // Array of change descriptions
    "string",
    "string"
  ]
}
```

**Note:** The `firmware_sha256` field is optional but recommended. It provides an additional layer of integrity checking before download. ESP-IDF's OTA system already performs automatic SHA256 verification of the downloaded binary (the firmware has SHA256 appended during build).

### Version Numbering Strategy

```
Version Format: MAJOR.MINOR.PATCH

MAJOR: Breaking changes, major features
  - Example: 1.0.0 → 2.0.0 (ESP32 → ESP32-S3 migration)

MINOR: New features, backward compatible
  - Example: 2.5.0 → 2.6.0 (Add OTA support)

PATCH: Bug fixes, no new features
  - Example: 2.6.2 → 2.6.3 (Fix partition state handling)
```

### Current Version History

```
v2.6.0 - Tiered health validation + network retry logic
v2.6.1 - Fix ota_mark_app_valid() ESP_FAIL warning
v2.6.2 - Fix dual-partition OTA support (partition table)
v2.6.3 - Fix fresh flash partition state handling (ESP_ERR_NOT_SUPPORTED)
```

---

## GitHub Release Creation

### Prerequisites

```bash
# Git tags
git tag -a v2.6.3 -m "v2.6.3 - Fix fresh flash partition state handling"
git push origin main
git push origin v2.6.3

# Prepare release files
mkdir -p /tmp/github_release
cp build/wordclock.bin /tmp/github_release/
# version.json created in previous step
```

### Manual Release Process (Web UI)

Since `gh` CLI is not available, use GitHub web interface:

1. **Navigate to releases page:**
   ```
   https://github.com/stani56/Stanis_Clock/releases/new
   ```

2. **Select tag:**
   - Choose existing tag: `v2.6.3`

3. **Fill release details:**
   - **Title:** `v2.6.3 - Fix Fresh Flash Partition State Handling`
   - **Description:** (See template below)

4. **Upload assets:**
   - `wordclock.bin` (1,327,152 bytes)
   - `version.json` (metadata)

5. **Publish release**

### Release Notes Template

```markdown
## 🔧 Bug Fix Release

This release fixes partition state handling for fresh flash scenarios.

### Fixed Issues

- **ESP_ERR_NOT_SUPPORTED Handling**: Fresh flash now properly recognized
- **Warning Elimination**: Removed false-positive validation warnings
- **State Coverage**: Added ESP_OTA_IMG_UNDEFINED handling

### Improvements

- Enhanced logging for partition state debugging
- Comprehensive coverage for all OTA scenarios
- Production-ready state management

### Technical Details

**Problem**: `esp_ota_get_state_partition()` returns `ESP_ERR_NOT_SUPPORTED`
for fresh flash because otadata partition is uninitialized.

**Solution**: Added explicit handling for fresh flash scenarios.

### Changelog

- FIX: Handle ESP_ERR_NOT_SUPPORTED in ota_mark_app_valid()
- FIX: Fresh flash to ota_0 partition now properly recognized
- ADD: ESP_OTA_IMG_UNDEFINED state handling (0xFFFFFFFF / -1)
- ADD: Enhanced debug logging

### Binary Info

- **Size**: 1,327,152 bytes (49% of partition)
- **Platform**: ESP32-S3-N16R8
- **ESP-IDF**: v5.4.2
```

### Release Assets

```
Release v2.6.3 Assets:
├── wordclock.bin
│   ├── Size: 1,327,152 bytes
│   ├── SHA256: [calculated by GitHub]
│   └── Download URL: https://github.com/.../v2.6.3/wordclock.bin
│
└── version.json
    ├── Size: ~500 bytes
    ├── Content-Type: application/json
    └── Download URL: https://github.com/.../v2.6.3/version.json
```

---

## OTA Download Flow

### Device-Side Process

```c
// 1. Check for updates
esp_http_client_config_t config = {
    .url = "https://github.com/stani56/Stanis_Clock/releases/latest/download/version.json",
    .cert_pem = github_root_ca_pem_start,
    .timeout_ms = 5000,
};

esp_http_client_handle_t client = esp_http_client_init(&config);
esp_http_client_perform(client);

// 2. Parse version.json
cJSON *root = cJSON_Parse(response_buffer);
const char *new_version = cJSON_GetObjectItem(root, "version")->valuestring;
const char *firmware_url = cJSON_GetObjectItem(root, "firmware_url")->valuestring;
int firmware_size = cJSON_GetObjectItem(root, "firmware_size")->valueint;

// 3. Compare versions
if (is_newer_version(new_version, CURRENT_VERSION)) {
    // 4. Download firmware
    esp_http_client_set_url(client, firmware_url);

    // 5. Determine target partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    // If running from ota_0 → returns ota_1
    // If running from ota_1 → returns ota_0

    // 6. Begin OTA
    esp_ota_handle_t update_handle;
    esp_ota_begin(update_partition, firmware_size, &update_handle);

    // 7. Download and write chunks
    while (bytes_remaining > 0) {
        int read = esp_http_client_read(client, buffer, BUFFER_SIZE);
        esp_ota_write(update_handle, buffer, read);
    }

    // 8. Finalize OTA
    esp_ota_end(update_handle);

    // 9. Set boot partition
    esp_ota_set_boot_partition(update_partition);
    // This writes to otadata partition:
    // - Active partition: ota_1 (or ota_0)
    // - State: ESP_OTA_IMG_PENDING_VERIFY
    // - Sequence: incremented

    // 10. Reboot
    esp_restart();
}
```

### After Reboot Flow

```
1. Bootloader reads otadata partition
   ├── Active partition: ota_1 (offset 0x2a0000)
   ├── State: ESP_OTA_IMG_PENDING_VERIFY
   └── Loads firmware from ota_1

2. Application starts
   └── ota_manager_init() creates health check task

3. Health Check Task (30s delay)
   ├── Wait for system stabilization
   ├── Check if first boot after update (NVS flag)
   ├── Run health checks (WiFi, MQTT, I2C, Memory, Partition)
   └── Decision:
       ├── All checks passed → Mark partition VALID
       ├── Critical checks failed → Trigger rollback
       └── Network failed → Retry 3x, then accept if critical OK

4. Mark Partition Valid
   ├── esp_ota_mark_app_valid_cancel_rollback()
   ├── Updates otadata: State = ESP_OTA_IMG_VALID
   └── Clears NVS first_boot flag

5. System Stable
   └── Partition marked VALID, rollback cancelled
```

### Rollback Scenario

```
If health checks fail:

1. ota_trigger_rollback()
   ├── Logs: "Triggering rollback to previous firmware"
   ├── Marks current partition as INVALID
   └── esp_restart()

2. Bootloader on reboot
   ├── Reads otadata: Current partition is INVALID
   ├── Checks previous partition (ota_0)
   ├── Sets ota_0 as boot partition
   └── Loads firmware from ota_0

3. Device boots to known-good firmware
   └── OTA update effectively reverted
```

---

## Build Automation Opportunities

### Future Enhancements

```bash
#!/bin/bash
# build-release.sh - Automated release build script

VERSION=$1  # e.g., "2.6.3"

# Clean build
idf.py fullclean
idf.py build

# Get firmware info
FIRMWARE_SIZE=$(stat -c%s build/wordclock.bin)
FIRMWARE_SHA256=$(sha256sum build/wordclock.bin | cut -d' ' -f1)
BUILD_DATE=$(date +%Y-%m-%d)

# Generate version.json with SHA256
cat > /tmp/github_release/version.json <<EOF
{
  "version": "${VERSION}",
  "build_date": "${BUILD_DATE}",
  "firmware_url": "https://github.com/stani56/Stanis_Clock/releases/download/v${VERSION}/wordclock.bin",
  "firmware_size": ${FIRMWARE_SIZE},
  "firmware_sha256": "${FIRMWARE_SHA256}",
  "changelog": [
    "See GitHub release notes"
  ]
}
EOF

# Copy binaries
cp build/wordclock.bin /tmp/github_release/

# Git operations
git add .
git commit -m "Release v${VERSION}"
git tag -a "v${VERSION}" -m "Release v${VERSION}"
git push origin main
git push origin "v${VERSION}"

echo "Build complete. Ready for GitHub release."
echo "Firmware: /tmp/github_release/wordclock.bin (${FIRMWARE_SIZE} bytes)"
echo "Metadata: /tmp/github_release/version.json"
```

### CI/CD Integration (Future)

```yaml
# .github/workflows/release.yml
name: Build Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.4.2

      - name: Build firmware
        run: |
          . $IDF_PATH/export.sh
          idf.py build

      - name: Generate version.json
        run: |
          ./scripts/generate-version-json.sh ${{ github.ref_name }}

      - name: Create release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            build/wordclock.bin
            version.json
```

---

## Summary

### Build Artifacts

| File | Size | Purpose |
|------|------|---------|
| `wordclock.bin` | 1.32 MB | Main firmware binary |
| `version.json` | ~500 B | OTA metadata |
| `bootloader.bin` | 22 KB | First-stage bootloader |
| `partition-table.bin` | 192 B | Partition layout |
| `ota_data_initial.bin` | 8 KB | Empty otadata partition |

### Build Time

- **Full clean build**: ~45 seconds
- **Incremental build**: ~5-10 seconds
- **Component count**: 25 custom + ESP-IDF framework

### Key Takeaways

1. **Dual-partition design** enables safe OTA with automatic rollback
2. **version.json** provides clients with update metadata
3. **SHA256 checksums** ensure firmware integrity
4. **Health validation** prevents broken firmware from staying active
5. **Partition states** track OTA lifecycle and enable rollback

---

**Document Version**: 1.0
**Last Updated**: 2025-11-08
**Author**: Claude Code
**Related**: [OTA Architecture](../implementation/ota/), [Partition Table](../../partitions.csv)
