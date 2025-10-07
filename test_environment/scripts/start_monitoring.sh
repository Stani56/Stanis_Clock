#!/bin/bash
# Start monitoring ESP32 serial output and MQTT messages

# Load configuration
source "$(dirname "$0")/../test_config.sh"

echo "🚀 Starting ESP32 Word Clock Monitoring Environment"
echo "=================================================="
echo ""

# Check for required tools
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo "❌ Error: $1 is not installed"
        echo "   Install with: $2"
        exit 1
    fi
}

check_tool "mosquitto_sub" "sudo apt install mosquitto-clients"
check_tool "screen" "sudo apt install screen"

# Create logs directory if it doesn't exist
mkdir -p "$LOGS_DIR"

# Clear previous logs
echo "🧹 Clearing previous logs..."
> "$SERIAL_LOG"
> "$MQTT_LOG"
> "$COMBINED_LOG"

# Function to monitor serial port
start_serial_monitor() {
    echo "📡 Starting serial monitor on $SERIAL_PORT..."
    
    # Check if serial port exists
    if [ ! -e "$SERIAL_PORT" ]; then
        echo "⚠️  Warning: Serial port $SERIAL_PORT not found"
        echo "   ESP32 may not be connected or using different port"
        echo "   Update SERIAL_PORT in test_config.sh"
        echo "   Common ports: /dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyACM0"
        echo ""
        echo "   Continuing without serial monitoring..."
        return 1
    fi
    
    # Start serial monitoring in background using screen
    screen -dmS esp32_serial bash -c "stty -F $SERIAL_PORT $SERIAL_BAUD && cat $SERIAL_PORT | tee -a $SERIAL_LOG $COMBINED_LOG"
    
    if [ $? -eq 0 ]; then
        echo "✅ Serial monitor started (screen session: esp32_serial)"
        return 0
    else
        echo "❌ Failed to start serial monitor"
        return 1
    fi
}

# Function to monitor MQTT messages
start_mqtt_monitor() {
    echo "🌐 Starting MQTT monitor..."
    
    # Start MQTT monitoring in background
    mosquitto_sub \
        -h "$MQTT_BROKER" \
        -p "$MQTT_PORT" \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        --capath /etc/ssl/certs \
        -t "$MONITOR_TOPICS" \
        -v 2>&1 | while read line; do
            echo "[MQTT] $line" | tee -a "$MQTT_LOG" "$COMBINED_LOG"
        done &
    
    MQTT_PID=$!
    echo $MQTT_PID > "$LOGS_DIR/mqtt_monitor.pid"
    
    echo "✅ MQTT monitor started (PID: $MQTT_PID)"
}

# Function to monitor combined output
start_combined_monitor() {
    echo "📊 Starting combined output monitor..."
    
    # Tail the combined log in a new screen session for real-time viewing
    screen -dmS combined_monitor bash -c "tail -f $COMBINED_LOG"
    
    echo "✅ Combined monitor started (screen session: combined_monitor)"
}

# Start all monitors
echo ""
start_serial_monitor
SERIAL_STATUS=$?

echo ""
start_mqtt_monitor

echo ""
start_combined_monitor

# Save monitor status
cat > "$LOGS_DIR/monitor_status.txt" << EOF
Monitor Status - $(date)
========================
Serial Monitor: $([ $SERIAL_STATUS -eq 0 ] && echo "Running" || echo "Not running")
MQTT Monitor: Running (PID: $(cat $LOGS_DIR/mqtt_monitor.pid 2>/dev/null || echo "unknown"))
Combined Monitor: Running

Log Files:
- Serial: $SERIAL_LOG
- MQTT: $MQTT_LOG
- Combined: $COMBINED_LOG
EOF

echo ""
echo "✅ Monitoring environment started!"
echo ""
echo "📋 View monitors:"
echo "   - Serial:   screen -r esp32_serial"
echo "   - Combined: screen -r combined_monitor"
echo "   - MQTT:     tail -f $MQTT_LOG"
echo ""
echo "🛑 To stop monitoring: ./stop_monitoring.sh"
echo ""
echo "📊 Logs are being saved to:"
echo "   $LOGS_DIR/"