#!/bin/bash
source mqtt_config.sh

# Start monitoring in background, save to file
mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs -t "wordclock/+" -t "home/esp32_core/+" -v >> monitor_output.txt 2>&1 &
MONITOR_PID=$!

sleep 2

# Send status command
echo "Sending status command..."
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs -t "wordclock/command" -m "status"

sleep 3

# Send time command
echo "Sending time set command..."
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs -t "wordclock/command" -m "set_time 14:30"

sleep 3

# Kill monitor
kill $MONITOR_PID 2>/dev/null

# Show results
echo "Messages received:"
cat monitor_output.txt