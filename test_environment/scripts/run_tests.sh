#!/bin/bash
# Run automated tests and capture results

# Load configuration
source "$(dirname "$0")/../test_config.sh"

# Test results file
TEST_RESULTS="$LOGS_DIR/test_results_$(date +%Y%m%d_%H%M%S).json"
TEST_SUMMARY="$LOGS_DIR/test_summary_$(date +%Y%m%d_%H%M%S).txt"

echo "🧪 ESP32 Word Clock Automated Test Suite"
echo "========================================"
echo ""

# Check if monitoring is running
check_monitoring() {
    if [ ! -f "$LOGS_DIR/mqtt_monitor.pid" ]; then
        echo "❌ Error: Monitoring not running!"
        echo "   Please run ./start_monitoring.sh first"
        exit 1
    fi
}

# Function to capture current log positions
mark_log_position() {
    SERIAL_POS=$(wc -l < "$SERIAL_LOG" 2>/dev/null || echo 0)
    MQTT_POS=$(wc -l < "$MQTT_LOG" 2>/dev/null || echo 0)
}

# Function to get new log entries since mark
get_new_logs() {
    local serial_new=$(tail -n +$((SERIAL_POS + 1)) "$SERIAL_LOG" 2>/dev/null)
    local mqtt_new=$(tail -n +$((MQTT_POS + 1)) "$MQTT_LOG" 2>/dev/null)
    
    echo "$serial_new" > "$LOGS_DIR/last_serial_response.txt"
    echo "$mqtt_new" > "$LOGS_DIR/last_mqtt_response.txt"
}

# Function to send MQTT command and capture response
test_mqtt_command() {
    local test_name="$1"
    local command="$2"
    local expected_response="$3"
    local wait_time="${4:-3}"
    
    echo ""
    echo "📋 Test: $test_name"
    echo "   Command: $command"
    
    # Mark current log position
    mark_log_position
    
    # Send command
    mosquitto_pub \
        -h "$MQTT_BROKER" \
        -p "$MQTT_PORT" \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs \
        -t "$COMMAND_TOPIC" \
        -m "$command" 2>&1
    
    # Wait for response
    sleep $wait_time
    
    # Get new log entries
    get_new_logs
    
    # Analyze response
    local serial_response=$(cat "$LOGS_DIR/last_serial_response.txt")
    local mqtt_response=$(cat "$LOGS_DIR/last_mqtt_response.txt")
    
    # Check for expected response
    local test_passed=false
    if [ -n "$expected_response" ]; then
        if echo "$serial_response" | grep -q "$expected_response" || \
           echo "$mqtt_response" | grep -q "$expected_response"; then
            test_passed=true
            echo "   ✅ PASSED - Found expected response"
        else
            echo "   ❌ FAILED - Expected response not found"
        fi
    else
        # If no expected response, just check if we got any response
        if [ -n "$serial_response" ] || [ -n "$mqtt_response" ]; then
            test_passed=true
            echo "   ✅ PASSED - Got response"
        else
            echo "   ⚠️  WARNING - No response detected"
        fi
    fi
    
    # Save test result
    cat >> "$TEST_RESULTS" << EOF
{
  "test_name": "$test_name",
  "command": "$command",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "passed": $test_passed,
  "serial_response_lines": $(echo "$serial_response" | wc -l),
  "mqtt_response_lines": $(echo "$mqtt_response" | wc -l)
},
EOF
    
    # Show response preview
    if [ -n "$mqtt_response" ]; then
        echo "   📨 MQTT Response:"
        echo "$mqtt_response" | head -5 | sed 's/^/      /'
    fi
    if [ -n "$serial_response" ]; then
        echo "   📟 Serial Response:"
        echo "$serial_response" | head -5 | sed 's/^/      /'
    fi
}

# Check monitoring is running
check_monitoring

# Initialize test results
echo "[" > "$TEST_RESULTS"

# Run test suite
echo "🚀 Starting test suite..."
echo "========================"

# Test 1: Status command
test_mqtt_command \
    "Device Status" \
    "status" \
    "status"

# Test 2: Set time
test_mqtt_command \
    "Set Time to 14:30" \
    "set_time 14:30" \
    "time.*14:30"

# Test 3: Transition test start
test_mqtt_command \
    "Start Transitions" \
    "test_transitions_start" \
    "transition.*start"

# Test 4: Wait and check transitions
sleep 5

# Test 5: Transition test stop
test_mqtt_command \
    "Stop Transitions" \
    "test_transitions_stop" \
    "transition.*stop"

# Test 6: JSON status command
test_mqtt_command \
    "JSON Status Command" \
    '{"command":"status"}' \
    "status"

# Test 7: JSON brightness command
test_mqtt_command \
    "JSON Set Brightness" \
    '{"command":"set_brightness","parameters":{"individual":50,"global":180}}' \
    "brightness"

# Test 8: Invalid command
test_mqtt_command \
    "Invalid Command" \
    "invalid_command_test" \
    "unknown.*command" \
    2

# Close JSON array
echo "]" >> "$TEST_RESULTS"

# Generate test summary
echo "" | tee "$TEST_SUMMARY"
echo "📊 Test Summary" | tee -a "$TEST_SUMMARY"
echo "===============" | tee -a "$TEST_SUMMARY"
echo "" | tee -a "$TEST_SUMMARY"

# Count pass/fail
TOTAL_TESTS=$(grep -c "test_name" "$TEST_RESULTS")
PASSED_TESTS=$(grep -c '"passed": true' "$TEST_RESULTS")
FAILED_TESTS=$(grep -c '"passed": false' "$TEST_RESULTS")

echo "Total Tests: $TOTAL_TESTS" | tee -a "$TEST_SUMMARY"
echo "Passed: $PASSED_TESTS ✅" | tee -a "$TEST_SUMMARY"
echo "Failed: $FAILED_TESTS ❌" | tee -a "$TEST_SUMMARY"
echo "" | tee -a "$TEST_SUMMARY"

# Save all logs
echo "📁 Test artifacts saved:" | tee -a "$TEST_SUMMARY"
echo "   - Results: $TEST_RESULTS" | tee -a "$TEST_SUMMARY"
echo "   - Summary: $TEST_SUMMARY" | tee -a "$TEST_SUMMARY"
echo "   - Serial log: $SERIAL_LOG" | tee -a "$TEST_SUMMARY"
echo "   - MQTT log: $MQTT_LOG" | tee -a "$TEST_SUMMARY"
echo "" | tee -a "$TEST_SUMMARY"

echo "✅ Test suite completed!"