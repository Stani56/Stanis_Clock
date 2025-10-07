#!/bin/bash
# Test MQTT Commands for ESP32 Word Clock

# Load configuration
if [ -f "mqtt_config.sh" ]; then
    source mqtt_config.sh
else
    echo "❌ mqtt_config.sh not found. Please create it with your credentials."
    exit 1
fi

# Check if mosquitto_pub is available
if ! command -v mosquitto_pub &> /dev/null; then
    echo "❌ mosquitto_pub not found. Please install: sudo apt install mosquitto-clients"
    exit 1
fi

echo "🔧 ESP32 Word Clock MQTT Tester"
echo "================================"
echo "Broker: $MQTT_BROKER:$MQTT_PORT"
echo ""

# Function to send command and show what we're doing
send_mqtt_command() {
    local command="$1"
    local description="$2"
    
    echo "📤 $description"
    echo "   Command: $command"
    
    mosquitto_pub \
        -h "$MQTT_BROKER" \
        -p "$MQTT_PORT" \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs \
        -t "$MQTT_COMMAND_TOPIC" \
        -m "$command" \
        -d 2>&1 | grep -E "(CONNECT|CONNACK|PUBLISH|Error|refused)"
    
    echo ""
    sleep 2
}

# Test 1: Simple status command
send_mqtt_command "status" "Testing device status command"

# Test 2: Set time command
send_mqtt_command "set_time 14:30" "Testing time setting to 14:30"

# Test 3: Start transition test
send_mqtt_command "test_transitions_start" "Starting transition test mode"

# Test 4: Wait a bit then stop
sleep 5
send_mqtt_command "test_transitions_stop" "Stopping transition test mode"

# Test 5: Final status check
send_mqtt_command "status" "Final status check"

echo "✅ Test sequence complete!"
echo ""
echo "To monitor responses, run in another terminal:"
echo "mosquitto_sub -h $MQTT_BROKER -p $MQTT_PORT -u $MQTT_USERNAME -P $MQTT_PASSWORD --capath /etc/ssl/certs -t 'wordclock/+' -t 'home/esp32_core/+'"