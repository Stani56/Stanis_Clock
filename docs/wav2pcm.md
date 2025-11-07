# WAV to PCM Conversion Guide

**For ESP32-S3 Word Clock Audio System**

This guide explains how to convert WAV audio files to raw PCM format compatible with the word clock's audio system.

## Requirements

- **ffmpeg** - Audio conversion tool (already installed on your system)
- Input: WAV files (any sample rate, stereo or mono)
- Output: Raw PCM (16kHz, 16-bit, mono, little-endian)

## Quick Conversion

### Single File

```bash
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -acodec pcm_s16le output.pcm
```

**Parameters:**
- `-i input.wav` - Input WAV file
- `-ar 16000` - Resample to 16000 Hz (required for ESP32 audio system)
- `-ac 1` - Convert to mono (1 channel)
- `-f s16le` - Output format: signed 16-bit little-endian (raw PCM)
- `-acodec pcm_s16le` - PCM codec
- `output.pcm` - Output filename

### Example

```bash
# Convert a doorbell chime
ffmpeg -i doorbell.wav -ar 16000 -ac 1 -f s16le doorbell.pcm

# Convert Westminster quarter chime
ffmpeg -i westminster_quarter.wav -ar 16000 -ac 1 -f s16le quarter_past.pcm
```

## Batch Conversion

### Convert All WAV Files in Directory

```bash
for file in *.wav; do
    ffmpeg -i "$file" -ar 16000 -ac 1 -f s16le -acodec pcm_s16le "${file%.wav}.pcm"
done
```

### Convert Westminster Chime Set

```bash
# Assuming you have these WAV files:
ffmpeg -i westminster_quarter_past.wav -ar 16000 -ac 1 -f s16le quarter_past.pcm
ffmpeg -i westminster_half_past.wav -ar 16000 -ac 1 -f s16le half_past.pcm
ffmpeg -i westminster_quarter_to.wav -ar 16000 -ac 1 -f s16le quarter_to.pcm
ffmpeg -i westminster_hour.wav -ar 16000 -ac 1 -f s16le hour.pcm
ffmpeg -i westminster_strike.wav -ar 16000 -ac 1 -f s16le strike.pcm
```

## Windows WSL Workflow

If your WAV files are on Windows and you're using WSL:

```bash
# Access Windows file from WSL
ffmpeg -i "/mnt/c/Users/YourName/Downloads/chime.wav" \
       -ar 16000 -ac 1 -f s16le \
       /tmp/chime.pcm

# Copy to SD card via Windows File Explorer:
# Open: \\wsl$\Ubuntu\tmp\chime.pcm
# Copy to: E:\chimes\westminster\chime.pcm (SD card)
```

## Verification

### Check File Size

```bash
# View file size
ls -lh output.pcm

# Calculate duration
# Formula: file_size / (sample_rate × bytes_per_sample)
# For 16kHz mono 16-bit: file_size / 32000 = seconds
echo "Duration: $(($(stat -c%s output.pcm) / 32000)) seconds"
```

**Example:**
- 64000 bytes = 2.0 seconds
- 224000 bytes = 7.0 seconds
- 896000 bytes = 28.0 seconds

### Play Back (Optional)

If you have `ffplay` installed:

```bash
ffplay -f s16le -ar 16000 -ac 1 output.pcm
```

### Verify Format

```bash
file output.pcm
# Should show: "data" (raw data, no headers)

# WAV files show:
file input.wav
# Shows: "RIFF (little-endian) data, WAVE audio..."
```

## Audio Quality Tips

### Volume Adjustment

If the audio is too quiet or too loud, adjust volume during conversion:

```bash
# Increase volume by 10dB
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -af "volume=10dB" output.pcm

# Decrease volume by 5dB
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -af "volume=-5dB" output.pcm

# Normalize to peak volume
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -af "loudnorm" output.pcm
```

### Trim Silence

Remove silence from beginning and end:

```bash
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le \
       -af "silenceremove=start_periods=1:start_silence=0.1:start_threshold=-50dB,\
            reverse,silenceremove=start_periods=1:start_silence=0.1:start_threshold=-50dB,\
            reverse" \
       output.pcm
```

### Fade In/Out

Add smooth fade effects:

```bash
# 0.5 second fade in and fade out
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le \
       -af "afade=t=in:st=0:d=0.5,afade=t=out:st=1.5:d=0.5" \
       output.pcm
```

## Expected File Sizes

For Westminster chimes at 16kHz mono 16-bit:

| Chime Type | Duration | Expected Size |
|------------|----------|---------------|
| Quarter Past (4 notes) | ~7 seconds | ~224 KB |
| Half Past (8 notes) | ~14 seconds | ~448 KB |
| Quarter To (12 notes) | ~21 seconds | ~672 KB |
| Hour (16 notes) | ~28 seconds | ~896 KB |
| Single Strike | ~2 seconds | ~64 KB |

**Formula:** `Duration (seconds) × 16000 Hz × 2 bytes = File size`

## Common Issues

### Issue: File Too Large

**Problem:** WAV file is very long or high quality

**Solution:**
```bash
# Compress by reducing quality (still maintains 16kHz)
ffmpeg -i large_input.wav -ar 16000 -ac 1 -f s16le \
       -compression_level 9 \
       output.pcm
```

### Issue: Audio Sounds Distorted

**Problem:** Audio clipping or distortion

**Solution:**
```bash
# Normalize audio levels
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le \
       -af "loudnorm=I=-16:TP=-1.5:LRA=11" \
       output.pcm
```

### Issue: Wrong Sample Rate

**Problem:** Audio plays too fast or too slow

**Solution:**
- Always use `-ar 16000` in the command
- The ESP32 audio system is hardcoded to 16kHz
- Files with other sample rates will play incorrectly

### Issue: Stereo to Mono Conversion

**Problem:** WAV is stereo, need mono

**Solution:**
```bash
# Default: Mix both channels equally
ffmpeg -i stereo.wav -ar 16000 -ac 1 -f s16le output.pcm

# Use only left channel
ffmpeg -i stereo.wav -ar 16000 -ac 1 -f s16le -map_channel 0.0.0 output.pcm

# Use only right channel
ffmpeg -i stereo.wav -ar 16000 -ac 1 -f s16le -map_channel 0.0.1 output.pcm
```

## Advanced: Creating Chimes from Scratch

### Generate Pure Tone Bell Sounds

Westminster bell frequencies:
- E♭5 = 622 Hz
- D♭5 = 554 Hz
- C5 = 523 Hz
- B♭4 = 466 Hz

```bash
# Generate individual bell notes (1.5 seconds each)
ffmpeg -f lavfi -i "sine=frequency=622:duration=1.5" -ar 16000 -ac 1 -f s16le bell_eb.pcm
ffmpeg -f lavfi -i "sine=frequency=554:duration=1.5" -ar 16000 -ac 1 -f s16le bell_db.pcm
ffmpeg -f lavfi -i "sine=frequency=523:duration=1.5" -ar 16000 -ac 1 -f s16le bell_c.pcm
ffmpeg -f lavfi -i "sine=frequency=466:duration=1.5" -ar 16000 -ac 1 -f s16le bell_bb.pcm

# Generate silence (0.3 seconds)
ffmpeg -f lavfi -i "anullsrc=r=16000:cl=mono:d=0.3" -ar 16000 -ac 1 -f s16le silence.pcm
```

### Concatenate Notes into Sequence

Quarter past sequence: E♭ - D♭ - C - B♭

```bash
# Concatenate with silence between notes
cat bell_eb.pcm silence.pcm bell_db.pcm silence.pcm bell_c.pcm silence.pcm bell_bb.pcm > quarter_past.pcm
```

## ESP32 Audio System Requirements

**Critical Specifications:**
- **Sample Rate:** 16000 Hz (16kHz) - FIXED, cannot change
- **Bit Depth:** 16-bit signed integer
- **Channels:** Mono (1 channel)
- **Byte Order:** Little-endian
- **Format:** Raw PCM (no headers, no compression)
- **File Extension:** .pcm

**Why these specs?**
- 16kHz: Balance between quality and memory/bandwidth
- 16-bit: Good dynamic range, fits in I2S buffer
- Mono: Single speaker (MAX98357A), saves bandwidth
- Little-endian: ESP32-S3 native byte order
- Raw: No header parsing needed, direct DMA streaming

## Testing on ESP32

### Copy to SD Card

```bash
# After conversion, copy to SD card root
cp test.pcm /path/to/sdcard/test.pcm

# For chime files, use subdirectory
mkdir -p /path/to/sdcard/chimes/westminster/
cp quarter_past.pcm /path/to/sdcard/chimes/westminster/
```

### Test via MQTT

```bash
# Test single file
mosquitto_pub -h YOUR_BROKER -p 8883 --capath /etc/ssl/certs/ \
  -u YOUR_USER -P YOUR_PASS \
  -t 'home/Clock_Stani_1/command' -m 'test_audio_file'

# Once Phase 2.3 is implemented:
mosquitto_pub -h YOUR_BROKER -p 8883 --capath /etc/ssl/certs/ \
  -u YOUR_USER -P YOUR_PASS \
  -t 'home/Clock_Stani_1/chimes/test' -m 'quarter_past'
```

### Monitor Serial Output

```bash
# Look for these messages:
# Success:
I (xxxx) audio_mgr: Playing audio from file: /sdcard/test.pcm
I (xxxx) audio_mgr: File size: 64000 bytes (32000 samples)
I (xxxx) audio_mgr: ✅ File playback complete

# File not found:
E (xxxx) audio_mgr: Failed to open file: /sdcard/test.pcm

# SD card error:
E (xxxx) sdmmc_cmd: sdmmc_read_sectors_dma: ... (0x107)
```

## Resources

### Where to Find Westminster Chime Audio

1. **Record from YouTube:**
   - Search: "Big Ben Westminster chimes"
   - Use youtube-dl or similar to download
   - Extract audio and convert

2. **Free Sound Libraries:**
   - freesound.org - Search "westminster chimes"
   - zapsplat.com - Church bells section
   - soundbible.com - Clock chimes category

3. **Generate Synthetically:**
   - Use the pure tone method above
   - Or use synthesizer software (Audacity, etc.)
   - Export as WAV, then convert

### Audio Editing Tools

- **Audacity** (GUI) - Free, cross-platform audio editor
- **ffmpeg** (CLI) - Powerful command-line tool (recommended)
- **sox** (CLI) - Alternative to ffmpeg for audio processing

## Quick Reference Card

```bash
# Basic conversion
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le output.pcm

# With volume adjustment
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -af "volume=5dB" output.pcm

# With normalization
ffmpeg -i input.wav -ar 16000 -ac 1 -f s16le -af "loudnorm" output.pcm

# Batch convert
for f in *.wav; do ffmpeg -i "$f" -ar 16000 -ac 1 -f s16le "${f%.wav}.pcm"; done

# Check duration
echo "$(($(stat -c%s file.pcm) / 32000)) seconds"
```

---

**Last Updated:** November 6, 2025
**Version:** 1.0
**Related:** Phase 2.2 (SD Card Integration), Phase 2.3 (Westminster Chimes)
