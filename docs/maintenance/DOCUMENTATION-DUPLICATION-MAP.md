# Documentation Duplication Map

Visual representation of duplicate and overlapping documentation in the Stanis_Clock repository.

---

## MQTT Documentation Duplication

```
┌─────────────────────────────────────────────────────────────────┐
│                    MQTT DOCUMENTATION                           │
│                     (5 files, ~3,500 lines)                     │
└─────────────────────────────────────────────────────────────────┘
                               │
           ┌───────────────────┼───────────────────┐
           │                   │                   │
           ▼                   ▼                   ▼
    ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
    │   ROOT       │   │    USER      │   │ IMPLEMENTATION│
    │   LEVEL      │   │    DOCS      │   │     DOCS     │
    └──────────────┘   └──────────────┘   └──────────────┘
           │                   │                   │
           │                   │                   │
           ▼                   ▼                   ▼
    ┌──────────────────────────────────────────────────────┐
    │ Mqtt_User_Guide.md                                   │
    │ 809 lines - MOST COMPREHENSIVE                       │
    │ • Connection setup                                   │
    │ • All MQTT commands                                  │
    │ • Home Assistant integration                         │
    │ • Troubleshooting                                    │
    └──────────────────────────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    ▼                     ▼
    ┌────────────────────────┐  ┌────────────────────────┐
    │ mqtt-commands.md       │  │ mqtt-discovery-ref.md  │
    │ 371 lines              │  │ 185 lines              │
    │ ~70% DUPLICATE ⚠️      │  │ Quick reference        │
    │ • Same commands        │  │ • Bug fixes applied    │
    │ • Same examples        │  │ • Config persistence   │
    └────────────────────────┘  └────────────────────────┘

    ┌─────────────────────────────────────────────────────┐
    │ mqtt-system-architecture.md                         │
    │ 1,586 lines - DEVELOPER REFERENCE                   │
    │ • Complete technical architecture                   │
    │ • Component interaction                             │
    │ • Security implementation                           │
    │ • Performance characteristics                       │
    └─────────────────────────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    ▼                     ▼
    ┌────────────────────────┐  ┌────────────────────────┐
    │ mqtt-discovery.md      │  │ mqtt-discovery-issues  │
    │ 352 lines              │  │ 546 lines              │
    │ Implementation details │  │ Lessons learned        │
    └────────────────────────┘  └────────────────────────┘
```

**Recommendation:**
- Keep: Mqtt_User_Guide.md (root), mqtt-system-architecture.md, mqtt-discovery-issues.md
- Merge: mqtt-commands.md + mqtt-discovery-reference.md → Mqtt_User_Guide.md
- Organize: Move mqtt-*.md to docs/implementation/mqtt/ subfolder

---

## LED Validation Documentation Evolution

```
┌─────────────────────────────────────────────────────────────────┐
│                 LED VALIDATION EVOLUTION                        │
│              (6 files, ~3,500 lines total)                      │
│                                                                 │
│  Timeline: September 2025 → October 2025                        │
└─────────────────────────────────────────────────────────────────┘

SEPTEMBER 2025 (PROPOSALS)
│
├─► LED_VALIDATION_PROPOSAL.md (617 lines)
│   ├─ Software-only validation
│   ├─ Mathematical foundation
│   └─ Initial architecture
│
├─► LED_VALIDATION_HARDWARE_READBACK.md (820 lines)
│   ├─ Added: TLC59116 register readback
│   ├─ Hardware state verification
│   └─ I2C read operations
│
├─► LED_VALIDATION_FAILURE_STRATEGY.md (842 lines)
│   ├─ Auto-recovery plan
│   ├─ Recovery workflows
│   └─ NOT IMPLEMENTED ⚠️ (manual recovery chosen)
│
└─► LED_USAGE_ANALYSIS.md (373 lines)
    ├─ LED matrix analysis
    └─ 91 used LEDs / 69 unused positions

OCTOBER 2025 (IMPLEMENTATION)
│
├─► led-validation-post-transition.md (200 lines)
│   ├─ Post-transition timing (CURRENT)
│   ├─ Auto-increment pointer state
│   └─ Event-driven validation
│
├─► tlc59116-validation-fix.md (~300 lines)
│   ├─ CRITICAL BUG FIX ⚠️
│   ├─ Auto-increment corruption resolved
│   ├─ Register addressing fix
│   └─ 100% accurate PWM readback
│
└─► tlc59116-autoincrement-analysis.md (~400 lines)
    ├─ Root cause analysis
    ├─ TLC59116 datasheet analysis
    └─ Two-level auto-increment control

USER DOCUMENTATION
│
└─► led-validation-guide.md (431 lines)
    ├─ End-user guide (CURRENT)
    ├─ How it works
    ├─ MQTT topics
    └─ Recovery procedures

┌─────────────────────────────────────────────────────────────────┐
│                    OVERLAP ANALYSIS                             │
├─────────────────────────────────────────────────────────────────┤
│ ALL 6 FILES cover LED validation, but from different angles:   │
│                                                                 │
│ • Proposals (3 files) = Evolution of design                    │
│ • Implementation (2 files) = Current system + critical fix     │
│ • User guide (1 file) = End-user documentation                 │
│                                                                 │
│ NOT duplicates - EVOLUTION TIMELINE                            │
└─────────────────────────────────────────────────────────────────┘
```

**Recommendation:**
- Keep: led-validation-guide.md (user), led-validation-post-transition.md (current), tlc59116-validation-fix.md (critical)
- Archive: LED_VALIDATION_*.md → archive/implemented-proposals/led-validation/

---

## Chime System Documentation Redundancy

```
┌─────────────────────────────────────────────────────────────────┐
│              CHIME SYSTEM DOCUMENTATION                         │
│            (7 files, ~6,800 lines total)                        │
│                                                                 │
│  Status: OBSOLETE FOR ESP32 (audio removed Nov 2025)           │
│          FUTURE for ESP32-S3                                    │
└─────────────────────────────────────────────────────────────────┘

IMPLEMENTATION PLANS (3 files - SUPERSEDED)
│
├─► chime-system-implementation-plan.md (1,990 lines)
│   ├─ Internal flash only
│   ├─ Limited audio capacity
│   └─ SUPERSEDED by W25Q64 plan
│
├─► chime-system-implementation-plan-w25q64.md (1,581 lines)
│   ├─ W25Q64 8MB external flash
│   ├─ Extended audio capacity
│   └─ SUPERSEDED by ESP32-S3 migration
│
└─► chimes-system-analysis.md (617 lines)
    ├─ Analysis of working Chimes_System
    └─ Porting strategy

INTEGRATION & COMPLETION
│
├─► integration-strategy-from-chimes.md (1,296 lines)
│   ├─ Phase 1 integration plan
│   └─ ✅ COMPLETED October 2025
│
└─► phase1-completion-summary.md (~250 lines)
    ├─ Phase 1 completion report
    ├─ External flash + filesystem
    └─ ✅ COMPLETE

AUDIO HARDWARE
│
├─► max98357a-integration-proposal.md (819 lines)
│   ├─ I2S amplifier integration
│   └─ ESP32-S3 FUTURE
│
└─► voice-chime-system-proposal.md (672 lines)
    ├─ Voice announcements
    └─ ESP32-S3 FUTURE

┌─────────────────────────────────────────────────────────────────┐
│                    WHY OBSOLETE?                                │
├─────────────────────────────────────────────────────────────────┤
│ ESP32 (original) has hardware conflicts:                       │
│ • WiFi + MQTT + I2S audio = CRASHES ⚠️                         │
│ • DMA channel conflicts                                        │
│ • Insufficient memory bandwidth                                │
│                                                                 │
│ Solution: Audio removed from ESP32 baseline (Nov 2025)        │
│          Will be re-enabled on ESP32-S3 (better hardware)     │
└─────────────────────────────────────────────────────────────────┘
```

**Recommendation:**
- Archive: chime-system-*.md → archive/esp32-audio-removed/chime-plans/
- Keep: phase1-completion-summary.md (Phase 1 complete)
- Future: max98357a-integration-proposal.md → proposals/esp32-s3-future/

---

## Audio System Documentation (OBSOLETE)

```
┌─────────────────────────────────────────────────────────────────┐
│                 AUDIO SYSTEM DOCUMENTATION                      │
│               (5 files, ~2,700 lines total)                     │
│                                                                 │
│  Status: REMOVED FROM ESP32 BASELINE                            │
└─────────────────────────────────────────────────────────────────┘

PROPOSALS (docs/proposals/)
│
├─► audio-performance-analysis.md (613 lines)
│   ├─ CPU usage analysis
│   ├─ Memory requirements
│   └─ ❌ OBSOLETE (audio removed)
│
└─► audio-storage-analysis.md (599 lines)
    ├─ Storage options comparison
    ├─ Internal vs external flash
    └─ ❌ OBSOLETE (audio removed)

TECHNICAL ANALYSIS
│
└─► usb-storage-analysis.md (487 lines)
    ├─ USB storage evaluation
    ├─ USB-OTG for audio files
    └─ ❌ OBSOLETE (SD card chosen for ESP32-S3)

RESOURCES
│
├─► audio-sources-guide.md (479 lines)
│   ├─ Audio file sources
│   ├─ Westminster chimes
│   └─ ✅ STILL VALID for ESP32-S3
│
└─► voice-chime-system-proposal.md (672 lines)
    ├─ Voice announcements
    └─ ESP32-S3 FUTURE
```

**Recommendation:**
- Archive: audio-*.md, usb-storage-analysis.md → archive/esp32-audio-removed/
- Keep: audio-sources-guide.md → proposals/esp32-s3-future/

---

## Implemented Proposals (RESOLVED)

```
┌─────────────────────────────────────────────────────────────────┐
│              IMPLEMENTED PROPOSALS                              │
│                                                                 │
│  Status: COMPLETED - Move to archive                            │
└─────────────────────────────────────────────────────────────────┘

ERROR LOGGING SYSTEM
│
└─► nvs-error-logging-proposal.md (680 lines)
    ├─ Persistent error storage in NVS
    ├─ 50-entry circular buffer
    ├─ MQTT query interface
    └─ ✅ IMPLEMENTED October 2025
         └─► docs/user/error-logging-guide.md

LED VALIDATION SYSTEM
│
└─► LED_VALIDATION_*.md (3 files, ~2,300 lines)
    ├─ LED validation proposals
    ├─ Hardware readback
    ├─ Failure strategies
    └─ ✅ IMPLEMENTED October 2025
         └─► docs/user/led-validation-guide.md
         └─► led-validation-post-transition.md (current)

MQTT DISCOVERY
│
└─► mqtt-discovery.md (352 lines)
    ├─ Home Assistant auto-discovery
    ├─ Entity registration
    └─ ✅ IMPLEMENTED
         └─► mqtt-system-architecture.md (complete docs)
```

**Recommendation:**
- Move to: archive/implemented-proposals/
- Add banner: "✅ IMPLEMENTED - See [current docs]"

---

## Summary Visualization

```
CURRENT STATE (Cluttered)
├── implementation/ (18 files) ⚠️ TOO MANY
│   ├── LED_VALIDATION_*.md (6 files) ⚠️ EVOLUTION
│   ├── chime-system-*.md (3 files) ⚠️ OBSOLETE
│   ├── mqtt-*.md (4 files) ⚠️ SCATTERED
│   └── [other files]
├── proposals/ (6 files)
│   ├── audio-*.md (3 files) ⚠️ OBSOLETE
│   └── [active proposals]
└── user/ (7 files)
    ├── mqtt-commands.md ⚠️ DUPLICATE
    └── mqtt-discovery-ref.md ⚠️ DUPLICATE

PROPOSED STATE (Clean)
├── implementation/ (10 files) ✅ REDUCED 44%
│   ├── mqtt/ (3 files) ✅ ORGANIZED
│   ├── led-validation-current.md ✅ CLEAR
│   ├── tlc59116-critical-fix.md ✅ IMPORTANT
│   └── [other files]
├── proposals/
│   ├── active/ (2 files) ✅ CURRENT
│   └── esp32-s3-future/ (3 files) ✅ FUTURE
├── user/ (6 files) ✅ MERGED MQTT
├── hardware/
│   ├── esp32-current/ ✅ ORGANIZED
│   └── esp32-s3-migration/ ✅ ORGANIZED
└── archive/
    ├── implemented-proposals/ ✅ HISTORY PRESERVED
    │   ├── led-validation/ (5 files)
    │   ├── error-logging/ (1 file)
    │   └── mqtt-discovery/ (2 files)
    └── esp32-audio-removed/ ✅ CLEAR REASON
        ├── chime-plans/ (3 files)
        └── audio-*.md (3 files)
```

---

## Metrics

### Duplication Metrics
```
MQTT:          5 files, ~3,500 lines, ~70% overlap in user docs
LED Validation: 6 files, ~3,500 lines, timeline evolution (not duplicates)
Chime System:  7 files, ~6,800 lines, superseded by hardware reality
Audio System:  5 files, ~2,700 lines, obsoleted by ESP32 limitations
```

### Cleanup Impact
```
Before:  73 active files (~43,000 lines)
After:   54 active files (~28,000 lines)
Reduction: 26% fewer active files, 35% fewer active lines
Archived: 25 files (~18,000 lines) moved to archive/
```

### Organization Improvement
```
Before:  implementation/ = 18 files (cluttered)
After:   implementation/ = 10 files + subdirectories (organized)

Before:  proposals/ = 6 files (mixed status)
After:   proposals/ = active/ + esp32-s3-future/ (clear separation)

Before:  hardware/ = 4 files (mixed platforms)
After:   hardware/ = esp32-current/ + esp32-s3-migration/ (organized)
```

---

**Full Analysis:** See [DOCUMENTATION-CLEANUP-ANALYSIS.md](DOCUMENTATION-CLEANUP-ANALYSIS.md)
**Summary:** See [DOCUMENTATION-CLEANUP-SUMMARY.md](DOCUMENTATION-CLEANUP-SUMMARY.md)

