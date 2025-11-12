# ESP32-S3 Word Clock - Makefile Wrapper
# Convenience wrapper for common development tasks

.PHONY: help build flash monitor clean fullclean ota-prepare ota-release test

# Default target
help:
	@echo "ESP32-S3 Word Clock - Development Commands"
	@echo ""
	@echo "Building:"
	@echo "  make build           - Build firmware"
	@echo "  make flash           - Flash firmware to device"
	@echo "  make monitor         - Open serial monitor"
	@echo "  make clean           - Clean build artifacts"
	@echo "  make fullclean       - Full clean (including config)"
	@echo ""
	@echo "OTA Release Workflow:"
	@echo "  make ota-prepare     - Prepare OTA files (interactive)"
	@echo "  make ota-release     - Build + prepare + auto-push to GitHub"
	@echo ""
	@echo "Testing:"
	@echo "  make test            - Run unit tests (if available)"
	@echo ""
	@echo "Combined:"
	@echo "  make all             - Build and flash"
	@echo ""

# Build firmware
build:
	idf.py build

# Flash firmware
flash:
	idf.py flash

# Serial monitor
monitor:
	idf.py monitor

# Build and flash
all: build flash

# Clean build
clean:
	idf.py clean

# Full clean
fullclean:
	idf.py fullclean

# Prepare OTA files (interactive mode)
ota-prepare: build
	@echo "Preparing OTA files..."
	./post_build_ota.sh

# Build and release OTA (automated mode)
ota-release: build
	@echo "Building and releasing OTA update..."
	./post_build_ota.sh --auto-push

# Test (placeholder for future unit tests)
test:
	@echo "No tests configured yet"
