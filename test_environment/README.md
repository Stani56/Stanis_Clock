# ESP32 Word Clock Automated Testing Environment

## Overview

This testing environment provides comprehensive monitoring and automated testing capabilities for the ESP32 Word Clock. It captures both serial monitor output and MQTT messages, enabling effective debugging and validation.

## Features

- **Dual Monitoring**: Simultaneous capture of ESP32 serial output and MQTT messages
- **Automated Testing**: Run predefined test suites with response validation
- **Log Analysis**: Python-based log analyzer for detailed insights
- **Real-time Viewing**: Live monitoring of all communication
- **Command Correlation**: Automatic correlation of commands with responses

## Directory Structure

```
test_environment/
├── README.md              # This file
├── test_config.sh         # Configuration (MQTT credentials, serial port)
├── scripts/
│   ├── start_monitoring.sh    # Start all monitors
│   ├── stop_monitoring.sh     # Stop all monitors
│   ├── run_tests.sh          # Run automated test suite
│   ├── quick_test.sh         # Send single command and check response
│   └── analyze_logs.py       # Analyze captured logs
└── logs/
    ├── serial_monitor.log    # ESP32 serial output
    ├── mqtt_messages.log     # MQTT messages
    ├── combined_output.log   # Both streams combined
    └── test_results_*.json   # Test execution results
```

## Quick Start

### 1. Initial Setup

```bash
# Navigate to test environment
cd /home/tanihp/esp_projects/wordclock/test_environment

# Check/update configuration
nano test_config.sh
# Update SERIAL_PORT if needed (e.g., /dev/ttyUSB0)
```

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

## Using with Claude Code

For future Claude Code sessions, provide this context:

```
I have an automated testing environment at:
/home/tanihp/esp_projects/wordclock/test_environment/

To use it:
1. cd /home/tanihp/esp_projects/wordclock/test_environment/scripts
2. ./start_monitoring.sh  # Start capturing
3. ./run_tests.sh         # Run tests
4. cat ../logs/mqtt_messages.log  # View MQTT
5. cat ../logs/serial_monitor.log # View serial
6. python3 analyze_logs.py # Analyze results
```

## Configuration

Edit `test_config.sh` to update:
- `SERIAL_PORT`: Your ESP32 serial port
- `MQTT_*`: MQTT broker credentials (already configured)
- `TEST_DELAY_MS`: Delay between test commands
- `MONITOR_TIMEOUT`: Monitor timeout duration

## Test Suite

The automated test suite (`run_tests.sh`) includes:
1. Device status command
2. Time setting
3. LED transition start/stop
4. JSON format commands
5. Brightness control
6. Invalid command handling

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