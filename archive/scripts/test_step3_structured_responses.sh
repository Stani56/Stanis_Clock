#!/bin/bash

# Test Step 3 - Structured JSON Responses
echo "Testing Step 3: Structured JSON Responses"
echo "=========================================="

# Test 1: Status command with structured response
echo "Test 1: JSON Status Command (expecting structured response)"
echo '{"command":"status"}' | mosquitto_pub -h cb6aa3dcb8a14ab887e8f8f8f9c73e2f2a.s1.eu.hivemq.cloud -p 8883 --capath /etc/ssl/certs -u stani56 -P StanPC56# -t home/esp32_core/command -l

# Test 2: Brightness command with structured response 
echo "Test 2: JSON Brightness Command (expecting structured response)"
echo '{"command":"set_brightness","parameters":{"individual":75,"global":180}}' | mosquitto_pub -h cb6aa3dcb8a14ab887e8f8f8f9c73e2f2a.s1.eu.hivemq.cloud -p 8883 --capath /etc/ssl/certs -u stani56 -P StanPC56# -t home/esp32_core/command -l

# Test 3: Invalid brightness (expecting validation)
echo "Test 3: Invalid Brightness (expecting validation error)"
echo '{"command":"set_brightness","parameters":{"individual":999}}' | mosquitto_pub -h cb6aa3dcb8a14ab887e8f8f8f9c73e2f2a.s1.eu.hivemq.cloud -p 8883 --capath /etc/ssl/certs -u stani56 -P StanPC56# -t home/esp32_core/command -l

echo "Tests sent. Check MQTT monitor for structured JSON responses."