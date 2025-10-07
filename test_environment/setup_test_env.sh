#!/bin/bash
# ESP32 Word Clock Automated Testing Environment Setup
# This script sets up a complete testing environment with serial and MQTT monitoring

# Configuration
PROJECT_ROOT="/home/tanihp/esp_projects/wordclock"
TEST_ENV_DIR="$PROJECT_ROOT/test_environment"
LOGS_DIR="$TEST_ENV_DIR/logs"
SCRIPTS_DIR="$TEST_ENV_DIR/scripts"
CONFIG_FILE="$TEST_ENV_DIR/test_config.sh"

echo "🔧 Setting up ESP32 Word Clock Testing Environment"
echo "================================================"

# Create directory structure
echo "📁 Creating directory structure..."
mkdir -p "$LOGS_DIR"
mkdir -p "$SCRIPTS_DIR"

# Create test configuration file
echo "📝 Creating test configuration..."
cat > "$CONFIG_FILE" << 'EOF'
#!/bin/bash
# Test Environment Configuration

# MQTT Settings (HiveMQ Cloud)
export MQTT_BROKER="5a68d83582614d8898aeb655da0c5f33.s1.eu.hivemq.cloud"
export MQTT_PORT="8883"
export MQTT_USERNAME="esp32_led_device"
export MQTT_PASSWORD="tufcux-3xuwda-Vomnys"

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

# Topics
export COMMAND_TOPIC="home/esp32_core/command"
export STATUS_TOPIC="home/esp32_core/status"
export MONITOR_TOPICS="home/esp32_core/+ wordclock/+"
EOF

chmod +x "$CONFIG_FILE"

echo "✅ Testing environment setup complete!"
echo ""
echo "📂 Directory structure:"
echo "   $TEST_ENV_DIR/"
echo "   ├── setup_test_env.sh      (this script)"
echo "   ├── test_config.sh         (configuration)"
echo "   ├── scripts/               (test scripts)"
echo "   └── logs/                  (captured output)"
echo ""
echo "🔧 Next steps:"
echo "1. Update SERIAL_PORT in test_config.sh"
echo "2. Run ./start_monitoring.sh to begin capture"
echo "3. Run ./run_tests.sh to execute tests"
echo "4. View results in logs/ directory"