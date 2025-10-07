# MQTT Testing Setup for ESP32 Word Clock

## Quick Start - Multiple Options

I've created several MQTT testing tools for you. Here are your options:

### Option 1: Install System MQTT Clients (Recommended)
```bash
sudo apt update
sudo apt install mosquitto-clients python3-paho-mqtt
```

After installation, test with:
```bash
./mqtt_quick_test.sh status
./mqtt_quick_test.sh monitor
```

### Option 2: Use Python Virtual Environment
```bash
python3 -m venv mqtt_env
source mqtt_env/bin/activate
pip install paho-mqtt
./mqtt_test_comprehensive.py --help
```

### Option 3: Manual mosquitto_pub Commands
If you just install mosquitto-clients:
```bash
# Send status command
mosquitto_pub -h your-broker.hivemq.cloud -p 8883 \
  -u username -P password --capath /etc/ssl/certs \
  -t "wordclock/command" -m "status"

# Monitor messages  
mosquitto_sub -h your-broker.hivemq.cloud -p 8883 \
  -u username -P password --capath /etc/ssl/certs \
  -t "wordclock/+" -t "home/esp32_core/+"
```

## Testing Tools Created

### 1. `mqtt_test_comprehensive.py` - Full-Featured Python Tester
```bash
# Interactive mode
./mqtt_test_comprehensive.py --broker broker.hivemq.cloud --username user --password pass

# Single command
./mqtt_test_comprehensive.py --broker broker.hivemq.cloud --username user --password pass --command "status"

# Automated test suite
./mqtt_test_comprehensive.py --broker broker.hivemq.cloud --username user --password pass --test

# Monitor mode
./mqtt_test_comprehensive.py --broker broker.hivemq.cloud --username user --password pass --monitor
```

### 2. `mqtt_quick_test.sh` - Simple Shell Script
```bash
# Edit the script first to add your HiveMQ credentials
nano mqtt_quick_test.sh

# Then use it
./mqtt_quick_test.sh status        # Get device status
./mqtt_quick_test.sh restart       # Restart device
./mqtt_quick_test.sh test_start     # Start transitions
./mqtt_quick_test.sh monitor       # Watch messages
./mqtt_quick_test.sh interactive   # Interactive mode
```

## Available Commands to Test

### Current Working Commands (should work now):
- `status` - Get device status
- `restart` - Restart the device
- `reset_wifi` - Clear WiFi credentials  
- `test_transitions_start` - Start transition testing
- `test_transitions_stop` - Stop transition testing
- `set_time 14:30` - Set time for testing

### Enhanced Commands (Tier 1 - currently disabled with #if 0):
```json
{"command": "status"}
{"command": "restart"}
{"command": "set_brightness", "parameters": {"individual": 50, "global": 180}}
{"command": "set_time", "parameters": {"time": "14:30"}}
```

## Configuration Steps

1. **Edit your credentials** in `mqtt_quick_test.sh`:
   ```bash
   BROKER="your-actual-broker.hivemq.cloud"
   USERNAME="your-username"  
   PASSWORD="your-password"
   ```

2. **Install MQTT client** (choose one):
   ```bash
   # System packages (recommended)
   sudo apt install mosquitto-clients python3-paho-mqtt
   
   # Or virtual environment
   python3 -m venv mqtt_env
   source mqtt_env/bin/activate
   pip install paho-mqtt
   ```

3. **Test the connection**:
   ```bash
   ./mqtt_quick_test.sh status
   ```

## Next Steps After Testing

1. **Flash the current build** to your ESP32
2. **Verify it works** the same as before (basic MQTT commands)
3. **Enable Tier 1 features** by changing `#if 0` to `#if 1` in mqtt_manager.c
4. **Rebuild and test** enhanced JSON commands

The system is designed for gradual rollout - current functionality preserved, enhanced features available when ready.