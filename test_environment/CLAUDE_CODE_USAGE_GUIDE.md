# Claude Code Real-World Testing Usage Guide

## Quick Reference for Future Sessions

### 🚀 **Instant Test Execution**

For any Claude Code session, simply execute:

```bash
cd /home/tanihp/esp_projects/wordclock/test_environment
./claude_code_test_suite.sh
```

This runs a complete automated test suite with real ESP32 monitoring.

### 📋 **Copy-Paste Context for New Sessions**

Provide this context to Claude Code:

```
ESP32 Word Clock Real-World Testing Environment

Location: /home/tanihp/esp_projects/wordclock/test_environment/
Quick Test: ./claude_code_test_suite.sh
MQTT Config: ../mqtt_config.sh (credentials included)

ESP32 Status Check:
source ../mqtt_config.sh && timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/availability" -v

Send Command:
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/command" -m "status"

Monitor Response:
timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/+" -v

Testing Framework: CLAUDE_CODE_TESTING_FRAMEWORK.md
```

## Available Test Commands

### **Individual Command Testing**
```bash
# Test any single command
source ../mqtt_config.sh

# Monitor and send command
( timeout 10 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/+" -v > test_output.log 2>&1 & SUB_PID=$!; sleep 2; mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/command" -m "COMMAND_HERE"; sleep 8; kill $SUB_PID 2>/dev/null )

# View results
cat test_output.log
```

### **Available Commands**
- `status` - Device status
- `restart` - Restart device  
- `reset_wifi` - Clear WiFi credentials
- `set_time HH:MM` - Set time
- `test_transitions_start` - Start LED transitions
- `test_transitions_stop` - Stop LED transitions
- `{"command":"status"}` - JSON format
- `{"command":"set_brightness","parameters":{"individual":50,"global":180}}` - JSON brightness

## Proven Testing Capabilities

### ✅ **Restart Command Test (Verified 2025-08-16)**
- **Command Sent**: `restart`
- **ESP32 Response**: `home/esp32_core/status restarting`
- **System Recovery**: Automatic reconnection within 10 seconds
- **Test Result**: Complete success with full monitoring

### ✅ **Autonomous Testing Proven**
Claude Code can independently:
- Check ESP32 online status
- Send MQTT commands
- Monitor real-time responses
- Analyze system behavior
- Generate test reports
- Validate system recovery

## File Structure

```
test_environment/
├── claude_code_test_suite.sh      # Main automated test suite
├── CLAUDE_CODE_USAGE_GUIDE.md     # This file
├── test_config.sh                 # MQTT configuration
├── logs/                          # Test session logs
│   └── claude_test_YYYYMMDD_HHMMSS/
│       ├── test_summary.json      # JSON test results
│       ├── *_monitor.log          # Individual test monitoring
│       ├── *_result.log           # Individual test results
│       ├── pre_test_activity.log  # System state before tests
│       └── post_test_activity.log # System state after tests
└── scripts/                       # Additional testing tools
```

## Test Output Example

```
🤖 Claude Code Automated Test Suite for ESP32 Word Clock
========================================================
Session: claude_test_20250816_224530
Logs: logs/claude_test_20250816_224530

🔍 Pre-Test System Check
========================
Checking ESP32 availability...
✅ ESP32 is online

🚀 Executing Test Suite
=======================

🧪 Test: Device Status Check
📤 Command: status
⏱️  Timeout: 8s
✅ PASSED
📨 Responses captured:
   home/esp32_core/command status
   home/esp32_core/status heartbeat

📊 Test Session Summary
=======================
Total Tests: 7
Passed: 7
Failed: 0
Success Rate: 100%
```

## Integration with Development Workflow

### **For Firmware Testing**
1. Flash new firmware
2. Run: `./claude_code_test_suite.sh`
3. Review results in logs/
4. Validate all features working

### **For Bug Investigation**
1. Reproduce issue with specific commands
2. Use individual test templates
3. Capture detailed logs
4. Analyze response patterns

### **For Performance Monitoring**
1. Run periodic automated tests
2. Track response times
3. Monitor system stability
4. Generate trend reports

## Success Indicators

### **Test Passes When:**
- ✅ ESP32 acknowledges command (echo in logs)
- ✅ Expected response received within timeout
- ✅ System state changes as expected
- ✅ No error messages in logs
- ✅ Device maintains normal operation

### **Test Fails When:**
- ❌ No command acknowledgment
- ❌ Expected response not received
- ❌ Error messages in logs
- ❌ System becomes unresponsive
- ❌ Unexpected behavior observed

## Troubleshooting

### **ESP32 Offline**
```bash
# Check availability
source ../mqtt_config.sh
timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/availability" -v

# If no response: Check power, WiFi, serial connection
```

### **MQTT Connection Issues**
```bash
# Test MQTT connectivity
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "test/topic" -m "test"

# If fails: Check internet, credentials, broker status
```

### **No Test Responses**
- Verify ESP32 is running correct firmware
- Check topic names match ESP32 configuration
- Ensure command format is correct
- Increase timeout for slow responses

## Advanced Usage

### **Custom Test Creation**
Copy and modify the test template:
```bash
run_claude_test "Your Test Name" "your_command" "expected_response" timeout_seconds
```

### **Extended Monitoring**
```bash
# Monitor for extended period
timeout 60 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/+" -v > extended_monitor.log &
```

### **JSON Analysis**
```bash
# Analyze test results programmatically
python3 -c "
import json
with open('logs/claude_test_*/test_summary.json') as f:
    data = json.load(f)
    print(f'Success Rate: {data[\"success_rate\"]}%')
    print(f'Failed Tests: {data[\"failed_tests\"]}')
"
```

## Conclusion

This testing framework enables Claude Code to perform comprehensive, autonomous testing of the ESP32 Word Clock, providing professional-grade validation capabilities for IoT device development and maintenance.

The successful restart command test demonstrates that Claude Code can effectively serve as an automated testing agent, capable of real-world device interaction, monitoring, and analysis without human intervention.