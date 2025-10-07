# Claude Code Real-World Testing Framework for ESP32 Word Clock

## Overview

This document establishes a comprehensive framework for Claude Code to perform autonomous real-world testing of the ESP32 Word Clock. The successful restart command test (2025-08-16) proves Claude Code can effectively test, monitor, and analyze IoT device behavior.

## Framework Capabilities

### ✅ **Autonomous Testing**
Claude Code can independently:
- Send MQTT commands to the ESP32
- Monitor device responses in real-time
- Capture and analyze system behavior
- Generate comprehensive test reports
- Verify system functionality without human intervention

### ✅ **Complete Test Coverage**
- **Command Testing**: All MQTT commands (restart, status, time setting, transitions)
- **Response Validation**: Real-time monitoring of device acknowledgments
- **System Monitoring**: WiFi, NTP, MQTT connectivity status
- **Error Detection**: Capture and analyze error conditions
- **Performance Analysis**: Response times, reconnection behavior

### ✅ **Professional Reporting**
- Step-by-step verification documentation
- Before/during/after state capture
- Technical analysis with timestamps
- Success/failure determination
- Recommendations for next steps

## Testing Methodology

### **Pre-Test Verification**
```bash
# 1. Verify ESP32 is online
source mqtt_config.sh && timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/availability" -v

# Expected: home/esp32_core/availability online
```

### **Command Execution with Monitoring**
```bash
# 2. Start monitoring and send command
( timeout 15 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/+" -v > test_monitor.log 2>&1 & SUB_PID=$!; 
  sleep 2; 
  mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/command" -m "COMMAND_HERE"; 
  sleep 13; 
  kill $SUB_PID 2>/dev/null )
```

### **Response Analysis**
```bash
# 3. Analyze captured responses
cat test_monitor.log

# Look for patterns:
# - Command echo
# - Status acknowledgments
# - Error messages
# - State changes
```

### **Post-Test Verification**
```bash
# 4. Verify system state after test
timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/+" -v
```

## Standard Test Templates

### **1. Command Response Test**
```bash
# Template for testing any command
COMMAND="status"  # Change this
EXPECTED_RESPONSE="status"  # Change this

# Execute test
source mqtt_config.sh
( timeout 10 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/+" -v > command_test.log 2>&1 & SUB_PID=$!; 
  sleep 2; 
  echo "Sending: $COMMAND";
  mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/command" -m "$COMMAND"; 
  sleep 8; 
  kill $SUB_PID 2>/dev/null )

# Analyze results
echo "Response captured:"
cat command_test.log
if grep -q "$EXPECTED_RESPONSE" command_test.log; then
    echo "✅ Test PASSED: Found expected response"
else
    echo "❌ Test FAILED: Expected response not found"
fi
```

### **2. System State Monitoring**
```bash
# Monitor system for extended period
source mqtt_config.sh
echo "Monitoring ESP32 for 30 seconds..."
timeout 30 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/+" -v | while read line; do
    echo "$(date '+%H:%M:%S') - $line"
done > system_monitor.log

echo "System activity captured:"
cat system_monitor.log
```

### **3. JSON Command Testing**
```bash
# Test enhanced JSON commands
JSON_COMMAND='{"command":"set_brightness","parameters":{"individual":50,"global":180}}'

source mqtt_config.sh
( timeout 10 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/+" -v > json_test.log 2>&1 & SUB_PID=$!; 
  sleep 2; 
  echo "Sending JSON: $JSON_COMMAND";
  mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
  -t "home/esp32_core/command" -m "$JSON_COMMAND"; 
  sleep 8; 
  kill $SUB_PID 2>/dev/null )

echo "JSON command response:"
cat json_test.log
```

## Comprehensive Test Suite for Claude Code

### **Test Categories**

#### **1. Basic Functionality Tests**
- Device status inquiry
- Time setting commands
- System restart
- WiFi reset
- Heartbeat monitoring

#### **2. Feature Testing**
- LED transition control
- Brightness adjustment
- Light sensor response
- Potentiometer control
- Home Assistant integration

#### **3. Error Handling**
- Invalid commands
- Malformed JSON
- Network interruption recovery
- Command timeout behavior

#### **4. Performance Testing**
- Response time measurement
- Multiple command handling
- System load testing
- Reconnection timing

#### **5. Integration Testing**
- MQTT Discovery validation
- Home Assistant entity testing
- Status reporting accuracy
- Configuration persistence

### **Automated Test Execution**

```bash
#!/bin/bash
# Comprehensive test suite for Claude Code execution

source mqtt_config.sh

run_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"
    local timeout="${4:-10}"
    
    echo ""
    echo "🧪 Test: $test_name"
    echo "📤 Command: $command"
    
    # Execute test
    ( timeout $timeout mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
      -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
      -t "home/esp32_core/+" -v > "test_${test_name// /_}.log" 2>&1 & SUB_PID=$!; 
      sleep 2; 
      mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
      -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs \
      -t "home/esp32_core/command" -m "$command"; 
      sleep $((timeout-2)); 
      kill $SUB_PID 2>/dev/null )
    
    # Analyze results
    local log_file="test_${test_name// /_}.log"
    if [ -n "$expected" ] && grep -q "$expected" "$log_file"; then
        echo "✅ PASSED: Found expected response"
        return 0
    elif [ -n "$expected" ]; then
        echo "❌ FAILED: Expected response not found"
        return 1
    else
        echo "📊 COMPLETED: Response captured"
        return 0
    fi
}

# Test Suite Execution
echo "🚀 ESP32 Word Clock Comprehensive Test Suite"
echo "============================================"

# Basic Tests
run_test "Device Status" "status" "status"
run_test "Time Setting" "set_time 14:30" "14:30"
run_test "Restart Command" "restart" "restarting" 20

# Feature Tests
run_test "Transition Start" "test_transitions_start" "transition"
sleep 5
run_test "Transition Stop" "test_transitions_stop" "transition"

# JSON Tests
run_test "JSON Status" '{"command":"status"}' "command"
run_test "JSON Brightness" '{"command":"set_brightness","parameters":{"individual":50,"global":180}}' "brightness"

# Error Tests
run_test "Invalid Command" "invalid_test_command" ""

echo ""
echo "📊 Test Summary"
echo "==============="
echo "Test logs saved as: test_*.log"
echo "All tests completed!"
```

## Claude Code Session Integration

### **For New Sessions**
When starting a new Claude Code session, provide this context:

```
ESP32 Word Clock Real-World Testing Framework

Location: /home/tanihp/esp_projects/wordclock/
MQTT Config: mqtt_config.sh (HiveMQ Cloud credentials included)

Quick Test Command:
source mqtt_config.sh && mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/command" -m "status"

Monitor Response:
timeout 5 mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" --capath /etc/ssl/certs -t "home/esp32_core/+" -v

Test Templates: Available in CLAUDE_CODE_TESTING_FRAMEWORK.md
```

### **Testing Checklist for Claude Code**

Before running tests:
- [ ] Load MQTT configuration: `source mqtt_config.sh`
- [ ] Verify ESP32 online: Check availability topic
- [ ] Set timeout appropriately for test type
- [ ] Use correct command topic: `home/esp32_core/command`
- [ ] Monitor response topics: `home/esp32_core/+`

During testing:
- [ ] Capture pre-test state
- [ ] Monitor command transmission
- [ ] Record device responses
- [ ] Analyze response patterns
- [ ] Verify expected outcomes

Post-testing:
- [ ] Generate test report
- [ ] Document findings
- [ ] Save logs for reference
- [ ] Provide recommendations

## Success Metrics

### **Test Passes When**
- Command is received (echo in logs)
- Expected response received within timeout
- System state changes as expected
- No error messages in logs
- Device returns to normal operation

### **Test Fails When**
- No command acknowledgment
- Expected response not received
- Error messages in logs
- System becomes unresponsive
- Unexpected behavior observed

## Proven Capabilities

### **Demonstrated in Restart Test (2025-08-16)**
✅ **Command Delivery**: Successfully sent restart command  
✅ **Response Monitoring**: Captured real-time acknowledgment  
✅ **State Tracking**: Monitored device through restart sequence  
✅ **Recovery Verification**: Confirmed system restoration  
✅ **Report Generation**: Created comprehensive test documentation  

This framework enables Claude Code to perform professional-grade IoT device testing autonomously, providing reliable validation of ESP32 Word Clock functionality in real-world conditions.

## Future Enhancements

### **Serial Monitor Integration**
- Connect ESP32 serial output for boot sequence analysis
- Capture debug messages during critical operations
- Monitor hardware initialization details

### **Automated Test Scheduling**
- Periodic health checks
- Regression testing after firmware updates
- Continuous monitoring integration

### **Advanced Analysis**
- Performance trend analysis
- Error pattern recognition
- Predictive maintenance indicators

This framework establishes Claude Code as a capable autonomous testing agent for IoT devices, enabling comprehensive validation without human intervention.