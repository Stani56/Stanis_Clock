#!/bin/bash
# Local OTA verification test script
# Tests SHA-256 calculation matches between build system and device
# Run this BEFORE deploying new firmware to GitHub

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OTA_DIR="$PROJECT_ROOT/ota_files"

echo "================================================"
echo "OTA SHA-256 Verification Test"
echo "================================================"

# Check if firmware exists
if [ ! -f "$BUILD_DIR/wordclock.bin" ]; then
    echo "❌ Error: Firmware not found at $BUILD_DIR/wordclock.bin"
    echo "   Run 'idf.py build' first"
    exit 1
fi

# Get firmware size
FIRMWARE_SIZE=$(stat -c%s "$BUILD_DIR/wordclock.bin")
echo "📦 Firmware size: $FIRMWARE_SIZE bytes"

# Calculate SHA-256 of actual firmware
ACTUAL_SHA256=$(sha256sum "$BUILD_DIR/wordclock.bin" | cut -d' ' -f1)
echo "🔐 Calculated SHA-256: $ACTUAL_SHA256"

# Simulate partition verification (hash only firmware_size bytes)
# This mimics what the device does during OTA update
PARTITION_FILE="/tmp/ota_test_partition.bin"

# Create simulated partition (2.5MB like real OTA partition)
dd if=/dev/zero of="$PARTITION_FILE" bs=1M count=2 status=none
dd if=/dev/urandom of="$PARTITION_FILE" bs=1M count=2 conv=notrunc status=none

# Write firmware to start of partition
dd if="$BUILD_DIR/wordclock.bin" of="$PARTITION_FILE" conv=notrunc status=none

# Hash only firmware_size bytes (what device does)
DEVICE_SHA256=$(head -c $FIRMWARE_SIZE "$PARTITION_FILE" | sha256sum | cut -d' ' -f1)
echo "📱 Device-simulated SHA-256: $DEVICE_SHA256"

# Compare
if [ "$ACTUAL_SHA256" == "$DEVICE_SHA256" ]; then
    echo "✅ SHA-256 MATCH - OTA verification will succeed"
    echo ""
    echo "Safe to deploy:"
    echo "  ./post_build_ota.sh --auto-push"
    rm -f "$PARTITION_FILE"
    exit 0
else
    echo "❌ SHA-256 MISMATCH - OTA verification will FAIL"
    echo ""
    echo "Expected: $ACTUAL_SHA256"
    echo "Got:      $DEVICE_SHA256"
    echo ""
    echo "DO NOT DEPLOY - Fix SHA-256 calculation in ota_manager.c"
    rm -f "$PARTITION_FILE"
    exit 1
fi
