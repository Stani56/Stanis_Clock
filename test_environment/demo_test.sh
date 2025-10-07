#!/bin/bash
# Demo script to show testing environment capabilities

cd "$(dirname "$0")"

echo "🎯 ESP32 Word Clock Testing Environment Demo"
echo "==========================================="
echo ""
echo "This demo will show how the testing environment works"
echo ""

# Check if monitoring is needed
if [ ! -f "logs/mqtt_monitor.pid" ]; then
    echo "📡 Starting monitoring environment..."
    cd scripts
    ./start_monitoring.sh
    cd ..
    sleep 3
    echo ""
fi

echo "📊 Current ESP32 Status:"
echo "----------------------"
cd scripts
./quick_test.sh "status" "status"
cd ..

echo ""
echo "🧪 Running Quick Tests:"
echo "---------------------"

# Test 1: Time setting
echo ""
echo "Test 1: Setting time to 14:30"
cd scripts
./quick_test.sh "set_time 14:30" "14:30"
cd ..

# Test 2: JSON command
echo ""
echo "Test 2: JSON status command"
cd scripts
./quick_test.sh '{"command":"status"}' "command"
cd ..

echo ""
echo "📈 Analyzing captured data..."
cd scripts
python3 analyze_logs.py | tail -20
cd ..

echo ""
echo "✅ Demo complete!"
echo ""
echo "📁 View detailed logs in: logs/"
echo "🔍 For real-time monitoring: screen -r combined_monitor"
echo "🛑 To stop monitoring: scripts/stop_monitoring.sh"