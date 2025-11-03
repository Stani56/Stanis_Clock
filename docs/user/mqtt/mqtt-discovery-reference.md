# MQTT Discovery Quick Reference

## ⚡ Critical Fixes Applied

### 🚨 Configuration Persistence Bug
```c
// ❌ BEFORE: Hardcoded defaults overwrite existing values
float thresholds[4] = {10.0, 50.0, 200.0, 500.0};

// ✅ AFTER: Load current values first  
float thresholds[4];
for (int i = 0; i < 4; i++) {
    light_range_config_t range_config;
    if (brightness_config_get_light_range(range_names[i], &range_config) == ESP_OK) {
        thresholds[i] = range_config.lux_max;  // Preserve current values
    }
}
```

### 🎯 Entity Ordering Solution
```c
// ✅ Strategic alphabetical naming for proper grouping
"A1. Animation: Fade In Curve"      // A-prefix = first
"1. Brightness: Very Dark Zone"     // 1-9 = middle  
"Z1. Action: Restart Device"        // Z-prefix = last
```

### 🎚️ Slider UI Compatibility
```c  
// ❌ BEFORE: Range too large (1200 lux) - showed as read-only
const float threshold_maxs[] = {30.0, 150.0, 400.0, 1200.0};

// ✅ AFTER: Reasonable range (800 lux) - shows as stepper input with arrows
const float threshold_maxs[] = {30.0, 150.0, 400.0, 800.0};
```

**Home Assistant UI Behavior:**
- **0-400 units**: Traditional slider
- **400-1000 units**: Stepper with up/down arrows (fully functional)
- **1000+ units**: May show as read-only value

## 📋 Implementation Checklist

### ✅ Configuration Handlers
- [ ] Load current values before processing updates
- [ ] Only update fields present in JSON
- [ ] Log configuration changes
- [ ] Save complete configuration after updates
- [ ] Publish updated status

### ✅ Entity Discovery  
- [ ] Use strategic naming (A-, 1-9, Z- prefixes)
- [ ] Choose range size for desired UI: <400 (slider) vs 400-1000 (stepper)
- [ ] Use non-overlapping ranges
- [ ] Include proper icons and units
- [ ] Add device association

### ✅ Status Publishing
- [ ] Publish complete current state
- [ ] Handle configuration read failures gracefully  
- [ ] Use appropriate JSON structure
- [ ] Match discovery configuration format
- [ ] Log publishing errors

### ✅ Entity Cleanup
- [ ] Clear old entity naming schemes
- [ ] Remove deprecated entities  
- [ ] Rate limit cleanup operations
- [ ] Test entity removal in HA

## 🔍 Quick Debug Commands

```bash
# Monitor MQTT discovery
mosquitto_sub -h broker -t "homeassistant/+/+/config" -v

# Test configuration updates  
mosquitto_pub -h broker -t "home/esp32_core/config/set" \
  -m '{"threshold_2": 350}'
  
# Check status publishing
mosquitto_sub -h broker -t "home/esp32_core/config/status" -v
```

## 🚨 Never Do This

- ❌ Initialize arrays with hardcoded defaults for partial updates
- ❌ Ignore Home Assistant's alphabetical entity sorting  
- ❌ Use overlapping or very large ranges (>1000 units)
- ❌ Publish empty/invalid JSON to status topics
- ❌ Forget to cleanup old entity names after changes

## ✅ Always Do This

- ✅ Load current configuration before applying changes
- ✅ Use strategic naming for entity organization
- ✅ Keep ranges reasonable and non-overlapping
- ✅ Log all configuration changes and errors
- ✅ Test partial updates thoroughly

---
*Quick Reference for ESP32 MQTT Discovery Implementation*