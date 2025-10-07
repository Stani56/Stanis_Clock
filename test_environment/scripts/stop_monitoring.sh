#!/bin/bash
# Stop all monitoring processes

# Load configuration
source "$(dirname "$0")/../test_config.sh"

echo "🛑 Stopping ESP32 Word Clock Monitoring Environment"
echo "=================================================="
echo ""

# Stop MQTT monitor
if [ -f "$LOGS_DIR/mqtt_monitor.pid" ]; then
    MQTT_PID=$(cat "$LOGS_DIR/mqtt_monitor.pid")
    if kill -0 $MQTT_PID 2>/dev/null; then
        kill $MQTT_PID
        echo "✅ Stopped MQTT monitor (PID: $MQTT_PID)"
    else
        echo "⚠️  MQTT monitor not running"
    fi
    rm -f "$LOGS_DIR/mqtt_monitor.pid"
else
    echo "⚠️  No MQTT monitor PID file found"
fi

# Stop serial monitor
if screen -list | grep -q "esp32_serial"; then
    screen -X -S esp32_serial quit
    echo "✅ Stopped serial monitor"
else
    echo "⚠️  Serial monitor not running"
fi

# Stop combined monitor
if screen -list | grep -q "combined_monitor"; then
    screen -X -S combined_monitor quit
    echo "✅ Stopped combined monitor"
else
    echo "⚠️  Combined monitor not running"
fi

echo ""
echo "📊 Log files preserved in:"
echo "   $LOGS_DIR/"
echo ""
echo "📋 Monitor status saved in:"
echo "   $LOGS_DIR/monitor_status.txt"