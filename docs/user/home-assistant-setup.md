# German Word Clock - Home Assistant User Guide

## Overview
Your ESP32 German Word Clock integrates seamlessly with Home Assistant, providing **40 entities** for complete device control. This guide walks you through every control and how to use them effectively.

## Quick Setup

### Initial Discovery
1. **Flash the latest firmware** to your ESP32 wordclock
2. **Ensure WiFi connection** - the device should connect automatically
3. **Open Home Assistant** → Settings → Devices & Services
4. **Look for "German Word Clock"** - it should appear automatically
5. **Click on the device** to see all 35 entities

If the device doesn't appear, restart Home Assistant and wait 2-3 minutes for MQTT Discovery.

---

## Entity Overview

Your German Word Clock appears as **one device** with **30 entities** organized into logical groups:

### 📊 **Entity Summary**
- **1 Main Light** - Primary display control
- **7 Sensors** - Status monitoring and real-time readings 
- **3 Basic Controls** - Transitions and brightness
- **2 Animation Curves** - Transition customization
- **4 Action Buttons** - System management, sensor refresh, and time setting
- **5 Zone Brightness Settings** - Brightness for each light zone
- **4 Zone Threshold Settings** - Lux thresholds between zones
- **2 Potentiometer Settings** - Min/max brightness range
- **1 Potentiometer Curve** - Response curve selection
- **1 Safety PWM Limit** - Maximum safe PWM protection

---

## Core Controls (16 Entities)

### 🏠 **Main Light: "Word Clock Display"**
**Type:** Light Entity  
**Purpose:** Primary on/off and brightness control

**Controls:**
- **On/Off Switch:** Turn the entire display on/off
- **Brightness Slider:** Adjust global display brightness (0-255)
- **Effect Selection:** 
  - `none` - Static display
  - `smooth_transitions` - Enable smooth word transitions  
  - `instant` - Instant word changes

**Usage Tips:**
- Use this as your main control in dashboards
- Brightness here controls the overall display brightness
- Effects control whether words transition smoothly or instantly

### 📡 **Sensors (7 entities)**

#### **WiFi Status**
**Shows:** `connected` / `disconnected`  
**Icon:** WiFi symbol  
**Use:** Monitor network connectivity

#### **Time Sync Status** 
**Shows:** `synchronized` / `not_synchronized`  
**Icon:** Clock with checkmark  
**Use:** Verify NTP time synchronization

#### **LED Brightness**
**Shows:** Current individual LED brightness (0-100%)  
**Icon:** Brightness symbol  
**Use:** Monitor potentiometer setting or manual brightness
**Updates:** Only when "Refresh Sensors" button is pressed
**NVS Impact:** None - read-only sensor value

#### **Display Brightness**
**Shows:** Current global brightness (0-100%)  
**Icon:** Display brightness symbol  
**Use:** Monitor light sensor adaptation level
**Updates:** Only when "Refresh Sensors" button is pressed
**NVS Impact:** None - read-only sensor value

#### **Current Light Level**
**Shows:** Current ambient light in lux (e.g., 150.5 lx)  
**Icon:** Light bulb  
**Use:** Monitor actual BH1750 sensor readings for calibration

#### **Potentiometer Reading**
**Shows:** Current potentiometer position as percentage (0.0-100.0%)  
**Icon:** Knob  
**Use:** Monitor actual ADC reading from physical knob

#### **Potentiometer Voltage**
**Shows:** Current potentiometer voltage (0.0-3.3 V)  
**Icon:** Lightning bolt  
**Use:** Debug physical wiring and ADC functionality
**Display:** Always shows 1 decimal place (e.g., 2.0 V, not 2 V)

### 🎛️ **Basic Controls (3 entities)**

#### **Smooth Transitions Switch**
**Type:** Switch  
**Purpose:** Enable/disable smooth word transitions  
**Options:** On/Off

**When On:** Words fade in/out over 1.5 seconds  
**When Off:** Words change instantly  
**Tip:** Turn off for faster response, on for elegant appearance

#### **Transition Duration**
**Type:** Number  
**Range:** 200-5000ms (step: 100ms)  
**Default:** 1500ms  
**Purpose:** Control how long transitions take

**Usage:**
- **Fast (200-800ms):** Quick, snappy transitions
- **Normal (1000-2000ms):** Smooth, elegant transitions  
- **Slow (2000-5000ms):** Very slow, dramatic transitions

#### **LED Brightness Control**
**Type:** Number  
**Range:** 5-255 (step: 5)  
**Purpose:** Override potentiometer setting from Home Assistant

**Usage:**
- Adjust when you can't physically reach the potentiometer
- Set specific brightness levels for automations
- Override physical control temporarily

### 🎨 **Animation Curves (2 entities)**

#### **Fade In Curve**
**Type:** Select  
**Options:** linear, ease_in, ease_out, ease_in_out, bounce  
**Purpose:** Control how new words appear

#### **Fade Out Curve**  
**Type:** Select  
**Options:** linear, ease_in, ease_out, ease_in_out, bounce  
**Purpose:** Control how old words disappear

**Curve Explanations:**
- **linear:** Constant speed transition
- **ease_in:** Slow start, fast finish
- **ease_out:** Fast start, slow finish (recommended)
- **ease_in_out:** Slow start and finish, fast middle
- **bounce:** Bouncy animation effect

### 🔧 **Action Buttons (4 entities)**

#### **Restart Button**
**Purpose:** Remotely restart the ESP32 device  
**Use:** When troubleshooting or after configuration changes

#### **Test Transitions Button**
**Purpose:** Start a 5-minute demonstration of transitions  
**Use:** See all transition effects and test the animation system

#### **Refresh Sensor Values Button**
**Purpose:** Update all sensor readings immediately  
**Use:** Get fresh light level, potentiometer, voltage, PWM, and GRPPWM readings on demand
**Note:** This is the only way sensor values update in Home Assistant - they are NOT continuously published
**NVS Impact:** None - only reads current hardware states without storing anything

#### **Set Time Button**
**Purpose:** Set DS3231 RTC time for testing different word displays  
**Use:** Test German word combinations and verify display accuracy
**Note:** Requires manual MQTT command for specific time (use Developer Tools → MQTT)
**Command Format:** `set_time HH:MM` (e.g., `set_time 09:05`)

**Testing Examples:**
- `set_time 08:50` → Test "ES IST ZEHN VOR NEUN"
- `set_time 09:00` → Test "ES IST NEUN UHR"
- `set_time 14:30` → Test "ES IST HALB DREI"

---

## Advanced Brightness Configuration (13 Entities)

### 🌟 **Simplified Zone System (9 entities)**

The light sensor uses **5 contiguous zones** with **simplified configuration:**

#### **Zone Brightness Settings (5 sliders)**
Set the brightness level for each light zone:
- **Very Dark Zone Brightness** (5-255) - Brightness in very dark conditions
- **Dim Zone Brightness** (5-255) - Brightness in dim lighting  
- **Normal Zone Brightness** (5-255) - Brightness in normal indoor lighting
- **Bright Zone Brightness** (5-255) - Brightness in bright indoor lighting
- **Very Bright Zone Brightness** (5-255) - Brightness in very bright/outdoor lighting

#### **Zone Threshold Settings (4 sliders)**
Set where zones change (automatically maintains contiguous zones):
- **Dark→Dim Threshold** (1-30 lux) - When to switch from Very Dark to Dim
- **Dim→Normal Threshold** (15-150 lux) - When to switch from Dim to Normal
- **Normal→Bright Threshold** (80-400 lux) - When to switch from Normal to Bright  
- **Bright→Very Bright Threshold** (300-1200 lux) - When to switch from Bright to Very Bright

**🔄 Automatic Zone Continuity:**
When you adjust a threshold, the system automatically ensures zones remain contiguous:
- Very Dark: 0 → Threshold 1
- Dim: Threshold 1 → Threshold 2
- Normal: Threshold 2 → Threshold 3
- Bright: Threshold 3 → Threshold 4
- Very Bright: Threshold 4 → ∞

#### **📝 Configuration Example:**

**Very Dark Zone:** Threshold ≤ 10 lux → Brightness 20 (dim for night viewing)  
**Dim Zone:** 10-50 lux → Brightness 60 (evening lighting)  
**Normal Zone:** 50-200 lux → Brightness 120 (normal indoor)  
**Bright Zone:** 200-500 lux → Brightness 180 (bright indoor)  
**Very Bright Zone:** ≥ 500 lux → Brightness 240 (daylight/outdoor)

### 🎚️ **Potentiometer Configuration (4 entities)**

#### **Potentiometer Min Brightness**
**Range:** 1-100  
**Purpose:** Lowest brightness when potentiometer is turned down  
**Tip:** Set to 5-10 for visibility in dark rooms

#### **Potentiometer Max Brightness**  
**Range:** 50-255  
**Purpose:** Highest brightness when potentiometer is turned up  
**Tip:** Set to 200-220 to avoid eye strain

#### **Potentiometer Response Curve**
**Options:** linear, logarithmic, exponential  
**Purpose:** How the potentiometer responds to turning

**Curve Characteristics:**
- **Linear:** Even response across the range
- **Logarithmic:** More sensitivity at low end (fine control in dim settings)
- **Exponential:** More sensitivity at high end (fine control in bright settings)

**Recommended:** `logarithmic` for better low-light control

#### **🛡️ Current Safety PWM Limit**
**Range:** 20-255 PWM units  
**Purpose:** Maximum safe PWM value to prevent LED overheating and component damage  
**Default:** 80 PWM units (conservative for long-term reliability)

**⚠️ Critical Safety Feature:**
- **Conservative Users:** Keep at 80 PWM for maximum hardware protection
- **Advanced Users:** Increase to 120-150 PWM for higher brightness (monitor temperature)  
- **Maximum Brightness:** Set to 200-255 PWM for full brightness capability (use with caution)
- **Override Effect:** This limit constrains all other brightness settings
- **Hardware Protection:** Prevents thermal damage and extends LED lifespan

**💡 Understanding Safety Limit Impact:**
```
Example: Potentiometer at 95% position
- Without safety limit: Would produce 190 PWM
- With 80 PWM safety limit: Actually produces 76 PWM (95% of 80)
- With 150 PWM safety limit: Would produce 142 PWM (95% of 150)
```

---

## NVS Flash Protection & Data Flow

### 🛡️ **Understanding What Causes NVS Writes**

**❌ What DOES cause NVS writes (protected with debouncing):**
- Moving brightness zone sliders (1-5)
- Adjusting threshold sliders (6-9)
- Changing potentiometer settings (min/max/curve)
- Modifying safety PWM limit
- Any configuration changes that need to persist across reboots

**✅ What DOES NOT cause NVS writes (safe to use frequently):**
- Viewing sensor values (LED Brightness, Display Brightness)
- Clicking "Refresh Sensors" button
- Reading current light level, potentiometer, or voltage
- Monitoring WiFi/NTP status
- All read-only sensor entities

### 📊 **How Sensor Monitoring Works**

```
1. Hardware State → ESP32 reads current values
2. User clicks "Refresh Sensors" → ESP32 publishes to MQTT
3. Home Assistant receives values → Updates sensor entities
4. No NVS writes occur → Only ephemeral data transmission
```

**Key Point:** Sensor values are **pull-based** (manual refresh), not **push-based** (continuous updates). This design prevents both NVS flooding and excessive MQTT traffic.

---

## Configuration Workflows

### 🛠️ **Initial Setup Workflow**

1. **Check Basic Operation**
   - Verify the main light turns on/off
   - Check that sensors show "connected" and "synchronized"
   - Test smooth transitions switch

2. **Configure Potentiometer**
   - Set Min Brightness: `10` (visible in dark)
   - Set Max Brightness: `200` (bright but comfortable)  
   - Set Response Curve: `logarithmic` (better low-end control)
   - Set Safety PWM Limit: `80` (conservative) or `120` (balanced)

3. **Calibrate Light Sensor Zones**
   - Use current lux reading to identify your environment
   - Adjust zones to match your room's lighting conditions
   - Test by changing room lighting and observing brightness changes

### 🌅 **Room-Specific Calibration**

#### **Bedroom Setup:**
```
Very Dark: 0.5-5 lux → 15-30 brightness (very dim for sleep)
Dim: 5-20 lux → 30-60 brightness (evening reading)
Normal: 20-100 lux → 60-120 brightness (daytime with curtains)
Bright: 100-300 lux → 120-180 brightness (bright indoor)
Very Bright: 300+ lux → 180-220 brightness (direct sunlight)
```

#### **Living Room Setup:**
```
Very Dark: 1-10 lux → 25-50 brightness (movie watching)
Dim: 10-40 lux → 50-100 brightness (evening ambiance)  
Normal: 40-150 lux → 100-160 brightness (normal living)
Bright: 150-400 lux → 160-200 brightness (bright day)
Very Bright: 400+ lux → 200-255 brightness (full daylight)
```

#### **Office Setup:**
```
Very Dark: 2-15 lux → 40-70 brightness (after hours)
Dim: 15-50 lux → 70-120 brightness (dim office lighting)
Normal: 50-200 lux → 120-180 brightness (normal office)
Bright: 200-600 lux → 180-220 brightness (bright office)
Very Bright: 600+ lux → 220-255 brightness (by window)
```

### 🎨 **Animation Customization**

#### **Elegant Setup (Recommended):**
- Smooth Transitions: `On`
- Duration: `1500ms`
- Fade In Curve: `ease_out`
- Fade Out Curve: `ease_in`

#### **Fast/Responsive Setup:**
- Smooth Transitions: `Off` or Duration: `500ms`
- Fade In Curve: `linear`
- Fade Out Curve: `linear`

#### **Dramatic Setup:**
- Duration: `3000ms`
- Fade In Curve: `ease_in_out`
- Fade Out Curve: `bounce`

---

## Troubleshooting

### ❌ **Device Not Appearing in Home Assistant**

1. **Check MQTT Connection:**
   - Verify wordclock is connected to WiFi
   - Check MQTT broker status in HA
   - Look for "availability: online" in MQTT

2. **Force Discovery:**
   - Restart Home Assistant
   - Restart the wordclock (use physical reset or power cycle)
   - Wait 2-3 minutes for auto-discovery

3. **Manual MQTT Check:**
   - Go to HA Developer Tools → MQTT
   - Listen to topic: `homeassistant/+/esp32_wordclock/+/config`
   - Should see discovery messages

### ⚠️ **Controls Not Working**

1. **Check Connection Status:**
   - WiFi Status sensor should show "connected"
   - Time Sync Status should show "synchronized"

2. **Verify MQTT Topics:**
   - Test basic commands in Developer Tools → MQTT
   - Publish to: `home/esp32_core/command`
   - Payload: `status`

3. **Reset Configuration:**
   - Use "Reset Brightness Config" button to restore defaults
   - Restart device if controls become unresponsive

### 🔧 **Light Sensor Issues**

1. **Check Current Lux Reading:**
   - Look at sensor data in HA logs
   - Verify reasonable lux values for your environment

2. **Recalibrate Zones:**
   - Ensure Min Lux < Max Lux for each zone
   - Ensure zones don't overlap inappropriately
   - Check that brightness min < max for each zone

3. **Test Physical Sensor:**
   - Cover sensor with hand (should go to Very Dark zone)
   - Shine flashlight on sensor (should go to Very Bright zone)

---

## Home Assistant Automation Examples

### 🌙 **Automatic Night Mode**
```yaml
automation:
  - alias: "Word Clock Night Mode"
    trigger:
      platform: time
      at: "22:00:00"
    action:
      - service: light.turn_on
        entity_id: light.word_clock_display
        data:
          brightness: 30
          effect: "smooth_transitions"
      - service: number.set_value
        entity_id: number.transition_duration  
        data:
          value: 2000
```

### ☀️ **Morning Brightness Boost**
```yaml
automation:
  - alias: "Word Clock Morning Mode"  
    trigger:
      platform: time
      at: "07:00:00"
    action:
      - service: light.turn_on
        entity_id: light.word_clock_display
        data:
          brightness: 180
      - service: select.select_option
        entity_id: select.fade_in_curve
        data:
          option: "ease_out"
```

### 🎬 **Movie Mode**
```yaml
automation:
  - alias: "Word Clock Movie Mode"
    trigger:
      platform: state
      entity_id: media_player.living_room_tv
      to: "playing"
    action:
      - service: switch.turn_off
        entity_id: switch.smooth_transitions
      - service: light.turn_on  
        entity_id: light.word_clock_display
        data:
          brightness: 25
```

### 📱 **Presence-Based Control**
```yaml
automation:
  - alias: "Word Clock Away Mode"
    trigger:
      platform: state
      entity_id: device_tracker.your_phone
      to: "not_home"
      for: "00:30:00"
    action:
      - service: light.turn_off
        entity_id: light.word_clock_display
      
  - alias: "Word Clock Home Mode"
    trigger:
      platform: state  
      entity_id: device_tracker.your_phone
      to: "home"
    action:
      - service: light.turn_on
        entity_id: light.word_clock_display
```

---

## Dashboard Configuration

### 📱 **Mobile Dashboard Card**
```yaml
type: entities
title: Word Clock
entities:
  - entity: light.word_clock_display
    name: Display
  - entity: switch.smooth_transitions  
    name: Smooth Mode
  - entity: number.transition_duration
    name: Speed
  - entity: sensor.wifi_status
    name: WiFi
  - entity: sensor.time_sync_status
    name: Time Sync
```

### 🖥️ **Advanced Configuration Panel**
```yaml
type: vertical-stack
cards:
  - type: light
    entity: light.word_clock_display
    
  - type: entities
    title: Animation Settings
    entities:
      - switch.smooth_transitions
      - number.transition_duration
      - select.fade_in_curve
      - select.fade_out_curve
      
  - type: entities  
    title: Brightness Control
    entities:
      - number.led_brightness_control
      - number.potentiometer_min_brightness
      - number.potentiometer_max_brightness
      - select.potentiometer_response_curve
      
  - type: entities
    title: Status & Actions  
    entities:
      - sensor.wifi_status
      - sensor.time_sync_status
      - button.restart
      - button.test_transitions
```

---

## Tips & Best Practices

### ✅ **Do:**
- **Start with defaults** and adjust gradually
- **Test changes** during different lighting conditions
- **Use logarithmic** potentiometer curve for better control
- **Set realistic** brightness ranges (avoid 0-255 unless needed)
- **Document your settings** in HA for future reference

### ❌ **Don't:**
- **Set overlapping** lux ranges between zones  
- **Use extreme values** (too bright in dark rooms)
- **Change too many settings** at once (hard to troubleshoot)
- **Ignore the sensors** - they show current status
- **Forget to test** after making changes

### 🎯 **Optimization Tips:**
1. **Monitor sensor values** for a few days before fine-tuning
2. **Adjust zones** based on your actual room lighting patterns  
3. **Use automations** for common scenarios (night mode, away mode)
4. **Group related controls** in your dashboard for easy access
5. **Set up notifications** for connectivity issues

---

## Support & Updates

### 📝 **Getting Help:**
- Check the main documentation in `CLAUDE.md`
- Monitor the ESP32 serial output for debugging
- Use HA Developer Tools → MQTT to test direct commands
- Reset to defaults if configuration becomes problematic

### 🔄 **Future Updates:**
The MQTT Discovery system will automatically add new entities when firmware is updated. Your existing configuration will be preserved.

---

**Enjoy your professionally integrated German Word Clock! 🕐✨🏠**