#!/bin/bash

# Monitor MQTT responses for Step 3 testing
echo "Monitoring MQTT responses for Step 3 structured JSON testing..."
echo "=============================================================="

# Start monitoring in background
timeout 60 mosquitto_sub -h cb6aa3dcb8a14ab887e8f8f8f9c73e2f2a.s1.eu.hivemq.cloud -p 8883 --capath /etc/ssl/certs -u stani56 -P StanPC56# -t "home/esp32_core/#" -v > step3_responses.log 2>&1 &
MONITOR_PID=$!

# Wait a moment for monitor to start
sleep 2

# Run the tests
echo "Running Step 3 tests..."
./test_step3_structured_responses.sh

# Wait for responses
echo "Waiting for responses..."
sleep 15

# Stop monitoring
kill $MONITOR_PID 2>/dev/null

# Show results
echo "Step 3 Test Results:"
echo "==================="
cat step3_responses.log

echo
echo "Analysis: Looking for structured JSON responses..."
grep -E '\{"result":|"command":|"status":' step3_responses.log || echo "No structured JSON responses found"