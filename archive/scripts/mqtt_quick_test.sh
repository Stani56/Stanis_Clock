#!/bin/bash
# Quick MQTT Testing Script for ESP32 Word Clock
# Usage: ./mqtt_quick_test.sh [command]

# MQTT Configuration (update these with your HiveMQ Cloud details)
BROKER="your-hivemq-broker.hivemq.cloud"
PORT="8883"
USERNAME="your-username"
PASSWORD="your-password"

# Check if credentials are configured
if [[ "$BROKER" == "your-hivemq-broker.hivemq.cloud" ]]; then
    echo "⚠️  Please configure your HiveMQ Cloud credentials in this script:"
    echo "   Edit variables: BROKER, USERNAME, PASSWORD"
    echo "   Example:"
    echo "     BROKER=\"abc123.s1.eu.hivemq.cloud\""
    echo "     USERNAME=\"esp32_user\""
    echo "     PASSWORD=\"your_password\""
    exit 1
fi
TOPIC="wordclock/command"

# Function to send MQTT command
send_command() {
    local command="$1"
    echo "📤 Sending command: $command"
    
    # Try multiple MQTT clients in order of preference
    if command -v mosquitto_pub >/dev/null 2>&1; then
        mosquitto_pub -h "$BROKER" -p "$PORT" -u "$USERNAME" -P "$PASSWORD" \
                      --capath /etc/ssl/certs -t "$TOPIC" -m "$command"
    elif command -v mqttx >/dev/null 2>&1; then
        mqttx pub -h "$BROKER" -p "$PORT" -u "$USERNAME" -P "$PASSWORD" \
                  --protocol mqtts -t "$TOPIC" -m "$command"
    elif python3 -c "import paho.mqtt.client" 2>/dev/null; then
        python3 mqtt_test_comprehensive.py --broker "$BROKER" --port "$PORT" \
                --username "$USERNAME" --password "$PASSWORD" --command "$command"
    else
        echo "❌ No MQTT client found. Please install:"
        echo "   sudo apt install mosquitto-clients  # for mosquitto_pub"
        echo "   pip3 install paho-mqtt              # for Python client"
        exit 1
    fi
}

# Function to monitor messages
monitor_messages() {
    echo "👀 Monitoring wordclock messages (Ctrl+C to stop)..."
    
    if command -v mosquitto_sub >/dev/null 2>&1; then
        mosquitto_sub -h "$BROKER" -p "$PORT" -u "$USERNAME" -P "$PASSWORD" \
                      --capath /etc/ssl/certs -t "wordclock/+" -t "home/esp32_core/+"
    elif python3 -c "import paho.mqtt.client" 2>/dev/null; then
        python3 mqtt_test_comprehensive.py --broker "$BROKER" --port "$PORT" \
                --username "$USERNAME" --password "$PASSWORD" --monitor
    else
        echo "❌ No MQTT client found for monitoring"
        exit 1
    fi
}

# Main script logic
case "${1:-help}" in
    "status")
        send_command "status"
        ;;
    "restart")
        send_command "restart"
        ;;
    "reset_wifi")
        send_command "reset_wifi"
        ;;
    "test_start")
        send_command "test_transitions_start"
        ;;
    "test_stop")
        send_command "test_transitions_stop"
        ;;
    "time_test")
        send_command "set_time 14:30"
        sleep 2
        send_command "set_time 09:15"
        ;;
    "monitor")
        monitor_messages
        ;;
    "interactive")
        if python3 -c "import paho.mqtt.client" 2>/dev/null; then
            python3 mqtt_test_comprehensive.py --broker "$BROKER" --port "$PORT" \
                    --username "$USERNAME" --password "$PASSWORD"
        else
            echo "❌ Python paho-mqtt required for interactive mode"
            echo "Install with: pip3 install paho-mqtt"
            exit 1
        fi
        ;;
    "help"|*)
        echo "ESP32 Word Clock MQTT Quick Test"
        echo "================================="
        echo
        echo "Usage: $0 [command]"
        echo
        echo "Quick Commands:"
        echo "  status      - Get device status"
        echo "  restart     - Restart device"
        echo "  reset_wifi  - Clear WiFi credentials"
        echo "  test_start  - Start transition testing"
        echo "  test_stop   - Stop transition testing"
        echo "  time_test   - Test time setting"
        echo "  monitor     - Monitor all messages"
        echo "  interactive - Interactive mode (requires paho-mqtt)"
        echo "  help        - Show this help"
        echo
        echo "Configuration:"
        echo "  Edit this script to set your HiveMQ Cloud credentials"
        echo "  BROKER, USERNAME, PASSWORD variables at the top"
        echo
        echo "Requirements:"
        echo "  mosquitto-clients OR paho-mqtt"
        echo "  Install: sudo apt install mosquitto-clients"
        echo "  Or:      pip3 install paho-mqtt"
        ;;
esac