# Voice & Chime System Proposal
## ESP32 Word Clock - Audio Enhancement

**Status:** Proposal
**Created:** 2025-10-15
**Author:** Claude (with user requirements)
**Target:** ESP32 Word Clock with Home Assistant Integration

---

## Executive Summary

Add audio functionality to the word clock with:
- **Westminster/Big Ben chimes** on quarter hours
- **Voice announcements** for time (optional)
- **Configurable schedules** (quiet hours, volume control)
- **Home Assistant integration** (full MQTT control)
- **Multiple chime options** (Westminster, simplified, custom)

---

## 1. Hardware Requirements

### Option A: I2S DAC (Recommended - Best Quality)
**Component:** MAX98357A I2S Class D Amplifier (~$5)

**Connections:**
```
ESP32          MAX98357A
GPIO 25   →    DIN (I2S Data)
GPIO 26   →    BCLK (Bit Clock)
GPIO 27   →    LRC (Left/Right Clock)
3.3V      →    VIN
GND       →    GND
           →    Speaker Out (8Ω, 3W)
```

**Advantages:**
- ✅ High quality audio (16-bit, 44.1kHz)
- ✅ Direct digital-to-analog conversion
- ✅ Built-in amplifier (up to 3W)
- ✅ No external DAC needed
- ✅ Clean signal, low noise

**Disadvantages:**
- ❌ Uses 3 GPIO pins (but ESP32 has plenty)
- ❌ Requires I2S library

---

### Option B: PWM DAC (Budget - Acceptable Quality)
**Component:** Simple passive filter + amplifier (e.g., LM386)

**Connections:**
```
ESP32          Filter          LM386         Speaker
GPIO 25   →    RC Filter  →    Input    →    8Ω Speaker
                ↓
              GND
```

**Advantages:**
- ✅ Very cheap (~$1-2)
- ✅ Only 1 GPIO pin needed
- ✅ Simple circuit
- ✅ Built into ESP-IDF (no external libraries)

**Disadvantages:**
- ❌ Lower quality (8-bit effective)
- ❌ More noise/hiss
- ❌ Requires external amplifier

---

### Option C: MQTT Audio Relay (No Hardware)
**Component:** Software only - relay audio to Home Assistant

**How it works:**
1. ESP32 publishes MQTT event: `chime_trigger`
2. Home Assistant automation plays audio on smart speaker
3. Node-RED or automation handles sound playback

**Advantages:**
- ✅ Zero hardware cost
- ✅ No GPIO pins used
- ✅ Can use existing smart speakers
- ✅ Easy to change sounds (no reflashing)

**Disadvantages:**
- ❌ Requires network connectivity
- ❌ Slight delay (100-500ms)
- ❌ Depends on Home Assistant/MQTT
- ❌ No standalone operation

---

## 2. Recommended Architecture

### 2.1 Component Structure

```
New Components:
├── audio_manager/               # Core audio playback
│   ├── audio_manager.c         # I2S/PWM driver abstraction
│   ├── audio_manager.h         # Public API
│   └── CMakeLists.txt
│
├── chime_scheduler/            # When to play chimes
│   ├── chime_scheduler.c      # Schedule logic
│   ├── chime_scheduler.h      # Config API
│   └── CMakeLists.txt
│
├── chime_library/              # Sound data
│   ├── chime_library.c        # Westminster, Big Ben, etc.
│   ├── chime_library.h        # Chime selection
│   ├── chimes/                # Audio data (embedded)
│   │   ├── westminster_quarter.h
│   │   ├── westminster_half.h
│   │   ├── westminster_full.h
│   │   └── bigben_hour.h
│   └── CMakeLists.txt
│
└── voice_synthesis/            # Optional voice announcements
    ├── voice_synthesis.c      # TTS or pre-recorded
    ├── voice_synthesis.h
    └── CMakeLists.txt
```

### 2.2 Integration Points

**Existing Components:**
- `mqtt_manager` → Add chime control topics
- `mqtt_discovery` → Add HA entities (switches, selects, numbers)
- `ntp_manager` → Trigger chimes on time events
- `nvs_manager` → Store chime configuration

---

## 3. Feature Design

### 3.1 Westminster Chimes

**Standard Westminster Chime Pattern:**

**Quarter Past (:15):**
- 4 notes (1st quarter)
- Example: E - D - C - G

**Half Past (:30):**
- 8 notes (2nd quarter)
- Example: E - D - C - G, C - E - D - G

**Quarter To (:45):**
- 12 notes (3rd quarter)
- Example: E - D - C - G, C - E - D - G, E - C - D - G

**On the Hour (:00):**
- 16 notes (full chime)
- Followed by hour strikes (1-12 strikes)
- Example: E - D - C - G, C - E - D - G, E - C - D - G, G - E - C - D
- Then: BONG (×N times for hour)

**Audio Format:**
- Sample Rate: 16 kHz (sufficient for chimes)
- Bit Depth: 16-bit mono
- Format: PCM WAV embedded as C arrays
- Duration: ~10 seconds max per chime

**Storage Estimate:**
```
Quarter chime:  ~30 KB (2 sec × 16 kHz × 2 bytes)
Half chime:     ~50 KB (3 sec)
3/4 chime:      ~80 KB (5 sec)
Full chime:     ~160 KB (10 sec)
Hour strikes:   ~50 KB (reusable)
-------------------------------------------
Total:          ~370 KB (worst case)
```

ESP32 4MB flash has plenty of room!

---

### 3.2 Chime Options

| Chime Type | Description | Size | Use Case |
|------------|-------------|------|----------|
| **Westminster Full** | Traditional 16-note + hour strikes | ~370 KB | Maximum authenticity |
| **Westminster Simplified** | 8-note pattern only | ~100 KB | Space-saving option |
| **Big Ben** | Hour strikes only (deep BONG) | ~50 KB | Minimal, classic |
| **Simple Beep** | Single tone | <1 KB | Testing/fallback |
| **Custom** | User-uploaded (future) | Variable | Personalization |
| **MQTT Relay** | No audio, just triggers | 0 KB | Network-based |

---

### 3.3 Voice Announcements (Optional)

**Approach 1: Pre-recorded Phrases (Recommended)**

Record common phrases:
- "It's [hour] o'clock"
- "It's quarter past [hour]"
- "It's half past [hour]"
- "It's quarter to [hour]"

**Storage:**
- 24 hours × 4 quarters = 96 phrases
- @ 2 sec/phrase × 16 kHz × 2 bytes = ~6 KB/phrase
- Total: ~600 KB

**Advantages:**
- ✅ Natural voice
- ✅ Good quality
- ✅ No processing needed

**Disadvantages:**
- ❌ Large storage
- ❌ Fixed phrases only

---

**Approach 2: Text-to-Speech (TTS)**

Use ESP32 TTS library (e.g., Flite, eSpeak port):

**Advantages:**
- ✅ Flexible - any phrase
- ✅ Small storage (just code)

**Disadvantages:**
- ❌ Robotic voice
- ❌ High CPU usage
- ❌ Complex integration

**Recommendation:** Skip TTS for MVP, use chimes only.

---

### 3.4 Scheduling & Quiet Hours

**Configuration Options:**

1. **Chime Mode:**
   - Off (silent)
   - Quarter hours (:00, :15, :30, :45)
   - Half hours only (:00, :30)
   - Hours only (:00)
   - Custom schedule

2. **Quiet Hours:**
   - Start time (default: 22:00)
   - End time (default: 07:00)
   - Days: Mon-Sun (individual enable/disable)

3. **Volume Control:**
   - 0-100% (mapped to I2S/PWM amplitude)
   - Separate day/night volumes

4. **Special Events:**
   - Birthday chime (custom melody)
   - Alarm chime
   - MQTT-triggered custom sounds

---

## 4. MQTT Integration (Home Assistant)

### 4.1 New MQTT Topics

**Control Topics:**
```
home/wordclock/chime/mode/set          # Chime mode selection
home/wordclock/chime/mode/status       # Current mode

home/wordclock/chime/volume/set        # Volume (0-100)
home/wordclock/chime/volume/status     # Current volume

home/wordclock/chime/quiet_hours/set   # Quiet hours config (JSON)
home/wordclock/chime/quiet_hours/status # Current config

home/wordclock/chime/style/set         # Westminster/BigBen/etc.
home/wordclock/chime/style/status      # Current style

home/wordclock/chime/trigger           # Manual trigger
home/wordclock/chime/test              # Test current chime
```

**Event Topics:**
```
home/wordclock/chime/played            # Published after chime plays
home/wordclock/chime/scheduled         # Next scheduled chime info
```

---

### 4.2 Home Assistant Entities

**NEW Entities (7 total):**

1. **Switch:** "Chimes Enabled"
   - ON/OFF master enable

2. **Select:** "Chime Style"
   - Options: Westminster Full, Westminster Simple, Big Ben, Simple Beep, Off, MQTT Relay

3. **Select:** "Chime Schedule"
   - Options: Quarter Hours, Half Hours, Hours Only, Custom

4. **Number:** "Chime Volume"
   - Range: 0-100, step 5
   - Unit: %

5. **Time:** "Quiet Hours Start"
   - Default: 22:00

6. **Time:** "Quiet Hours End"
   - Default: 07:00

7. **Button:** "Test Chime"
   - Plays current chime immediately

**Updated Entity Count:** 37 + 7 = **44 entities**

---

### 4.3 Example Home Assistant Config

```yaml
# Automation: Play chime on quarter hours
automation:
  - alias: "Word Clock Chimes"
    trigger:
      - platform: time_pattern
        minutes: "/15"  # Every 15 minutes
    condition:
      - condition: state
        entity_id: switch.wordclock_chimes_enabled
        state: "on"
      # Add quiet hours check
    action:
      - service: mqtt.publish
        data:
          topic: "home/wordclock/chime/trigger"
          payload: "auto"

# Card for chime control
type: entities
title: Clock Chimes
entities:
  - entity: switch.wordclock_chimes_enabled
  - entity: select.wordclock_chime_style
  - entity: select.wordclock_chime_schedule
  - entity: number.wordclock_chime_volume
  - entity: input_datetime.wordclock_quiet_start
  - entity: input_datetime.wordclock_quiet_end
  - entity: button.wordclock_test_chime
```

---

## 5. Implementation Plan

### Phase 1: Hardware Setup & Basic Audio (Week 1)

**Goal:** Get basic audio output working

**Steps:**
1. **Hardware wiring** (MAX98357A or PWM circuit)
2. **Create `audio_manager` component**
   - I2S driver initialization
   - Play PCM buffer function
   - Volume control
3. **Test with simple beep** (sine wave generation)
4. **Verify audio quality**

**Deliverable:** ESP32 can play a test tone through speaker

**Estimated Time:** 2-3 days

---

### Phase 2: Chime Library (Week 1-2)

**Goal:** Embed Westminster chimes

**Steps:**
1. **Source/record chime audio**
   - Download royalty-free Westminster chimes
   - Convert to 16 kHz, 16-bit mono PCM
   - Use `xxd -i` to convert to C arrays
2. **Create `chime_library` component**
   - Embed WAV data as const arrays
   - Chime selection function
   - Play chime via audio_manager
3. **Test all 4 chime patterns**

**Deliverable:** Can play Westminster quarter/half/3-quarter/hour chimes

**Tools:**
```bash
# Convert WAV to C array
ffmpeg -i westminster.wav -ar 16000 -ac 1 -f s16le westminster.pcm
xxd -i westminster.pcm > westminster_chime.h
```

**Estimated Time:** 2-3 days

---

### Phase 3: Chime Scheduler (Week 2)

**Goal:** Automatic chiming on schedule

**Steps:**
1. **Create `chime_scheduler` component**
   - NVS configuration storage
   - Quiet hours logic
   - Schedule evaluation (quarter/half/hour)
2. **Integrate with NTP time**
   - Hook into minute change events
   - Trigger chimes at :00, :15, :30, :45
3. **Add quiet hours logic**
   - Check time range
   - Suppress chimes if in quiet period

**Deliverable:** Clock automatically chimes on schedule

**Estimated Time:** 2 days

---

### Phase 4: MQTT Integration (Week 2-3)

**Goal:** Home Assistant control

**Steps:**
1. **Add MQTT topics to `mqtt_manager`**
   - Command handlers for chime/mode/set, volume/set, etc.
   - Status publishers
2. **Add MQTT Discovery configs**
   - 7 new entities (switches, selects, numbers, buttons)
3. **Test HA integration**
   - Toggle switches in HA
   - Verify MQTT commands work
   - Test volume control

**Deliverable:** Full Home Assistant control of chimes

**Estimated Time:** 2-3 days

---

### Phase 5: Testing & Refinement (Week 3)

**Goal:** Polish and bug fixes

**Steps:**
1. **Audio quality tuning**
   - Adjust volume curves
   - Reduce clicks/pops
   - Test speaker output
2. **Timing accuracy**
   - Verify chimes trigger on exact minute
   - Test quiet hours edge cases
3. **NVS persistence testing**
   - Config survives reboots
   - Volume settings retained
4. **Documentation**
   - User guide
   - MQTT API reference
   - Hardware assembly guide

**Deliverable:** Production-ready chime system

**Estimated Time:** 2-3 days

---

## 6. Storage & Memory Requirements

### Flash Storage

| Component | Size | Notes |
|-----------|------|-------|
| Audio Manager Code | ~20 KB | I2S driver wrapper |
| Chime Scheduler Code | ~15 KB | Scheduling logic |
| Westminster Full Library | ~370 KB | All 4 chime patterns + strikes |
| Westminster Simple | ~100 KB | Alternative (space-saving) |
| Big Ben Only | ~50 KB | Minimal option |

**Current App Size:** 1,156,128 bytes (~1.1 MB)
**Available:** ~1.4 MB free (2.5 MB partition)
**After Chimes (Full):** ~1.5 MB (still fits comfortably!)

### RAM Usage

| Component | RAM | Type |
|-----------|-----|------|
| I2S DMA Buffer | 4 KB | DMA-capable RAM |
| Audio Decode Buffer | 2 KB | Heap |
| Chime Scheduler State | <1 KB | Static |

**Total Added:** ~7 KB RAM (ESP32 has 520 KB total)
**Impact:** Negligible

---

## 7. Alternative: MQTT Relay Approach (No Hardware)

For users who don't want to add hardware, implement a lightweight "chime trigger" system:

### How It Works:

1. **ESP32 Side:**
   - Detect time events (:00, :15, :30, :45)
   - Check schedule/quiet hours
   - Publish MQTT message: `home/wordclock/chime/event`
   - Payload: `{"time": "10:15", "type": "quarter", "chime": "westminster"}`

2. **Home Assistant Side:**
   - Automation listens to `chime/event` topic
   - Plays audio file on media player (smart speaker, Sonos, etc.)
   - Can use any sound files (no firmware changes needed)

**Example HA Automation:**
```yaml
automation:
  - alias: "Play Westminster Chime"
    trigger:
      platform: mqtt
      topic: "home/wordclock/chime/event"
    action:
      service: media_player.play_media
      target:
        entity_id: media_player.living_room_speaker
      data:
        media_content_id: "/local/sounds/westminster_{{ trigger.payload_json.type }}.mp3"
        media_content_type: "music"
```

**Advantages:**
- ✅ No hardware changes
- ✅ Use existing speakers
- ✅ Easy to customize sounds
- ✅ Zero flash storage

**Disadvantages:**
- ❌ Requires network
- ❌ 100-500ms latency
- ❌ Not standalone

---

## 8. Cost Analysis

### Option A: I2S DAC (MAX98357A)
| Item | Cost |
|------|------|
| MAX98357A Module | $5 |
| 8Ω 3W Speaker | $3 |
| Wires/Connectors | $1 |
| **Total** | **~$9** |

### Option B: PWM + LM386
| Item | Cost |
|------|------|
| LM386 Amplifier IC | $1 |
| Resistors/Capacitors | $0.50 |
| 8Ω Speaker | $2 |
| **Total** | **~$3.50** |

### Option C: MQTT Relay
| Item | Cost |
|------|------|
| Software only | $0 |
| **Total** | **$0** |

---

## 9. Recommendation

### For Best Experience:
**Option A (I2S DAC)** - High quality, worth the $9 investment

### For Budget Build:
**Option B (PWM)** - Acceptable quality, minimal cost

### For Testing/Network-Only:
**Option C (MQTT Relay)** - Zero hardware, good for prototyping

---

## 10. Future Enhancements

1. **Custom Chimes**
   - Upload custom sounds via web interface
   - Store in SPIFFS partition

2. **Voice Announcements**
   - Pre-recorded time announcements
   - German language support

3. **Smart Volume**
   - Auto-adjust based on ambient noise (using existing mic if added)
   - Gradual volume fade-in/out

4. **Alarm Function**
   - Wake-up chimes with crescendo
   - Snooze via MQTT

5. **Multi-Chime Support**
   - Different chimes for different times of day
   - Special occasion melodies

---

## 11. Risk Analysis

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Audio quality poor | Medium | Medium | Test with I2S first; provide PWM fallback |
| Storage overflow | Low | High | Offer multiple chime size options |
| GPIO conflicts | Low | Medium | Document GPIO usage; use configurable pins |
| Timing drift | Low | Low | Use NTP-synced RTC (already implemented) |
| Speaker failure | Low | Medium | Add audio output status monitoring |
| Network latency (MQTT) | Medium | Low | Provide local hardware option |

---

## 12. Success Criteria

### MVP (Minimum Viable Product):
- ✅ Plays Westminster chimes on quarter hours
- ✅ Volume control (0-100%)
- ✅ Quiet hours (configurable start/end)
- ✅ Home Assistant integration
- ✅ NVS persistence of settings

### Nice-to-Have:
- 🎯 Multiple chime styles
- 🎯 Per-day quiet hours
- 🎯 Voice announcements
- 🎯 Custom chime upload

---

## 13. Questions for Decision

1. **Hardware Preference:** I2S DAC or PWM? (Recommend I2S)
2. **Chime Style:** Westminster full or simplified first?
3. **Voice:** Include voice announcements in MVP or defer?
4. **Volume Range:** Absolute (0-100) or separate day/night sliders?
5. **Fallback:** Include MQTT relay mode as option?

---

## 14. Next Steps

1. **Approve Proposal** → Confirm hardware approach
2. **Order Hardware** → MAX98357A + speaker (~1 week shipping)
3. **Create Component Stubs** → Set up directory structure
4. **Begin Phase 1** → Hardware wiring + I2S driver
5. **Iterate** → Follow 3-week implementation plan

---

**Estimated Total Time:** 2-3 weeks part-time
**Estimated Cost:** $0-$9 depending on hardware choice
**Complexity:** Medium (similar to LED validation system)

**Ready to proceed?** 🔔
