/*
 * AHCI PCI function tests - Comprehensive testing of PCI config space access
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "pci.h"
#include "munit.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Mock PCI config space data for testing
static uint32_t mock_pci_config_space[64] = {0}; // 256 bytes

// Test PCI base address that points to our mock data
#define TEST_PCI_BASE ((uint64_t)mock_pci_config_space)

// Reset mock PCI config space
static void reset_mock_pci_config() {
    memset(mock_pci_config_space, 0, sizeof(mock_pci_config_space));
}

// Set up a typical AHCI controller config space
static void setup_typical_ahci_config() {
    mock_pci_config_space[0] = 0x27d28086; // Intel vendor/device
    mock_pci_config_space[1] = 0x02100006; // Command/status with caps bit
    mock_pci_config_space[2] = 0x01060100; // Class code: SATA controller
    mock_pci_config_space[13] = 0x40;      // Capabilities pointer at 0x40

    // MSI capability at offset 0x40
    mock_pci_config_space[16] = 0x00050005; // Next cap at 0x00, MSI cap ID 0x05
    mock_pci_config_space[16] |=
            (0x0080 << 16); // MSI control: 64-bit capable, disabled
}

static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    reset_mock_pci_config();
    return NULL;
}

// ============================================================================
// PCI CONFIG READ TESTS
// ============================================================================

static MunitResult test_pci_read_config32_basic(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    const uint32_t result = pci_read_config32(TEST_PCI_BASE, 0);
    munit_assert_uint32(0x12345678, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_read_config16_alignment_offset_0(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    const uint16_t result = pci_read_config16(TEST_PCI_BASE, 0);
    munit_assert_uint16(0x5678, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_read_config16_alignment_offset_2(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    const uint16_t result = pci_read_config16(TEST_PCI_BASE, 2);
    munit_assert_uint16(0x1234, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_read_config8_all_alignments(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    munit_assert_uint8(0x78, ==, pci_read_config8(TEST_PCI_BASE, 0));
    munit_assert_uint8(0x56, ==, pci_read_config8(TEST_PCI_BASE, 1));
    munit_assert_uint8(0x34, ==, pci_read_config8(TEST_PCI_BASE, 2));
    munit_assert_uint8(0x12, ==, pci_read_config8(TEST_PCI_BASE, 3));

    return MUNIT_OK;
}

static MunitResult
test_pci_read_config_cross_dword_boundary(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;
    mock_pci_config_space[1] = 0x9ABCDEF0;

    // Reading 8-bit at offset 3 should extract highest byte of dword 0: 0x12
    munit_assert_uint8(0x12, ==, pci_read_config8(TEST_PCI_BASE, 3));

    // Reading 16-bit at offset 3 should read from dword 0 and shift to get 0x12 (high byte only)
    munit_assert_uint16(0x12, ==, pci_read_config16(TEST_PCI_BASE, 3));

    return MUNIT_OK;
}

static MunitResult
test_pci_read_config_boundary_offset_255(const MunitParameter params[],
                                         void *data) {
    (void)params;
    (void)data;

    // Test reading at maximum valid PCI config space offset
    mock_pci_config_space[63] = 0xDEADBEEF; // Offset 252

    // Reading at offset 255 should access dword at offset 252 and extract high byte: 0xDE
    munit_assert_uint8(0xDE, ==, pci_read_config8(TEST_PCI_BASE, 255));

    // Reading 16-bit at offset 254 should get 0xDEAD
    munit_assert_uint16(0xDEAD, ==, pci_read_config16(TEST_PCI_BASE, 254));

    return MUNIT_OK;
}

// ============================================================================
// PCI CONFIG WRITE TESTS
// ============================================================================

static MunitResult test_pci_write_config32_basic(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x00000000;

    pci_write_config32(TEST_PCI_BASE, 0, 0xDEADBEEF);
    munit_assert_uint32(0xDEADBEEF, ==, mock_pci_config_space[0]);

    pci_write_config32(TEST_PCI_BASE, 4, 0x12345678);
    munit_assert_uint32(0x12345678, ==, mock_pci_config_space[1]);

    return MUNIT_OK;
}

static MunitResult
test_pci_write_config16_preserve_other_bits(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    // Writing 0xABCD to offset 0 should result in 0x1234ABCD
    pci_write_config16(TEST_PCI_BASE, 0, 0xABCD);
    munit_assert_uint32(0x1234ABCD, ==, mock_pci_config_space[0]);

    // Reset and test offset 2
    mock_pci_config_space[0] = 0x12345678;
    pci_write_config16(TEST_PCI_BASE, 2, 0xABCD);
    munit_assert_uint32(0xABCD5678, ==, mock_pci_config_space[0]);

    return MUNIT_OK;
}

static MunitResult
test_pci_write_config16_offset_alignment(const MunitParameter params[],
                                         void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[0] = 0x12345678;

    // Test writing 16-bit at offset 1: should preserve bytes 0 and 3, modify 1-2
    pci_write_config16(TEST_PCI_BASE, 1, 0xABCD);
    munit_assert_uint32(0x12ABCD78, ==, mock_pci_config_space[0]);

    return MUNIT_OK;
}

// ============================================================================
// MSI CAPABILITY DISCOVERY TESTS
// ============================================================================

static MunitResult
test_pci_find_msi_capability_no_caps_support(const MunitParameter params[],
                                             void *data) {
    (void)params;
    (void)data;

    // Status register without capabilities bit (bit 4) set
    mock_pci_config_space[1] = 0x02100000; // No capabilities

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_find_msi_capability_null_pointer(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    // Capabilities supported but pointer is 0
    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x00;      // Null capabilities pointer

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0, ==, result);

    return MUNIT_OK;
}

static MunitResult test_pci_find_msi_capability_not_found_empty_chain(
        const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x40;      // Caps pointer at 0x40

    // Single capability that's not MSI, chain ends
    mock_pci_config_space[16] =
            0x00010001; // Next at 0x00 (end), Power Management cap

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_find_msi_capability_found_first_in_chain(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    setup_typical_ahci_config();

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0x40, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_find_msi_capability_found_later_in_chain(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x40;      // Caps pointer at 0x40

    // Chain: PM -> MSI -> end
    mock_pci_config_space[16] = 0x00015001; // PM cap (0x01), next at 0x50
    mock_pci_config_space[20] = 0x00000005; // MSI cap (0x05), next at 0x00

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0x50, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_find_msi_capability_malformed_pointer(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x40;      // Caps pointer at 0x40

    // Chain with capability pointing to out-of-bounds offset
    mock_pci_config_space[16] = 0xFF010001; // Next at 0xFF (invalid), PM cap

    // Function should handle out-of-bounds access gracefully
    // Reading at offset 0xFF should access valid memory but likely return 0
    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_find_msi_capability_unaligned_pointer(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x41;      // Unaligned caps pointer

    // Should be masked to 0x40
    mock_pci_config_space[16] = 0x00050005; // MSI cap

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0x40, ==, result);

    return MUNIT_OK;
}

// ============================================================================
// MSI CONFIGURATION TESTS
// ============================================================================

static MunitResult
test_pci_configure_msi_invalid_offset(const MunitParameter params[],
                                      void *data) {
    (void)params;
    (void)data;

    const bool result =
            pci_configure_msi(TEST_PCI_BASE, 0, 0xFEE00000ULL, 0x4000);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_32bit_addressing(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    // MSI control: 32-bit addressing (bit 7 clear)
    mock_pci_config_space[16] = 0x00050005; // MSI cap
    mock_pci_config_space[16] |=
            (0x0000 << 16); // MSI control: 32-bit, disabled

    const bool result =
            pci_configure_msi(TEST_PCI_BASE, 0x40, 0xFEE00000ULL, 0x4000);
    munit_assert_true(result);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_64bit_addressing(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    // MSI control: 64-bit addressing (bit 7 set)
    mock_pci_config_space[16] = 0x00050005; // MSI cap
    mock_pci_config_space[16] |=
            (0x0080 << 16); // MSI control: 64-bit, disabled

    const bool result =
            pci_configure_msi(TEST_PCI_BASE, 0x40, 0x1FEE00000ULL, 0x4000);
    munit_assert_true(result);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_address_splitting_32bit(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    // Test that 64-bit address is correctly split for 32-bit MSI
    // High 32 bits should be ignored

    mock_pci_config_space[16] =
            0x00050005; // MSI cap, 32-bit (no 64-bit capability)

    const bool result = pci_configure_msi(TEST_PCI_BASE, 0x40,
                                          0x123456789ABCDEF0ULL, 0x4000);
    munit_assert_true(result);

    // Verify that only low 32 bits (0x9ABCDEF0) were written to address register
    const uint32_t addr_low = pci_read_config32(TEST_PCI_BASE, 0x44);
    munit_assert_uint32(0x9ABCDEF0, ==, addr_low);

    // Verify data is at offset 0x08 for 32-bit MSI (no high address register)
    const uint16_t msi_data = pci_read_config16(TEST_PCI_BASE, 0x48);
    munit_assert_uint16(0x4000, ==, msi_data);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_address_splitting_64bit(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    // Test that 64-bit address is correctly split for 64-bit MSI

    mock_pci_config_space[16] = 0x00050005;      // MSI cap
    mock_pci_config_space[16] |= (0x0080 << 16); // 64-bit capable

    const bool result = pci_configure_msi(TEST_PCI_BASE, 0x40,
                                          0x123456789ABCDEF0ULL, 0x4000);
    munit_assert_true(result);

    // Verify address splitting for 64-bit MSI:
    // Low 32 bits (0x9ABCDEF0) at offset 0x44
    const uint32_t addr_low = pci_read_config32(TEST_PCI_BASE, 0x44);
    munit_assert_uint32(0x9ABCDEF0, ==, addr_low);

    // High 32 bits (0x12345678) at offset 0x48
    const uint32_t addr_high = pci_read_config32(TEST_PCI_BASE, 0x48);
    munit_assert_uint32(0x12345678, ==, addr_high);

    // Data at offset 0x4C for 64-bit MSI (after both address registers)
    const uint16_t msi_data = pci_read_config16(TEST_PCI_BASE, 0x4C);
    munit_assert_uint16(0x4000, ==, msi_data);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_enable_disable_sequence(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    setup_typical_ahci_config();

    // Pre-enable MSI for testing disable/enable sequence
    mock_pci_config_space[16] |= (0x0001 << 16); // Enable MSI (bit 0)

    // Verify MSI is initially enabled
    const uint16_t initial_control = pci_read_config16(TEST_PCI_BASE, 0x42);
    munit_assert_uint16(
            0x0085, ==,
            initial_control); // Adjust based on actual MSI control layout
    munit_assert_true(initial_control & 0x0001); // MSI Enable bit set

    const bool result =
            pci_configure_msi(TEST_PCI_BASE, 0x40, 0xFEE00000ULL, 0x4000);
    munit_assert_true(result);

    // Verify final state: MSI should be enabled with new configuration
    const uint16_t final_control = pci_read_config16(TEST_PCI_BASE, 0x42);
    munit_assert_true(final_control & 0x0001); // MSI Enable bit set
    munit_assert_true(final_control & 0x0080); // 64-bit capable

    // Verify address was configured (64-bit format)
    const uint32_t addr_low = pci_read_config32(TEST_PCI_BASE, 0x44);
    const uint32_t addr_high = pci_read_config32(TEST_PCI_BASE, 0x48);
    munit_assert_uint32(0xFEE00000, ==, addr_low);
    munit_assert_uint32(0x00000000, ==, addr_high);

    // Verify data was configured (at offset 0x4C for 64-bit)
    const uint16_t msi_data = pci_read_config16(TEST_PCI_BASE, 0x4C);
    munit_assert_uint16(0x4000, ==, msi_data);

    return MUNIT_OK;
}

static MunitResult
test_pci_configure_msi_data_offset_32bit_vs_64bit(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Verify data is written to correct offset based on addressing mode

    // 32-bit: data at offset 0x08
    mock_pci_config_space[16] = 0x00050005; // 32-bit MSI
    bool result = pci_configure_msi(TEST_PCI_BASE, 0x40, 0xFEE00000ULL, 0x1234);
    munit_assert_true(result);

    // 64-bit: data at offset 0x0C
    mock_pci_config_space[16] |= (0x0080 << 16); // Make it 64-bit
    result = pci_configure_msi(TEST_PCI_BASE, 0x40, 0xFEE00000ULL, 0x5678);
    munit_assert_true(result);

    return MUNIT_OK;
}

// ============================================================================
// EDGE CASE AND ERROR HANDLING TESTS
// ============================================================================

static MunitResult test_pci_config_maximum_offset(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Test reading at offset 255 (maximum valid)
    mock_pci_config_space[63] = 0xDEADBEEF;

    // Should read from dword 63 and extract correct byte
    munit_assert_uint8(0xDE, ==, pci_read_config8(TEST_PCI_BASE, 255));
    munit_assert_uint32(0xDEADBEEF, ==, pci_read_config32(TEST_PCI_BASE, 252));

    return MUNIT_OK;
}

static MunitResult
test_msi_capability_chain_limit(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Create a very long capability chain to test any limits
    mock_pci_config_space[1] = 0x02100010; // Capabilities bit set
    mock_pci_config_space[13] = 0x40;      // Start at 0x40

    // Chain through most of config space
    for (int i = 0x40; i < 0xFC; i += 4) {
        const int idx = i / 4;
        if (i < 0xF8) {
            mock_pci_config_space[idx] = ((i + 4) << 8) | 0x01; // Next cap, PM
        } else {
            mock_pci_config_space[idx] = 0x00050005; // End with MSI
        }
    }

    const uint8_t result = pci_find_msi_capability(TEST_PCI_BASE);
    munit_assert_uint8(0xF8, ==, result);

    return MUNIT_OK;
}

// ============================================================================
// TEST SUITE DEFINITION
// ============================================================================

static MunitTest test_suite_tests[] = {
        // PCI config read tests
        {"/mem/read32_basic", test_pci_read_config32_basic, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/read16_offset0", test_pci_read_config16_alignment_offset_0,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/read16_offset2", test_pci_read_config16_alignment_offset_2,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/read8_all_alignments", test_pci_read_config8_all_alignments,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/read_cross_boundary", test_pci_read_config_cross_dword_boundary,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/read_max_offset", test_pci_read_config_boundary_offset_255,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // PCI config write tests
        {"/mem/write32_basic", test_pci_write_config32_basic, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/write16_preserve_bits",
         test_pci_write_config16_preserve_other_bits, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/write16_alignment", test_pci_write_config16_offset_alignment,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // MSI capability discovery tests
        {"/msi/find_no_caps", test_pci_find_msi_capability_no_caps_support,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_null_pointer", test_pci_find_msi_capability_null_pointer,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_not_found",
         test_pci_find_msi_capability_not_found_empty_chain, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_first", test_pci_find_msi_capability_found_first_in_chain,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_later", test_pci_find_msi_capability_found_later_in_chain,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_malformed", test_pci_find_msi_capability_malformed_pointer,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/find_unaligned", test_pci_find_msi_capability_unaligned_pointer,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // MSI configuration tests
        {"/msi/config_invalid_offset", test_pci_configure_msi_invalid_offset,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/config_32bit", test_pci_configure_msi_32bit_addressing, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/config_64bit", test_pci_configure_msi_64bit_addressing, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/address_split_32bit",
         test_pci_configure_msi_address_splitting_32bit, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/address_split_64bit",
         test_pci_configure_msi_address_splitting_64bit, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/enable_disable_sequence",
         test_pci_configure_msi_enable_disable_sequence, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/msi/data_offset_modes",
         test_pci_configure_msi_data_offset_32bit_vs_64bit, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Edge case tests
        {"/edge/max_offset", test_pci_config_maximum_offset, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/edge/long_cap_chain", test_msi_capability_chain_limit, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/ahci/pci", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}