# German Word Clock LED Usage Analysis

## Overview
Comprehensive analysis of all valid and invalid LED positions based on German time display logic for a 10×16 LED matrix word clock.

## LED Matrix Layout
```
Row/Col  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
───────────────────────────────────────────────────────────
   0  │  E  S  •  I  S  T  •  F  Ü  N  F  •  •  •  •  •
   1  │  Z  E  H  N  Z  W  A  N  Z  I  G  •  •  •  •  •
   2  │  D  R  E  I  V  I  E  R  T  E  L  •  •  •  •  •
   3  │  V  O  R  •  •  •  •  N  A  C  H  •  •  •  •  •
   4  │  H  A  L  B  •  E  L  F  Ü  N  F  •  •  •  •  •
   5  │  E  I  N  S  •  •  •  Z  W  E  I  •  •  •  •  •
   6  │  D  R  E  I  •  •  •  V  I  E  R  •  •  •  •  •
   7  │  S  E  C  H  S  •  •  A  C  H  T  •  •  •  •  •
   8  │  S  I  E  B  E  N  Z  W  Ö  L  F  •  •  •  •  •
   9  │  Z  E  H  N  E  U  N  •  U  H  R  • [●][●][●][●]
```

## Word Database
### Always Active Words
- **ES IST** (Row 0, Cols 0-1 + 3-5) - Used in ALL time displays

### Minute Words (Base 5-minute increments)
| Base Minutes | Words | Row-Col Positions |
|--------------|-------|-------------------|
| :00 | *(none)* | - |
| :05 | FÜNF NACH | R0[7-10] + R3[7-10] |
| :10 | ZEHN NACH | R1[0-3] + R3[7-10] |
| :15 | VIERTEL NACH | R2[4-10] + R3[7-10] |
| :20 | ZWANZIG NACH | R1[4-10] + R3[7-10] |
| :25 | FÜNF VOR HALB | R0[7-10] + R3[0-2] + R4[0-3] |
| :30 | HALB | R4[0-3] |
| :35 | FÜNF NACH HALB | R0[7-10] + R3[7-10] + R4[0-3] |
| :40 | ZWANZIG VOR | R1[4-10] + R3[0-2] |
| :45 | VIERTEL VOR | R2[4-10] + R3[0-2] |
| :50 | ZEHN VOR | R1[0-3] + R3[0-2] |
| :55 | FÜNF VOR | R0[7-10] + R3[0-2] |

### Hour Words
| Hour | Word(s) | Row-Col Position | Notes |
|------|---------|------------------|-------|
| 12/0 | ZWÖLF | R8[6-10] | |
| 1 | EIN / EINS | R5[0-2] / R5[0-3] | EIN at :00, EINS otherwise |
| 2 | ZWEI | R5[7-10] | |
| 3 | DREI | R6[0-3] | |
| 4 | VIER | R6[7-10] | |
| 5 | FÜNF | R4[7-10] | Same position as minute FÜNF! |
| 6 | SECHS | R7[0-4] | |
| 7 | SIEBEN | R8[0-5] | |
| 8 | ACHT | R7[7-10] | |
| 9 | NEUN | R9[3-6] | |
| 10 | ZEHN | R9[0-3] | Same position as minute ZEHN! |
| 11 | ELF | R4[5-7] | |

### Special Words
- **UHR** (Row 9, Cols 8-10) - Only used at exact hours (:00)
- **Minute Indicators** (Row 9, Cols 11-14) - 0-4 LEDs for minutes 1-4 beyond 5-min increment

## Complete LED Analysis

### Row 0: E S • I S T • F Ü N F • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | E | ✅ ALWAYS | 100% | Part of "ES" |
| 1 | S | ✅ ALWAYS | 100% | Part of "ES" |
| 2 | • | ❌ NEVER | 0% | Spacing character |
| 3 | I | ✅ ALWAYS | 100% | Part of "IST" |
| 4 | S | ✅ ALWAYS | 100% | Part of "IST" |
| 5 | T | ✅ ALWAYS | 100% | Part of "IST" |
| 6 | • | ❌ NEVER | 0% | Spacing character |
| 7 | F | ✅ CONDITIONAL | 41.7% | Part of "FÜNF" (:05, :25, :35, :55) + indicators |
| 8 | Ü | ✅ CONDITIONAL | 41.7% | Part of "FÜNF" |
| 9 | N | ✅ CONDITIONAL | 41.7% | Part of "FÜNF" |
| 10 | F | ✅ CONDITIONAL | 41.7% | Part of "FÜNF" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 1: Z E H N Z W A N Z I G • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | Z | ✅ CONDITIONAL | 33.3% | Part of "ZEHN" (:10, :50) |
| 1 | E | ✅ CONDITIONAL | 33.3% | Part of "ZEHN" |
| 2 | H | ✅ CONDITIONAL | 33.3% | Part of "ZEHN" |
| 3 | N | ✅ CONDITIONAL | 33.3% | Part of "ZEHN" |
| 4 | Z | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" (:20, :40) |
| 5 | W | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 6 | A | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 7 | N | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 8 | Z | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 9 | I | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 10 | G | ✅ CONDITIONAL | 25.0% | Part of "ZWANZIG" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 2: D R E I V I E R T E L • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | D | ✅ CONDITIONAL | 16.7% | Part of "DREIVIERTEL" (:15 variant - NOT USED in code) |
| 1 | R | ✅ CONDITIONAL | 16.7% | Part of "DREIVIERTEL" / "VIERTEL" |
| 2 | E | ✅ CONDITIONAL | 16.7% | Part of "DREIVIERTEL" / "VIERTEL" |
| 3 | I | ✅ CONDITIONAL | 16.7% | Part of "DREIVIERTEL" / "VIERTEL" |
| 4 | V | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" (:15, :45) |
| 5 | I | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 6 | E | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 7 | R | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 8 | T | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 9 | E | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 10 | L | ✅ CONDITIONAL | 16.7% | Part of "VIERTEL" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

**Note:** "DREIVIERTEL" (cols 0-10) is defined but NEVER used in current code - only "VIERTEL" (cols 4-10) is used.

### Row 3: V O R • • • • N A C H • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | V | ✅ CONDITIONAL | 41.7% | Part of "VOR" (:25, :40, :45, :50, :55) |
| 1 | O | ✅ CONDITIONAL | 41.7% | Part of "VOR" |
| 2 | R | ✅ CONDITIONAL | 41.7% | Part of "VOR" |
| 3-6 | • | ❌ NEVER | 0% | Spacing |
| 7 | N | ✅ CONDITIONAL | 41.7% | Part of "NACH" (:05, :10, :15, :20, :35) |
| 8 | A | ✅ CONDITIONAL | 41.7% | Part of "NACH" |
| 9 | C | ✅ CONDITIONAL | 41.7% | Part of "NACH" |
| 10 | H | ✅ CONDITIONAL | 41.7% | Part of "NACH" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 4: H A L B • E L F Ü N F • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | H | ✅ CONDITIONAL | 25.0% | Part of "HALB" (:25, :30, :35) |
| 1 | A | ✅ CONDITIONAL | 25.0% | Part of "HALB" |
| 2 | L | ✅ CONDITIONAL | 25.0% | Part of "HALB" |
| 3 | B | ✅ CONDITIONAL | 25.0% | Part of "HALB" |
| 4 | • | ❌ NEVER | 0% | Spacing |
| 5 | E | ✅ CONDITIONAL | 8.3% | Part of "ELF" (hour 11) |
| 6 | L | ✅ CONDITIONAL | 8.3% | Part of "ELF" |
| 7 | F | ✅ CONDITIONAL | 16.7% | Part of "ELF" + "FÜNF_H" (hour 5) |
| 8 | Ü | ✅ CONDITIONAL | 8.3% | Part of "FÜNF_H" (hour 5) |
| 9 | N | ✅ CONDITIONAL | 8.3% | Part of "FÜNF_H" |
| 10 | F | ✅ CONDITIONAL | 8.3% | Part of "FÜNF_H" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

**Note:** Position [7] can be part of both "ELF" and "FÜNF_H" - context dependent.

### Row 5: E I N S • • • Z W E I • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | E | ✅ CONDITIONAL | 8.3% | Part of "EIN" (hour 1 at :00) or "EINS" |
| 1 | I | ✅ CONDITIONAL | 8.3% | Part of "EIN"/"EINS" |
| 2 | N | ✅ CONDITIONAL | 8.3% | Part of "EIN"/"EINS" |
| 3 | S | ✅ CONDITIONAL | ~7.9% | Part of "EINS" (hour 1, NOT at :00) |
| 4-6 | • | ❌ NEVER | 0% | Spacing |
| 7 | Z | ✅ CONDITIONAL | 8.3% | Part of "ZWEI" (hour 2) |
| 8 | W | ✅ CONDITIONAL | 8.3% | Part of "ZWEI" |
| 9 | E | ✅ CONDITIONAL | 8.3% | Part of "ZWEI" |
| 10 | I | ✅ CONDITIONAL | 8.3% | Part of "ZWEI" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

**Grammar Note:** "EIN" (cols 0-2) at :00, "EINS" (cols 0-3) at other times.

### Row 6: D R E I • • • V I E R • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | D | ✅ CONDITIONAL | 8.3% | Part of "DREI" (hour 3) |
| 1 | R | ✅ CONDITIONAL | 8.3% | Part of "DREI" |
| 2 | E | ✅ CONDITIONAL | 8.3% | Part of "DREI" |
| 3 | I | ✅ CONDITIONAL | 8.3% | Part of "DREI" |
| 4-6 | • | ❌ NEVER | 0% | Spacing |
| 7 | V | ✅ CONDITIONAL | 8.3% | Part of "VIER" (hour 4) |
| 8 | I | ✅ CONDITIONAL | 8.3% | Part of "VIER" |
| 9 | E | ✅ CONDITIONAL | 8.3% | Part of "VIER" |
| 10 | R | ✅ CONDITIONAL | 8.3% | Part of "VIER" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 7: S E C H S • • A C H T • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | S | ✅ CONDITIONAL | 8.3% | Part of "SECHS" (hour 6) |
| 1 | E | ✅ CONDITIONAL | 8.3% | Part of "SECHS" |
| 2 | C | ✅ CONDITIONAL | 8.3% | Part of "SECHS" |
| 3 | H | ✅ CONDITIONAL | 8.3% | Part of "SECHS" |
| 4 | S | ✅ CONDITIONAL | 8.3% | Part of "SECHS" |
| 5-6 | • | ❌ NEVER | 0% | Spacing |
| 7 | A | ✅ CONDITIONAL | 8.3% | Part of "ACHT" (hour 8) |
| 8 | C | ✅ CONDITIONAL | 8.3% | Part of "ACHT" |
| 9 | H | ✅ CONDITIONAL | 8.3% | Part of "ACHT" |
| 10 | T | ✅ CONDITIONAL | 8.3% | Part of "ACHT" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 8: S I E B E N Z W Ö L F • • • • •
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | S | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" (hour 7) |
| 1 | I | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" |
| 2 | E | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" |
| 3 | B | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" |
| 4 | E | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" |
| 5 | N | ✅ CONDITIONAL | 8.3% | Part of "SIEBEN" |
| 6 | Z | ✅ CONDITIONAL | 16.7% | Part of "ZWÖLF" (hours 0/12) |
| 7 | W | ✅ CONDITIONAL | 16.7% | Part of "ZWÖLF" |
| 8 | Ö | ✅ CONDITIONAL | 16.7% | Part of "ZWÖLF" |
| 9 | L | ✅ CONDITIONAL | 16.7% | Part of "ZWÖLF" |
| 10 | F | ✅ CONDITIONAL | 16.7% | Part of "ZWÖLF" |
| 11-15 | • | ❌ NEVER | 0% | Unused positions |

### Row 9: Z E H N E U N • U H R • [●][●][●][●]
| Col | Char | Valid | Usage Frequency | Notes |
|-----|------|-------|-----------------|-------|
| 0 | Z | ✅ CONDITIONAL | 16.7% | Part of "ZEHN_H" (hour 10) - overlaps with minute ZEHN! |
| 1 | E | ✅ CONDITIONAL | 16.7% | Part of "ZEHN_H"/"NEUN" |
| 2 | H | ✅ CONDITIONAL | 16.7% | Part of "ZEHN_H" |
| 3 | N | ✅ CONDITIONAL | 16.7% | Part of "ZEHN_H"/"NEUN" (shared N!) |
| 4 | E | ✅ CONDITIONAL | 8.3% | Part of "NEUN" (hour 9) |
| 5 | U | ✅ CONDITIONAL | 8.3% | Part of "NEUN" |
| 6 | N | ✅ CONDITIONAL | 8.3% | Part of "NEUN" |
| 7 | • | ❌ NEVER | 0% | Spacing |
| 8 | U | ✅ CONDITIONAL | 8.3% | Part of "UHR" (only at :00) |
| 9 | H | ✅ CONDITIONAL | 8.3% | Part of "UHR" |
| 10 | R | ✅ CONDITIONAL | 8.3% | Part of "UHR" |
| 11 | [●] | ✅ CONDITIONAL | ~33.3% | Minute indicator 1 (minutes 1, 6, 11, 16...) |
| 12 | [●] | ✅ CONDITIONAL | ~25.0% | Minute indicator 2 (minutes 2, 7, 12, 17...) |
| 13 | [●] | ✅ CONDITIONAL | ~16.7% | Minute indicator 3 (minutes 3, 8, 13, 18...) |
| 14 | [●] | ✅ CONDITIONAL | ~8.3% | Minute indicator 4 (minutes 4, 9, 14, 19...) |
| 15 | • | ❌ NEVER | 0% | Unused position |

**Critical Note:** Row 9, Col 3 ('N') is shared between "ZEHN" (hour 10) and "NEUN" (hour 9)!

## Summary Statistics

### Total LED Count: 160 (10 rows × 16 columns)

### Valid LEDs by Category:
| Category | LED Count | Percentage | Notes |
|----------|-----------|------------|-------|
| **ALWAYS ON** | 5 | 3.125% | ES IST (always active) |
| **CONDITIONAL** | 86 | 53.75% | Used in valid time displays |
| **NEVER VALID** | 69 | 43.125% | Spacing/unused positions |

### Invalid LED Positions (NEVER Valid):
```
Row 0: Cols 2, 6, 11-15      (7 LEDs)
Row 1: Cols 11-15            (5 LEDs)
Row 2: Cols 11-15            (5 LEDs)
Row 3: Cols 3-6, 11-15       (9 LEDs)
Row 4: Cols 4, 11-15         (6 LEDs)
Row 5: Cols 4-6, 11-15       (8 LEDs)
Row 6: Cols 4-6, 11-15       (8 LEDs)
Row 7: Cols 5-6, 11-15       (7 LEDs)
Row 8: Cols 11-15            (5 LEDs)
Row 9: Cols 7, 15            (2 LEDs)
───────────────────────────────
TOTAL INVALID: 69 LEDs (43.1%)
```

## Critical Overlapping Positions

### 1. Row 9, Col 3: Shared 'N' between ZEHN and NEUN
- Used by "ZEHN_H" (hour 10): Cols 0-3
- Used by "NEUN" (hour 9): Cols 3-6
- **Conflict:** Both words share the 'N' at position [9,3]
- **Result:** Works correctly due to German grammar - never displayed simultaneously

### 2. Row 4, Col 7: Shared 'F' between ELF and FÜNF_H
- Used by "ELF" (hour 11): Cols 5-7
- Used by "FÜNF_H" (hour 5): Cols 7-10
- **Conflict:** Both words can share the 'F' at position [4,7]
- **Result:** Works correctly - never displayed simultaneously

### 3. Row 5, Cols 0-2: EIN vs EINS Grammar
- "EIN" (cols 0-2): Only at exact hours (:00)
- "EINS" (cols 0-3): At all other times
- **Logic:** German grammar distinction - "EIN UHR" vs "FÜNF NACH EINS"

## Usage Frequency Analysis

### High Usage LEDs (>50% of time displays):
- **Row 0, Cols 0-1, 3-5**: ES IST (100% - always on, 5 LEDs total)

### Medium Usage LEDs (25-50%):
- **Row 0, Cols 7-10**: FÜNF (41.7% - 5 times per hour)
- **Row 1, Cols 0-3**: ZEHN (33.3% - 4 times per hour)
- **Row 1, Cols 4-10**: ZWANZIG (25.0% - 3 times per hour)
- **Row 3, Cols 0-2**: VOR (41.7% - 5 times per hour)
- **Row 3, Cols 7-10**: NACH (41.7% - 5 times per hour)
- **Row 4, Cols 0-3**: HALB (25.0% - 3 times per hour)

### Low Usage LEDs (8-16%):
- All hour words: 8.3% each (1/12 hours)
- VIERTEL: 16.7% (2 times per hour)
- ZWÖLF: 16.7% (appears at both 0:00 and 12:00)
- UHR: 8.3% (only at :00)

### Minute Indicators Usage:
- Indicator 1 (Col 11): 20% (12 times per hour: minutes 1,6,11,16,21,26,31,36,41,46,51,56)
- Indicator 2 (Col 12): 20% (12 times per hour: minutes 2,7,12,17,22,27,32,37,42,47,52,57)
- Indicator 3 (Col 13): 20% (12 times per hour: minutes 3,8,13,18,23,28,33,38,43,48,53,58)
- Indicator 4 (Col 14): 20% (12 times per hour: minutes 4,9,14,19,24,29,34,39,44,49,54,59)

## Validation Rules

### LED Activation Constraints:

1. **ES IST Rule**: Cols [0,1,3,4,5] in Row 0 must ALWAYS be active (100% of valid displays)

2. **Spacing Rule**: The following positions must NEVER be active:
   - Row 0: Cols 2, 6, 11-15
   - Row 1: Cols 11-15
   - Row 2: Cols 11-15
   - Row 3: Cols 3-6, 11-15
   - Row 4: Col 4, 11-15
   - Row 5: Cols 4-6, 11-15
   - Row 6: Cols 4-6, 11-15
   - Row 7: Cols 5-6, 11-15
   - Row 8: Cols 11-15
   - Row 9: Cols 7, 15

3. **Complete Word Rule**: Words must be displayed completely or not at all (no partial words)

4. **Grammar Rules**:
   - If Col [9,8-10] (UHR) is active, base_minutes must be 0
   - If Col [9,8-10] (UHR) is active, hour word must use "EIN" not "EINS"
   - Minute indicators [9,11-14] show only 0-4 LEDs

5. **Mutual Exclusivity Rules**:
   - VOR and NACH cannot both be active
   - Only one hour word can be active at a time
   - ZEHN_M and ZEHN_H cannot both be active (different hours)
   - DREIVIERTEL should never be active (code uses VIERTEL only)

## Testing Recommendations

### 1. Invalid LED Detection Test
Test that the following LEDs are NEVER activated:
```c
const uint8_t invalid_leds[][2] = {
    {0,2}, {0,6}, {0,11}, {0,12}, {0,13}, {0,14}, {0,15},  // Row 0 spacing
    {1,11}, {1,12}, {1,13}, {1,14}, {1,15},                // Row 1 unused
    {2,11}, {2,12}, {2,13}, {2,14}, {2,15},                // Row 2 unused
    {3,3}, {3,4}, {3,5}, {3,6}, {3,11}, {3,12}, {3,13}, {3,14}, {3,15},  // Row 3
    {4,4}, {4,11}, {4,12}, {4,13}, {4,14}, {4,15},         // Row 4
    {5,4}, {5,5}, {5,6}, {5,11}, {5,12}, {5,13}, {5,14}, {5,15},  // Row 5
    {6,4}, {6,5}, {6,6}, {6,11}, {6,12}, {6,13}, {6,14}, {6,15},  // Row 6
    {7,5}, {7,6}, {7,11}, {7,12}, {7,13}, {7,14}, {7,15},  // Row 7
    {8,11}, {8,12}, {8,13}, {8,14}, {8,15},                // Row 8 unused
    {9,7}, {9,15}                                          // Row 9 spacing
};
// Total: 69 invalid LED positions
```

### 2. Complete Word Integrity Test
Verify that words are never partially displayed:
- If any LED of a word is ON, ALL LEDs of that word must be ON
- Example: If "FÜNF" is active, ALL of [0,7-10] must be ON

### 3. Grammar Consistency Test
- When "UHR" is active → hour must use EIN (not EINS)
- When "UHR" is NOT active → hour 1 must use EINS (not EIN)
- Minute indicators must show 0-4 LEDs only

### 4. Time Coverage Test
Generate all 1440 possible times (24h × 60min) and verify:
- Each results in a valid LED pattern
- No invalid LEDs are ever activated
- All word combinations are grammatically correct

## Conclusion

Out of 160 total LEDs in the 10×16 matrix:
- **5 LEDs (3.125%)** are ALWAYS active (ES IST)
- **86 LEDs (53.75%)** are CONDITIONALLY active based on time
- **69 LEDs (43.125%)** should NEVER be active (spacing/unused)

Any activation of the 69 invalid LED positions indicates a bug in the display logic.
