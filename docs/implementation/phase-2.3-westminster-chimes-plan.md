# Phase 2.3: Westminster Chimes Implementation Plan

**Date:** November 6, 2025
**Status:** Planning Phase
**Prerequisites:** Phase 2.1 (Audio System) ✅ + Phase 2.2 (SD Card Storage) ✅

## Overview

Implement Westminster quarter-hour chimes for the German word clock, using the established audio playback system from Phases 2.1 and 2.2. This phase will add authentic clock tower chime sounds triggered by RTC time events.

## Westminster Chimes Background

###Traditional Westminster Quarter Chimes

Westminster chimes follow a specific melodic pattern based on the quarter-hour:

**Quarter Past (15 minutes):** 4 notes
```
E♭ - D♭ - C - B♭
```

**Half Past (30 minutes):** 8 notes
```
E♭ - C - D♭ - B♭,  E♭ - D♭ - C - B♭
```

**Quarter To (45 minutes):** 12 notes
```
C - E♭ - D♭ - B♭,  E♭ - C - D♭ - B♭,  E♭ - D♭ - C - B♭
```

**On the Hour (00 minutes):** 16 notes + hour strikes
```
E♭ - D♭ - C - B♭,  C - E♭ - D♭ - B♭,  E♭ - C - D♭ - B♭,  E♭ - D♭ - C - B♭
+ [number of hour strikes]
```

### Technical Specifications

- **Bell Tones:** E♭5 (622Hz), D♭5 (554Hz), C5 (523Hz), B♭4 (466Hz)
- **Note Duration:** ~1.5 seconds per note
- **Gap Between Notes:** ~0.3 seconds
- **Total Duration:**
  - Quarter past: ~7 seconds
  - Half past: ~14 seconds
  - Quarter to: ~21 seconds
  - Hour: ~28 seconds + (hour × 2 seconds)

## Phase 2.3 Goals

### Core Features

1. **Automatic Chime Playback**
   - Trigger chimes at :00, :15, :30, :45 based on RTC time
   - Play appropriate Westminster sequence for each quarter
   - Strike hour count on the hour (1-12 strikes)

2. **Audio File Management**
   - Store chime PCM files on SD card
   - Organize files in `/sdcard/chimes/` directory
   - Support multiple chime styles/sounds

3. **User Control via MQTT**
   - Enable/disable chimes
   - Set quiet hours (e.g., 22:00-07:00)
   - Test individual chime sequences
   - Adjust chime volume

4. **Integration with Word Clock**
   - Chimes play without interrupting LED display
   - Coordinate with transition system
   - Thread-safe operation

### Optional Enhancements

- Multiple chime melodies (Big Ben, Westminster, custom)
- Different sounds for day/night
- Home Assistant automation support
- Chime preview before hour change
- Volume ramping (fade in/out)

## Implementation Strategy

### Step 1: Chime Manager Component

**New Component:** `components/chime_manager/`

**Purpose:** Coordinate chime playback based on time events

**Key Functions:**
```c
esp_err_t chime_manager_init(void);
esp_err_t chime_manager_deinit(void);
esp_err_t chime_manager_enable(bool enabled);
esp_err_t chime_manager_set_quiet_hours(uint8_t start_hour, uint8_t end_hour);
esp_err_t chime_manager_play_test(chime_type_t type);  // Manual test
esp_err_t chime_manager_check_time(void);  // Called every minute
```

**Chime Types:**
```c
typedef enum {
    CHIME_QUARTER_PAST,   // 15 minutes
    CHIME_HALF_PAST,      // 30 minutes
    CHIME_QUARTER_TO,     // 45 minutes
    CHIME_HOUR,           // 00 minutes + hour strikes
    CHIME_TEST_SINGLE,    // Test single bell
} chime_type_t;
```

**Configuration Storage (NVS):**
```c
typedef struct {
    bool enabled;
    uint8_t quiet_start_hour;  // 22 = 10 PM
    uint8_t quiet_end_hour;    // 7 = 7 AM
    uint8_t volume;            // 0-100 (future: volume control)
    char chime_set[32];        // "westminster", "bigben", etc.
} chime_config_t;
```

### Step 2: Audio File Organization

**Directory Structure on SD Card:**
```
/sdcard/
├── chimes/
│   ├── westminster/
│   │   ├── quarter_past.pcm    # 4-note sequence
│   │   ├── half_past.pcm       # 8-note sequence
│   │   ├── quarter_to.pcm      # 12-note sequence
│   │   ├── hour.pcm            # 16-note sequence
│   │   └── strike.pcm          # Single hour strike
│   ├── bigben/
│   │   └── [similar structure]
│   └── custom/
│       └── [user files]
└── test.pcm
```

**File Specifications:**
- **Format:** Raw PCM, 16-bit signed little-endian, mono
- **Sample Rate:** 16000 Hz
- **Estimated Sizes:**
  - `quarter_past.pcm`: ~224 KB (7 seconds)
  - `half_past.pcm`: ~448 KB (14 seconds)
  - `quarter_to.pcm`: ~672 KB (21 seconds)
  - `hour.pcm`: ~896 KB (28 seconds)
  - `strike.pcm`: ~64 KB (2 seconds)
- **Total Storage:** ~2.3 MB per chime set

### Step 3: Time-Based Triggering

**Integration with RTC (DS3231):**

Current system already reads RTC every 5 seconds for display updates. Extend this to check for chime triggers:

```c
// In wordclock.c main loop
static uint8_t last_minute = 255;  // Track minute changes

void wordclock_main_loop(void) {
    struct tm time;
    ds3231_get_time_struct(&time);

    // Check for minute change
    if (time.tm_min != last_minute) {
        last_minute = time.tm_min;

        // Check if chimes should play
        if (time.tm_min % 15 == 0) {  // :00, :15, :30, :45
            chime_manager_check_time();
        }
    }

    // Existing display update code...
}
```

**Chime Decision Logic:**
```c
esp_err_t chime_manager_check_time(void) {
    struct tm time;
    ds3231_get_time_struct(&time);

    // Check if enabled
    if (!config.enabled) {
        return ESP_OK;
    }

    // Check quiet hours
    if (is_quiet_hour(time.tm_hour)) {
        ESP_LOGI(TAG, "Chimes suppressed during quiet hours");
        return ESP_OK;
    }

    // Determine chime type
    chime_type_t type;
    switch (time.tm_min) {
        case 0:  type = CHIME_HOUR; break;
        case 15: type = CHIME_QUARTER_PAST; break;
        case 30: type = CHIME_HALF_PAST; break;
        case 45: type = CHIME_QUARTER_TO; break;
        default: return ESP_OK;  // Not a chime minute
    }

    // Play chime
    return play_chime(type, time.tm_hour);
}
```

**Hour Strike Logic:**
```c
esp_err_t play_chime(chime_type_t type, uint8_t hour) {
    char filepath[64];

    // Play quarter chime
    snprintf(filepath, sizeof(filepath), "/sdcard/chimes/%s/%s.pcm",
             config.chime_set, get_chime_filename(type));

    esp_err_t ret = audio_play_from_file(filepath);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to play chime: %s", filepath);
        return ret;
    }

    // If hour chime, play hour strikes
    if (type == CHIME_HOUR) {
        uint8_t strikes = (hour % 12 == 0) ? 12 : (hour % 12);

        for (uint8_t i = 0; i < strikes; i++) {
            // Wait for previous audio to finish
            while (audio_is_playing()) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            // Play single strike
            snprintf(filepath, sizeof(filepath), "/sdcard/chimes/%s/strike.pcm",
                     config.chime_set);
            audio_play_from_file(filepath);

            // Gap between strikes (if not last)
            if (i < strikes - 1) {
                vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second gap
            }
        }
    }

    return ESP_OK;
}
```

### Step 4: MQTT Integration

**New MQTT Topics:**

**Control Topics (Subscribe):**
```
home/Clock_Stani_1/chimes/enable         # "true" or "false"
home/Clock_Stani_1/chimes/quiet_hours    # {"start": 22, "end": 7}
home/Clock_Stani_1/chimes/test           # "quarter_past", "half_past", "quarter_to", "hour"
home/Clock_Stani_1/chimes/set            # "westminster", "bigben", "custom"
```

**Status Topics (Publish):**
```
home/Clock_Stani_1/chimes/status         # "enabled" or "disabled"
home/Clock_Stani_1/chimes/last_played    # "2025-11-06T14:00:00Z"
home/Clock_Stani_1/chimes/config         # Full config JSON
```

**MQTT Command Handlers:**
```c
// In mqtt_manager.c
else if (strcmp(command, "chimes_enable") == 0) {
    bool enabled = (strcmp(data, "true") == 0);
    chime_manager_enable(enabled);
    mqtt_publish_status(enabled ? "chimes_enabled" : "chimes_disabled");
}
else if (strcmp(command, "chimes_test") == 0) {
    chime_type_t type = parse_chime_type(data);
    chime_manager_play_test(type);
    mqtt_publish_status("chimes_test_playing");
}
```

### Step 5: Home Assistant Discovery

**New Home Assistant Entities:**

1. **Switch: Chimes Enable/Disable**
   ```json
   {
     "name": "Chimes",
     "unique_id": "clock_stani_1_chimes_enable",
     "state_topic": "home/Clock_Stani_1/chimes/status",
     "command_topic": "home/Clock_Stani_1/chimes/enable",
     "payload_on": "true",
     "payload_off": "false"
   }
   ```

2. **Number: Quiet Hours Start**
   ```json
   {
     "name": "Quiet Hours Start",
     "unique_id": "clock_stani_1_quiet_start",
     "min": 0,
     "max": 23,
     "mode": "slider"
   }
   ```

3. **Number: Quiet Hours End**
   ```json
   {
     "name": "Quiet Hours End",
     "unique_id": "clock_stani_1_quiet_end",
     "min": 0,
     "max": 23,
     "mode": "slider"
   }
   ```

4. **Select: Chime Set**
   ```json
   {
     "name": "Chime Sound",
     "unique_id": "clock_stani_1_chime_set",
     "options": ["westminster", "bigben", "custom"]
   }
   ```

5. **Button: Test Quarter Past**
6. **Button: Test Half Past**
7. **Button: Test Quarter To**
8. **Button: Test Hour Chime**

9. **Sensor: Last Chime Played**
   ```json
   {
     "name": "Last Chime",
     "unique_id": "clock_stani_1_last_chime",
     "device_class": "timestamp"
   }
   ```

### Step 6: Audio File Generation

**Creating Westminster Chime PCM Files:**

Option 1: Generate from pure sine waves (simple, for testing)
```bash
# Quarter past (4 notes: E♭-D♭-C-B♭)
ffmpeg -f lavfi -i "sine=frequency=622:duration=1.5" -ar 16000 -ac 1 -f s16le note_eb.pcm
ffmpeg -f lavfi -i "sine=frequency=554:duration=1.5" -ar 16000 -ac 1 -f s16le note_db.pcm
ffmpeg -f lavfi -i "sine=frequency=523:duration=1.5" -ar 16000 -ac 1 -f s16le note_c.pcm
ffmpeg -f lavfi -i "sine=frequency=466:duration=1.5" -ar 16000 -ac 1 -f s16le note_bb.pcm

# Concatenate with gaps
ffmpeg -i note_eb.pcm -i note_db.pcm -i note_c.pcm -i note_bb.pcm \
       -filter_complex "[0][1][2][3]concat=n=4:v=0:a=1" \
       -ar 16000 -ac 1 -f s16le quarter_past.pcm
```

Option 2: Use bell samples (realistic, preferred)
```bash
# Download bell sound samples (WAV format)
# Convert to PCM with proper sample rate
ffmpeg -i westminster_quarter_past.wav -ar 16000 -ac 1 -f s16le quarter_past.pcm
ffmpeg -i westminster_half_past.wav -ar 16000 -ac 1 -f s16le half_past.pcm
ffmpeg -i westminster_quarter_to.wav -ar 16000 -ac 1 -f s16le quarter_to.pcm
ffmpeg -i westminster_hour.wav -ar 16000 -ac 1 -f s16le hour.pcm
ffmpeg -i westminster_strike.wav -ar 16000 -ac 1 -f s16le strike.pcm
```

Option 3: Record from actual clock tower (authentic, best quality)
- Record audio from Big Ben or similar clock
- Edit to extract clean quarter sequences
- Normalize volume and remove background noise
- Convert to 16kHz mono PCM

## Implementation Phases

### Phase 2.3a: Core Infrastructure (Week 1)

**Tasks:**
1. Create `chime_manager` component skeleton
2. Implement configuration storage (NVS)
3. Add time-based triggering logic
4. Create MQTT command handlers
5. Test with simple test tone file

**Deliverables:**
- `components/chime_manager/` component
- MQTT integration for enable/disable
- Time trigger working (logs only, no audio)

### Phase 2.3b: Audio File Integration (Week 2)

**Tasks:**
1. Create chime audio files (PCM format)
2. Organize files on SD card
3. Implement chime playback logic
4. Add hour strike counting
5. Test all four quarter variations

**Deliverables:**
- Complete set of Westminster chime PCM files
- Working playback at :00, :15, :30, :45
- Hour strikes (1-12) functional

### Phase 2.3c: Advanced Features (Week 3)

**Tasks:**
1. Implement quiet hours
2. Add Home Assistant discovery for chime entities
3. Create test buttons for manual chime triggering
4. Add chime set selection (westminster/bigben/custom)
5. Implement volume control (future enhancement)

**Deliverables:**
- Full Home Assistant integration
- Quiet hours functional
- Multiple chime sets supported
- Production-ready Phase 2.3

## Technical Challenges and Solutions

### Challenge 1: Audio Playback Duration

**Problem:** Chime sequences can be 20+ seconds. Must not block other operations.

**Solution:**
- `audio_play_from_file()` already uses DMA-based I2S transmission
- Audio plays asynchronously without blocking
- Word clock display updates continue during playback
- Use `audio_is_playing()` to check completion before hour strikes

### Challenge 2: SD Card File Access During Playback

**Problem:** Cannot access SD card while audio file is open.

**Solution:**
- Read entire chime file into RAM buffer before playback (for small files)
- Or: Use streaming approach with double-buffering
- Phase 2.2 already handles file I/O correctly

### Challenge 3: Memory Constraints

**Problem:** Large chime files may not fit in RAM.

**Solution:**
- ESP32-S3 has 8MB PSRAM - more than enough
- Largest file: ~896 KB (hour chime) - easily fits in PSRAM
- Use `heap_caps_malloc(MALLOC_CAP_SPIRAM)` for large buffers

### Challenge 4: Time Accuracy

**Problem:** Must trigger chimes precisely at quarter-hour boundaries.

**Solution:**
- DS3231 RTC has ±2ppm accuracy (~1 minute/year drift)
- NTP sync corrects RTC every 5 minutes
- Check time every second (existing loop)
- Trigger when `tm_min % 15 == 0` and seconds near :00

### Challenge 5: Chime Interruption

**Problem:** What if user changes time during chime playback?

**Solution:**
- Add `chime_manager_stop()` function
- Call before time changes
- Check `audio_is_playing()` before starting new chime

## File Structure

```
components/
├── chime_manager/
│   ├── include/
│   │   └── chime_manager.h
│   ├── chime_manager.c
│   └── CMakeLists.txt
│
├── audio_manager/
│   └── audio_manager.c  (add audio_is_playing() function)
│
└── mqtt_manager/
    └── mqtt_manager.c  (add chime command handlers)

main/
└── wordclock.c  (add chime_manager_init(), time checking)

/sdcard/
└── chimes/
    └── westminster/
        ├── quarter_past.pcm
        ├── half_past.pcm
        ├── quarter_to.pcm
        ├── hour.pcm
        └── strike.pcm
```

## Testing Plan

### Unit Tests

1. **Chime Manager Init/Deinit**
   - Initialize component
   - Load configuration from NVS
   - Clean shutdown

2. **Time Trigger Logic**
   - Trigger at :00, :15, :30, :45
   - Skip non-quarter minutes
   - Respect quiet hours

3. **Hour Strike Counting**
   - 1 strike at 1:00
   - 12 strikes at 12:00 and 00:00
   - Correct modulo arithmetic

4. **MQTT Commands**
   - Enable/disable chimes
   - Set quiet hours
   - Test individual chimes
   - Change chime set

### Integration Tests

1. **Full Hour Sequence**
   - Play 16-note Westminster sequence
   - Play correct number of hour strikes
   - Complete without errors

2. **All Quarter Variations**
   - Quarter past (4 notes)
   - Half past (8 notes)
   - Quarter to (12 notes)
   - Hour (16 notes + strikes)

3. **Quiet Hours**
   - Chimes suppressed during quiet period
   - Chimes resume after quiet period
   - Configuration persists across reboots

4. **SD Card Error Handling**
   - Missing chime file
   - Corrupted PCM file
   - SD card removed during playback

### User Acceptance Tests

1. **Home Assistant Control**
   - Enable/disable switch works
   - Quiet hours sliders functional
   - Test buttons trigger chimes
   - Chime set selector changes sounds

2. **Audio Quality**
   - Chimes sound clear and pleasant
   - Volume appropriate (not too loud/quiet)
   - No distortion or crackling
   - Smooth transitions between notes

3. **Real-World Usage**
   - Chimes play correctly over 24 hours
   - No missed chimes
   - No false triggers
   - Quiet hours respected

## Performance Estimates

### Memory Usage

**Code Size:**
- `chime_manager.c`: ~8 KB
- MQTT handlers: ~2 KB
- **Total:** ~10 KB additional binary size

**RAM Usage:**
- Configuration: ~64 bytes
- State tracking: ~32 bytes
- **Total:** ~96 bytes static RAM

**PSRAM (Chime Buffers):**
- Option 1: Stream from SD (0 bytes PSRAM)
- Option 2: Buffer in RAM (~896 KB max for hour chime)

### CPU Usage

**Idle:** 0% (no chimes playing)
**Active Chime:** <5% (DMA handles I2S, minimal CPU)
**Time Checking:** <0.1% (runs every second, quick check)

### Battery Impact

If powered by battery:
- Chimes play 96 times per day (4 per hour × 24 hours)
- Average 10 seconds per chime
- Total: 16 minutes of audio per day
- I2S and amplifier power: ~100mA during playback
- Additional power: ~27mAh per day

## Future Enhancements

### Phase 2.3+ Ideas

1. **Custom Chime Composer**
   - Web interface to create custom melodies
   - Upload via MQTT or WiFi file transfer
   - Save as custom chime set

2. **Seasonal Chimes**
   - Different sounds for holidays
   - Automatic switching by date
   - Christmas chimes, New Year, etc.

3. **Volume Ramping**
   - Fade in at start of chime
   - Fade out at end
   - Dynamic volume based on time of day

4. **Multiple Speakers**
   - Add second MAX98357A for stereo
   - Directional sound (front/back)
   - Surround effect for realism

5. **Chime Visualization**
   - LED patterns during chimes
   - Pulse LEDs in sync with notes
   - Visual "bell ringing" animation

6. **Smart Quiet Hours**
   - Integrate with Home Assistant "away" mode
   - Disable chimes when nobody home
   - Auto-adjust based on sleep patterns

## Dependencies

### Hardware
- ✅ ESP32-S3-DevKitC-1 (from base system)
- ✅ MAX98357A I2S amplifier (Phase 2.1)
- ✅ MicroSD card (Phase 2.2)
- ✅ DS3231 RTC (from base system)

### Software
- ✅ ESP-IDF 5.4.2
- ✅ audio_manager component (Phase 2.1)
- ✅ sdcard_manager component (Phase 2.2)
- ✅ mqtt_manager component (from base system)
- ✅ DS3231 RTC driver (from base system)

### External Resources
- Westminster chime audio files (to be created/sourced)
- ffmpeg for audio conversion
- Audio editing software (Audacity, etc.)

## Success Criteria

Phase 2.3 will be considered complete when:

1. ✅ Chimes play automatically at :00, :15, :30, :45
2. ✅ Hour strikes count correctly (1-12)
3. ✅ MQTT enable/disable control works
4. ✅ Quiet hours suppress chimes as configured
5. ✅ Home Assistant entities created and functional
6. ✅ Multiple chime sets selectable
7. ✅ Test buttons trigger individual chimes
8. ✅ No interference with word clock display
9. ✅ Audio quality meets expectations
10. ✅ System stable for 24+ hours of operation

## Timeline

**Estimated Duration:** 2-3 weeks

- **Week 1:** Core infrastructure, time triggering, MQTT basics
- **Week 2:** Audio file creation, playback integration, hour strikes
- **Week 3:** Home Assistant discovery, quiet hours, polish

**Milestone:** Production-ready Westminster chimes by end of November 2025

---

**Document Version:** 1.0
**Last Updated:** November 6, 2025
**Status:** Ready for Implementation
**Next Step:** Begin Phase 2.3a (Core Infrastructure)
