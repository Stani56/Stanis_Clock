# Documentation Archive

This directory contains archived documentation that is no longer actively maintained but preserved for historical reference.

## Archive Structure

### esp32-audio-removed/
Documentation for the audio subsystem that was removed from ESP32 baseline (Nov 2025).

**Reason for Removal:** ESP32 hardware cannot handle WiFi+MQTT+I2S concurrently due to DMA conflicts. Audio functionality will be re-implemented on ESP32-S3 hardware.

**Contents:**
- Audio performance analysis
- Audio storage analysis
- MAX98357A integration proposal
- Voice chime system proposal
- Audio sources guide

### implemented-proposals/
Proposals that have been successfully implemented and integrated into the main codebase.

**Contents:**
- NVS error logging (implemented Oct 2025)
- LED validation system (implemented Oct 2025)

### superseded-plans/
Implementation plans that were superseded by hardware changes or alternative approaches.

**Contents:**
- Chime system implementation plans (superseded by ESP32-S3 migration decision)
- Integration strategies from Chimes_System

### 2024-history/
Historical documentation from 2024 development, including issue fixes and early analysis.

**Contents:**
- Legacy issue fixes (MQTT, NTP sync problems)
- Original project analysis
- Early setup guides (WSL2, USB alternatives)

## Archive Policy

Documents are archived when they:
1. Describe features removed from the codebase
2. Document proposals that have been fully implemented
3. Contain implementation plans superseded by hardware/design changes
4. Represent historical snapshots no longer relevant to current development

All archived content is preserved with full context and rationale for archival.

## Finding Current Documentation

For active documentation, see:
- **User guides:** `docs/user/`
- **Developer reference:** `docs/developer/`
- **Implementation details:** `docs/implementation/`
- **Active proposals:** `docs/proposals/active/`
- **ESP32-S3 future:** `docs/proposals/esp32-s3-future/`
