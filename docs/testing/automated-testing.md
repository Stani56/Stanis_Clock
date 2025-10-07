# Automated Testing Environment for ESP32 Word Clock

## Overview

I've created a comprehensive automated testing environment that enables Claude Code (or any developer) to:
1. Monitor ESP32 serial output
2. Capture MQTT messages
3. Send test commands
4. Analyze responses
5. Generate reports

## Quick Start for Future Sessions

When you start a new Claude Code session, use this guide:

### 1. Navigate to Test Environment
```bash
cd /home/tanihp/esp_projects/wordclock/test_environment
```

### 2. Start Monitoring (if ESP32 is connected)
```bash
cd scripts
./start_monitoring.sh
```
This captures both serial and MQTT messages to log files.

### 3. Send Test Commands
```bash
# Quick single command test
./quick_test.sh "status"

# Run full test suite
./run_tests.sh

# Send JSON command
./quick_test.sh '{"command":"status"}'
```

### 4. View Captured Data
```bash
# View MQTT messages
cat ../logs/mqtt_messages.log

# View serial output
cat ../logs/serial_monitor.log

# View combined output
cat ../logs/combined_output.log
```

### 5. Analyze Results
```bash
python3 analyze_logs.py
```

## Testing Without Physical ESP32

Even without the ESP32 connected, you can:

### Test MQTT Communication
```bash
# Direct MQTT test
source ../mqtt_config.sh
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
  --capath /etc/ssl/certs -t "home/esp32_core/command" -m "status"

# Monitor MQTT responses
mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
  --capath /etc/ssl/certs -t "home/esp32_core/+" -v
```

## Key Files and Locations

### Configuration
- **MQTT Credentials**: `/home/tanihp/esp_projects/wordclock/mqtt_config.sh`
  ```bash
  MQTT_BROKER="5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud"
  MQTT_USERNAME="esp32_led_device"
  MQTT_PASSWORD="tufcux-3xuwda-Vomnys"
  ```

### Test Environment
- **Location**: `/home/tanihp/esp_projects/wordclock/test_environment/`
- **Scripts**: `test_environment/scripts/`
- **Logs**: `test_environment/logs/`

### Topics
- **Command**: `home/esp32_core/command`
- **Status**: `home/esp32_core/status`
- **Monitor**: `home/esp32_core/+` and `wordclock/+`

## Test Commands Reference

### Basic Commands
- `status` - Get device status
- `restart` - Restart device
- `reset_wifi` - Clear WiFi credentials
- `set_time HH:MM` - Set time (e.g., `set_time 14:30`)
- `test_transitions_start` - Start LED transitions
- `test_transitions_stop` - Stop LED transitions

### JSON Commands (Tier 1)
```json
{"command": "status"}
{"command": "restart"}
{"command": "set_brightness", "parameters": {"individual": 50, "global": 180}}
{"command": "set_time", "parameters": {"time": "14:30"}}
```

## Automated Test Suite

The `run_tests.sh` script automatically:
1. Sends multiple test commands
2. Captures responses
3. Validates expected results
4. Generates JSON test report
5. Produces summary statistics

## For CI/CD Integration

```bash
#!/bin/bash
# Example CI/CD test script

# Start monitoring
cd test_environment/scripts
./start_monitoring.sh

# Flash ESP32 (if connected)
cd ../.. && idf.py flash monitor &
MONITOR_PID=$!

# Wait for boot
sleep 30

# Run tests
cd test_environment/scripts
./run_tests.sh

# Analyze results
python3 analyze_logs.py

# Check test results
if grep -q '"passed": false' ../logs/test_results_*.json; then
    echo "Tests failed!"
    exit 1
fi

# Cleanup
./stop_monitoring.sh
kill $MONITOR_PID
```

## Current System Status

As of the last test:
- ✅ ESP32 is online and publishing to MQTT
- ✅ WiFi connected
- ✅ NTP synchronized
- ✅ MQTT broker connection working
- ✅ Home Assistant discovery active (39 entities)
- ✅ Brightness control active
- ✅ Transition system enabled

## Troubleshooting

### No Serial Output
- Check `SERIAL_PORT` in `test_config.sh`
- Common ports: `/dev/ttyUSB0`, `/dev/ttyUSB1`, `/dev/ttyACM0`
- Ensure user permissions: `sudo usermod -a -G dialout $USER`

### MQTT Connection Issues
- Verify credentials in `mqtt_config.sh`
- Check internet connection
- Test with: `mosquitto_pub -h [broker] -p 8883 -u [user] -P [pass] --capath /etc/ssl/certs -t test -m test`

### Screen Sessions
- View: `screen -r esp32_serial` or `screen -r combined_monitor`
- Detach: `Ctrl+A` then `D`
- List: `screen -ls`

## Summary

This automated testing environment provides everything needed for comprehensive ESP32 Word Clock testing:
- Real-time monitoring of serial and MQTT
- Automated test execution
- Response validation
- Detailed analysis and reporting

All tools are command-line based and perfect for use with Claude Code or CI/CD pipelines.