#!/bin/bash
# Quick test for immediate command execution

# Load configuration
source "$(dirname "$0")/../test_config.sh"

# Check arguments
if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [expected_response]"
    echo ""
    echo "Examples:"
    echo "  $0 status"
    echo "  $0 'set_time 14:30'"
    echo "  $0 '{\"command\":\"status\"}' 'status'"
    exit 1
fi

COMMAND="$1"
EXPECTED="${2:-}"

echo "📤 Sending: $COMMAND"

# Mark log position
MQTT_BEFORE=$(wc -l < "$MQTT_LOG" 2>/dev/null || echo 0)
SERIAL_BEFORE=$(wc -l < "$SERIAL_LOG" 2>/dev/null || echo 0)

# Send command
mosquitto_pub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs \
    -t "$COMMAND_TOPIC" \
    -m "$COMMAND"

# Wait for response
sleep 2

# Get new logs
MQTT_AFTER=$(wc -l < "$MQTT_LOG" 2>/dev/null || echo 0)
SERIAL_AFTER=$(wc -l < "$SERIAL_LOG" 2>/dev/null || echo 0)

echo ""
echo "📨 Responses:"

if [ $MQTT_AFTER -gt $MQTT_BEFORE ]; then
    echo "MQTT:"
    tail -n $((MQTT_AFTER - MQTT_BEFORE)) "$MQTT_LOG" | sed 's/^/  /'
fi

if [ $SERIAL_AFTER -gt $SERIAL_BEFORE ]; then
    echo "Serial:"
    tail -n $((SERIAL_AFTER - SERIAL_BEFORE)) "$SERIAL_LOG" | sed 's/^/  /'
fi

# Check expected response
if [ -n "$EXPECTED" ]; then
    if tail -n $((MQTT_AFTER - MQTT_BEFORE)) "$MQTT_LOG" | grep -q "$EXPECTED" || \
       tail -n $((SERIAL_AFTER - SERIAL_BEFORE)) "$SERIAL_LOG" | grep -q "$EXPECTED"; then
        echo ""
        echo "✅ Found expected response: $EXPECTED"
    else
        echo ""
        echo "❌ Expected response not found: $EXPECTED"
    fi
fi