# ESP32 Word Clock Automated Testing Environment

## Overview

This testing environment provides comprehensive monitoring and automated testing capabilities for the ESP32 Word Clock. It captures both serial monitor output and MQTT messages, enabling effective debugging and validation.

**✅ Recently Updated:** MQTT topics now use dynamic device name configuration matching `mqtt_manager.h` - see [COMPATIBILITY_FIX.md](COMPATIBILITY_FIX.md) for details.

## Features

- **Dual Monitoring**: Simultaneous capture of ESP32 serial output and MQTT messages
- **Automated Testing**: Run predefined test suites with response validation
- **Log Analysis**: Python-based log analyzer for detailed insights
- **Real-time Viewing**: Live monitoring of all communication
- **Command Correlation**: Automatic correlation of commands with responses

## Directory Structure

```
test_environment/
├── README.md                      # This file
├── test_config.sh                 # Configuration (MQTT credentials, device name, serial port)
├── COMPATIBILITY_FIX.md           # MQTT topic compatibility documentation (NEW)
├── claude_code_test_suite.sh     # Claude Code automated test suite
├── setup_test_env.sh             # Environment setup script
├── scripts/
│   ├── start_monitoring.sh        # Start all monitors
│   ├── stop_monitoring.sh         # Stop all monitors
│   ├── run_tests.sh              # Run automated test suite
│   ├── quick_test.sh             # Send single command and check response
│   └── analyze_logs.py           # Analyze captured logs
└── logs/
    ├── serial_monitor.log        # ESP32 serial output
    ├── mqtt_messages.log         # MQTT messages
    ├── combined_output.log       # Both streams combined
    ├── test_results_*.json       # Test execution results
    └── claude_test_*/            # Claude Code test session directories
```

## Quick Start

### 1. Initial Setup

```bash
# Navigate to test environment
cd /home/tanihp/esp_projects/Stanis_Clock/test_environment

# Check/update configuration
nano test_config.sh

# IMPORTANT: Verify MQTT_DEVICE_NAME matches mqtt_manager.h
# Current device: Clock_Stani_1
# Topics will be: home/Clock_Stani_1/*

# Update SERIAL_PORT if needed (e.g., /dev/ttyUSB0)
```

**⚠️ Multi-Device Setup:** If you have multiple word clocks, each needs a unique `MQTT_DEVICE_NAME` in both:
- `components/mqtt_manager/include/mqtt_manager.h` (ESP32 firmware)
- `test_environment/test_config.sh` (test suite configuration)

### 2. Start Monitoring

```bash
cd scripts
./start_monitoring.sh
```

This will:
- Start serial monitoring (if ESP32 connected)
- Start MQTT monitoring
- Create screen sessions for real-time viewing

### 3. Run Tests

In another terminal:
```bash
cd scripts
./run_tests.sh
```

This executes the full test suite and saves results.

### 4. Quick Command Test

```bash
./quick_test.sh "status"
./quick_test.sh "set_time 14:30"
./quick_test.sh '{"command":"status"}'
```

### 5. Analyze Results

```bash
python3 analyze_logs.py
```

Generates detailed analysis report.

### 6. Stop Monitoring

```bash
./stop_monitoring.sh
```

## Claude Code Automated Test Suite

**NEW:** Standalone automated test suite designed for Claude Code execution.

### Quick Run

```bash
cd /home/tanihp/esp_projects/Stanis_Clock/test_environment
./claude_code_test_suite.sh
```

**What it does:**
1. Validates MQTT configuration (device name, topics)
2. Checks ESP32 availability via MQTT
3. Runs 6 automated tests (status, time setting, transitions, JSON commands)
4. Monitors responses in real-time
5. Generates comprehensive JSON test reports

**Output:** Timestamped session logs in `logs/claude_test_YYYYMMDD_HHMMSS/`

### Test Suite Features

- ✅ **Zero manual intervention** - Fully automated
- ✅ **Configuration validation** - Fails fast if topics don't match
- ✅ **Real-time monitoring** - Captures all MQTT traffic during tests
- ✅ **JSON structured results** - Machine-parsable test outcomes
- ✅ **Multi-device support** - Works with any configured device name

### Using with Claude Code (Traditional Scripts)

For traditional test scripts, provide this context:

```
I have an automated testing environment at:
/home/tanihp/esp_projects/Stanis_Clock/test_environment/

To use traditional scripts:
1. cd /home/tanihp/esp_projects/Stanis_Clock/test_environment/scripts
2. ./start_monitoring.sh  # Start capturing
3. ./run_tests.sh         # Run tests
4. cat ../logs/mqtt_messages.log  # View MQTT
5. cat ../logs/serial_monitor.log # View serial
6. python3 analyze_logs.py # Analyze results

To use Claude Code test suite:
1. cd /home/tanihp/esp_projects/Stanis_Clock/test_environment
2. ./claude_code_test_suite.sh  # Run automated tests
3. Check logs/claude_test_*/test_summary.json
```

## Configuration

Edit `test_config.sh` to update:

### Critical Settings (MUST Match Firmware)
- **`MQTT_DEVICE_NAME`**: Device name - **MUST match** `mqtt_manager.h` line 27
  - Current: `"Clock_Stani_1"`
  - Topics generated: `home/Clock_Stani_1/*`
  - **⚠️ Mismatch = 0% test success rate**

### MQTT Broker Settings
- `MQTT_BROKER`: HiveMQ Cloud broker hostname (already configured)
- `MQTT_PORT`: 8883 (TLS port)
- `MQTT_USERNAME`: esp32_led_device
- `MQTT_PASSWORD`: Already configured

### Hardware Settings
- `SERIAL_PORT`: Your ESP32 serial port (e.g., `/dev/ttyUSB0`)
- `SERIAL_BAUD`: 115200 (default)

### Test Timing
- `TEST_DELAY_MS`: Delay between test commands (3000ms default)
- `MONITOR_TIMEOUT`: Monitor timeout duration (300s default)

**See [COMPATIBILITY_FIX.md](COMPATIBILITY_FIX.md) for detailed multi-device setup instructions.**

## Test Suites

### Claude Code Automated Test Suite (`claude_code_test_suite.sh`)

**6 Automated Tests:**
1. **Device Status Check** - Basic connectivity validation
2. **Time Setting** - `set_time 14:30` command
3. **Start Transitions** - Animation system test
4. **Stop Transitions** - Animation control test
5. **JSON Status Command** - JSON parsing validation
6. **JSON Brightness Control** - Complex JSON command test

**Features:**
- Real-time MQTT monitoring during each test
- Individual test result logs (JSON format)
- Session summary with success rate
- Pre/post system health checks

### Traditional Test Suite (`scripts/run_tests.sh`)

**Includes:**
1. Device status command
2. Time setting
3. LED transition start/stop
4. JSON format commands
5. Brightness control
6. Invalid command handling

**Difference:** Requires screen sessions and manual log analysis

## Log Files

All logs are saved with timestamps in the `logs/` directory:
- `serial_monitor.log`: Raw ESP32 serial output
- `mqtt_messages.log`: All MQTT messages with topics
- `combined_output.log`: Both streams with [MQTT] prefix
- `test_results_*.json`: Test execution results
- `analysis_report_*.json`: Detailed analysis reports

## Viewing Real-time Output

While monitoring is running:
```bash
# View combined output
screen -r combined_monitor

# View serial only
screen -r esp32_serial

# View MQTT only
tail -f ../logs/mqtt_messages.log
```

To detach from screen: `Ctrl+A` then `D`

## Troubleshooting

### Serial Port Not Found
1. Check ESP32 is connected: `ls /dev/tty*`
2. Update `SERIAL_PORT` in `test_config.sh`
3. Common ports: `/dev/ttyUSB0`, `/dev/ttyUSB1`, `/dev/ttyACM0`

### MQTT Connection Failed
1. Check internet connection
2. Verify MQTT credentials in `test_config.sh`
3. Check HiveMQ Cloud service status

### All Tests Failing (0% Success Rate)
**Most Common Cause:** Device name mismatch between firmware and test config

1. **Check firmware device name:**
   ```bash
   grep MQTT_DEVICE_NAME components/mqtt_manager/include/mqtt_manager.h
   # Should show: #define MQTT_DEVICE_NAME "Clock_Stani_1"
   ```

2. **Check test config device name:**
   ```bash
   grep MQTT_DEVICE_NAME test_environment/test_config.sh
   # Should show: export MQTT_DEVICE_NAME="Clock_Stani_1"
   ```

3. **If they don't match:**
   - Option A: Update `test_config.sh` to match firmware
   - Option B: Rebuild firmware with matching device name
   - See [COMPATIBILITY_FIX.md](COMPATIBILITY_FIX.md) for details

### No Serial Output
1. Check baud rate matches ESP32 (115200)
2. Ensure user has permission: `sudo usermod -a -G dialout $USER`
3. Logout and login again for group changes

## Advanced Usage

### Custom Test Commands

Add to `run_tests.sh`:
```bash
test_mqtt_command \
    "Test Name" \
    "command_to_send" \
    "expected_response" \
    wait_seconds
```

### Continuous Monitoring

For long-term monitoring:
```bash
nohup ./start_monitoring.sh &
```

### Export Logs

Create archive of test session:
```bash
tar -czf test_logs_$(date +%Y%m%d_%H%M%S).tar.gz ../logs/
```

## Integration with CI/CD

This environment can be integrated into CI/CD pipelines:
```bash
# Start monitoring
./start_monitoring.sh

# Flash ESP32
cd ../../ && idf.py flash

# Wait for boot
sleep 10

# Run tests
cd test_environment/scripts && ./run_tests.sh

# Check results
python3 analyze_logs.py

# Stop monitoring
./stop_monitoring.sh
```

## Notes

- Logs are append-only; clear manually if needed
- Screen sessions persist across SSH disconnections
- MQTT credentials are stored in plaintext (use secrets management in production)
- Serial monitoring requires physical ESP32 connection

## Recent Updates (January 2025)

### ✅ MQTT Compatibility Fix
**Issue:** Test suite used hardcoded `esp32_core` device name, but firmware uses `Clock_Stani_1`

**Resolution:**
- Added `MQTT_DEVICE_NAME` configuration to `test_config.sh`
- Updated all test scripts to use dynamic topic generation
- Topics now automatically match firmware configuration
- Added configuration validation to prevent silent failures

**Files Changed:**
- `test_config.sh` - Added device name and dynamic topics
- `claude_code_test_suite.sh` - Replaced hardcoded topics with variables
- `COMPATIBILITY_FIX.md` (NEW) - Complete documentation

**Impact:** Test suite now 100% compatible with current MQTT implementation

**For Multi-Device Networks:** Simply change `MQTT_DEVICE_NAME` in `test_config.sh` to test different word clocks - no code changes needed!

See [COMPATIBILITY_FIX.md](COMPATIBILITY_FIX.md) for full details.