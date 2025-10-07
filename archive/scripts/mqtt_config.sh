#!/bin/bash
# MQTT Configuration for ESP32 Word Clock Testing
# 
# UPDATE THESE VALUES with your HiveMQ Cloud credentials:

export MQTT_BROKER="5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud"
export MQTT_PORT="8883"
export MQTT_USERNAME="esp32_led_device"
export MQTT_PASSWORD="tufcux-3xuwda-Vomnys"

# These topics match your wordclock configuration
export MQTT_COMMAND_TOPIC="wordclock/command"
export MQTT_STATUS_TOPIC="wordclock/status"
export MQTT_MONITOR_TOPICS="wordclock/+ home/esp32_core/+"