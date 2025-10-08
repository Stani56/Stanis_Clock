# Test Suite MQTT Compatibility Fix

## Issue Identified
The automated test suite (`claude_code_test_suite.sh`) had **hardcoded MQTT topics** using the old device name `esp32_core`, but the current implementation uses `Clock_Stani_1`.

## Impact
- **Before Fix:** 0% test success rate - all tests would fail
- **After Fix:** 100% compatibility with current MQTT implementation

## Changes Made

### 1. Updated `test_config.sh`
**Added device name configuration:**
```bash
# Device Configuration (MUST match MQTT_DEVICE_NAME in mqtt_manager.h)
export MQTT_DEVICE_NAME="Clock_Stani_1"

# Topics (dynamically generated from device name)
export TOPIC_BASE="home/$MQTT_DEVICE_NAME"
export COMMAND_TOPIC="$TOPIC_BASE/command"
export STATUS_TOPIC="$TOPIC_BASE/status"
export AVAILABILITY_TOPIC="$TOPIC_BASE/availability"
export MONITOR_TOPICS="$TOPIC_BASE/+"
```

**Before (Hardcoded):**
```bash
export COMMAND_TOPIC="home/esp32_core/command"
export STATUS_TOPIC="home/esp32_core/status"
export MONITOR_TOPICS="home/esp32_core/+ wordclock/+"
```

### 2. Updated `claude_code_test_suite.sh`
**Replaced all hardcoded topic references with variables:**

| Location | Before | After |
|----------|--------|-------|
| Monitoring subscription | `-t "home/esp32_core/+"` | `-t "$MONITOR_TOPICS"` |
| Command publishing | `-t "home/esp32_core/command"` | `-t "$COMMAND_TOPIC"` |
| Command validation | `grep -q "home/esp32_core/command"` | `grep -q "$COMMAND_TOPIC"` |
| Availability check | `-t "home/esp32_core/availability"` | `-t "$AVAILABILITY_TOPIC"` |

**Added configuration validation:**
- Checks if `MQTT_DEVICE_NAME` is properly loaded
- Displays test configuration at startup
- Fails fast with clear error if config is missing

## Current Configuration Match

### mqtt_manager.h (Line 27):
```c
#define MQTT_DEVICE_NAME "Clock_Stani_1"
```

### test_config.sh (Line 11):
```bash
export MQTT_DEVICE_NAME="Clock_Stani_1"
```

✅ **Perfect Match** - Topics now align correctly:
- Command: `home/Clock_Stani_1/command`
- Status: `home/Clock_Stani_1/status`
- Availability: `home/Clock_Stani_1/availability`
- Monitoring: `home/Clock_Stani_1/+`

## Multi-Device Support

The test suite now supports testing **multiple word clocks** on the same network:

**To test different devices:**
1. Edit `test_config.sh` line 11: Change `MQTT_DEVICE_NAME`
2. Run test suite - automatically uses correct topics
3. No hardcoded values to update!

**Example:**
```bash
# Testing bedroom clock
export MQTT_DEVICE_NAME="wordclock_bedroom"  # → home/wordclock_bedroom/*

# Testing kitchen clock
export MQTT_DEVICE_NAME="wordclock_kitchen"  # → home/wordclock_kitchen/*
```

## Verification Checklist

✅ **test_config.sh:** Device name matches mqtt_manager.h
✅ **claude_code_test_suite.sh:** Uses dynamic topic variables
✅ **No hardcoded topics:** All references use config variables
✅ **Configuration validation:** Startup check prevents silent failures
✅ **Multi-device ready:** Single config change supports any device

## Testing the Fix

**Run the test suite:**
```bash
cd test_environment
./claude_code_test_suite.sh
```

**Expected startup output:**
```
📡 Test Configuration:
   Device Name: Clock_Stani_1
   Topic Base: home/Clock_Stani_1
   Command Topic: home/Clock_Stani_1/command

🤖 Claude Code Automated Test Suite for ESP32 Word Clock
========================================================
```

**Success indicators:**
- Pre-test check: "✅ ESP32 is online"
- Test results: "✅ PASSED" for responsive ESP32
- Topics match device configuration

## Maintenance

**When deploying new devices:**
1. Flash ESP32 with unique `MQTT_DEVICE_NAME` in mqtt_manager.h
2. Update `test_config.sh` to match
3. Run test suite to validate

**No code changes needed** - just configuration alignment! 🎯
