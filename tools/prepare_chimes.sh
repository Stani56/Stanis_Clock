#!/bin/bash

# Prepare Westminster chimes for ESP32
# This script converts WAV files to C arrays for embedding in firmware
#
# Requirements:
#   - ffmpeg (audio processing)
#   - xxd (hex dump tool, usually pre-installed)
#
# Usage:
#   1. Place source WAV files in source_audio/
#   2. Run: ./prepare_chimes.sh
#   3. Find C headers in c_arrays/

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Westminster Chime Preparation for ESP32 ===${NC}"
echo ""

# Check for required tools
command -v ffmpeg >/dev/null 2>&1 || {
    echo -e "${RED}Error: ffmpeg is not installed${NC}"
    echo "Install with: sudo apt-get install ffmpeg"
    exit 1
}

command -v xxd >/dev/null 2>&1 || {
    echo -e "${RED}Error: xxd is not installed${NC}"
    echo "Install with: sudo apt-get install vim-common"
    exit 1
}

# Create directories
mkdir -p source_audio
mkdir -p processed_audio
mkdir -p c_arrays

# Check if source files exist
if [ ! "$(ls -A source_audio/*.wav 2>/dev/null)" ]; then
    echo -e "${YELLOW}No WAV files found in source_audio/${NC}"
    echo ""
    echo "Please place your downloaded WAV files in source_audio/"
    echo "Expected files:"
    echo "  - westminster_quarter.wav   (4 notes, ~2 seconds)"
    echo "  - westminster_half.wav      (8 notes, ~3 seconds)"
    echo "  - westminster_three_quarter.wav (12 notes, ~5 seconds)"
    echo "  - westminster_full.wav      (16 notes, ~8 seconds)"
    echo "  - bigben_strike.wav         (single BONG, ~1 second)"
    echo ""
    echo "See docs/resources/audio-sources-guide.md for download sources"
    echo ""
    exit 1
fi

echo -e "${GREEN}Found WAV files:${NC}"
ls -lh source_audio/*.wav
echo ""

# Process each file
total_size=0
file_count=0

for file in source_audio/*.wav; do
    basename=$(basename "$file" .wav)
    file_count=$((file_count + 1))

    echo -e "${BLUE}Processing [$file_count]: ${basename}${NC}"

    # Step 1: Convert to 16kHz, 16-bit mono
    echo -n "  → Converting to 16kHz mono..."
    ffmpeg -y -i "$file" -ar 16000 -ac 1 -sample_fmt s16 \
        "processed_audio/${basename}_16k.wav" \
        -loglevel error
    echo -e " ${GREEN}✓${NC}"

    # Step 2: Trim silence
    echo -n "  → Trimming silence..."
    ffmpeg -y -i "processed_audio/${basename}_16k.wav" \
        -af silenceremove=start_periods=1:start_silence=0.1:start_threshold=-50dB \
        "processed_audio/${basename}_trimmed.wav" \
        -loglevel error
    echo -e " ${GREEN}✓${NC}"

    # Step 3: Normalize volume
    echo -n "  → Normalizing volume..."
    ffmpeg -y -i "processed_audio/${basename}_trimmed.wav" \
        -af loudnorm \
        "processed_audio/${basename}_final.wav" \
        -loglevel error
    echo -e " ${GREEN}✓${NC}"

    # Step 4: Convert to raw PCM
    echo -n "  → Converting to PCM..."
    ffmpeg -y -i "processed_audio/${basename}_final.wav" \
        -f s16le -ar 16000 -ac 1 \
        "processed_audio/${basename}.pcm" \
        -loglevel error
    echo -e " ${GREEN}✓${NC}"

    # Step 5: Convert to C array
    echo -n "  → Generating C header..."
    xxd -i "processed_audio/${basename}.pcm" > "c_arrays/${basename}.h"

    # Fix variable names (replace dots and dashes with underscores)
    sed -i 's/processed_audio_//g' "c_arrays/${basename}.h"
    sed -i 's/\.pcm//g' "c_arrays/${basename}.h"
    sed -i 's/-/_/g' "c_arrays/${basename}.h"

    echo -e " ${GREEN}✓${NC}"

    # Calculate size
    if [ "$(uname)" == "Darwin" ]; then
        # macOS
        size=$(stat -f%z "processed_audio/${basename}.pcm")
    else
        # Linux
        size=$(stat -c%s "processed_audio/${basename}.pcm")
    fi

    size_kb=$((size / 1024))
    total_size=$((total_size + size))

    # Get duration
    duration=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "processed_audio/${basename}_final.wav")
    duration_rounded=$(printf "%.1f" "$duration")

    echo -e "  ${GREEN}✓ Complete:${NC} ${size} bytes (${size_kb} KB) - ${duration_rounded} seconds"
    echo ""
done

# Summary
total_kb=$((total_size / 1024))
echo -e "${GREEN}=== Processing Complete ===${NC}"
echo ""
echo "Processed files: $file_count"
echo "Total size: $total_size bytes ($total_kb KB)"
echo ""
echo "C header files are in: c_arrays/"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review generated files in c_arrays/"
echo "2. Copy to: components/chime_library/chimes/"
echo "3. Include in chime_library.c"
echo "4. Build and flash firmware"
echo ""

# Create a summary file
cat > c_arrays/README.md <<EOF
# Generated Chime Audio Files

Generated on: $(date)
Source files: $(ls source_audio/*.wav | wc -l)
Total size: $total_kb KB

## Files Generated:

EOF

for file in c_arrays/*.h; do
    basename=$(basename "$file")
    if [ "$(uname)" == "Darwin" ]; then
        size=$(stat -f%z "$file")
    else
        size=$(stat -c%s "$file")
    fi
    size_kb=$((size / 1024))
    echo "- \`$basename\` - ${size_kb} KB" >> c_arrays/README.md
done

cat >> c_arrays/README.md <<EOF

## Usage in ESP32:

\`\`\`c
// In chime_library.c
#include "chimes/westminster_quarter.h"

// Play the chime
audio_manager_play_pcm(westminster_quarter, westminster_quarter_len);
\`\`\`

## License:

Ensure you comply with the license of your source audio files.
See: docs/resources/audio-sources-guide.md
EOF

echo -e "${GREEN}Summary written to: c_arrays/README.md${NC}"
echo ""
echo -e "${BLUE}=== All Done! ===${NC}"
