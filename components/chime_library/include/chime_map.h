/**
 * @file chime_map.h
 * @brief W25Q64 External Flash Address Map for Audio Storage
 *
 * This file defines the memory layout for audio files stored in the
 * W25Q64 8MB SPI flash chip. All addresses are sector-aligned (4KB).
 *
 * Flash Chip: W25Q64 (8MB total = 0x800000 bytes)
 * Sector Size: 4KB (0x1000 bytes)
 * Page Size: 256 bytes (for programming)
 *
 * @note Address must be sector-aligned for erase operations
 * @note Each address range is sized generously to allow for variations
 */

#ifndef CHIME_MAP_H
#define CHIME_MAP_H

#include <stdint.h>

// ========================================================================
// WESTMINSTER CHIMES (608 KB total)
// ========================================================================
// Primary chime storage - full Westminster quarter-hour system

/** Westminster Quarter Chime (2 sec, 4 notes) - 64 KB allocated */
#define EXT_FLASH_WESTMINSTER_QUARTER       0x000000

/** Westminster Half Chime (3 sec, 8 notes) - 96 KB allocated */
#define EXT_FLASH_WESTMINSTER_HALF          0x010000

/** Westminster Three-Quarter Chime (5 sec, 12 notes) - 160 KB allocated */
#define EXT_FLASH_WESTMINSTER_THREE         0x020000

/** Westminster Full Chime (8 sec, 16 notes) - 256 KB allocated */
#define EXT_FLASH_WESTMINSTER_FULL          0x030000

/** Big Ben Hour Strike (1 sec, single BONG) - 32 KB allocated */
#define EXT_FLASH_BIGBEN_STRIKE             0x050000

/** End of Westminster chimes section */
#define EXT_FLASH_WESTMINSTER_END           0x060000


// ========================================================================
// ALTERNATIVE CHIME STYLES (1 MB total)
// ========================================================================
// Additional chime varieties for user selection

/** Church Bells Quarter Chime - 64 KB allocated */
#define EXT_FLASH_CHURCH_QUARTER            0x100000

/** Church Bells Half Chime - 96 KB allocated */
#define EXT_FLASH_CHURCH_HALF               0x110000

/** Church Bells Full Chime - 256 KB allocated */
#define EXT_FLASH_CHURCH_FULL               0x120000

/** Simple Beep/Tone (for testing/fallback) - 32 KB allocated */
#define EXT_FLASH_SIMPLE_BEEP               0x150000

/** Modern/Digital Chimes - 256 KB allocated */
#define EXT_FLASH_MODERN_CHIMES             0x160000

/** Custom User Chime Slot 1 - 128 KB allocated */
#define EXT_FLASH_CUSTOM_CHIME_1            0x1A0000

/** Custom User Chime Slot 2 - 128 KB allocated */
#define EXT_FLASH_CUSTOM_CHIME_2            0x1C0000

/** End of alternative chimes section */
#define EXT_FLASH_ALTSTYLES_END             0x200000


// ========================================================================
// VOICE ANNOUNCEMENTS - ENGLISH (2 MB total)
// ========================================================================
// Pre-recorded time announcements in English
// Format: "It's [quarter past/half past/quarter to] [hour]"
// Or: "It's [hour] o'clock"

/** English voice announcements start */
#define EXT_FLASH_VOICE_EN_START            0x200000

// Example sub-allocations (can be further organized as needed):
// 0x200000 - 0x220000: Hour announcements (1-12) ~128 KB
// 0x220000 - 0x240000: Minute announcements (15, 30, 45) ~128 KB
// 0x240000 - 0x3F0000: Full time phrases ~1.7 MB
// 0x3F0000 - 0x400000: Special announcements ~64 KB

/** English voice announcements end */
#define EXT_FLASH_VOICE_EN_END              0x400000


// ========================================================================
// VOICE ANNOUNCEMENTS - GERMAN (2 MB total)
// ========================================================================
// Pre-recorded time announcements in German
// Format: "Es ist [viertel/halb/dreiviertel] [Stunde]"
// Or: "Es ist [Stunde] Uhr"

/** German voice announcements start */
#define EXT_FLASH_VOICE_DE_START            0x400000

// Example sub-allocations (can be further organized as needed):
// 0x400000 - 0x420000: Stunde (1-12) ~128 KB
// 0x420000 - 0x440000: Minute phrases ~128 KB
// 0x440000 - 0x5F0000: Full time phrases ~1.7 MB
// 0x5F0000 - 0x600000: Special announcements ~64 KB

/** German voice announcements end */
#define EXT_FLASH_VOICE_DE_END              0x600000


// ========================================================================
// MUSIC & MELODIES (1 MB total)
// ========================================================================
// Short music clips, alarms, special occasion sounds

/** Birthday melody (Happy Birthday) - 256 KB allocated */
#define EXT_FLASH_MUSIC_BIRTHDAY            0x600000

/** Alarm/Wake-up melody - 256 KB allocated */
#define EXT_FLASH_MUSIC_ALARM               0x640000

/** Custom music slot 1 (user-uploadable) - 256 KB allocated */
#define EXT_FLASH_MUSIC_CUSTOM_1            0x680000

/** Custom music slot 2 (user-uploadable) - 256 KB allocated */
#define EXT_FLASH_MUSIC_CUSTOM_2            0x6C0000

/** End of music section */
#define EXT_FLASH_MUSIC_END                 0x700000


// ========================================================================
// RESERVED FOR FUTURE USE (1.4 MB)
// ========================================================================
// Unallocated space for future features

/** Reserved area start */
#define EXT_FLASH_RESERVED_START            0x700000

/** Total flash size (8 MB) */
#define EXT_FLASH_TOTAL_SIZE                0x800000


// ========================================================================
// HELPER MACROS
// ========================================================================

/** Sector size for W25Q64 (4 KB) */
#define SECTOR_SIZE                         0x1000

/** Page size for W25Q64 (256 bytes) */
#define PAGE_SIZE                           0x100

/** Align address down to sector boundary */
#define SECTOR_ALIGN(addr)                  ((addr) & ~(SECTOR_SIZE - 1))

/** Calculate number of sectors needed for a given size */
#define SECTORS_NEEDED(size)                (((size) + SECTOR_SIZE - 1) / SECTOR_SIZE)

/** Check if address is sector-aligned */
#define IS_SECTOR_ALIGNED(addr)             (((addr) & (SECTOR_SIZE - 1)) == 0)

/** Calculate end address for a region */
#define REGION_END(start, size)             ((start) + (size))


// ========================================================================
// VALIDATION MACROS
// ========================================================================

/** Check if address is within flash bounds */
#define IS_VALID_ADDRESS(addr) \
    ((addr) < EXT_FLASH_TOTAL_SIZE)

/** Check if a write operation is within flash bounds */
#define IS_VALID_WRITE(addr, size) \
    (IS_VALID_ADDRESS(addr) && IS_VALID_ADDRESS((addr) + (size) - 1))


// ========================================================================
// STORAGE REGION INFORMATION
// ========================================================================

/**
 * @brief Storage region descriptor structure
 */
typedef struct {
    const char *name;       /**< Human-readable name */
    uint32_t start_addr;    /**< Start address in flash */
    uint32_t end_addr;      /**< End address in flash */
    uint32_t size;          /**< Size in bytes */
} storage_region_t;

/**
 * @brief Get storage region information
 * @param region_id Region identifier (0-based index)
 * @return Pointer to region descriptor, NULL if invalid
 */
const storage_region_t* chime_map_get_region(uint8_t region_id);

/**
 * @brief Get total number of defined regions
 * @return Number of storage regions
 */
uint8_t chime_map_get_region_count(void);


// ========================================================================
// NOTES FOR DEVELOPERS
// ========================================================================

/*
 * AUDIO FORMAT REQUIREMENTS:
 * - Sample Rate: 16 kHz (16000 Hz)
 * - Bit Depth: 16-bit signed PCM
 * - Channels: Mono (1 channel)
 * - Data Rate: 32,000 bytes/sec (16000 samples × 2 bytes)
 * - Endianness: Little-endian (ESP32 native)
 *
 * STORAGE CALCULATIONS:
 * - 1 second of audio = 32,000 bytes (32 KB)
 * - 1 minute of audio = 1,920,000 bytes (1.875 MB)
 *
 * USAGE EXAMPLE:
 *
 * // Read Westminster quarter chime from external flash
 * uint8_t buffer[64000];  // 64 KB max
 * external_flash_read(EXT_FLASH_WESTMINSTER_QUARTER, buffer, 64000);
 * audio_manager_play_pcm(buffer, 64000);
 *
 * // Or stream directly:
 * audio_manager_play_external(EXT_FLASH_WESTMINSTER_QUARTER, 64000);
 *
 * SECTOR ERASE BEFORE WRITE:
 *
 * // Before writing, must erase sectors
 * uint32_t sectors = SECTORS_NEEDED(audio_size);
 * for (uint32_t i = 0; i < sectors; i++) {
 *     external_flash_erase_sector(EXT_FLASH_WESTMINSTER_QUARTER + (i * SECTOR_SIZE));
 * }
 * external_flash_write(EXT_FLASH_WESTMINSTER_QUARTER, audio_data, audio_size);
 */

#endif // CHIME_MAP_H
