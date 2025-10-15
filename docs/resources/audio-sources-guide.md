# Audio Sources Guide for Chime System

## Where to Get Westminster & Big Ben Chime Sound Files

---

## 1. Free & Royalty-Free Sources (Recommended)

### A. Freesound.org (Best Option - Community Uploads)
**URL:** https://freesound.org

**Search Terms:**
- "Westminster chime"
- "Big Ben"
- "Clock chime"
- "Bell chime"
- "Quarter hour chime"

**License Types:**
- ✅ **CC0 (Public Domain)** - Best choice, no attribution needed
- ✅ **CC-BY** - Free, requires attribution (easy to comply)
- ❌ Avoid: CC-BY-NC (non-commercial restrictions)

**Specific Recommendations:**
```
Westminster Quarter Chime:
https://freesound.org/people/nicktermer/sounds/176872/
License: CC0 (Public Domain)

Big Ben Hour Strike:
https://freesound.org/people/InspectorJ/sounds/435408/
License: CC-BY 4.0

Clock Tower Chimes:
https://freesound.org/people/juskiddink/sounds/78955/
License: CC0
```

**Download Process:**
1. Create free account on Freesound.org
2. Search for chimes
3. Preview audio
4. Download as WAV (highest quality)
5. Check license before using

---

### B. ZapSplat (Good Quality Library)
**URL:** https://www.zapsplat.com

**Category:** Sound Effects → Bells & Chimes → Clock Chimes

**License:** Free for personal/commercial use with account
**Registration:** Free (requires email)

**Available Files:**
- Westminster quarter/half/full chimes
- Big Ben strikes
- Various clock bell sounds
- Multiple formats (WAV, MP3)

**Example Files:**
```
"Clock, grandfather, Westminster chimes, quarter"
"Clock, Big Ben, single bell strike"
"Clock tower, chime sequence"
```

---

### C. BBC Sound Effects Library
**URL:** https://sound-effects.bbcrewind.co.uk

**Note:** BBC has released many sound effects as **RemArc License** (Research & Education)
- ✅ Free for non-commercial use
- ❌ May require attribution
- ⚠️ Check license carefully for commercial projects

**Search:** "Big Ben" or "Clock chimes"

**Known Files:**
- Big Ben actual recordings (BBC archives)
- Westminster chimes variations
- Historical clock recordings

---

### D. Archive.org (Public Domain Archives)
**URL:** https://archive.org/details/audio

**Search:** "Westminster chime" OR "Big Ben"

**Advantages:**
- ✅ Many historical recordings in public domain
- ✅ High quality archival audio
- ✅ No registration needed

**Disadvantages:**
- ❌ May need cleanup (noise, clicks)
- ❌ Variable quality

---

### E. YouTube Audio Library
**URL:** https://studio.youtube.com/channel/UC.../music (requires YouTube account)

**License:** Free to use, no attribution required

**Search Process:**
1. Go to YouTube Studio
2. Click "Audio Library"
3. Search for "chime" or "bell"
4. Download directly

**Note:** Limited selection of clock chimes, but what's available is high quality.

---

## 2. Generate Your Own (Open Source Tools)

### Option A: Synthesize Chimes with Audacity (Free)

**Software:** Audacity (https://www.audacityteam.org)

**Process:**
1. Generate bell tones using "Generate → Tone"
2. Westminster notes: E4, D4, C4, G3 (traditional pitches)
3. Apply "Reverb" effect for bell resonance
4. Export as WAV (16 kHz, 16-bit mono)

**Westminster Chime Note Sequence:**
```
First Quarter (:15):
E4 - D4 - C4 - G3
(329.6 Hz, 293.7 Hz, 261.6 Hz, 196.0 Hz)

Second Quarter (:30):
C4 - E4 - D4 - G3, G3 - C4 - E4 - C4

Third Quarter (:45):
E4 - D4 - C4 - G3, C4 - E4 - D4 - G3, E4 - C4 - D4 - G3

Full Hour (:00):
E4 - D4 - C4 - G3, C4 - E4 - D4 - G3, E4 - C4 - D4 - G3, G3 - E4 - C4 - D4
```

**Audacity Recipe:**
```
1. Generate → Tone → Sine wave
   - Frequency: 329.6 Hz (E4)
   - Amplitude: 0.8
   - Duration: 1.5 seconds

2. Effect → Reverb
   - Room Size: 75
   - Damping: 50
   - Wet Level: -5 dB

3. Effect → Fade Out (last 0.5 seconds)

4. Repeat for each note

5. Export → WAV → 16 kHz, 16-bit mono
```

---

### Option B: MIDI to Audio Conversion

**Tools:**
- MuseScore (free) - https://musescore.org
- FluidSynth (command-line)
- TiMidity++

**Process:**
1. Find Westminster chime MIDI file
2. Load in MuseScore
3. Apply "Bells" or "Tubular Bells" soundfont
4. Export as WAV

**Westminster MIDI Sources:**
- MuseScore.com (search "Westminster chimes")
- Free-Scores.com
- IMSLP.org (International Music Score Library)

---

## 3. Commercial Options (High Quality)

### A. AudioJungle / Envato Elements
**URL:** https://audiojungle.net

**Cost:** $1-10 per file
**License:** Royalty-free commercial use

**Search:** "Westminster chime" or "Big Ben"

**Quality:** Professional recordings

---

### B. Pond5
**URL:** https://www.pond5.com

**Cost:** $5-50 per file
**License:** Royalty-free

**Advantages:**
- ✅ Extremely high quality
- ✅ Multiple variations
- ✅ Professional recordings

---

## 4. Recommended Quick Start

### For Testing (Immediate):
1. Go to **Freesound.org**
2. Search: "Westminster quarter chime CC0"
3. Download first good quality result
4. Test with your ESP32

### For Production (Best Quality):
1. **Download from ZapSplat** (free with account)
   - Westminster quarter: `clock_grandfather_westminster_quarter.wav`
   - Westminster half: `clock_grandfather_westminster_half.wav`
   - Big Ben strike: `big_ben_strike_single.wav`

2. **OR Generate with Audacity**
   - Full control over pitch, duration, reverb
   - Custom tailored to your speaker
   - No licensing concerns

---

## 5. Audio Processing Workflow

Once you have the audio files, process them for ESP32:

### Step 1: Convert to Correct Format
```bash
# Install ffmpeg (if not already installed)
sudo apt-get install ffmpeg

# Convert to 16kHz, 16-bit mono PCM WAV
ffmpeg -i input.wav -ar 16000 -ac 1 -sample_fmt s16 output.wav
```

### Step 2: Trim Silence
```bash
# Remove silence from start/end (saves space)
ffmpeg -i output.wav -af silenceremove=start_periods=1:start_silence=0.1:start_threshold=-50dB output_trimmed.wav
```

### Step 3: Normalize Volume
```bash
# Ensure consistent volume across all chimes
ffmpeg -i output_trimmed.wav -af loudnorm output_normalized.wav
```

### Step 4: Convert to C Array
```bash
# Convert WAV to raw PCM
ffmpeg -i output_normalized.wav -f s16le -ar 16000 -ac 1 output.pcm

# Convert PCM to C header file
xxd -i output.pcm > chime_data.h

# This creates:
# unsigned char output_pcm[] = {
#   0x00, 0x01, 0x02, ...
# };
# unsigned int output_pcm_len = 12345;
```

### Step 5: Optimize Size (Optional)
```bash
# Reduce to 8kHz if quality acceptable (halves size)
ffmpeg -i output.wav -ar 8000 -ac 1 output_8k.wav

# Or compress with ADPCM (4:1 compression)
ffmpeg -i output.wav -ar 16000 -ac 1 -acodec adpcm_ima_wav output_adpcm.wav
```

---

## 6. Complete Example: Download & Process

### Bash Script (`prepare_chimes.sh`):
```bash
#!/bin/bash

# Prepare Westminster chimes for ESP32
# Usage: ./prepare_chimes.sh

set -e  # Exit on error

echo "=== Westminster Chime Preparation for ESP32 ==="

# Create directories
mkdir -p source_audio
mkdir -p processed_audio
mkdir -p c_arrays

# Download from Freesound.org (example IDs - replace with actual)
# NOTE: You need to download manually from freesound.org
# This is just a placeholder for the workflow

echo "Step 1: Place your downloaded WAV files in source_audio/"
echo "  - westminster_quarter.wav"
echo "  - westminster_half.wav"
echo "  - westminster_three_quarter.wav"
echo "  - westminster_full.wav"
echo "  - bigben_strike.wav"
echo ""
read -p "Press Enter when files are ready..."

# Process each file
for file in source_audio/*.wav; do
    basename=$(basename "$file" .wav)

    echo "Processing: $basename"

    # Step 1: Convert to 16kHz, 16-bit mono
    ffmpeg -y -i "$file" -ar 16000 -ac 1 -sample_fmt s16 \
        "processed_audio/${basename}_16k.wav"

    # Step 2: Trim silence
    ffmpeg -y -i "processed_audio/${basename}_16k.wav" \
        -af silenceremove=start_periods=1:start_silence=0.1:start_threshold=-50dB \
        "processed_audio/${basename}_trimmed.wav"

    # Step 3: Normalize
    ffmpeg -y -i "processed_audio/${basename}_trimmed.wav" \
        -af loudnorm \
        "processed_audio/${basename}_final.wav"

    # Step 4: Convert to PCM
    ffmpeg -y -i "processed_audio/${basename}_final.wav" \
        -f s16le -ar 16000 -ac 1 \
        "processed_audio/${basename}.pcm"

    # Step 5: Convert to C array
    xxd -i "processed_audio/${basename}.pcm" > "c_arrays/${basename}.h"

    # Calculate size
    size=$(stat -f%z "processed_audio/${basename}.pcm" 2>/dev/null || stat -c%s "processed_audio/${basename}.pcm")
    echo "  → Size: $size bytes ($(expr $size / 1024) KB)"
done

echo ""
echo "=== Processing Complete ==="
echo "C header files are in: c_arrays/"
echo ""
echo "Next steps:"
echo "1. Copy c_arrays/*.h to components/chime_library/chimes/"
echo "2. Include in chime_library.c"
echo "3. Build and flash firmware"
```

---

## 7. Sample Size Estimates

Based on 16 kHz, 16-bit mono:

| File | Duration | Size | Notes |
|------|----------|------|-------|
| Westminster Quarter | 2 sec | 64 KB | 4 notes |
| Westminster Half | 3 sec | 96 KB | 8 notes |
| Westminster 3/4 | 5 sec | 160 KB | 12 notes |
| Westminster Full | 8 sec | 256 KB | 16 notes |
| Big Ben Strike (single) | 1 sec | 32 KB | BONG |
| **Total** | ~19 sec | **~608 KB** | All files |

**With 8 kHz sampling (lower quality):**
Total: ~304 KB (half the size)

---

## 8. Recommended Starter Pack

### Quick Start Kit (Free):

1. **Freesound Pack:**
   - Download "Westminster Quarter" by nicktermer (CC0)
   - Download "Big Ben Strike" by InspectorJ (CC-BY)
   - Total: 2 files, ~5 minutes to download

2. **Process with provided script above**

3. **Embed in ESP32:**
   - Start with just quarter chime for testing
   - Expand to full set once working

---

## 9. Legal Considerations

### Safe Choices:
- ✅ CC0 (Public Domain) - No restrictions
- ✅ CC-BY - Free with attribution (add to README)
- ✅ Self-generated (Audacity) - You own it
- ✅ Freesound/ZapSplat standard licenses - Clear terms

### Avoid:
- ❌ YouTube rips (copyright violations)
- ❌ Commercial clock recordings (likely copyrighted)
- ❌ CC-BY-NC for commercial projects

### Attribution Example (for CC-BY):
```markdown
## Audio Credits

Westminster Chime sound effect by InspectorJ (www.jshaw.co.uk) of Freesound.org
License: CC-BY 4.0
```

---

## 10. My Recommendation

**For this project, I recommend:**

### Path 1: Download Ready-Made (Easiest)
1. Go to **ZapSplat.com** (create free account)
2. Download their Westminster chime pack
3. Process with the bash script above
4. Embed in ESP32

**Time:** 30 minutes

### Path 2: Generate Custom (Most Control)
1. Use **Audacity**
2. Follow the synthesis guide above
3. Create exactly the notes you want
4. Tweak reverb for your speaker

**Time:** 1-2 hours

### Path 3: Freesound.org (Free & Simple)
1. Search for "Westminster chime CC0"
2. Download 2-3 good quality files
3. Process and test
4. Refine as needed

**Time:** 20 minutes

---

## Next Steps

1. **Choose your path** (ZapSplat, Audacity, or Freesound)
2. **Download audio files**
3. **Run processing script** (I'll create it for you if needed)
4. **Test one file** with ESP32 before processing all
5. **Expand to full chime set** once working

**Would you like me to:**
- Create the complete processing script?
- Find specific Freesound.org links that work right now?
- Generate synthesized chimes with exact frequencies?
- Create a minimal test file to verify I2S audio works first?

Let me know what would be most helpful! 🔔
