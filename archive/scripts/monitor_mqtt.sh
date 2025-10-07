#!/bin/bash
# Monitor MQTT Messages from ESP32 Word Clock

# Load configuration
if [ -f "mqtt_config.sh" ]; then
    source mqtt_config.sh
else
    echo "❌ mqtt_config.sh not found. Please create it with your credentials."
    exit 1
fi

echo "👀 Monitoring ESP32 Word Clock MQTT Messages"
echo "==========================================="
echo "Broker: $MQTT_BROKER:$MQTT_PORT"
echo "Topics: wordclock/+, home/esp32_core/+"
echo ""
echo "Press Ctrl+C to stop monitoring"
echo ""

mosquitto_sub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    --capath /etc/ssl/certs \
    -t "wordclock/+" \
    -t "home/esp32_core/+" \
    -v