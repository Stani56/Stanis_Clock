#!/bin/bash
# Test Environment Configuration

# MQTT Settings (HiveMQ Cloud)
export MQTT_BROKER="5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud"
export MQTT_PORT="8883"
export MQTT_USERNAME="esp32_led_device"
export MQTT_PASSWORD="tufcux-3xuwda-Vomnys"

# Device Configuration (MUST match MQTT_DEVICE_NAME in mqtt_manager.h)
export MQTT_DEVICE_NAME="Clock_Stani_1"

# ESP32 Serial Settings
export SERIAL_PORT="/dev/ttyUSB0"  # Update this to your ESP32 port
export SERIAL_BAUD="115200"

# Log Files
export LOGS_DIR="$(dirname "${BASH_SOURCE[0]}")/logs"
export SERIAL_LOG="$LOGS_DIR/serial_monitor.log"
export MQTT_LOG="$LOGS_DIR/mqtt_messages.log"
export TEST_LOG="$LOGS_DIR/test_results.log"
export COMBINED_LOG="$LOGS_DIR/combined_output.log"

# Test Settings
export TEST_DELAY_MS=3000  # Delay between test commands
export MONITOR_TIMEOUT=300  # Monitor timeout in seconds

# Topics (dynamically generated from device name)
export TOPIC_BASE="home/$MQTT_DEVICE_NAME"
export COMMAND_TOPIC="$TOPIC_BASE/command"
export STATUS_TOPIC="$TOPIC_BASE/status"
export AVAILABILITY_TOPIC="$TOPIC_BASE/availability"
export MONITOR_TOPICS="$TOPIC_BASE/+"
