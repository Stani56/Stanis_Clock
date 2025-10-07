# 🔍 User Troubleshooting Guide

Common issues and solutions for the ESP32 German Word Clock.

## 🚨 Emergency Quick Fixes

### Clock Not Working At All
1. **Check Power**: Verify 5V supply connected and ESP32 powered
2. **Check WiFi**: Look for "ESP32-LED-Config" network
3. **Reset Device**: Long press reset button (GPIO 5) for 5+ seconds
4. **Factory Reset**: Flash firmware again if all else fails

### Wrong Time Display
1. **Force NTP Sync**: Send MQTT command `force_ntp_sync`
2. **Check WiFi**: Verify internet connectivity
3. **Check Timezone**: Should be Vienna (CET/CEST)
4. **Manual Set**: Use `set_time HH:MM` command for testing

## 📶 WiFi Connection Issues

### Cannot Connect to Home WiFi

**Symptoms**: Clock creates "ESP32-LED-Config" network repeatedly
**Causes**: Wrong credentials, weak signal, unsupported security

**Solutions**:
1. **Reset WiFi Credentials**:
   ```bash
   # Via MQTT
   reset_wifi
   
   # Via hardware
   Long press reset button (GPIO 5) for 5+ seconds
   ```

2. **Check Network Compatibility**:
   - Supports WPA2/WPA3 Personal
   - 2.4GHz networks only (not 5GHz)
   - Hidden networks supported

3. **Signal Strength**:
   - Move closer to router during setup
   - Check for interference (microwave, etc.)
   - Consider WiFi extender if needed

4. **Reconfigure WiFi**:
   ```bash
   1. Connect to "ESP32-LED-Config" (password: 12345678)
   2. Open http://192.168.4.1
   3. Select your network from scan results
   4. Enter correct password
   5. Click "Connect"
   ```

### WiFi Keeps Disconnecting

**Symptoms**: Intermittent connectivity, frequent reconnections

**Solutions**:
1. **Check Router Settings**:
   - Disable power saving modes
   - Set fixed channel (not auto)
   - Increase DHCP lease time

2. **Power Supply Issues**:
   - Use stable 5V supply (2A minimum)
   - Check for voltage drops under load
   - Add power supply filtering

3. **Router Compatibility**:
   - Update router firmware
   - Check maximum client limits
   - Verify 2.4GHz band is enabled

## 🕐 Time Synchronization Issues

### Wrong Time Zone Display

**Symptoms**: Time is off by 1+ hours, ignores daylight saving

**Solutions**:
1. **Verify NTP Sync**:
   ```bash
   # Check status
   status
   
   # Force sync
   force_ntp_sync
   ```

2. **Check Home Assistant Display**:
   - "Last NTP Sync" sensor should show recent timestamp
   - Time should match your local Vienna time

3. **Firmware Issues**:
   - Ensure latest firmware (timezone fix included)
   - Check UTC formatting in HA timestamps

### NTP Sync Failing

**Symptoms**: "Last NTP Sync" shows old timestamp or empty

**Solutions**:
1. **Internet Connectivity**:
   ```bash
   # Test with manual sync
   force_ntp_sync
   
   # Check WiFi status
   status
   ```

2. **NTP Server Issues**:
   - Primary: pool.ntp.org
   - Secondary: time.google.com
   - Firmware retries automatically

3. **Firewall/Router Issues**:
   - NTP uses UDP port 123
   - Check router firewall settings
   - Some routers block NTP by default

## 💡 LED and Display Issues

### LEDs Not Lighting

**Symptoms**: No LED output, partial lighting, incorrect patterns

**Solutions**:
1. **Check Hardware**:
   - Verify 3.3V power to TLC59116 controllers
   - Check I2C bus connections (GPIO 25/26)
   - Verify TLC59116 address configuration

2. **I2C Communication**:
   ```bash
   # Firmware debug output should show:
   LEDs Bus (Port 0): 10 devices found
   - TLC59116 Controllers: 0x60, 0x61, 0x62, ...
   ```

3. **Test Mode**:
   ```bash
   # Test LED patterns
   test_transitions_start
   ```

### Brightness Issues

**Symptoms**: Too dim, too bright, not responding to controls

**Solutions**:
1. **Check Potentiometer**:
   - Verify GPIO 34 connection
   - Test potentiometer resistance (0-10kΩ range)
   - Check for loose connections

2. **Light Sensor Issues**:
   - Verify BH1750 connection (I2C address 0x23)
   - Check sensor placement (not blocked)
   - Test in different lighting conditions

3. **Home Assistant Control**:
   - Adjust brightness via HA light entity
   - Configure light sensor zones in HA
   - Check potentiometer configuration entities

### Wrong Words Displayed

**Symptoms**: Incorrect German words, missing letters, garbled display

**Solutions**:
1. **Check Time Setting**:
   ```bash
   # Set known time for testing
   set_time 14:30
   # Should display "ES IST HALB DREI"
   ```

2. **LED Matrix Issues**:
   - Verify complete word matrix wiring
   - Check for short circuits between LEDs
   - Test individual rows/columns

3. **Firmware Issues**:
   - Reflash latest firmware
   - Check for memory corruption
   - Verify German word layout mapping

## 🏠 Home Assistant Integration Issues

### Device Not Discovered

**Symptoms**: No "German Word Clock" device in HA

**Solutions**:
1. **MQTT Connection**:
   ```bash
   # Check MQTT status
   status
   
   # Should show: MQTT Status: Connected
   ```

2. **HA MQTT Integration**:
   - Verify MQTT broker configured in HA
   - Check discovery topic: `homeassistant/+/+/config`
   - Look for discovery messages in MQTT logs

3. **Manual Discovery**:
   - Restart HA MQTT integration
   - Send `restart` command to device
   - Check HA logs for MQTT discovery errors

### Missing Entities

**Symptoms**: Some of 37 entities missing from HA

**Solutions**:
1. **Check Entity Count**:
   - Should have 37 total entities
   - 12 core controls + 24 brightness + 1 timestamp

2. **Entity Naming**:
   - Look for "german_word_clock_" prefix
   - Check HA Developer Tools → States
   - Search for `sensor.german_word_clock_*`

3. **Force Rediscovery**:
   ```bash
   # Restart device to republish discovery
   restart
   ```

### Wrong Timestamp Display

**Symptoms**: "Last NTP Sync" shows "Unknown" or wrong time

**Solutions**:
1. **Timestamp Format**:
   - Should be UTC format: `2025-08-17T17:30:00Z`
   - HA displays in local timezone automatically

2. **Force Update**:
   ```bash
   # Force NTP sync to update timestamp
   force_ntp_sync
   ```

## 📡 MQTT Communication Issues

### Commands Not Working

**Symptoms**: MQTT commands return "unknown_command" or no response

**Solutions**:
1. **Check Topic and Format**:
   ```bash
   # Correct topic
   Topic: home/esp32_core/command
   
   # Valid commands
   status
   restart
   force_ntp_sync
   set_time 14:30
   ```

2. **MQTT Client Setup**:
   - Use QoS 0 for commands
   - No retain flag needed
   - UTF-8 encoding

3. **Connection Issues**:
   ```bash
   # Check device MQTT status
   status
   
   # Look for connection confirmations in logs
   ```

### No Status Updates

**Symptoms**: No heartbeat or status messages from device

**Solutions**:
1. **Check MQTT Topics**:
   ```bash
   # Monitor these topics
   home/esp32_core/status
   home/esp32_core/availability
   home/esp32_core/wifi
   home/esp32_core/ntp
   ```

2. **Subscription Issues**:
   - Device publishes every 60 seconds
   - Check MQTT broker logs
   - Verify topic subscriptions

## 🔄 System Recovery Procedures

### Complete Reset Procedure
1. **Hardware Reset**: Long press reset button (5+ seconds)
2. **WiFi Reconfiguration**: Connect to "ESP32-LED-Config"
3. **Verify Functionality**: Check time display and HA integration
4. **Test Commands**: Send basic MQTT commands

### Firmware Recovery
1. **Backup Settings**: Note current WiFi/MQTT configuration
2. **Flash Firmware**: Use latest build from main branch
3. **Reconfigure**: Set up WiFi and verify MQTT connection
4. **Test Integration**: Verify all 37 HA entities appear

### Factory Reset
```bash
# Complete factory reset via MQTT
reset_wifi

# Or via hardware
# Long press reset button until WiFi LED starts blinking rapidly
```

## 📞 Getting Additional Help

### Information to Collect
Before seeking help, gather:
1. **Firmware Version**: Check git commit hash in logs
2. **Error Messages**: Copy exact error text
3. **Configuration**: WiFi/MQTT setup details
4. **Environment**: HA version, MQTT broker type
5. **Hardware**: ESP32 board type, component versions

### Debug Information
```bash
# Get comprehensive status
status

# Check device discovery
# Look for "device found" messages in logs

# Test basic functionality
set_time 14:30
test_transitions_start
```

---
🔍 **Quick Resolution**: Most issues are WiFi/MQTT related  
⏱️ **Typical Fix Time**: 5-15 minutes  
🆘 **Last Resort**: Factory reset and reconfigure