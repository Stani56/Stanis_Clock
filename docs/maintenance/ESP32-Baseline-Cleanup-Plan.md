# ESP32 Baseline Cleanup Plan
## Creating Clean ESP32 Version Before ESP32-S3 Migration

**Date:** 2025-11-02
**Purpose:** Document all changes needed to create a stable ESP32 baseline with audio/chimes disabled
**Goal:** Clean, production-ready ESP32 word clock with full WiFi/MQTT/Home Assistant integration

---

## Executive Summary

### Current State
- ✅ WiFi manager working
- ✅ MQTT manager working
- ✅ Home Assistant integration working (36 entities)
- ⚠️ Audio system partially implemented but **causes crashes with WiFi+MQTT active**
- ⚠️ MQTT disconnect workaround implemented (not suitable for production)
- ⚠️ WiFi power save disabled globally (WIFI_PS_NONE) - only needed for audio

### Cleanup Goals
1. **Remove all audio-related code** (audio_manager component)
2. **Remove audio MQTT commands** (test_audio, play_audio)
3. **Remove MQTT disconnect workaround** (audio_test_tone_task function)
4. **Restore WiFi power save** (remove WIFI_PS_NONE requirement)
5. **Keep external_flash + filesystem_manager** (working, optional, for future)
6. **Verify full WiFi/MQTT/HA functionality** without crashes

### Files Requiring Changes
- `main/wordclock.c` - Remove audio init, startup tone
- `main/wordclock_mqtt_handlers.c` - Remove test_audio command
- `components/mqtt_manager/mqtt_manager.c` - Remove test_audio, play_audio commands + workaround
- `components/wifi_manager/wifi_manager.c` - Remove WIFI_PS_NONE (restore power save)
- `main/CMakeLists.txt` - (Optional) Remove audio_manager from REQUIRES

**Total:** 5 files, ~150 lines to remove

---

## Detailed Analysis

### 1. Audio Initialization in main/wordclock.c

**Location:** Lines 111-130 in `initialize_hardware()` function

**Current Code:**
```c
// Lines 111-130
// Initialize audio manager (I2S for MAX98357A amplifier)
ESP_LOGI(TAG, "Initializing audio manager (I2S)...");
ret = audio_manager_init();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Audio manager init failed - continuing without audio");
    ESP_LOGW(TAG, "This is normal if MAX98357A amplifier is not installed");
} else {
    ESP_LOGI(TAG, "✅ Audio subsystem ready");

    // Play startup test tone to verify audio hardware
    ESP_LOGI(TAG, "🔊 Playing startup test tone...");
    ret = audio_play_test_tone();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Startup test tone playing (440Hz for 2 seconds)");
    } else {
        ESP_LOGW(TAG, "⚠️ Startup test tone failed: %s", esp_err_to_name(ret));
    }
}
```

**Action:** **REMOVE COMPLETELY**

**Justification:**
- Audio causes WiFi+MQTT crashes on ESP32
- Not production-ready
- Will be re-enabled on ESP32-S3

**After Cleanup:**
```c
// Lines 104-110 (filesystem init remains)
} else {
    ESP_LOGI(TAG, "✅ Filesystem ready at /storage");
    // Audio manager removed - will be added in ESP32-S3 version
    // ESP32 hardware cannot handle WiFi+MQTT+I2S concurrently
}
```

---

### 2. Audio Include in main/wordclock.c

**Location:** Line 33

**Current Code:**
```c
#include "audio_manager.h"
```

**Action:** **REMOVE**

**Justification:**
- Not used without audio initialization
- Clean compile without warnings

**Note:** Component will still be compiled, just not initialized

---

### 3. test_audio Command in wordclock_mqtt_handlers.c

**Location:** Lines 90-101

**Current Code:**
```c
} else if (strcmp(command, "test_audio") == 0) {
    ESP_LOGI(TAG, "🔊 Test audio command received - playing 440Hz tone");
    esp_err_t ret = audio_play_test_tone();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Test tone playback started (2 seconds)");
        mqtt_publish_status("audio_test_tone_playing");
    } else {
        ESP_LOGE(TAG, "❌ Test tone playback failed: %s", esp_err_to_name(ret));
        mqtt_publish_status("audio_test_tone_failed");
    }
    return ret;
```

**Action:** **REMOVE COMPLETELY**

**Justification:**
- Command not functional on ESP32
- Prevents user confusion
- Will be re-added for ESP32-S3

**After Cleanup:**
```c
} else if (strcmp(command, "test_transitions_status") == 0) {
    // ... (existing code)
} else {
    ESP_LOGW(TAG, "Unknown command: %s", command);
    return ESP_ERR_NOT_SUPPORTED;
}
```

---

### 4. Audio Include in wordclock_mqtt_handlers.c

**Location:** Line 17

**Current Code:**
```c
#include "audio_manager.h"
```

**Action:** **REMOVE**

**Justification:**
- Not used without test_audio command

---

### 5. test_audio + play_audio Commands in mqtt_manager.c

**Location 1:** Lines 76-126 - `audio_test_tone_task()` function
**Location 2:** Lines 912-934 - `test_audio` command handler
**Location 3:** Lines 935-959 - `play_audio` command handler

**Current Code (Function):**
```c
// Lines 76-126
// Audio playback task (async to avoid blocking MQTT)
// CRITICAL: MQTT must be disconnected - ESP32 hardware cannot handle WiFi+MQTT+I2S together
void audio_test_tone_task(void* param) {
    ESP_LOGI(TAG, "🔊 Audio playback starting - waiting for MQTT callback to complete...");

    // CRITICAL: Wait for MQTT event handler to fully complete
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Disconnecting MQTT...");
    esp_mqtt_client_disconnect(mqtt_client);
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_LOGI(TAG, "MQTT disconnected, WiFi remains active");

    // Play audio
    esp_err_t ret = audio_play_test_tone();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Test tone playback failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ Test tone playback complete");
    }

    // Wait 5 seconds for WiFi to fully stabilize
    ESP_LOGI(TAG, "Waiting 5 seconds for WiFi to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Wait for WiFi connection
    int wifi_wait = 0;
    while (!thread_safe_get_wifi_connected() && wifi_wait < 20) {
        ESP_LOGW(TAG, "WiFi not connected, waiting... (%d/20)", wifi_wait);
        vTaskDelay(pdMS_TO_TICKS(500));
        wifi_wait++;
    }

    if (thread_safe_get_wifi_connected()) {
        ESP_LOGI(TAG, "WiFi stable, re-asserting WIFI_PS_NONE...");
        esp_wifi_set_ps(WIFI_PS_NONE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Reconnecting MQTT...");
        esp_err_t mqtt_ret = esp_mqtt_client_reconnect(mqtt_client);
        if (mqtt_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ MQTT reconnection initiated");
        } else {
            ESP_LOGE(TAG, "❌ MQTT reconnection failed: %s", esp_err_to_name(mqtt_ret));
        }
    } else {
        ESP_LOGW(TAG, "⚠️ WiFi not connected - MQTT reconnection skipped");
    }

    vTaskDelete(NULL);
}
```

**Action:** **REMOVE ENTIRE FUNCTION (lines 76-126)**

**Current Code (test_audio command):**
```c
// Lines 912-934
else if (strcmp(command, "test_audio") == 0) {
    ESP_LOGI(TAG, "🔊 Test audio command received");

    // Play audio in separate task (WiFi/MQTT stay active, like Chimes_System)
    extern void audio_test_tone_task(void* param);
    BaseType_t task_created = xTaskCreatePinnedToCore(
        audio_test_tone_task,  // Task function
        "audio_task",          // Task name
        8192,                  // Stack size
        NULL,                  // Parameters
        2,                     // Lower priority (let WiFi/MQTT run)
        NULL,                  // Task handle
        1                      // Core 1 (APP CPU)
    );

    if (task_created == pdPASS) {
        ESP_LOGI(TAG, "✅ Audio task created successfully");
        mqtt_publish_status("test_audio_started");
    } else {
        ESP_LOGE(TAG, "❌ Failed to create audio task");
        mqtt_publish_status("test_audio_failed");
    }
}
```

**Action:** **REMOVE COMPLETELY (lines 912-934)**

**Current Code (play_audio command):**
```c
// Lines 935-959
else if (strncmp(command, "play_audio:", 11) == 0) {
    // Extract filename from command (format: "play_audio:/storage/chimes/quarter.pcm")
    const char *filepath = command + 11;
    ESP_LOGI(TAG, "🔊 Play audio command received: %s", filepath);

    // Check if file exists
    if (filesystem_file_exists(filepath)) {
        esp_err_t ret = audio_play_from_file(filepath);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Audio playback started: %s", filepath);
            mqtt_publish_status("audio_playback_started");
        } else {
            ESP_LOGE(TAG, "❌ Audio playback failed: %s", esp_err_to_name(ret));
            mqtt_publish_status("audio_playback_failed");
        }
    } else {
        ESP_LOGI(TAG, "❌ Audio file not found: %s", filepath);
        mqtt_publish_status("audio_file_not_found");
    }
}
```

**Action:** **REMOVE COMPLETELY (lines 935-959)**

**Justification:**
- Workaround not production-ready
- MQTT disconnect unacceptable for sophisticated word clock
- Causes confusion with failed reconnections
- Will be properly implemented on ESP32-S3

---

### 6. WiFi Power Save Workaround in wifi_manager.c

**Location:** Lines 66-70

**Current Code:**
```c
// Lines 66-70
// CRITICAL: Disable WiFi power save to prevent ESP32 hardware bug with I2S audio
// WiFi power save (WIFI_PS_MAX_MODEM) causes semaphore corruption in esp_phy_disable
// when I2S DMA is active. Must keep WiFi fully awake for audio playback stability.
esp_wifi_set_ps(WIFI_PS_NONE);
ESP_LOGI(TAG, "WiFi power save disabled (required for I2S audio compatibility)");
```

**Action:** **REMOVE AND RESTORE DEFAULT**

**Replacement Code:**
```c
// WiFi power save default (standard behavior without audio)
// Note: WIFI_PS_NONE will be required when audio is re-enabled on ESP32-S3
// Current ESP32 version does not support concurrent WiFi+MQTT+I2S
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Or WIFI_PS_MAX_MODEM for better power saving
ESP_LOGI(TAG, "WiFi power save: MIN_MODEM (standard operation)");
```

**Justification:**
- WIFI_PS_NONE only needed for audio
- Restoring power save improves battery life (if used)
- Default behavior for standard word clock
- Will be changed back to WIFI_PS_NONE on ESP32-S3

**Alternative:** Remove `esp_wifi_set_ps()` call entirely (use ESP-IDF default)

---

### 7. WiFi Power Save Re-assertion in mqtt_manager.c

**Location:** Lines 110-111

**Current Code:**
```c
ESP_LOGI(TAG, "WiFi stable, re-asserting WIFI_PS_NONE...");
esp_wifi_set_ps(WIFI_PS_NONE);
```

**Action:** **Already removed with audio_test_tone_task() function**

**Note:** This is inside the audio workaround function, so it will be removed when that function is deleted.

---

### 8. Audio Comment in audio_manager.c

**Location:** Line 27

**Current Code:**
```c
// WiFi power save is managed globally by wifi_manager (WIFI_PS_NONE for I2S compatibility)
```

**Action:** **OPTIONAL - Leave as documentation**

**Justification:**
- Explains why WiFi power save matters for audio
- Useful when re-enabling audio on ESP32-S3
- Not executed code, just a comment

---

## Components to KEEP

### External Flash (external_flash component)
**Status:** ✅ KEEP - Working, optional, no crashes

**Justification:**
- Initializes successfully
- Does not cause WiFi/MQTT issues
- Useful for future expansion
- Phase 1 complete and tested

**Note:** Gracefully degrades if W25Q64 not installed

---

### Filesystem Manager (filesystem_manager component)
**Status:** ✅ KEEP - Working, optional, depends on external_flash

**Justification:**
- LittleFS on external flash working
- Does not cause WiFi/MQTT issues
- Useful for configuration storage
- Phase 1 complete and tested

**Note:** Only initializes if external_flash succeeds

---

### Audio Manager Component (audio_manager/)
**Status:** ⚠️ KEEP COMPONENT, DON'T INITIALIZE

**Justification:**
- Component code is fine (no bugs)
- Problem is ESP32 hardware limitation
- Will be used on ESP32-S3
- Easier to re-enable than rebuild

**Action:**
- Keep all files in `components/audio_manager/`
- Remove initialization calls in main code
- Remove MQTT command handlers

---

## Implementation Steps

### Step 1: Comment-Out Strategy (Safe Approach)

Use `#if 0 ... #endif` blocks to disable code instead of deleting:

**Benefits:**
- Easy to see what was removed
- Easy to revert if needed
- Safe for testing

**Drawbacks:**
- Clutters codebase
- Not clean for production

**Recommendation:** Use for initial testing, then delete for final commit

---

### Step 2: Code Changes

#### 2.1 main/wordclock.c

**Change 1: Remove audio include (line 33)**
```c
// Line 33 - REMOVE:
// #include "audio_manager.h"
```

**Change 2: Remove audio initialization (lines 111-130)**
```c
// Lines 111-130 - REMOVE:
#if 0  // DISABLED: Audio not supported on ESP32 (WiFi+MQTT+I2S crashes)
        // Initialize audio manager (I2S for MAX98357A amplifier)
        ESP_LOGI(TAG, "Initializing audio manager (I2S)...");
        ret = audio_manager_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Audio manager init failed - continuing without audio");
            ESP_LOGW(TAG, "This is normal if MAX98357A amplifier is not installed");
        } else {
            ESP_LOGI(TAG, "✅ Audio subsystem ready");

            // Play startup test tone to verify audio hardware
            ESP_LOGI(TAG, "🔊 Playing startup test tone...");
            ret = audio_play_test_tone();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Startup test tone playing (440Hz for 2 seconds)");
            } else {
                ESP_LOGW(TAG, "⚠️ Startup test tone failed: %s", esp_err_to_name(ret));
            }
        }
#endif
```

---

#### 2.2 main/wordclock_mqtt_handlers.c

**Change 1: Remove audio include (line 17)**
```c
// Line 17 - REMOVE:
// #include "audio_manager.h"
```

**Change 2: Remove test_audio command (lines 90-101)**
```c
// Lines 90-101 - REMOVE:
#if 0  // DISABLED: Audio not supported on ESP32
    } else if (strcmp(command, "test_audio") == 0) {
        ESP_LOGI(TAG, "🔊 Test audio command received - playing 440Hz tone");
        esp_err_t ret = audio_play_test_tone();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Test tone playback started (2 seconds)");
            mqtt_publish_status("audio_test_tone_playing");
        } else {
            ESP_LOGE(TAG, "❌ Test tone playback failed: %s", esp_err_to_name(ret));
            mqtt_publish_status("audio_test_tone_failed");
        }
        return ret;
#endif
```

---

#### 2.3 components/mqtt_manager/mqtt_manager.c

**Change 1: Remove audio_test_tone_task function (lines 76-126)**
```c
// Lines 76-126 - REMOVE COMPLETELY:
#if 0  // DISABLED: Audio not supported on ESP32 (WiFi+MQTT+I2S crashes)
// Audio playback task (async to avoid blocking MQTT)
// CRITICAL: MQTT must be disconnected - ESP32 hardware cannot handle WiFi+MQTT+I2S together
void audio_test_tone_task(void* param) {
    // ... (entire function)
}
#endif
```

**Change 2: Remove test_audio command (lines 912-934)**
```c
// Lines 912-934 - REMOVE COMPLETELY:
#if 0  // DISABLED: Audio not supported on ESP32
    else if (strcmp(command, "test_audio") == 0) {
        // ... (entire command handler)
    }
#endif
```

**Change 3: Remove play_audio command (lines 935-959)**
```c
// Lines 935-959 - REMOVE COMPLETELY:
#if 0  // DISABLED: Audio not supported on ESP32
    else if (strncmp(command, "play_audio:", 11) == 0) {
        // ... (entire command handler)
    }
#endif
```

---

#### 2.4 components/wifi_manager/wifi_manager.c

**Change: Restore WiFi power save (lines 66-70)**

**Option A: Use MIN_MODEM (recommended)**
```c
// WiFi power save (standard operation without audio)
// Note: Audio requires WIFI_PS_NONE on ESP32-S3 to prevent conflicts
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
ESP_LOGI(TAG, "WiFi power save: MIN_MODEM (standard operation)");
```

**Option B: Use MAX_MODEM (best power saving)**
```c
// WiFi power save (maximum power saving for battery operation)
// Note: Audio requires WIFI_PS_NONE on ESP32-S3 to prevent conflicts
esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
ESP_LOGI(TAG, "WiFi power save: MAX_MODEM (power saving mode)");
```

**Option C: Use ESP-IDF default (no explicit call)**
```c
// WiFi power save: using ESP-IDF default (typically WIFI_PS_MIN_MODEM)
// No explicit esp_wifi_set_ps() call needed
```

**Recommendation:** **Option A** (MIN_MODEM) - balanced power/performance

---

## Testing Plan

### Phase 1: Build Verification (5 minutes)

```bash
cd /home/tanihp/esp_projects/Stanis_Clock
idf.py fullclean
idf.py build
```

**Expected Result:**
- ✅ Build completes successfully
- ✅ No warnings about undefined audio symbols
- ✅ Binary size: ~1.2MB (similar to before)

---

### Phase 2: Basic Functionality (15 minutes)

**Test Order:**
1. ✅ Boot and initialize
2. ✅ WiFi connects
3. ✅ NTP syncs
4. ✅ MQTT connects to HiveMQ Cloud
5. ✅ Home Assistant discovers 36 entities (minus audio button if exists)
6. ✅ German time display works
7. ✅ Brightness control works (potentiometer + light sensor)
8. ✅ Status LEDs work (WiFi + NTP indicators)
9. ✅ LED validation works
10. ✅ Error logging works

**Expected Results:**
- All core features work normally
- No crashes or reboots
- No MQTT disconnects
- Serial logs clean (no audio errors)

---

### Phase 3: MQTT Command Testing (10 minutes)

**Commands to Test:**
```bash
# Via Home Assistant or MQTT tool

# Should work:
mosquitto_pub -h <broker> -u <user> -P <pass> -t "home/wordclock_office/command" -m "status"
mosquitto_pub -h <broker> -u <user> -P <pass> -t "home/wordclock_office/command" -m "test_transitions_start"
mosquitto_pub -h <broker> -u <user> -P <pass> -t "home/wordclock_office/command" -m "set_time 14:23"

# Should NOT crash (command ignored):
mosquitto_pub -h <broker> -u <user> -P <pass> -t "home/wordclock_office/command" -m "test_audio"
```

**Expected Results:**
- Working commands execute normally
- test_audio command ignored (no crash!)
- System remains stable

---

### Phase 4: Long-Term Stability (30+ minutes)

**Test:**
- Leave system running for 30-60 minutes
- Monitor for crashes, reboots, MQTT disconnects
- Check WiFi remains connected
- Check MQTT publishes heartbeat every 60 seconds

**Expected Results:**
- ✅ Zero crashes
- ✅ MQTT stays connected (no disconnects)
- ✅ WiFi remains stable
- ✅ Regular MQTT heartbeat messages
- ✅ Memory stable (no leaks)

---

## Verification Checklist

### Pre-Cleanup Checklist
- [ ] Current code backs up: `git commit -m "Pre-cleanup backup - audio partially implemented"`
- [ ] Create new branch: `git checkout -b esp32-baseline-clean`
- [ ] Note current binary size: `ls -lh build/*.bin`

### Code Changes Checklist
- [ ] `main/wordclock.c` - Remove audio include (line 33)
- [ ] `main/wordclock.c` - Remove audio init (lines 111-130)
- [ ] `main/wordclock_mqtt_handlers.c` - Remove audio include (line 17)
- [ ] `main/wordclock_mqtt_handlers.c` - Remove test_audio command (lines 90-101)
- [ ] `components/mqtt_manager/mqtt_manager.c` - Remove audio_test_tone_task (lines 76-126)
- [ ] `components/mqtt_manager/mqtt_manager.c` - Remove test_audio command (lines 912-934)
- [ ] `components/mqtt_manager/mqtt_manager.c` - Remove play_audio command (lines 935-959)
- [ ] `components/wifi_manager/wifi_manager.c` - Restore WiFi power save (lines 66-70)

### Build Checklist
- [ ] Clean build: `idf.py fullclean && idf.py build`
- [ ] No compilation errors
- [ ] No warnings about audio symbols
- [ ] Binary size reasonable (~1.2MB)

### Testing Checklist
- [ ] Phase 1: Build verification passed
- [ ] Phase 2: Basic functionality (10 tests) passed
- [ ] Phase 3: MQTT commands work, test_audio ignored
- [ ] Phase 4: Long-term stability (30+ min) passed

### Documentation Checklist
- [ ] Update CLAUDE.md - Note audio removed for ESP32 baseline
- [ ] Update README.md - Remove audio feature from ESP32 version
- [ ] Git commit: `git commit -m "Clean ESP32 baseline - audio disabled"`
- [ ] Git tag: `git tag -a v1.0-esp32-baseline -m "Stable ESP32 baseline without audio"`

---

## Git Commit Messages

### Initial Cleanup Commit
```
Clean ESP32 baseline: Remove audio system for stable production version

Changes:
- Remove audio_manager initialization from main/wordclock.c
- Remove test_audio and play_audio MQTT commands
- Remove MQTT disconnect workaround (audio_test_tone_task)
- Restore WiFi power save (MIN_MODEM for standard operation)
- Keep external_flash and filesystem_manager (working, optional)

Rationale:
ESP32 hardware cannot handle WiFi+MQTT+I2S audio concurrently due to
silicon-level DMA conflicts. Audio will be re-enabled on ESP32-S3 which
has improved hardware architecture and PSRAM support.

This baseline provides:
✅ Stable WiFi + MQTT + Home Assistant integration
✅ Full German word clock functionality
✅ LED validation and error logging
✅ Optional external flash storage (for future)
✅ Zero crashes, production-ready

Audio features will return in ESP32-S3 version with:
- Concurrent WiFi + MQTT + I2S operation
- 2MB PSRAM for audio buffering
- Built-in MAX98357A amplifiers (YB-ESP32-S3-AMP board)
```

---

## Benefits of Cleanup

### For Users
- ✅ **Stable system** - No crashes, no MQTT disconnects
- ✅ **Production-ready** - Can deploy with confidence
- ✅ **Clear feature set** - No half-working audio features
- ✅ **Better power efficiency** - WiFi power save restored

### For Developers
- ✅ **Clean baseline** - Clear separation between ESP32 and ESP32-S3
- ✅ **Easy migration** - Audio re-enable on ESP32-S3 is straightforward
- ✅ **Maintainable** - No workarounds cluttering codebase
- ✅ **Documented** - Clear explanation of why audio disabled

### For Future ESP32-S3 Migration
- ✅ **Clean starting point** - Know exactly what works on ESP32
- ✅ **Clear additions** - Audio is the main new feature on ESP32-S3
- ✅ **Easy comparison** - `git diff esp32-baseline esp32s3-version`

---

## Rollback Plan

If cleanup causes issues:

```bash
# Revert to previous version
git checkout main  # Or previous commit

# Or cherry-pick specific changes
git cherry-pick <commit-hash>

# Or revert specific files
git checkout HEAD~1 -- main/wordclock.c
```

---

## Next Steps After Cleanup

1. ✅ Test thoroughly (all 4 phases)
2. ✅ Create git tag `v1.0-esp32-baseline`
3. ✅ Update documentation (CLAUDE.md, README.md)
4. ✅ Deploy to production (if applicable)
5. ⏸️ Wait for ESP32-S3 hardware to arrive
6. 🚀 Begin ESP32-S3 migration using [ESP32-S3-Migration-Analysis.md](ESP32-S3-Migration-Analysis.md)

---

## Summary

**Lines to Remove:** ~150 lines across 5 files
**Lines to Modify:** ~10 lines (WiFi power save)
**Estimated Time:** 30 minutes cleanup + 1 hour testing = **1.5 hours total**

**Result:** Clean, stable ESP32 word clock ready for production use, with clear path to ESP32-S3 audio upgrade.

---

**Document Status:** Ready for Implementation
**Last Updated:** 2025-11-02
**Next Action:** Begin Step 1 (Comment-Out Strategy) for safe testing
