# OTA System: Complete Analysis & Permanent Fix

**Date:** November 14, 2025
**Context:** "We run from one issue to the next - we don't really understand"
**Goal:** Thorough analysis of all aspects of the complex OTA process

---

## Executive Summary

**The Core Problem:** We have a **broken workflow** where:
1. Git tags and version numbers are created **before** code is finalized
2. Post-tag commits break the version synchronization
3. Manual interventions create state mismatches
4. The OTA comparison logic doesn't handle git-describe format correctly

**Impact:** Device shows `v2.10.6-1-g6d80f43` but GitHub has `v2.10.6`, causing "no update available" even though there ARE updates.

---

## Part 1: Understanding the Complete OTA Flow

### 1.1 The Actors and Their Roles

```
┌─────────────────────────────────────────────────────────────┐
│ DEVELOPER                                                    │
│ - Writes code                                                │
│ - Creates git tags                                           │
│ - Builds firmware                                            │
│ - Deploys to GitHub                                          │
└────────────────┬────────────────────────────────────────────┘
                 │
                 │ Pushes firmware + version.json
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ GITHUB REPOSITORY (Truth Source)                            │
│ - ota_files/wordclock.bin                                    │
│ - ota_files/version.json                                     │
│   - version: "v2.10.6"                                       │
│   - sha256: "d47c..."                                        │
│   - commit_hash: "6d80f43..."                                │
└────────────────┬────────────────────────────────────────────┘
                 │
                 │ Device fetches version.json
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ ESP32 DEVICE (Consumer)                                      │
│ - Current version: v2.10.6-1-g6d80f43                        │
│ - Compares with GitHub: v2.10.6                              │
│ - Version comparison logic decides: UPDATE or NO UPDATE      │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Git Version Format (The Root of Confusion)

**ESP-IDF uses `git describe` to generate version strings:**

```bash
git describe --always --tags --dirty
# Output: v2.10.6-1-g6d80f43
```

**Format breakdown:**
- `v2.10.6` - Most recent git tag
- `-1` - **1 commit AFTER that tag**
- `-g6d80f43` - Git commit hash (abbreviated)
- `-dirty` - (if present) Uncommitted changes exist

**The Problem:** This format tells you you're **AHEAD** of the tag, not **ON** the tag.

### 1.3 Version Comparison Logic

Current code in `ota_manager.c`:

```c
static void normalize_version(const char *version, char *normalized, size_t max_len)
{
    // Strips everything after first '-' or '+'
    // "v2.10.6-1-g6d80f43" → "v2.10.6"
    // "v2.10.7" → "v2.10.7"
}

static int compare_versions(const char *v1, const char *v2)
{
    // Compares: "v2.10.6" vs "v2.10.6"
    // Result: EQUAL (0)
    // Decision: NO UPDATE AVAILABLE ❌
}
```

**The Bug:** Normalization strips git metadata, making `v2.10.6-1-g6d80f43` equal to `v2.10.6`.

---

## Part 2: What Actually Happened (Chronological Reconstruction)

### Timeline of Events

```
Commit: 7c891be
├─ Created tag: v2.10.6
├─ Purpose: Document partition conflict fix
└─ No firmware built yet

Commit: 6d80f43 (1 commit after v2.10.6 tag)
├─ Updated version banner: "Firmware v2.10.6"
├─ Built firmware: wordclock.bin
├─ Git version string: v2.10.6-1-g6d80f43
└─ Flashed to device via USB (idf.py flash)

Commit: 8a726fc (2 commits after v2.10.6 tag)
├─ Created version.json: "version": "v2.10.6"
├─ Deployed to GitHub: ota_files/
└─ Git version string: v2.10.6-2-g8a726fc

Device State:
├─ Running commit: 6d80f43
├─ Reports version: v2.10.6-1-g6d80f43
├─ GitHub advertises: v2.10.6
└─ Comparison: "v2.10.6" == "v2.10.6" → NO UPDATE
```

### Why OTA Didn't Trigger

**Device's perspective:**
```
Current: v2.10.6-1-g6d80f43 → normalized → "v2.10.6"
GitHub:  v2.10.6              → normalized → "v2.10.6"
Result:  "v2.10.6" == "v2.10.6" → NO UPDATE AVAILABLE
```

**Reality:**
- Device runs commit `6d80f43` (SHA-256: unknown, not on device)
- GitHub has commit `8a726fc` (SHA-256: `d47c...`)
- **Different code, but version comparison says they're the same**

---

## Part 3: Root Cause Analysis

### Problem 1: **Workflow Order is Wrong**

**Current (Broken) Workflow:**
```
1. Write code
2. Create git tag (v2.10.6)
3. Make more commits (version banner update)
4. Build firmware (from commit AFTER tag)
5. Deploy to GitHub
6. Result: Tag points to old commit, firmware is from newer commit
```

**Consequence:** Tag doesn't represent the deployed firmware.

### Problem 2: **Version Comparison is Too Simplistic**

Current logic:
```
"v2.10.6-1-g6d80f43" → normalize → "v2.10.6"
"v2.10.6"             → normalize → "v2.10.6"
Result: EQUAL (but they're not!)
```

**The normalization throws away critical information:**
- `-1` means "1 commit ahead"
- `-g6d80f43` is the actual commit being compared

### Problem 3: **No Commit Hash Comparison**

`version.json` contains:
```json
{
  "version": "v2.10.6",
  "commit_hash": "6d80f432..."  // ← This is NEVER checked!
}
```

**The device never compares commit hashes**, only version strings.

### Problem 4: **Manual Interventions Break State**

- USB flashing: Writes to ota_0, updates otadata
- OTA update: Should write to ota_1, switch boot partition
- But we USB flashed AFTER creating the tag, before deploying OTA

**Result:** Device runs "v2.10.6-1" but there's no OTA path to get there.

---

## Part 4: The Permanent Fix (Comprehensive Solution)

### Fix 1: **Correct Workflow Order**

**New Workflow:**
```
1. Write ALL code (including version banners)
2. Commit everything
3. Build firmware
4. Test locally (make ota-test)
5. Create git tag (v2.10.X) ← Tag points to complete code
6. Push tag immediately
7. Rebuild (now version is clean "v2.10.X", no -1 suffix)
8. Deploy to GitHub
9. Device sees clean version match
```

**Key Principle:** Tag after code is complete, build after tagging.

### Fix 2: **Improved Version Comparison Logic**

**Option A: Parse Git Metadata (Recommended)**

```c
static int compare_versions(const char *v1, const char *v2)
{
    // Parse v2.10.6-1-g6d80f43:
    // - Base: v2.10.6
    // - Commits ahead: 1
    // - Commit hash: 6d80f43

    // If v1 has commits ahead (e.g., "-1-g"), it's NEWER than base tag
    // Compare: v2.10.6-1 > v2.10.6 → UPDATE AVAILABLE ✅
}
```

**Option B: Always Compare Commit Hashes**

```c
static bool needs_update(const char *current_version,
                         const char *available_version,
                         const char *available_commit_hash)
{
    // Extract running commit from version string
    char running_commit[16] = {0};
    extract_commit_hash(current_version, running_commit);  // g6d80f43 → 6d80f43

    // Compare with available commit hash
    if (strncmp(running_commit, available_commit_hash, 7) != 0) {
        ESP_LOGI(TAG, "Commit mismatch: %s vs %s", running_commit, available_commit_hash);
        return true;  // Different commits → UPDATE AVAILABLE ✅
    }

    // Fall back to version number comparison
    return (compare_semver(current_version, available_version) < 0);
}
```

### Fix 3: **Automated Version Banner Updates**

**Problem:** We manually edit `main/wordclock.c` to update version banners, causing post-tag commits.

**Solution:** Generate version banner from git at compile time.

**Implementation:**

Create `main/version.c.in` (template):
```c
const char *get_firmware_version_banner(void) {
    return "@PROJECT_VERSION@";
}

const char *get_firmware_description(void) {
    return "@VERSION_DESCRIPTION@";
}
```

Update `main/CMakeLists.txt`:
```cmake
# Get version from git
execute_process(
    COMMAND git describe --always --tags --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Configure version file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.c.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.c"
    @ONLY
)

# Add to sources
idf_component_register(
    SRCS "wordclock.c" "${CMAKE_CURRENT_BINARY_DIR}/version.c"
    ...
)
```

**Result:** Version banner auto-updates at build time, no manual edits needed.

### Fix 4: **Post-Build Script Improvements**

**Current Issues:**
- Prompts for user input even with `--auto-push`
- Doesn't validate that current commit matches a tag
- Allows deploying from "dirty" builds

**Improved Script:**

```bash
#!/bin/bash
# post_build_ota.sh (improved)

set -e

# Get git version
GIT_VERSION=$(git describe --always --tags --dirty)

# Validate clean state
if [[ "$GIT_VERSION" == *"-dirty"* ]]; then
    echo "❌ Error: Uncommitted changes detected"
    echo "   Commit or stash changes before deploying OTA"
    exit 1
fi

# Validate on a tag (no -N-g suffix)
if [[ "$GIT_VERSION" =~ -[0-9]+-g[0-9a-f]+ ]]; then
    echo "⚠️  Warning: Building from commit AFTER a tag"
    echo "   Current: $GIT_VERSION"
    echo ""
    echo "Recommendation:"
    echo "  1. Commit all changes"
    echo "  2. Create new tag: git tag -a v2.X.Y -m 'Description'"
    echo "  3. Rebuild and redeploy"
    read -p "Continue anyway? (y/N): " choice
    if [[ "$choice" != "y" ]]; then
        exit 1
    fi
fi

# Extract version (strip leading 'v')
VERSION="${GIT_VERSION#v}"

# Auto-generate title and description from git log
TITLE=$(git log -1 --pretty=%s)
DESCRIPTION=$(git log -1 --pretty=%b | head -1)

# Get full commit hash
COMMIT_HASH=$(git rev-parse HEAD)

# Build version.json
cat > ota_files/version.json << EOF
{
  "version": "$GIT_VERSION",
  "title": "$TITLE",
  "description": "$DESCRIPTION",
  "release_date": "$(date -u +"%Y-%m-%d")",
  "firmware_url": "https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin",
  "firmware_size": $(stat -c%s build/wordclock.bin),
  "sha256": "$(sha256sum build/wordclock.bin | cut -d' ' -f1)",
  "commit_hash": "$COMMIT_HASH",
  "esp_idf_version": "5.4.2",
  "build_date": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "requires_restart": true
}
EOF

# Copy firmware
cp build/wordclock.bin ota_files/

# Commit and push
git add ota_files/
git commit -m "OTA Release $GIT_VERSION"
git push
```

**Key Improvements:**
- Auto-detects version from git
- Validates clean state
- Warns if not on a tag
- Auto-generates title/description from commit messages
- No interactive prompts

### Fix 5: **Partition State Tracking**

**Add to `version.json`:**
```json
{
  "version": "v2.10.7",
  "target_partition": "ota_1",  // ← NEW: Expected next partition
  "previous_version": "v2.10.6",  // ← NEW: What this updates from
  ...
}
```

**Device logic:**
```c
// Check if we're already on the target
const esp_partition_t *running = esp_ota_get_running_partition();
if (strcmp(running->label, target_partition) == 0) {
    ESP_LOGI(TAG, "Already running from target partition %s", target_partition);
    return false;  // No update needed
}
```

---

## Part 5: Implementation Plan

### Phase 1: Immediate Fix (Today)

1. **Clean up current state:**
   ```bash
   # Commit any remaining changes
   git add -A
   git commit -m "Clean state before v2.10.7"

   # Create proper tag
   git tag -a v2.10.7 -m "OTA System Fix: Proper workflow + version comparison"
   git push && git push --tags
   ```

2. **Update version banner to be auto-generated:**
   - Remove hardcoded version from `main/wordclock.c`
   - Add `version.c.in` template
   - Update `CMakeLists.txt` to generate version at build time

3. **Rebuild from clean tag:**
   ```bash
   idf.py fullclean
   idf.py build
   # Version will be clean "v2.10.7" (no -1 suffix)
   ```

4. **Deploy with improved script:**
   ```bash
   ./post_build_ota.sh --auto-push
   ```

### Phase 2: Improve Version Comparison (This Week)

1. **Update `ota_manager.c`:**
   - Add `parse_git_version()` function
   - Add `extract_commit_hash()` function
   - Update `compare_versions()` to handle git-describe format
   - Add commit hash comparison as fallback

2. **Add unit tests:**
   ```c
   TEST_CASE("Version comparison handles git-describe format", "[ota]")
   {
       // v2.10.6-1 > v2.10.6
       TEST_ASSERT_GREATER_THAN(0, compare_versions("v2.10.6-1-g6d80f43", "v2.10.6"));

       // v2.10.7 > v2.10.6-1
       TEST_ASSERT_GREATER_THAN(0, compare_versions("v2.10.7", "v2.10.6-1-g6d80f43"));

       // v2.10.6 == v2.10.6
       TEST_ASSERT_EQUAL(0, compare_versions("v2.10.6", "v2.10.6"));
   }
   ```

3. **Test on device:**
   - Flash v2.10.7 via USB
   - Deploy v2.10.8 via OTA
   - Verify progress reporting works
   - Verify partition switching (ota_0 → ota_1)

### Phase 3: Automated CI/CD (Next Week)

1. **GitHub Actions workflow:**
   ```yaml
   name: OTA Release
   on:
     push:
       tags:
         - 'v*'

   jobs:
     build-and-release:
       runs-on: ubuntu-latest
       steps:
         - name: Checkout
           uses: actions/checkout@v4

         - name: Setup ESP-IDF
           uses: espressif/esp-idf-ci-action@v1

         - name: Build
           run: idf.py build

         - name: Test SHA-256
           run: ./scripts/test_ota_locally.sh

         - name: Deploy OTA
           run: ./post_build_ota.sh --auto-push
           env:
             GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
   ```

2. **Result:** Tag creation automatically triggers build and deployment.

---

## Part 6: Testing Strategy

### Test Case 1: Clean Tag Build

```bash
# Setup
git tag -a v2.11.0 -m "Test release"
idf.py build

# Verify
git describe --always --tags
# Expected: v2.11.0 (no -N-g suffix)

# Check version in binary
strings build/wordclock.bin | grep "v2.11.0"
# Expected: Firmware v2.11.0
```

### Test Case 2: OTA Update Flow

```bash
# Initial state: Device on v2.10.7, running from ota_0

# Deploy v2.11.0 to GitHub
./post_build_ota.sh --auto-push

# Trigger OTA
mosquitto_pub -t "home/Clock_Stani_1/command" -m "ota_check_update"
# Expected MQTT response:
# {
#   "current_version": "v2.10.7",
#   "available_version": "v2.11.0",
#   "update_available": true
# }

mosquitto_pub -t "home/Clock_Stani_1/command" -m "ota_start_update"
# Monitor progress:
# home/Clock_Stani_1/ota/progress: {"progress_percent": 5, ...}
# home/Clock_Stani_1/ota/progress: {"progress_percent": 10, ...}
# ...
# home/Clock_Stani_1/ota/progress: {"progress_percent": 100, ...}

# After reboot:
# home/Clock_Stani_1/ota/version:
# {
#   "current_version": "v2.11.0",
#   "running_partition": "ota_1"  // ← Switched!
# }
```

### Test Case 3: Version Comparison Edge Cases

```bash
# Test all comparison scenarios
./test_version_comparison.sh

# Test cases:
# v2.10.6 vs v2.10.7 → UPDATE AVAILABLE
# v2.10.6-1-g6d80f43 vs v2.10.6 → NO UPDATE (already ahead)
# v2.10.6 vs v2.10.6-1-g6d80f43 → UPDATE AVAILABLE (catch up to commit)
# v2.10.7 vs v2.10.6-1-g6d80f43 → UPDATE AVAILABLE (newer tag)
```

---

## Part 7: Documentation Requirements

### 1. Developer Workflow Guide

**File:** `docs/developer/OTA_RELEASE_WORKFLOW.md`

**Contents:**
- Correct git tagging procedure
- Build and test checklist
- Deployment steps
- Rollback procedure
- Common mistakes and solutions

### 2. Version Numbering Policy

**File:** `docs/developer/VERSION_NUMBERING.md`

**Contents:**
- Semantic versioning rules
- When to increment major/minor/patch
- Git tag naming convention
- Version string format explanation

### 3. Troubleshooting Guide

**File:** `docs/developer/OTA_TROUBLESHOOTING.md`

**Contents:**
- "No update available" when there should be
- Partition conflict errors
- SHA-256 mismatch errors
- Progress not showing in Home Assistant
- Device stuck on old version

---

## Part 8: Success Criteria

### Immediate Goals (This Session)

- [ ] Clean git state (no uncommitted changes)
- [ ] Proper v2.10.7 tag created
- [ ] Firmware built from tagged commit
- [ ] version.json deployed to GitHub
- [ ] Device can successfully OTA update

### Short-term Goals (This Week)

- [ ] Version comparison handles git-describe format
- [ ] Commit hash comparison implemented
- [ ] Unit tests for version comparison
- [ ] Automated version banner generation
- [ ] Improved post_build_ota.sh script

### Long-term Goals (Next Week)

- [ ] CI/CD pipeline for OTA releases
- [ ] Comprehensive documentation
- [ ] Test device successfully updates v2.10.7 → v2.11.0
- [ ] Progress reporting verified in Home Assistant
- [ ] Zero manual interventions needed for releases

---

## Conclusion

**The Real Problem:** Not a single bug, but a **systemic workflow issue**:

1. ❌ Creating tags before code is finalized
2. ❌ Building from commits that don't match tags
3. ❌ Version comparison that ignores git metadata
4. ❌ Manual version banner updates causing post-tag commits
5. ❌ No validation in deployment script

**The Solution:** Not a quick fix, but a **complete workflow redesign**:

1. ✅ Tag after code is complete
2. ✅ Build from tagged commits
3. ✅ Smart version comparison with git-describe support
4. ✅ Automated version generation
5. ✅ Validated, automated deployment

**Next Step:** Implement Phase 1 (Immediate Fix) to get to a clean, working state. Then incrementally apply Phases 2-3 to prevent future issues.

---

**Key Insight:** "We run from one issue to the next" because we were fixing symptoms (partition conflicts, NULL pointers, version mismatches) without addressing the root cause (broken release workflow). This analysis identifies and fixes the actual problem.
