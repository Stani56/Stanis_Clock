/**
 * @file external_flash_test.c
 * @brief Comprehensive test suite for W25Q64 external flash
 *
 * Tests all major operations: initialization, JEDEC ID, read/write/erase,
 * page boundaries, sector boundaries, and stress tests.
 *
 * Usage: Call external_flash_run_all_tests() from main application
 */

#include "external_flash.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "flash_test";

// Test pattern data
#define TEST_PATTERN_SIZE 256
static const uint8_t test_pattern_0x55[TEST_PATTERN_SIZE] = {
    [0 ... 255] = 0x55
};
static const uint8_t test_pattern_0xAA[TEST_PATTERN_SIZE] = {
    [0 ... 255] = 0xAA
};
static const uint8_t test_pattern_0xFF[TEST_PATTERN_SIZE] = {
    [0 ... 255] = 0xFF
};

// Test addresses (safe test areas - won't interfere with chime data)
#define TEST_SECTOR_1       0x700000    // Reserved area
#define TEST_SECTOR_2       0x701000    // Reserved area
#define TEST_SECTOR_3       0x702000    // Reserved area

// ========================================================================
// TEST HELPER FUNCTIONS
// ========================================================================

/**
 * @brief Print test result
 */
static void print_test_result(const char *test_name, bool passed)
{
    if (passed) {
        ESP_LOGI(TAG, "✅ PASS: %s", test_name);
    } else {
        ESP_LOGE(TAG, "❌ FAIL: %s", test_name);
    }
}

/**
 * @brief Compare two buffers and log differences
 */
static bool compare_buffers(const uint8_t *expected, const uint8_t *actual, size_t size, const char *test_name)
{
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            ESP_LOGE(TAG, "%s: Mismatch at offset %u: expected 0x%02X, got 0x%02X",
                     test_name, i, expected[i], actual[i]);
            return false;
        }
    }
    return true;
}

// ========================================================================
// INDIVIDUAL TESTS
// ========================================================================

/**
 * @brief Test 1: Initialization and availability
 */
static bool test_initialization(void)
{
    ESP_LOGI(TAG, "Test 1: Initialization and availability");

    // Check if already initialized
    if (!external_flash_available()) {
        ESP_LOGE(TAG, "Flash not available - initialization may have failed");
        return false;
    }

    ESP_LOGI(TAG, "Flash is available");

    // Verify size
    uint32_t size = external_flash_get_size();
    if (size != EXTERNAL_FLASH_SIZE) {
        ESP_LOGE(TAG, "Size mismatch: expected %u, got %u", (unsigned int)EXTERNAL_FLASH_SIZE, (unsigned int)size);
        return false;
    }

    ESP_LOGI(TAG, "Flash size: %u bytes (8 MB)", (unsigned int)size);
    return true;
}

/**
 * @brief Test 2: JEDEC ID verification
 */
static bool test_jedec_id(void)
{
    ESP_LOGI(TAG, "Test 2: JEDEC ID verification");

    uint8_t mfg, mem_type, capacity;
    esp_err_t ret = external_flash_read_jedec_id(&mfg, &mem_type, &capacity);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read JEDEC ID: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "JEDEC ID: 0x%02X 0x%02X 0x%02X", mfg, mem_type, capacity);

    if (mfg != W25Q64_JEDEC_MFG) {
        ESP_LOGE(TAG, "Manufacturer ID mismatch: expected 0x%02X, got 0x%02X",
                 W25Q64_JEDEC_MFG, mfg);
        return false;
    }

    if (mem_type != W25Q64_JEDEC_MEM_TYPE) {
        ESP_LOGE(TAG, "Memory type mismatch: expected 0x%02X, got 0x%02X",
                 W25Q64_JEDEC_MEM_TYPE, mem_type);
        return false;
    }

    if (capacity != W25Q64_JEDEC_CAPACITY) {
        ESP_LOGE(TAG, "Capacity mismatch: expected 0x%02X, got 0x%02X",
                 W25Q64_JEDEC_CAPACITY, capacity);
        return false;
    }

    // Also test verify function
    ret = external_flash_verify_chip();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Chip verification failed");
        return false;
    }

    return true;
}

/**
 * @brief Test 3: Basic erase operation
 */
static bool test_erase(void)
{
    ESP_LOGI(TAG, "Test 3: Sector erase");

    // Erase test sector
    esp_err_t ret = external_flash_erase_sector(TEST_SECTOR_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erase failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Read back and verify all 0xFF (erased state)
    uint8_t buffer[256];
    ret = external_flash_read(TEST_SECTOR_1, buffer, sizeof(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read after erase failed: %s", esp_err_to_name(ret));
        return false;
    }

    if (!compare_buffers(test_pattern_0xFF, buffer, sizeof(buffer), "Erase verification")) {
        return false;
    }

    ESP_LOGI(TAG, "Sector erased successfully (all 0xFF)");
    return true;
}

/**
 * @brief Test 4: Basic write and read
 */
static bool test_write_read(void)
{
    ESP_LOGI(TAG, "Test 4: Basic write and read");

    // Erase sector first
    esp_err_t ret = external_flash_erase_sector(TEST_SECTOR_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pre-erase failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Write test pattern 0x55
    ret = external_flash_write(TEST_SECTOR_1, test_pattern_0x55, TEST_PATTERN_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Read back
    uint8_t buffer[TEST_PATTERN_SIZE];
    ret = external_flash_read(TEST_SECTOR_1, buffer, TEST_PATTERN_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Verify
    if (!compare_buffers(test_pattern_0x55, buffer, TEST_PATTERN_SIZE, "Write/Read 0x55")) {
        return false;
    }

    ESP_LOGI(TAG, "Write/Read successful (pattern 0x55)");
    return true;
}

/**
 * @brief Test 5: Page boundary handling
 */
static bool test_page_boundary(void)
{
    ESP_LOGI(TAG, "Test 5: Page boundary write");

    // Erase sector first
    esp_err_t ret = external_flash_erase_sector(TEST_SECTOR_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pre-erase failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Write data that crosses page boundary
    // Page size is 256 bytes, so write 512 bytes starting at offset 128
    uint32_t test_address = TEST_SECTOR_1 + 128;
    size_t test_size = 512;

    uint8_t *write_buffer = malloc(test_size);
    if (!write_buffer) {
        ESP_LOGE(TAG, "Failed to allocate write buffer");
        return false;
    }

    // Fill with incrementing pattern
    for (size_t i = 0; i < test_size; i++) {
        write_buffer[i] = i & 0xFF;
    }

    // Write across page boundary
    ret = external_flash_write(test_address, write_buffer, test_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write across page boundary failed: %s", esp_err_to_name(ret));
        free(write_buffer);
        return false;
    }

    // Read back
    uint8_t *read_buffer = malloc(test_size);
    if (!read_buffer) {
        ESP_LOGE(TAG, "Failed to allocate read buffer");
        free(write_buffer);
        return false;
    }

    ret = external_flash_read(test_address, read_buffer, test_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read after boundary write failed: %s", esp_err_to_name(ret));
        free(write_buffer);
        free(read_buffer);
        return false;
    }

    // Verify
    bool result = compare_buffers(write_buffer, read_buffer, test_size, "Page boundary");

    free(write_buffer);
    free(read_buffer);

    if (result) {
        ESP_LOGI(TAG, "Page boundary write successful");
    }

    return result;
}

/**
 * @brief Test 6: Multiple sector erase
 */
static bool test_erase_range(void)
{
    ESP_LOGI(TAG, "Test 6: Multiple sector erase");

    // Erase 3 sectors (12 KB)
    esp_err_t ret = external_flash_erase_range(TEST_SECTOR_1, 3 * EXTERNAL_FLASH_SECTOR_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Range erase failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Verify all three sectors are erased
    uint8_t buffer[256];
    uint32_t test_addresses[] = {TEST_SECTOR_1, TEST_SECTOR_2, TEST_SECTOR_3};

    for (int i = 0; i < 3; i++) {
        ret = external_flash_read(test_addresses[i], buffer, sizeof(buffer));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Read sector %d failed: %s", i, esp_err_to_name(ret));
            return false;
        }

        if (!compare_buffers(test_pattern_0xFF, buffer, sizeof(buffer), "Range erase check")) {
            ESP_LOGE(TAG, "Sector %d not properly erased", i);
            return false;
        }
    }

    ESP_LOGI(TAG, "Range erase successful (3 sectors)");
    return true;
}

/**
 * @brief Test 7: Alternating patterns (stress test)
 */
static bool test_alternating_patterns(void)
{
    ESP_LOGI(TAG, "Test 7: Alternating pattern stress test");

    uint8_t buffer[TEST_PATTERN_SIZE];

    // Test pattern 1: 0x55
    external_flash_erase_sector(TEST_SECTOR_1);
    external_flash_write(TEST_SECTOR_1, test_pattern_0x55, TEST_PATTERN_SIZE);
    external_flash_read(TEST_SECTOR_1, buffer, TEST_PATTERN_SIZE);
    if (!compare_buffers(test_pattern_0x55, buffer, TEST_PATTERN_SIZE, "Pattern 0x55")) {
        return false;
    }

    // Test pattern 2: 0xAA
    external_flash_erase_sector(TEST_SECTOR_1);
    external_flash_write(TEST_SECTOR_1, test_pattern_0xAA, TEST_PATTERN_SIZE);
    external_flash_read(TEST_SECTOR_1, buffer, TEST_PATTERN_SIZE);
    if (!compare_buffers(test_pattern_0xAA, buffer, TEST_PATTERN_SIZE, "Pattern 0xAA")) {
        return false;
    }

    // Test pattern 3: Incrementing
    uint8_t inc_pattern[TEST_PATTERN_SIZE];
    for (int i = 0; i < TEST_PATTERN_SIZE; i++) {
        inc_pattern[i] = i;
    }
    external_flash_erase_sector(TEST_SECTOR_1);
    external_flash_write(TEST_SECTOR_1, inc_pattern, TEST_PATTERN_SIZE);
    external_flash_read(TEST_SECTOR_1, buffer, TEST_PATTERN_SIZE);
    if (!compare_buffers(inc_pattern, buffer, TEST_PATTERN_SIZE, "Incrementing pattern")) {
        return false;
    }

    ESP_LOGI(TAG, "Alternating pattern test successful");
    return true;
}

/**
 * @brief Test 8: Large block read/write (4KB)
 */
static bool test_large_block(void)
{
    ESP_LOGI(TAG, "Test 8: Large block (4KB) read/write");

    const size_t block_size = 4096;  // Full sector

    uint8_t *write_buffer = malloc(block_size);
    uint8_t *read_buffer = malloc(block_size);

    if (!write_buffer || !read_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(write_buffer);
        free(read_buffer);
        return false;
    }

    // Fill with pseudo-random pattern
    for (size_t i = 0; i < block_size; i++) {
        write_buffer[i] = (i * 7 + 13) & 0xFF;
    }

    // Erase and write
    esp_err_t ret = external_flash_erase_sector(TEST_SECTOR_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erase failed: %s", esp_err_to_name(ret));
        free(write_buffer);
        free(read_buffer);
        return false;
    }

    ret = external_flash_write(TEST_SECTOR_1, write_buffer, block_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
        free(write_buffer);
        free(read_buffer);
        return false;
    }

    // Read back
    ret = external_flash_read(TEST_SECTOR_1, read_buffer, block_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
        free(write_buffer);
        free(read_buffer);
        return false;
    }

    // Verify
    bool result = compare_buffers(write_buffer, read_buffer, block_size, "4KB block");

    free(write_buffer);
    free(read_buffer);

    if (result) {
        ESP_LOGI(TAG, "Large block test successful (4096 bytes)");
    }

    return result;
}

/**
 * @brief Test 9: Statistics tracking
 */
static bool test_statistics(void)
{
    ESP_LOGI(TAG, "Test 9: Statistics tracking");

    uint32_t reads, writes, erases;

    // Reset statistics
    external_flash_reset_stats();

    // Perform some operations
    external_flash_erase_sector(TEST_SECTOR_1);
    uint8_t buffer[256];
    external_flash_write(TEST_SECTOR_1, test_pattern_0x55, sizeof(buffer));
    external_flash_read(TEST_SECTOR_1, buffer, sizeof(buffer));

    // Check statistics
    external_flash_get_stats(&reads, &writes, &erases);

    ESP_LOGI(TAG, "Stats: Reads=%u, Writes=%u, Erases=%u",
             (unsigned int)reads, (unsigned int)writes, (unsigned int)erases);

    if (reads != 1 || writes != 1 || erases != 1) {
        ESP_LOGE(TAG, "Statistics mismatch: expected R=1, W=1, E=1");
        return false;
    }

    ESP_LOGI(TAG, "Statistics tracking working correctly");
    return true;
}

// ========================================================================
// MAIN TEST RUNNER
// ========================================================================

/**
 * @brief Run all external flash tests
 * @return Number of failed tests (0 = all passed)
 */
int external_flash_run_all_tests(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  W25Q64 EXTERNAL FLASH TEST SUITE");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    int failed = 0;
    int total = 9;

    // Run all tests
    print_test_result("Test 1: Initialization", test_initialization());
    if (!test_initialization()) failed++;

    print_test_result("Test 2: JEDEC ID", test_jedec_id());
    if (!test_jedec_id()) failed++;

    print_test_result("Test 3: Erase", test_erase());
    if (!test_erase()) failed++;

    print_test_result("Test 4: Write/Read", test_write_read());
    if (!test_write_read()) failed++;

    print_test_result("Test 5: Page Boundary", test_page_boundary());
    if (!test_page_boundary()) failed++;

    print_test_result("Test 6: Erase Range", test_erase_range());
    if (!test_erase_range()) failed++;

    print_test_result("Test 7: Alternating Patterns", test_alternating_patterns());
    if (!test_alternating_patterns()) failed++;

    print_test_result("Test 8: Large Block (4KB)", test_large_block());
    if (!test_large_block()) failed++;

    print_test_result("Test 9: Statistics", test_statistics());
    if (!test_statistics()) failed++;

    // Summary
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  TEST SUMMARY");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Total tests: %d", total);
    ESP_LOGI(TAG, "Passed: %d", total - failed);
    ESP_LOGI(TAG, "Failed: %d", failed);

    if (failed == 0) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "✅ ALL TESTS PASSED!");
        ESP_LOGI(TAG, "");
    } else {
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "❌ %d TEST(S) FAILED", failed);
        ESP_LOGE(TAG, "");
    }

    return failed;
}

/**
 * @brief Quick smoke test (initialization + basic read/write)
 * @return true if basic functionality works
 */
bool external_flash_quick_test(void)
{
    ESP_LOGI(TAG, "Running quick smoke test...");

    if (!test_initialization()) {
        ESP_LOGE(TAG, "Quick test failed: initialization");
        return false;
    }

    if (!test_jedec_id()) {
        ESP_LOGE(TAG, "Quick test failed: JEDEC ID");
        return false;
    }

    if (!test_write_read()) {
        ESP_LOGE(TAG, "Quick test failed: write/read");
        return false;
    }

    ESP_LOGI(TAG, "✅ Quick test passed - flash is working!");
    return true;
}
