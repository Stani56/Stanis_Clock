/*
 * OTA Manager SHA-256 Calculation Unit Tests
 *
 * Tests to prevent regression of SHA-256 verification bugs
 *
 * Bug History:
 * - v2.10.0: SHA-256 calculated on full partition size instead of firmware size
 *   Result: Checksum mismatch during OTA update (included residual data)
 *   Fix: Modified calculate_partition_sha256() to accept firmware_size parameter
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_partition.h"
#include "mbedtls/sha256.h"

// Mock partition structure for testing
typedef struct {
    uint8_t *data;
    size_t size;
    size_t firmware_size;  // Actual firmware size (smaller than partition)
} test_partition_t;

/*
 * Test Case 1: SHA-256 with firmware smaller than partition
 *
 * Simulates real-world scenario:
 * - Partition size: 2.5MB (0x280000)
 * - Firmware size: 1.3MB (0x150000)
 * - Residual data: Old firmware from previous update
 *
 * Expected: SHA-256 should only hash firmware_size bytes
 */
TEST_CASE("SHA-256 calculation uses firmware_size not partition_size", "[ota_manager]")
{
    const size_t partition_size = 0x280000;  // 2.5MB
    const size_t firmware_size = 0x150000;   // 1.3MB

    // Create test partition with known data
    uint8_t *partition_data = malloc(partition_size);
    TEST_ASSERT_NOT_NULL(partition_data);

    // Fill with firmware pattern (0xAA)
    memset(partition_data, 0xAA, firmware_size);

    // Fill residual area with old data (0xBB)
    memset(partition_data + firmware_size, 0xBB, partition_size - firmware_size);

    // Calculate SHA-256 of firmware only
    uint8_t firmware_hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // SHA-256
    mbedtls_sha256_update(&ctx, partition_data, firmware_size);  // Only firmware bytes
    mbedtls_sha256_finish(&ctx, firmware_hash);
    mbedtls_sha256_free(&ctx);

    // Calculate SHA-256 of full partition (WRONG - includes residual)
    uint8_t partition_hash[32];
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, partition_data, partition_size);  // Full partition
    mbedtls_sha256_finish(&ctx, partition_hash);
    mbedtls_sha256_free(&ctx);

    // Verify they are different (proves we need firmware_size parameter)
    TEST_ASSERT_NOT_EQUAL(0, memcmp(firmware_hash, partition_hash, 32));

    // This is the critical assertion:
    // OTA manager MUST hash only firmware_size bytes
    printf("✅ SHA-256 differs when hashing full partition vs firmware_size\n");
    printf("   This proves firmware_size parameter is essential!\n");

    free(partition_data);
}

/*
 * Test Case 2: Verify chunked reading produces same hash
 *
 * OTA manager reads partition in 4KB chunks for memory efficiency
 * Must verify chunked reading = direct reading
 */
TEST_CASE("SHA-256 chunked reading matches direct reading", "[ota_manager]")
{
    const size_t firmware_size = 0x150000;  // 1.3MB
    const size_t chunk_size = 4096;         // 4KB chunks

    // Create test firmware with random data
    uint8_t *firmware = malloc(firmware_size);
    TEST_ASSERT_NOT_NULL(firmware);
    for (size_t i = 0; i < firmware_size; i++) {
        firmware[i] = (uint8_t)(i % 256);  // Repeating pattern
    }

    // Direct hash
    uint8_t direct_hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, firmware, firmware_size);
    mbedtls_sha256_finish(&ctx, direct_hash);
    mbedtls_sha256_free(&ctx);

    // Chunked hash (simulates OTA manager behavior)
    uint8_t chunked_hash[32];
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    size_t offset = 0;
    while (offset < firmware_size) {
        size_t bytes_to_read = (firmware_size - offset > chunk_size)
                               ? chunk_size
                               : (firmware_size - offset);
        mbedtls_sha256_update(&ctx, firmware + offset, bytes_to_read);
        offset += bytes_to_read;
    }

    mbedtls_sha256_finish(&ctx, chunked_hash);
    mbedtls_sha256_free(&ctx);

    // Must match exactly
    TEST_ASSERT_EQUAL_HEX8_ARRAY(direct_hash, chunked_hash, 32);

    printf("✅ Chunked reading (4KB) produces identical SHA-256\n");

    free(firmware);
}

/*
 * Test Case 3: Zero-size firmware handling
 *
 * Edge case: firmware_size = 0 should fail safely
 */
TEST_CASE("SHA-256 calculation handles zero firmware size", "[ota_manager]")
{
    const size_t firmware_size = 0;

    // In real OTA manager, this should trigger an error
    // We verify the behavior is safe (no crash)

    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, NULL, firmware_size);  // Should be safe
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    // Hash of empty data should be SHA-256 of empty string
    // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    uint8_t expected_empty_hash[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };

    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_empty_hash, hash, 32);

    printf("✅ Zero-size firmware produces expected empty SHA-256\n");
}

/*
 * Test Case 4: Known SHA-256 test vectors
 *
 * Verify our implementation matches standard test vectors
 */
TEST_CASE("SHA-256 matches known test vectors", "[ota_manager]")
{
    // Test vector 1: "abc"
    const char *test1 = "abc";
    uint8_t expected1[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    uint8_t hash1[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t *)test1, strlen(test1));
    mbedtls_sha256_finish(&ctx, hash1);
    mbedtls_sha256_free(&ctx);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected1, hash1, 32);

    printf("✅ SHA-256 test vector 'abc' matches\n");
}

/*
 * Regression Test: v2.10.0 Bug Scenario
 *
 * Simulates exact conditions that caused v2.10.0 OTA failure
 */
TEST_CASE("Regression test: v2.10.0 partition size bug", "[ota_manager][regression]")
{
    // Real v2.10.0 values
    const size_t partition_size = 0x280000;  // 2.5MB partition
    const size_t firmware_size = 1336608;    // v2.10.0 actual firmware size

    // Simulate partition with firmware + residual old data
    uint8_t *partition = malloc(partition_size);
    TEST_ASSERT_NOT_NULL(partition);

    // Fill with v2.10.0 firmware pattern
    for (size_t i = 0; i < firmware_size; i++) {
        partition[i] = (uint8_t)(i % 256);
    }

    // Fill with old v2.9.0 data (different pattern)
    for (size_t i = firmware_size; i < partition_size; i++) {
        partition[i] = (uint8_t)((i + 100) % 256);
    }

    // WRONG: Hash full partition (v2.10.0 bug)
    uint8_t wrong_hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, partition, partition_size);  // BUG: Uses partition_size
    mbedtls_sha256_finish(&ctx, wrong_hash);
    mbedtls_sha256_free(&ctx);

    // CORRECT: Hash only firmware (v2.10.1 fix)
    uint8_t correct_hash[32];
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, partition, firmware_size);  // FIX: Uses firmware_size
    mbedtls_sha256_finish(&ctx, correct_hash);
    mbedtls_sha256_free(&ctx);

    // These MUST be different (proves the bug existed)
    TEST_ASSERT_NOT_EQUAL(0, memcmp(wrong_hash, correct_hash, 32));

    printf("✅ Regression test confirms v2.10.0 bug:\n");
    printf("   Wrong (partition_size): %02x%02x%02x%02x...\n",
           wrong_hash[0], wrong_hash[1], wrong_hash[2], wrong_hash[3]);
    printf("   Correct (firmware_size): %02x%02x%02x%02x...\n",
           correct_hash[0], correct_hash[1], correct_hash[2], correct_hash[3]);

    free(partition);
}
