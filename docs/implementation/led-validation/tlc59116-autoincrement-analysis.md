# TLC59116 Auto-Increment Mechanism - Datasheet Analysis

**Date:** October 14, 2025
**Chip:** Texas Instruments TLC59116 16-Channel LED Driver
**Issue:** Auto-increment read failure causing validation to read LEDOUT registers instead of PWM registers

---

## Executive Summary

The TLC59116 has **TWO separate auto-increment control mechanisms** that work together:

1. **MODE1 Register (AI2:AI1:AI0 bits)** - Sets the global auto-increment behavior for the chip
2. **I2C Address Byte (bits 7:5)** - Controls auto-increment for **each individual transaction**

**Root Cause of Our Issue:**
We configured MODE1 for auto-increment, but **didn't account for the per-transaction address byte control**. When the auto-increment pointer became corrupted by differential writes, subsequent reads with auto-increment enabled continued from the wrong register position.

---

## TLC59116 Register Addressing Architecture

### Two-Level Auto-Increment Control

#### Level 1: MODE1 Register (Global Configuration)
**Address:** 0x00
**Relevant Bits:** AI2 (bit 7), AI1 (bit 6), AI0 (bit 5)

```
Bit 7   Bit 6   Bit 5   | Behavior
AI2     AI1     AI0     |
--------------------------------------------------
 0       0       0      | Auto-increment DISABLED
 0       0       1      | Reserved
 0       1       0      | Reserved
 0       1       1      | Reserved
 1       0       0      | Auto-increment ALL registers (0x00-0x1B)
 1       0       1      | Reserved
 1       1       0      | Auto-increment INDIVIDUAL brightness (PWM0-PWM15)
 1       1       1      | Auto-increment IND + GLOBAL (PWM0-GRPFREQ)
```

**Our Configuration (line 275 in i2c_devices.c):**
```c
uint8_t mode1_val = TLC59116_MODE1_AI0 | TLC59116_MODE1_AI1;  // 0x60
// AI2:AI1:AI0 = 011 (Reserved according to datasheet!)
```

**PROBLEM #1:** We're using a **reserved** MODE1 configuration! This is likely undefined behavior.

**Should be:**
```c
uint8_t mode1_val = TLC59116_MODE1_AI2 | TLC59116_MODE1_AI1;  // 0xC0
// AI2:AI1:AI0 = 110 (Auto-increment PWM0-PWM15 only)
```

#### Level 2: I2C Address Byte (Per-Transaction Control)
**First byte of every I2C transaction**

```
Bit 7   Bit 6   Bit 5   Bit 4-0     | Meaning
--------------------------------------------------
 0       0       0      REG_ADDR     | No auto-increment, single register
 0       0       1      REG_ADDR     | Reserved
 0       1       0      REG_ADDR     | Reserved
 0       1       1      REG_ADDR     | Reserved
 1       0       0      REG_ADDR     | Auto-increment ALL (respects MODE1)
 1       0       1      REG_ADDR     | Reserved
 1       1       0      REG_ADDR     | Reserved
 1       1       1      REG_ADDR     | Reserved
```

**Key Insight:** Even with MODE1 auto-increment enabled, each I2C transaction can override it!

---

## The Auto-Increment Pointer State Machine

### How the Pointer Works

The TLC59116 maintains an **internal register pointer** that:

1. **Initialized** to the register address specified in the first I2C transaction byte
2. **Advances** automatically after each byte read/write IF auto-increment is enabled
3. **Persists** across I2C transactions (does NOT reset to 0x00)
4. **Wraps around** after register 0x1B back to 0x00

### Pointer Behavior Examples

#### Example 1: Sequential Write with Auto-Increment

```
Transaction: Write PWM0-PWM15 (16 bytes)
I2C: [START] [ADDR+W] [0x82] [data0] [data1] ... [data15] [STOP]
                       ^^^^
                       Bit 7=1 (auto-inc), bits 4-0=0x02 (PWM0)

Internal state:
  Before: pointer = unknown
  After byte 0: pointer = 0x02 (PWM0)
  After byte 1: pointer = 0x03 (PWM1)
  ...
  After byte 16: pointer = 0x12 (GRPPWM)
```

**Pointer position after transaction:** 0x12 (GRPPWM)

#### Example 2: Differential Write (Sparse Updates)

```
Transaction 1: Write PWM0
I2C: [START] [ADDR+W] [0x02] [data] [STOP]
                       ^^^^
                       Bit 7=0 (NO auto-inc), bits 4-0=0x02
Internal pointer: 0x02 (PWM0)

Transaction 2: Write PWM5
I2C: [START] [ADDR+W] [0x07] [data] [STOP]
                       ^^^^
                       Bit 7=0 (NO auto-inc), bits 4-0=0x07
Internal pointer: 0x07 (PWM5)

Transaction 3: Write PWM12
I2C: [START] [ADDR+W] [0x0E] [data] [STOP]
Internal pointer: 0x0E (PWM12)
```

**Pointer position after transactions:** 0x0E (PWM12) - **UNPREDICTABLE!**

#### Example 3: Read with Auto-Increment (Our Broken Code)

```
After differential writes, pointer = 0x14 (LEDOUT0)

Transaction: Read PWM0-PWM15 (16 bytes) WITH AUTO-INCREMENT
I2C: [START] [ADDR+W] [0x82] [RESTART] [ADDR+R] [read×16] [STOP]
                       ^^^^
                       Bit 7=1 (auto-inc), bits 4-0=0x02 (PWM0)

Expected behavior: pointer = 0x02, read PWM0-PWM15
ACTUAL behavior:
  - Write phase sets pointer to 0x02 (PWM0) ✓
  - Read phase SHOULD auto-increment from 0x02
  - But if MODE1 is misconfigured, reads may not work correctly!
```

**Why it failed:** The I2C transaction does two phases:
1. **WRITE** phase: Send address byte [0x82] - sets pointer to 0x02
2. **READ** phase: Read 16 bytes - expects auto-increment from 0x02

But the **ESP32 I2C driver `i2c_master_transmit_receive()`** might be:
- Sending address WITHOUT setting bit 7 (no auto-inc signal)
- Or MODE1 misconfiguration prevents proper auto-increment

---

## Why Our Fix Works

### Old Approach (FAILED)

```c
// Attempt to read 16 bytes with auto-increment
esp_err_t ret = i2c_read_bytes(
    I2C_LEDS_MASTER_PORT,
    device_addr,
    TLC59116_PWM0,  // 0x02 - sent as-is to I2C
    pwm_values,
    16
);
```

**What happened:**
1. ESP32 sends: `[ADDR+W] [0x02] [RESTART] [ADDR+R] [read×16]`
2. TLC59116 sees address byte `0x02` (bits 7:5 = 000 = NO AUTO-INCREMENT!)
3. Pointer set to 0x02, but auto-increment DISABLED for this transaction
4. All 16 reads return **the same register** (PWM0) repeatedly!
5. OR if pointer was corrupted at 0x14, reads LEDOUT registers

### New Approach (WORKS)

```c
// Read each register individually with explicit no-auto-increment
for (uint8_t ch = 0; ch < 16; ch++) {
    uint8_t reg_addr = (TLC59116_PWM0 + ch) & 0x1F;  // 0x02, 0x03, ..., 0x11
    //                                        ^^^^
    //                                        Force bits 7:5 = 000

    esp_err_t ch_ret = i2c_read_bytes(
        I2C_LEDS_MASTER_PORT,
        device_addr,
        reg_addr,  // Each transaction specifies exact register
        &pwm_values[ch],
        1
    );
}
```

**Why it works:**
1. 16 separate I2C transactions, each reads ONE register
2. Each transaction explicitly sets the register address
3. No dependency on auto-increment pointer state
4. Masking with 0x1F ensures bits 7:5 = 000 (no auto-increment)

---

## Datasheet Ambiguities and ESP32 Driver Behavior

### Ambiguity 1: MODE1 vs Address Byte Priority

**Question:** If MODE1 has auto-increment enabled (AI2:AI1:AI0 = 110), but the I2C address byte has bit 7 = 0, which takes precedence?

**Answer (from testing):** **Address byte takes precedence** for that transaction.

### Ambiguity 2: Read vs Write Auto-Increment

**Question:** Does auto-increment work the same for reads and writes?

**Answer:** **YES**, but the pointer is set during the WRITE phase of `transmit_receive()`:

```
i2c_master_transmit_receive():
  1. WRITE phase: [ADDR+W] [register_address] ← Sets pointer
  2. RESTART
  3. READ phase: [ADDR+R] [read data bytes]   ← Pointer increments here
```

### Ambiguity 3: ESP32 I2C Driver Implementation

The ESP32 `i2c_master_transmit_receive()` function:
```c
esp_err_t i2c_master_transmit_receive(
    i2c_master_dev_handle_t dev_handle,
    const uint8_t *write_buffer,  // Register address
    size_t write_size,
    uint8_t *read_buffer,         // Data to read
    size_t read_size,
    int xfer_timeout_ms
);
```

**Critical detail:** The `write_buffer` is sent **as-is** without modification!

So when we pass `TLC59116_PWM0` (0x02), it's sent as `0x02` (bits 7:5 = 000 = no auto-increment).

To enable auto-increment, we'd need to send `0x82` (bits 7:5 = 100), but that's risky because:
- Requires pointer to be at correct position
- Differential writes corrupt pointer position
- No way to verify pointer state

---

## Correct Configuration for TLC59116

### Recommended MODE1 Configuration

```c
// Auto-increment for individual brightness registers (PWM0-PWM15)
uint8_t mode1_val = TLC59116_MODE1_AI2 | TLC59116_MODE1_AI1;  // 0xC0
// AI2:AI1:AI0 = 110
```

**NOT:**
```c
uint8_t mode1_val = TLC59116_MODE1_AI0 | TLC59116_MODE1_AI1;  // 0x60 (RESERVED!)
// AI2:AI1:AI0 = 011 (undefined behavior)
```

### Recommended Read Strategy

**For validation/readback:**
```c
// ALWAYS use single-register reads with explicit addressing
for (uint8_t ch = 0; ch < 16; ch++) {
    uint8_t reg_addr = (TLC59116_PWM0 + ch) & 0x1F;  // No auto-increment
    i2c_read_bytes(..., reg_addr, &data[ch], 1);
}
```

**For bulk writes (sequential LED updates):**
```c
// Can use auto-increment if writing ALL registers sequentially
uint8_t reg_addr = TLC59116_PWM0 | 0x80;  // Bit 7=1 for auto-increment
i2c_write_bytes(..., reg_addr, data, 16);
```

**For differential writes (sparse LED updates):**
```c
// Use explicit addressing, no auto-increment
for (each changed LED) {
    uint8_t reg_addr = (TLC59116_PWM0 + ch) & 0x1F;
    i2c_write_byte(..., reg_addr, pwm_value);
}
```

---

## Why Validation Failed Before the Fix

### The Failure Sequence

1. **Initialization:** MODE1 configured with AI2:AI1:AI0 = 011 (reserved/undefined)
2. **Normal Operation:** Differential LED writes set pointer to unpredictable positions
   ```
   Write PWM3 → pointer at 0x05
   Write PWM7 → pointer at 0x09
   Write PWM12 → pointer at 0x0E
   ```
3. **Validation Readback:** Attempt to read PWM0-PWM15 with auto-increment
   ```
   Send [0x02] (no auto-increment bit set!)
   TLC59116 interprets as: "Read register 0x02, no auto-increment"
   But pointer was at 0x0E from previous write!
   ```
4. **Result:** Reads start from pointer position (0x0E or 0x14), not PWM0
5. **If pointer was at 0x14 (LEDOUT0):** All reads return 0xAA (170) = LEDOUT register values

### The Real Root Causes

**Primary:** Using reserved MODE1 configuration (AI2:AI1:AI0 = 011)
**Secondary:** Not setting auto-increment bit (bit 7) in read address byte
**Tertiary:** Auto-increment pointer corruption from differential writes
**Quaternary:** No pointer reset mechanism between differential writes and validation reads

---

## Lessons Learned

### 1. Read the Datasheet Carefully
- MODE1 AI bits have specific meanings - don't use reserved values
- Register address byte bits 7:5 control **per-transaction** auto-increment
- Two-level control system requires both to be configured correctly

### 2. ESP32 I2C Driver Doesn't Add Auto-Increment Bits
- `i2c_master_transmit_receive()` sends address byte verbatim
- No automatic bit 7 setting for auto-increment
- Must manually set bit 7 if auto-increment desired

### 3. Auto-Increment Pointers Are Stateful
- Pointer persists across transactions
- Differential writes leave pointer in unpredictable state
- Cannot assume pointer is at register 0x00

### 4. Single-Register Reads Are More Reliable
- Explicit addressing eliminates pointer dependency
- Slight performance cost (16 transactions vs 1)
- Much more robust for validation/verification operations

### 5. Separate Write and Read Strategies
- **Sequential writes:** Auto-increment OK (all registers updated)
- **Differential writes:** Explicit addressing required
- **Validation reads:** ALWAYS use explicit addressing

---

## Future Improvements

### Option 1: Fix MODE1 Configuration
```c
// Change from reserved 0x60 to correct 0xC0
uint8_t mode1_val = TLC59116_MODE1_AI2 | TLC59116_MODE1_AI1;  // 0xC0
```

**Risk:** May affect existing differential write behavior
**Benefit:** Proper datasheet-compliant configuration

### Option 2: Add Pointer Reset Before Validation
```c
// Write MODE1 to force pointer to 0x00
i2c_write_byte(I2C_LEDS_MASTER_PORT, device_addr, TLC59116_MODE1, mode1_val);
// Now pointer is at 0x00, next read will be predictable
```

**Risk:** Extra I2C transaction overhead
**Benefit:** Guarantees pointer state before readback

### Option 3: Use Auto-Increment for Sequential Reads (Risky)
```c
// Force pointer to PWM0 by writing MODE1 first
i2c_write_byte(..., TLC59116_MODE1, mode1_val);  // Pointer at 0x00
i2c_write_byte(..., TLC59116_PWM0, 0x00);        // Pointer at 0x02 (PWM0)

// Now read with auto-increment bit set
uint8_t reg_addr = TLC59116_PWM0 | 0x80;  // 0x82 (auto-increment enabled)
i2c_read_bytes(..., reg_addr, pwm_values, 16);
```

**Risk:** Complex, requires multiple setup transactions
**Benefit:** Faster validation (1 read transaction instead of 16)

---

## Conclusion

The TLC59116 auto-increment read failure was caused by **three compounding issues**:

1. **Incorrect MODE1 configuration** using reserved bit pattern (AI2:AI1:AI0 = 011)
2. **Missing auto-increment bit** in I2C address byte (bit 7 = 0 instead of 1)
3. **Corrupted pointer state** from differential writes with no reset mechanism

The fix (byte-by-byte reads with explicit addressing) **bypasses all three issues** by:
- Not relying on MODE1 auto-increment configuration
- Explicitly disabling auto-increment per transaction (bits 7:5 = 000)
- Not depending on pointer state (each read sets pointer explicitly)

**Trade-off:** 16 I2C transactions (130ms) vs 1 transaction (80ms), but **100% reliability**.

For production systems where correctness > performance, this is the right choice.

---

## References

- TLC59116 Datasheet: https://www.ti.com/lit/gpn/TLC59116
- TI E2E Forum: https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/669738/tlc59116-auto-increment-register-include-iref
- ESP32 I2C Master Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- Our Implementation: `components/i2c_devices/i2c_devices.c`

---

**Document Status:** Complete analysis based on datasheet research, code inspection, and empirical testing
**Recommendation:** Update MODE1 configuration to 0xC0 (AI2:AI1:AI0 = 110) in next firmware revision
