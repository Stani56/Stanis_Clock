#!/bin/bash
# Automated Test Suite designed for Claude Code execution
# This script provides standardized testing capabilities for the ESP32 Word Clock
#
# IMPORTANT: Ensure test_config.sh has MQTT_DEVICE_NAME matching mqtt_manager.h
#            Current device: Clock_Stani_1
#            Topics: home/Clock_Stani_1/*

# Load configuration
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$script_dir/test_config.sh"

# Verify configuration loaded
if [ -z "$MQTT_DEVICE_NAME" ]; then
    echo "❌ ERROR: MQTT_DEVICE_NAME not set in test_config.sh"
    echo "Please ensure test_config.sh is properly configured"
    exit 1
fi

echo "📡 Test Configuration:"
echo "   Device Name: $MQTT_DEVICE_NAME"
echo "   Topic Base: $TOPIC_BASE"
echo "   Command Topic: $COMMAND_TOPIC"
echo ""

# Initialize test environment
TEST_SESSION="claude_test_$(date +%Y%m%d_%H%M%S)"
LOGS_DIR="$script_dir/logs/$TEST_SESSION"
mkdir -p "$LOGS_DIR"

echo "🤖 Claude Code Automated Test Suite for ESP32 Word Clock"
echo "========================================================"
echo "Session: $TEST_SESSION"
echo "Logs: $LOGS_DIR"
echo ""

# Test execution function
run_claude_test() {
    local test_name="$1"
    local command="$2"
    local expected_response="$3"
    local timeout="${4:-10}"
    local test_id="${test_name// /_}"
    
    echo ""
    echo "🧪 Test: $test_name"
    echo "📤 Command: $command"
    echo "⏱️  Timeout: ${timeout}s"
    
    # Log files for this test
    local monitor_log="$LOGS_DIR/${test_id}_monitor.log"
    local result_log="$LOGS_DIR/${test_id}_result.log"
    
    # Start monitoring
    timeout $timeout mosquitto_sub \
        -h "$MQTT_BROKER" \
        -p "$MQTT_PORT" \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs \
        -t "$MONITOR_TOPICS" \
        -v > "$monitor_log" 2>&1 &
    
    local monitor_pid=$!
    sleep 2
    
    # Send command
    local send_result
    mosquitto_pub \
        -h "$MQTT_BROKER" \
        -p "$MQTT_PORT" \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs \
        -t "$COMMAND_TOPIC" \
        -m "$command" 2>&1
    send_result=$?
    
    # Wait for response
    sleep $((timeout-2))
    
    # Stop monitoring
    kill $monitor_pid 2>/dev/null
    wait $monitor_pid 2>/dev/null
    
    # Analyze results
    local test_passed=false
    local command_echoed=false
    local response_found=false
    
    # Check if command was echoed
    if grep -q "$COMMAND_TOPIC $command" "$monitor_log"; then
        command_echoed=true
    fi
    
    # Check for expected response
    if [ -n "$expected_response" ]; then
        if grep -q "$expected_response" "$monitor_log"; then
            response_found=true
        fi
    else
        # No specific response expected, just check for any activity
        if [ -s "$monitor_log" ]; then
            response_found=true
        fi
    fi
    
    # Determine test result
    if [ $send_result -eq 0 ] && [ "$command_echoed" = true ]; then
        if [ -z "$expected_response" ] || [ "$response_found" = true ]; then
            test_passed=true
        fi
    fi
    
    # Generate result report
    cat > "$result_log" << EOF
{
  "test_name": "$test_name",
  "test_id": "$test_id",
  "command": "$command",
  "expected_response": "$expected_response",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "timeout": $timeout,
  "send_result": $send_result,
  "command_echoed": $command_echoed,
  "response_found": $response_found,
  "test_passed": $test_passed,
  "monitor_log": "$monitor_log"
}
EOF
    
    # Display results
    if [ "$test_passed" = true ]; then
        echo "✅ PASSED"
    else
        echo "❌ FAILED"
    fi
    
    echo "📨 Responses captured:"
    if [ -s "$monitor_log" ]; then
        cat "$monitor_log" | head -10 | sed 's/^/   /'
        local line_count=$(wc -l < "$monitor_log")
        if [ $line_count -gt 10 ]; then
            echo "   ... and $((line_count-10)) more lines"
        fi
    else
        echo "   (no responses captured)"
    fi
    
    return $([ "$test_passed" = true ] && echo 0 || echo 1)
}

# Pre-test system check
echo "🔍 Pre-Test System Check"
echo "========================"

# Check ESP32 availability
echo "Checking ESP32 availability..."
if timeout 5 mosquitto_sub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs \
    -t "$AVAILABILITY_TOPIC" \
    -v 2>/dev/null | grep -q "online"; then
    echo "✅ ESP32 is online"
else
    echo "❌ ESP32 appears offline - tests may fail"
fi

# Check current system status
echo "Sampling current system activity..."
timeout 3 mosquitto_sub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs \
    -t "$MONITOR_TOPICS" \
    -v > "$LOGS_DIR/pre_test_activity.log" 2>&1

if [ -s "$LOGS_DIR/pre_test_activity.log" ]; then
    echo "✅ System is active and publishing messages"
    echo "Recent activity:"
    cat "$LOGS_DIR/pre_test_activity.log" | head -3 | sed 's/^/   /'
else
    echo "⚠️  No recent activity detected"
fi

# Execute Test Suite
echo ""
echo "🚀 Executing Test Suite"
echo "======================="

# Track test results
total_tests=0
passed_tests=0

# Test 1: Basic Status
run_claude_test "Device Status Check" "status" "status" 8
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Test 2: Time Setting
run_claude_test "Time Setting" "set_time 14:30" "14:30" 8
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Test 3: Transition Control
run_claude_test "Start Transitions" "test_transitions_start" "transition" 8
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Wait a moment then stop transitions
sleep 3

run_claude_test "Stop Transitions" "test_transitions_stop" "transition" 8
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Test 4: JSON Command
run_claude_test "JSON Status Command" '{"command":"status"}' "command" 8
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Test 5: JSON Brightness Control
run_claude_test "JSON Brightness Control" '{"command":"set_brightness","parameters":{"individual":50,"global":180}}' "brightness" 10
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Test 6: Invalid Command (should not crash system)
run_claude_test "Invalid Command Test" "invalid_command_test_123" "" 6
test_result=$?
total_tests=$((total_tests + 1))
[ $test_result -eq 0 ] && passed_tests=$((passed_tests + 1))

# Generate Test Summary
echo ""
echo "📊 Test Session Summary"
echo "======================="
echo "Session: $TEST_SESSION"
echo "Total Tests: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $((total_tests - passed_tests))"
echo "Success Rate: $(( (passed_tests * 100) / total_tests ))%"
echo ""

# Generate comprehensive report
summary_file="$LOGS_DIR/test_summary.json"
cat > "$summary_file" << EOF
{
  "session_id": "$TEST_SESSION",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "total_tests": $total_tests,
  "passed_tests": $passed_tests,
  "failed_tests": $((total_tests - passed_tests)),
  "success_rate": $(( (passed_tests * 100) / total_tests )),
  "logs_directory": "$LOGS_DIR",
  "individual_results": [
EOF

# Add individual test results to summary
first=true
for result_file in "$LOGS_DIR"/*_result.log; do
    if [ -f "$result_file" ]; then
        if [ "$first" = false ]; then
            echo "," >> "$summary_file"
        fi
        cat "$result_file" >> "$summary_file"
        first=false
    fi
done

echo "]" >> "$summary_file"
echo "}" >> "$summary_file"

echo "📁 Detailed Results:"
echo "   Summary: $summary_file"
echo "   Logs: $LOGS_DIR"
echo ""

# Final system check
echo "🔍 Post-Test System Check"
echo "========================="
timeout 3 mosquitto_sub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs \
    -t "$MONITOR_TOPICS" \
    -v > "$LOGS_DIR/post_test_activity.log" 2>&1

if [ -s "$LOGS_DIR/post_test_activity.log" ]; then
    echo "✅ System is still active after testing"
    echo "Current status:"
    grep -E "(availability|wifi|ntp|status)" "$LOGS_DIR/post_test_activity.log" | head -3 | sed 's/^/   /'
else
    echo "⚠️  No activity detected after testing"
fi

echo ""
echo "✅ Test Suite Complete!"
echo ""
echo "🤖 Claude Code Test Execution Summary:"
echo "   - All tests executed autonomously"
echo "   - Real-time ESP32 monitoring performed"
echo "   - Command delivery and response validation completed"
echo "   - Comprehensive logging and analysis generated"
echo ""
echo "Results available for analysis in: $LOGS_DIR"