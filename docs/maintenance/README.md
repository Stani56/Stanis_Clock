# 🔧 Maintenance & Operations

Operational procedures, deployment guidelines, and maintenance information for the ESP32 German Word Clock.

## 🚀 Quick Deployment

### Production Deployment (5 Minutes)
```bash
# 1. Flash firmware
idf.py build flash

# 2. Configure WiFi
# Connect to "ESP32-LED-Config" → http://192.168.4.1

# 3. Verify operation
# Check HA entities: 37 should appear automatically

# 4. Test functionality
# Send MQTT command: "status"
```

## 📚 Maintenance Guides

| Guide | Description |
|-------|-------------|
| [Deployment Procedures](deployment.md) | Production deployment steps |
| [Rollback Procedures](rollback-procedures.md) | Emergency rollback instructions |
| [Monitoring](monitoring.md) | System health monitoring |
| [Security](security.md) | Security considerations and updates |
| [Changelog](changelog.md) | Version history and changes |

## 🔄 Deployment Procedures

### Standard Deployment
```bash
# Environment setup
. $HOME/esp/esp-idf/export.sh
cd wordclock

# Build verification
idf.py build
# Expected: Successful build with <70% flash usage

# Device preparation
idf.py flash
# Expected: Programming successful

# Initial configuration
# Connect to ESP32-LED-Config network
# Configure WiFi via web interface

# Verification checklist
✅ German time display working
✅ WiFi connected and stable  
✅ NTP synchronized (check status LED)
✅ MQTT connected to HiveMQ Cloud
✅ Home Assistant discovers 37 entities
✅ Brightness controls responsive
```

### Automated Deployment
```bash
# Scripted deployment for multiple devices
./deploy_production.sh [DEVICE_PORT]

# Features:
- Automatic firmware flashing
- WiFi configuration backup/restore
- Health check verification
- Rollback capability on failure
```

## 📊 System Monitoring

### Health Check Indicators

#### Physical Indicators
```bash
✅ GPIO 21 LED: WiFi Status
   - OFF: Disconnected
   - BLINKING: Connecting  
   - SOLID: Connected

✅ GPIO 22 LED: NTP Status
   - OFF: Not synchronized
   - BLINKING: Synchronizing
   - SOLID: Synchronized

✅ Word Display: Current time in German
✅ Brightness: Responsive to ambient light and potentiometer
```

#### MQTT Monitoring
```bash
# Real-time status monitoring
./monitor_mqtt.sh

# Key topics to monitor:
home/esp32_core/availability    # online/offline
home/esp32_core/status         # heartbeat every 60s
home/esp32_core/wifi           # connected/disconnected  
home/esp32_core/ntp            # synced/not_synced
home/esp32_core/ntp/last_sync  # UTC timestamp
```

#### Home Assistant Monitoring
```bash
# Device status in HA
Device: "German Word Clock"
Entities: 37 total
- 12 Core controls (light, sensors, switches, buttons)
- 24 Brightness configuration entities
- 1 NTP timestamp sensor

# Key entities to monitor:
sensor.german_word_clock_wifi_status
sensor.german_word_clock_ntp_status  
sensor.german_word_clock_ntp_last_sync
light.german_word_clock_display
```

### Performance Monitoring

#### System Metrics
```bash
Flash Usage: ~1.1MB / 2.5MB (56% used, 44% free)
RAM Usage: Monitored via ESP-IDF heap functions
I2C Reliability: >99% success rate (no timeouts)
Network Uptime: >95% (with auto-reconnect)
Command Response: <1 second average
```

#### Network Metrics
```bash
WiFi Signal Strength: Monitor RSSI
MQTT Connection: Persistent with auto-reconnect
NTP Sync Interval: 1 hour (3600000ms)
Heartbeat Interval: 60 seconds
Data Usage: <1KB/minute average
```

## ⚠️ Troubleshooting Procedures

### Standard Troubleshooting Workflow
```bash
1. Check Physical Indicators
   - Power supply stable (5V, 2A minimum)
   - Status LEDs indicating correct state
   - German time display visible

2. Verify Network Connectivity  
   - WiFi connection stable
   - Internet connectivity working
   - MQTT broker accessible

3. Test MQTT Communication
   - Send "status" command
   - Verify response within 1 second
   - Check all topics publishing

4. Validate Home Assistant Integration
   - 37 entities discovered and available
   - Entity states updating correctly
   - Commands processed successfully
```

### Emergency Recovery Procedures
```bash
# Level 1: Soft Reset
Topic: home/esp32_core/command
Payload: restart

# Level 2: WiFi Reset  
Topic: home/esp32_core/command
Payload: reset_wifi

# Level 3: Hardware Reset
Long press reset button (GPIO 5) for 5+ seconds

# Level 4: Firmware Recovery
idf.py erase_flash
idf.py flash
# Then reconfigure WiFi/MQTT
```

## 🔐 Security Maintenance

### Security Checklist
```bash
✅ TLS Encryption: All MQTT communication encrypted
✅ Certificate Validation: ESP32 certificate bundle current
✅ Credential Security: WiFi passwords in encrypted NVS
✅ Access Control: MQTT broker authentication required
✅ Network Security: No open debug interfaces
✅ Firmware Integrity: Verified build and flash process
```

### Security Updates
```bash
# Regular security maintenance
1. Monitor ESP-IDF security advisories
2. Update certificate bundle if needed
3. Rotate MQTT credentials periodically
4. Review network access logs
5. Update router firmware regularly
```

### Vulnerability Assessment
```bash
# Periodic security review
- Network exposure analysis
- Credential strength validation  
- Certificate expiration monitoring
- Access log review
- Firmware update assessment
```

## 📈 Performance Optimization

### System Performance Tuning
```bash
# I2C Optimization
- Conservative 100kHz timing for reliability
- Differential LED updates (95% I/O reduction)
- Direct device handles (modern ESP-IDF APIs)

# Memory Management
- Static allocation preferred
- Component isolation for memory safety
- Stack size optimization per task

# Network Optimization  
- Heartbeat interval tuning (60s default)
- MQTT QoS optimization for reliability
- Reconnection strategy refinement
```

### Capacity Planning
```bash
Current Resource Usage:
- Flash: 56% (1.1MB / 2.5MB partition)
- Available for updates: 44% (1.4MB)
- Component capacity: 25+ components successfully integrated
- Entity capacity: 37+ Home Assistant entities

Growth Considerations:
- Flash usage trend monitoring
- Component addition impact
- Network bandwidth scaling
- Processing capacity limits
```

## 🔄 Update Management

### Firmware Update Process
```bash
# Pre-update checklist
1. Backup current configuration
2. Test new firmware in development environment
3. Plan rollback procedure
4. Schedule maintenance window

# Update execution
1. Build and verify new firmware
2. Flash to device during maintenance window
3. Verify functionality post-update
4. Update documentation if needed

# Post-update validation
1. Verify all 37 HA entities functional
2. Test MQTT command processing
3. Confirm network connectivity stable
4. Validate time display accuracy
```

### Configuration Management
```bash
# Configuration backup
./backup_device_config.sh
# Saves: WiFi credentials, MQTT settings, brightness config

# Configuration restore
./restore_device_config.sh [BACKUP_FILE]
# Restores: All device settings and calibration

# Configuration versioning
# Track changes to default configurations
# Document configuration dependencies
# Maintain configuration migration procedures
```

## 📊 Operational Metrics

### Key Performance Indicators (KPIs)
```bash
System Availability: >99% uptime target
Command Response Time: <1 second average
Network Reconnection: <5 minutes maximum
NTP Sync Success Rate: >95%
I2C Communication: >99% success rate
HA Entity Availability: 100% discovery rate
```

### Reporting and Analytics
```bash
# Daily reports
- System uptime and availability
- Network connectivity statistics
- Command processing metrics
- Error rate trends

# Weekly reports  
- Performance trend analysis
- Resource usage patterns
- Security event summary
- Maintenance activity log

# Monthly reports
- Capacity planning review
- Security posture assessment
- Feature usage analytics
- System optimization opportunities
```

## 🆘 Emergency Procedures

### Critical Failure Response
```bash
# Device completely unresponsive
1. Check power supply and connections
2. Attempt hardware reset (reset button)
3. Reflash firmware if needed
4. Escalate to hardware diagnosis

# Network connectivity lost
1. Verify WiFi router operational
2. Check internet connectivity
3. Reset WiFi credentials if needed
4. Monitor for automatic reconnection

# Home Assistant integration broken
1. Restart HA MQTT integration
2. Send device restart command
3. Wait for entity rediscovery (2-3 minutes)
4. Manually trigger discovery if needed
```

### Business Continuity
```bash
# Backup device preparation
- Maintain spare ESP32 with firmware
- Document hardware configuration
- Keep backup of device settings
- Maintain component inventory

# Rapid deployment capability
- Pre-configured backup devices
- Automated deployment scripts  
- Configuration restore procedures
- Testing and validation checklists
```

---
🔧 **Scope**: Complete operational procedures for production deployment  
🎯 **Audience**: System administrators and maintenance personnel  
⏱️ **Deployment Time**: 5 minutes (automated) to 30 minutes (manual)