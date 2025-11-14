#!/bin/bash
################################################################################
# ESP32-S3 Word Clock - Post-Build OTA Automation Script
################################################################################
# This script automates the OTA firmware deployment process after a successful
# build. It handles firmware file preparation, SHA-256 checksum generation,
# version.json updates, and optional GitHub deployment.
#
# Usage:
#   ./post_build_ota.sh [options]
#
# Options:
#   --version VERSION    Set version number (e.g., v2.8.1)
#   --title TITLE        Set version title
#   --description DESC   Set version description
#   --auto-push          Automatically commit and push to GitHub
#   --skip-git           Skip all git operations
#   --help               Show this help message
#
# Examples:
#   # Interactive mode (prompts for version info)
#   ./post_build_ota.sh
#
#   # Automated mode with auto-push
#   ./post_build_ota.sh --version v2.8.1 --title "Bug Fixes" --auto-push
#
#   # Just prepare files, no git operations
#   ./post_build_ota.sh --skip-git
################################################################################

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
OTA_DIR="${PROJECT_ROOT}/ota_files"
FIRMWARE_BIN="${BUILD_DIR}/wordclock.bin"
VERSION_JSON="${OTA_DIR}/version.json"

# Default values
AUTO_PUSH=false
SKIP_GIT=false
VERSION=""
TITLE=""
DESCRIPTION=""

################################################################################
# Helper Functions
################################################################################

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

show_help() {
    head -n 30 "$0" | grep "^#" | sed 's/^# //; s/^#//'
    exit 0
}

################################################################################
# Parse Command Line Arguments
################################################################################

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --title)
            TITLE="$2"
            shift 2
            ;;
        --description)
            DESCRIPTION="$2"
            shift 2
            ;;
        --auto-push)
            AUTO_PUSH=true
            shift
            ;;
        --skip-git)
            SKIP_GIT=true
            shift
            ;;
        --help)
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            ;;
    esac
done

################################################################################
# Pre-flight Checks
################################################################################

print_header "Pre-flight Checks"

# Check if firmware binary exists
if [ ! -f "$FIRMWARE_BIN" ]; then
    print_error "Firmware binary not found: $FIRMWARE_BIN"
    print_info "Run 'idf.py build' first"
    exit 1
fi
print_success "Firmware binary found: $FIRMWARE_BIN"

# Check if OTA directory exists
if [ ! -d "$OTA_DIR" ]; then
    print_warning "OTA directory not found, creating: $OTA_DIR"
    mkdir -p "$OTA_DIR"
fi
print_success "OTA directory ready: $OTA_DIR"

# Check if shasum is available
if ! command -v shasum &> /dev/null; then
    print_error "shasum command not found. Install coreutils or sha256sum"
    exit 1
fi
print_success "SHA-256 tools available"

# Check if jq is available (for JSON manipulation)
if ! command -v jq &> /dev/null; then
    print_warning "jq not found (recommended for JSON pretty-printing)"
    print_info "Install with: sudo apt install jq"
    JQ_AVAILABLE=false
else
    print_success "jq available for JSON formatting"
    JQ_AVAILABLE=true
fi

################################################################################
# Step 1: Copy Firmware to OTA Directory
################################################################################

print_header "Step 1: Copy Firmware Files"

cp -v "$FIRMWARE_BIN" "$OTA_DIR/wordclock.bin"
FIRMWARE_SIZE=$(stat -c%s "$OTA_DIR/wordclock.bin" 2>/dev/null || stat -f%z "$OTA_DIR/wordclock.bin")
print_success "Copied firmware ($(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo "$FIRMWARE_SIZE bytes"))"

# SHA-256 verification reminder (prevent v2.10.0 bug recurrence)
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}⚠️  IMPORTANT: SHA-256 Verification${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}Before deploying, ALWAYS test SHA-256 calculation:${NC}"
echo -e "${BLUE}  make ota-test${NC}"
echo -e "${YELLOW}This prevents OTA failures like the v2.10.0 bug.${NC}"
echo -e "${YELLOW}See: docs/archive/bug-reports/v2.10.0-sha256-mismatch.md${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

################################################################################
# Step 2: Calculate SHA-256 Checksums
################################################################################

print_header "Step 2: Calculate SHA-256 Checksums"

cd "$OTA_DIR"
FIRMWARE_SHA256=$(shasum -a 256 wordclock.bin | cut -d' ' -f1)
print_success "Firmware SHA-256: $FIRMWARE_SHA256"

# Save checksum to file for verification
echo "$FIRMWARE_SHA256  wordclock.bin" > wordclock.bin.sha256
print_success "Saved checksum to: wordclock.bin.sha256"

################################################################################
# Step 3: Get Git Information
################################################################################

print_header "Step 3: Gather Version Information"

cd "$PROJECT_ROOT"

# Get current git commit hash
if [ "$SKIP_GIT" = false ] && git rev-parse HEAD &> /dev/null; then
    COMMIT_HASH=$(git rev-parse HEAD)
    print_success "Git commit: ${COMMIT_HASH:0:8}"
else
    COMMIT_HASH="unknown"
    print_warning "Not a git repository or --skip-git enabled"
fi

# Get build date (ISO 8601 format)
BUILD_DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
print_success "Build date: $BUILD_DATE"

# Get ESP-IDF version
if [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/version.txt" ]; then
    ESP_IDF_VERSION=$(cat "$IDF_PATH/version.txt" | tr -d '\n')
else
    ESP_IDF_VERSION="5.4.2"  # Default fallback
fi
print_success "ESP-IDF version: $ESP_IDF_VERSION"

################################################################################
# Step 4: Interactive or Automated Version Info
################################################################################

print_header "Step 4: Version Information"

# If version not provided via command line, prompt user
if [ -z "$VERSION" ]; then
    # Read current version from version.json if it exists
    if [ -f "$VERSION_JSON" ] && [ "$JQ_AVAILABLE" = true ]; then
        CURRENT_VERSION=$(jq -r '.version' "$VERSION_JSON")
        print_info "Current version: $CURRENT_VERSION"
    fi

    echo -n "Enter new version (e.g., v2.8.1): "
    read VERSION

    if [ -z "$VERSION" ]; then
        print_error "Version is required"
        exit 1
    fi
fi

if [ -z "$TITLE" ]; then
    echo -n "Enter version title (e.g., 'Bug Fixes'): "
    read TITLE
fi

if [ -z "$DESCRIPTION" ]; then
    echo -n "Enter version description: "
    read DESCRIPTION
fi

print_success "Version: $VERSION"
print_success "Title: $TITLE"
print_success "Description: $DESCRIPTION"

################################################################################
# Step 5: Generate version.json
################################################################################

print_header "Step 5: Generate version.json"

# GitHub URLs (will be auto-updated on push)
GITHUB_FIRMWARE_URL="https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/wordclock.bin"
GITHUB_VERSION_URL="https://raw.githubusercontent.com/Stani56/Stanis_Clock/main/ota_files/version.json"

# Create version.json
cat > "$VERSION_JSON" << EOF
{
  "version": "$VERSION",
  "title": "$TITLE",
  "description": "$DESCRIPTION",
  "release_date": "$(date -u +"%Y-%m-%d")",
  "firmware_url": "$GITHUB_FIRMWARE_URL",
  "firmware_size": $FIRMWARE_SIZE,
  "sha256": "$FIRMWARE_SHA256",
  "commit_hash": "$COMMIT_HASH",
  "esp_idf_version": "$ESP_IDF_VERSION",
  "build_date": "$BUILD_DATE",
  "requires_restart": true
}
EOF

# Pretty-print with jq if available
if [ "$JQ_AVAILABLE" = true ]; then
    jq '.' "$VERSION_JSON" > "${VERSION_JSON}.tmp"
    mv "${VERSION_JSON}.tmp" "$VERSION_JSON"
fi

print_success "Generated version.json"

# Display version.json content
echo ""
cat "$VERSION_JSON"
echo ""

################################################################################
# Step 6: Git Operations (Optional)
################################################################################

if [ "$SKIP_GIT" = false ]; then
    print_header "Step 6: Git Operations"

    # Check if there are changes to commit
    cd "$PROJECT_ROOT"

    if git diff --quiet ota_files/; then
        print_info "No changes to commit in ota_files/"
    else
        print_info "Changes detected in ota_files/"

        # Show what will be committed
        git status --short ota_files/

        if [ "$AUTO_PUSH" = true ]; then
            SHOULD_COMMIT=true
        else
            echo ""
            echo -n "Commit and push OTA files to GitHub? [y/N]: "
            read -r CONFIRM
            SHOULD_COMMIT=false
            if [[ $CONFIRM =~ ^[Yy]$ ]]; then
                SHOULD_COMMIT=true
            fi
        fi

        if [ "$SHOULD_COMMIT" = true ]; then
            # Stage OTA files
            git add ota_files/wordclock.bin ota_files/wordclock.bin.sha256 ota_files/version.json

            # Create commit message
            COMMIT_MSG="Release $VERSION: $TITLE

Firmware update with SHA-256 verification support.

Changes:
- Version: $VERSION
- Size: $(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo "$FIRMWARE_SIZE bytes")
- SHA-256: $FIRMWARE_SHA256
- Build date: $BUILD_DATE
- Commit: ${COMMIT_HASH:0:8}

$DESCRIPTION

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>"

            # Commit
            git commit -m "$COMMIT_MSG"
            print_success "Committed OTA files"

            # Push to GitHub
            print_info "Pushing to GitHub..."
            if git push; then
                print_success "Pushed to GitHub"
                echo ""
                print_success "✨ OTA firmware is now available at:"
                print_info "   $GITHUB_FIRMWARE_URL"
                print_info "   $GITHUB_VERSION_URL"
            else
                print_error "Failed to push to GitHub"
                exit 1
            fi
        else
            print_info "Skipping git commit/push"
        fi
    fi
else
    print_info "Git operations skipped (--skip-git)"
fi

################################################################################
# Step 7: Summary
################################################################################

print_header "Summary"

print_success "OTA firmware preparation complete!"
echo ""
echo "📦 Files prepared in: $OTA_DIR"
echo "   - wordclock.bin ($(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo "$FIRMWARE_SIZE bytes"))"
echo "   - wordclock.bin.sha256"
echo "   - version.json"
echo ""
echo "🔐 SHA-256: $FIRMWARE_SHA256"
echo "📌 Version: $VERSION"
echo "📅 Build Date: $BUILD_DATE"
echo ""

if [ "$SKIP_GIT" = false ] && git rev-parse HEAD &> /dev/null; then
    echo "📡 OTA Sources:"
    echo "   • GitHub (primary): Auto-updated on git push ✅"
    echo "   • Cloudflare R2 (backup): Manual upload required"
    echo ""
    echo "To upload to Cloudflare R2:"
    echo "   1. Install rclone: https://rclone.org/install/"
    echo "   2. Configure R2: rclone config"
    echo "   3. Upload: rclone copy $OTA_DIR/wordclock.bin r2:your-bucket/"
    echo "   4. Upload: rclone copy $OTA_DIR/version.json r2:your-bucket/"
fi

echo ""
print_success "🎉 Ready for OTA updates!"
