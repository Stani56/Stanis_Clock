# Preventing OTA Failures - Quick Start

**TL;DR:** Run `make ota-test` before every deployment. It's that simple.

## The Problem We Solved

**v2.10.0 Bug:** OTA updates failed because SHA-256 was calculated on the full 2.5MB partition instead of just the 1.3MB firmware, including residual old data.

**Result:** Checksum mismatch → OTA update rejected → Users had to USB flash.

## The Solution (Multi-Layered)

### 1️⃣ Automated Testing (Primary Defense)

**Test script:** `scripts/test_ota_locally.sh`
- Simulates device partition behavior
- Verifies SHA-256 matches between build system and device
- **Blocks deployment if test fails**

**Usage:**
```bash
make ota-test
```

### 2️⃣ Build System Integration

**Makefile enforces testing:**
```bash
make ota-release  # Automatically runs ota-test first
```

**Cannot deploy without passing test!**

### 3️⃣ Unit Tests

**Regression tests prevent bug recurrence:**
```bash
cd components/ota_manager
idf.py test
```

Includes specific test for v2.10.0 bug scenario.

### 4️⃣ Visual Warnings

**Post-build script shows warning:**
```
⚠️  IMPORTANT: SHA-256 Verification
Before deploying, ALWAYS test SHA-256 calculation:
  make ota-test
```

### 5️⃣ Documentation

- [OTA Testing Guide](ota-testing-guide.md) - Complete testing procedures
- [OTA Safety Checklist](ota-safety-checklist.md) - Quick reference
- [v2.10.0 Bug Report](../archive/bug-reports/v2.10.0-sha256-mismatch.md) - Root cause analysis

## Quick Workflow

```bash
# 1. Make code changes
vim main/wordclock.c

# 2. Build
idf.py build

# 3. Test SHA-256 (CRITICAL!)
make ota-test

# 4. If test passes, deploy
make ota-release
```

**That's it!** The safety systems handle the rest.

## What NOT To Do

❌ **NEVER** deploy without running `make ota-test`
❌ **NEVER** manually edit `version.json` SHA-256
❌ **NEVER** skip testing "just this once"
❌ **NEVER** assume SHA-256 calculation is correct

## Test Device OTA Update

**After deploying to GitHub, test on one device first:**

```bash
# Trigger OTA update
mosquitto_pub -t home/Clock_Stani_1/command -m "ota_start_update"

# Watch serial monitor for:
# ✅ SHA-256 checksum verified successfully
```

**If successful, deploy to production devices.**

## Emergency Recovery

**If OTA update fails on device:**

1. USB flash known-good firmware
2. Investigate root cause
3. Run `make ota-test` locally
4. Fix issue and re-test
5. Deploy again

## Files Created

| File | Purpose |
|------|---------|
| `scripts/test_ota_locally.sh` | Local SHA-256 verification test |
| `Makefile` (updated) | Enforces testing before deployment |
| `post_build_ota.sh` (updated) | Warning banner reminder |
| `components/ota_manager/test/test_sha256_calculation.c` | Unit tests |
| `docs/developer/ota-testing-guide.md` | Complete testing guide |
| `docs/developer/ota-safety-checklist.md` | Multi-layer safety reference |
| `docs/archive/bug-reports/v2.10.0-sha256-mismatch.md` | Bug analysis |
| `docs/developer/PREVENTING_OTA_FAILURES.md` | This file |

## Learn More

- **Testing Guide:** [ota-testing-guide.md](ota-testing-guide.md)
- **Safety Checklist:** [ota-safety-checklist.md](ota-safety-checklist.md)
- **Bug Report:** [v2.10.0-sha256-mismatch.md](../archive/bug-reports/v2.10.0-sha256-mismatch.md)

---

**Remember:** 30 seconds of testing prevents hours of debugging.
