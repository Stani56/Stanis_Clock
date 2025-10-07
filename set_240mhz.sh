#!/bin/bash
# Script to configure ESP32 for 240MHz operation

echo "Configuring ESP32 for 240MHz CPU frequency..."

# Backup existing sdkconfig
cp sdkconfig sdkconfig.backup.160mhz

# Update CPU frequency settings
sed -i 's/CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160=y/# CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160 is not set/g' sdkconfig
sed -i 's/CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160/CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240/g' sdkconfig
sed -i 's/CONFIG_ESP32_DEFAULT_CPU_FREQ_160=y/# CONFIG_ESP32_DEFAULT_CPU_FREQ_160 is not set/g' sdkconfig
sed -i 's/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=160/CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=240/g' sdkconfig

# Add 240MHz configuration if not present
if ! grep -q "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y" sdkconfig; then
    echo "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y" >> sdkconfig
fi

if ! grep -q "CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y" sdkconfig; then
    echo "CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y" >> sdkconfig
fi

# Update FreeRTOS tick rate
sed -i 's/CONFIG_FREERTOS_HZ=100/CONFIG_FREERTOS_HZ=1000/g' sdkconfig

echo "Configuration updated. Please run:"
echo "  idf.py build"
echo "  idf.py flash monitor"