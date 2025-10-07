#!/bin/bash
# Real-world MQTT test with monitoring

source mqtt_config.sh

echo "🔍 ESP32 Word Clock Real-World MQTT Test"
echo "========================================"
echo ""

# Start monitoring in background and save to file
echo "📡 Starting MQTT monitor..."
mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs -t "wordclock/+" -t "home/esp32_core/+" -v > mqtt_responses.log 2>&1 &
MONITOR_PID=$!

# Give monitor time to connect
sleep 2

# Function to send command and wait for response
test_command() {
    local cmd="$1"
    local desc="$2"
    
    echo ""
    echo "📤 Testing: $desc"
    echo "   Command: $cmd"
    
    # Clear previous responses
    > mqtt_responses_temp.log
    
    # Send command
    mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs -t "wordclock/command" -m "$cmd"
    
    # Wait for responses
    sleep 3
    
    # Show recent responses
    echo "📨 Responses:"
    tail -n 20 mqtt_responses.log | grep -v "^$" | while IFS= read -r line; do
        echo "   $line"
    done
}

# Test 1: Status command
test_command "status" "Device Status Request"

# Test 2: Set time
test_command "set_time 14:30" "Set Time to 14:30"

# Test 3: Transition test
test_command "test_transitions_start" "Start LED Transition Test"
sleep 5
test_command "test_transitions_stop" "Stop LED Transition Test"

# Test 4: JSON command
test_command '{"command":"status"}' "JSON Status Command"

# Clean up
echo ""
echo "🛑 Stopping monitor..."
kill $MONITOR_PID 2>/dev/null

echo ""
echo "📊 Test Summary:"
echo "================"
echo "Total messages received:"
wc -l < mqtt_responses.log
echo ""
echo "Unique topics:"
cut -d' ' -f1 mqtt_responses.log | sort -u

echo ""
echo "✅ Real-world test complete!"
echo "Full log saved in: mqtt_responses.log"