# HALB-Centric Time Expression Test Matrix

## Test Overview
This document provides a comprehensive test matrix for validating the HALB-centric time expression feature.

## Testing Method

### Via MQTT (Recommended for Quick Testing):
```bash
# Enable HALB-centric mode
mosquitto_pub -t "home/wordclock/brightness/config/set" -m '{"halb_centric_style": true}'

# Set specific time for testing
mosquitto_pub -t "home/wordclock/command" -m "set_time 10:20"

# Disable HALB-centric mode
mosquitto_pub -t "home/wordclock/brightness/config/set" -m '{"halb_centric_style": false}'
```

### Via Home Assistant:
1. Toggle "HALB-Centric Time Style" switch
2. Use "Set Time" button with time input
3. Observe display changes

## Complete Test Matrix

| Time | Base Min | NACH/VOR Mode (Default) | HALB-Centric Mode | Changed? |
|------|----------|-------------------------|-------------------|----------|
| 10:00 | 0 | ES IST ZEHN UHR | ES IST ZEHN UHR | ❌ No |
| 10:01 | 0 | ES IST ZEHN UHR + 1 LED | ES IST ZEHN UHR + 1 LED | ❌ No |
| 10:05 | 5 | ES IST FÜNF NACH ZEHN | ES IST FÜNF NACH ZEHN | ❌ No |
| 10:10 | 10 | ES IST ZEHN NACH ZEHN | ES IST ZEHN NACH ZEHN | ❌ No |
| 10:15 | 15 | ES IST VIERTEL NACH ZEHN | ES IST VIERTEL NACH ZEHN | ❌ No |
| **10:20** | **20** | **ES IST ZWANZIG NACH ZEHN** | **ES IST ZEHN VOR HALB ELF** | ✅ **YES** |
| 10:21 | 20 | ES IST ZWANZIG NACH ZEHN + 1 LED | ES IST ZEHN VOR HALB ELF + 1 LED | ✅ YES |
| 10:23 | 20 | ES IST ZWANZIG NACH ZEHN + 3 LED | ES IST ZEHN VOR HALB ELF + 3 LED | ✅ YES |
| 10:25 | 25 | ES IST FÜNF VOR HALB ELF | ES IST FÜNF VOR HALB ELF | ❌ No |
| 10:30 | 30 | ES IST HALB ELF | ES IST HALB ELF | ❌ No |
| 10:35 | 35 | ES IST FÜNF NACH HALB ELF | ES IST FÜNF NACH HALB ELF | ❌ No |
| **10:40** | **40** | **ES IST ZWANZIG VOR ELF** | **ES IST ZEHN NACH HALB ELF** | ✅ **YES** |
| 10:42 | 40 | ES IST ZWANZIG VOR ELF + 2 LED | ES IST ZEHN NACH HALB ELF + 2 LED | ✅ YES |
| **10:45** | **45** | **ES IST VIERTEL VOR ELF** | **ES IST DREIVIERTEL ELF** | ✅ **YES** |
| 10:46 | 45 | ES IST VIERTEL VOR ELF + 1 LED | ES IST DREIVIERTEL ELF + 1 LED | ✅ YES |
| 10:50 | 50 | ES IST ZEHN VOR ELF | ES IST ZEHN VOR ELF | ❌ No |
| 10:55 | 55 | ES IST FÜNF VOR ELF | ES IST FÜNF VOR ELF | ❌ No |
| 10:59 | 55 | ES IST FÜNF VOR ELF + 4 LED | ES IST FÜNF VOR ELF + 4 LED | ❌ No |

## Critical Test Cases

### Test 1: Hour Rollover at :20
```bash
# Set to 23:20
mosquitto_pub -t "home/wordclock/command" -m "set_time 23:20"

# Enable HALB-centric
mosquitto_pub -t "home/wordclock/brightness/config/set" -m '{"halb_centric_style": true}'
```
**Expected:** "ES IST ZEHN VOR HALB ZWÖLF" (not "HALB NULL")

### Test 2: Hour Rollover at :40
```bash
# Set to 23:40
mosquitto_pub -t "home/wordclock/command" -m "set_time 23:40"
```
**Expected:** "ES IST ZEHN NACH HALB ZWÖLF"

### Test 3: Hour Rollover at :45
```bash
# Set to 23:45
mosquitto_pub -t "home/wordclock/command" -m "set_time 23:45"
```
**Expected:** "ES IST DREIVIERTEL ZWÖLF"

### Test 4: Noon/Midnight Boundary :20
```bash
# Set to 11:20
mosquitto_pub -t "home/wordclock/command" -m "set_time 11:20"
```
**Expected:** "ES IST ZEHN VOR HALB ZWÖLF"

```bash
# Set to 12:20
mosquitto_pub -t "home/wordclock/command" -m "set_time 12:20"
```
**Expected:** "ES IST ZEHN VOR HALB EINS"

### Test 5: NVS Persistence
```bash
# Enable HALB-centric
mosquitto_pub -t "home/wordclock/brightness/config/set" -m '{"halb_centric_style": true}'

# Restart device
mosquitto_pub -t "home/wordclock/command" -m "restart"

# Wait for reboot...
# Check if HALB-centric is still enabled
```
**Expected:** Switch stays ON in Home Assistant after reboot

### Test 6: Toggle During Same Minute
```bash
# Set to 10:20
mosquitto_pub -t "home/wordclock/command" -m "set_time 10:20"

# Display should show: "ES IST ZWANZIG NACH ZEHN"

# Toggle ON
mosquitto_pub -t "home/wordclock/brightness/config/set" -m '{"halb_centric_style": true}'

# Wait for next minute (10:21) - display should update
```
**Expected:** Display changes at 10:21 to "ES IST ZEHN VOR HALB ELF"

## Quick Test Script

Save as `test_halb_centric.sh`:

```bash
#!/bin/bash

MQTT_TOPIC_BASE="home/wordclock"
MQTT_HOST="your-mqtt-broker"  # Update this

echo "=== HALB-Centric Test Script ==="
echo ""

# Function to test a time
test_time() {
    local time=$1
    local expected_nach=$2
    local expected_halb=$3

    echo "Testing $time..."

    # Set time
    mosquitto_pub -h $MQTT_HOST -t "$MQTT_TOPIC_BASE/command" -m "set_time $time"
    sleep 1

    # NACH/VOR mode
    mosquitto_pub -h $MQTT_HOST -t "$MQTT_TOPIC_BASE/brightness/config/set" -m '{"halb_centric_style": false}'
    echo "  NACH/VOR: $expected_nach"
    sleep 2

    # HALB-centric mode
    mosquitto_pub -h $MQTT_HOST -t "$MQTT_TOPIC_BASE/brightness/config/set" -m '{"halb_centric_style": true}'
    echo "  HALB:     $expected_halb"
    sleep 2

    echo ""
}

# Test critical cases
test_time "10:20" "ZWANZIG NACH ZEHN" "ZEHN VOR HALB ELF"
test_time "10:40" "ZWANZIG VOR ELF" "ZEHN NACH HALB ELF"
test_time "10:45" "VIERTEL VOR ELF" "DREIVIERTEL ELF"
test_time "23:20" "ZWANZIG NACH ELF (PM)" "ZEHN VOR HALB ZWÖLF"
test_time "11:20" "ZWANZIG NACH ELF" "ZEHN VOR HALB ZWÖLF"
test_time "12:20" "ZWANZIG NACH ZWÖLF" "ZEHN VOR HALB EINS"

# Reset to NACH/VOR mode
mosquitto_pub -h $MQTT_HOST -t "$MQTT_TOPIC_BASE/brightness/config/set" -m '{"halb_centric_style": false}'

echo "=== Test Complete ==="
```

## Pass/Fail Criteria

### ✅ PASS:
- All 3 modified time slots (:20, :40, :45) show correct HALB-centric expressions
- Correct hour displayed (next hour for HALB-centric at :20, :40, :45)
- Indicator LEDs work correctly (0-4 LEDs)
- All unchanged time slots (:00, :05, :10, :15, :25, :30, :35, :50, :55) identical in both modes
- Configuration persists across reboots
- Home Assistant switch reflects correct state
- No display glitches during mode switching

### ❌ FAIL:
- Wrong hour displayed (e.g., "HALB ZEHN" instead of "HALB ELF" at 10:20)
- Missing or incorrect words
- Indicator LEDs incorrect
- Configuration lost after reboot
- Display corruption
- System crashes or reboots

## Known Limitations

1. **Display doesn't update immediately when toggling switch** - Display updates at next minute boundary (up to 60 seconds wait)
2. **No instant visual feedback** - Consider adding immediate refresh in future enhancement

## Bug Fixes Applied

- ✅ **Hour calculation bug fixed** (commit be9418e): HALB-centric :20, :40, :45 now correctly use next hour
