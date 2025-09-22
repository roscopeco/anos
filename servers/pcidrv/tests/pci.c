/*
 * Tests for low-level PCI routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "enumerate.h"
#include "munit.h"
#include "pci.h"

// Mock syscalls for testing
#include <anos/syscalls.h>

// Mock syscall implementations
SyscallResult anos_find_named_channel(const char *name) {
    (void)name; // Unused in tests
    SyscallResult result = {.result = SYSCALL_ERR_NOT_FOUND};
    return result;
}

SyscallResult anos_send_message(uint64_t channel_id, void *buffer,
                                size_t msg_size, void *msg_buffer) {
    (void)channel_id;
    (void)buffer;
    (void)msg_size;
    (void)msg_buffer;
    // Mock - return failure since DEVMAN is not found
    SyscallResult result = {.result = SYSCALL_FAILURE};
    return result;
}

void spawn_ahci_driver(uint64_t ahci_base, uint64_t pci_config_base,
                       uint64_t pci_device_id) {
    (void)ahci_base;
    (void)pci_config_base;
    (void)pci_device_id;
    // Mock - do nothing for tests
}

void spawn_xhci_driver(uint64_t xhci_base, uint64_t pci_config_base,
                       uint64_t pci_device_id) {
    (void)xhci_base;
    (void)pci_config_base;
    (void)pci_device_id;
    // Mock - do nothing for tests
}

// Mock ECAM memory space (16MB to cover bus 0-15)
static uint8_t mock_ecam_space[16 * 1024 * 1024];
static PCIBusDriver test_bus_driver;

// Mock ECAM base address for testing
#define MOCK_ECAM_BASE 0x90000000ULL
#define MOCK_ECAM_VIRTUAL_BASE ((volatile void *)mock_ecam_space)

static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    // Clear mock ECAM space
    memset(mock_ecam_space, 0xFF,
           sizeof(mock_ecam_space)); // Default to 0xFF (no device)

    // Set up test bus driver
    memset(&test_bus_driver, 0, sizeof(test_bus_driver));
    test_bus_driver.ecam_base = MOCK_ECAM_BASE;
    test_bus_driver.segment = 0;
    test_bus_driver.bus_start = 0;
    test_bus_driver.bus_end = 15;
    test_bus_driver.mapped_ecam = MOCK_ECAM_VIRTUAL_BASE;
    test_bus_driver.mapped_size = sizeof(mock_ecam_space);

    return NULL;
}

// Helper function to set up a mock PCI device
static void setup_mock_device(uint8_t bus, uint8_t device, uint8_t function,
                              uint16_t vendor_id, uint16_t device_id,
                              uint8_t class_code, uint8_t subclass,
                              uint8_t prog_if) {
    // Calculate device offset in ECAM space
    const uint64_t device_offset = ((uint64_t)bus << 20) |
                                   ((uint64_t)device << 15) |
                                   ((uint64_t)function << 12);
    uint8_t *device_config = mock_ecam_space + device_offset;

    // Set up basic configuration space
    *(uint16_t *)(device_config + 0x00) = vendor_id; // Vendor ID
    *(uint16_t *)(device_config + 0x02) = device_id; // Device ID
    *(uint16_t *)(device_config + 0x04) = 0x0000;    // Command
    *(uint16_t *)(device_config + 0x06) = 0x0010;  // Status (capabilities bit)
    *(uint8_t *)(device_config + 0x08) = 0x00;     // Revision ID
    *(uint8_t *)(device_config + 0x09) = prog_if;  // Programming Interface
    *(uint8_t *)(device_config + 0x0A) = subclass; // Subclass
    *(uint8_t *)(device_config + 0x0B) = class_code; // Class code
    *(uint8_t *)(device_config + 0x0E) = 0x00; // Header type (single function)
}

static MunitResult test_PCI_REG_UU_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_UU_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_UU_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_UU_B(0x12345678);
    munit_assert_uint8(result, ==, 0x12);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_UM_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_UM_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_UM_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_UM_B(0x12345678);
    munit_assert_uint8(result, ==, 0x34);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_LM_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_LM_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_LM_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_LM_B(0x12345678);
    munit_assert_uint8(result, ==, 0x56);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_LL_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_LL_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_LL_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_LL_B(0x12345678);
    munit_assert_uint8(result, ==, 0x78);

    return MUNIT_OK;
}

// ============================================================================
// PCI CONFIGURATION SPACE ACCESS TESTS
// ============================================================================

static MunitResult test_pci_config_read32_basic(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    // Set up a mock device at bus 0, device 1, function 0
    setup_mock_device(0, 1, 0, 0x8086, 0x1234, 0x01, 0x06, 0x01);

    // Test reading vendor/device ID
    uint32_t vendor_device = pci_config_read32(&test_bus_driver, 0, 1, 0, 0);
    munit_assert_uint32(
            0x12348086, ==,
            vendor_device); // Little endian: device_id << 16 | vendor_id

    return MUNIT_OK;
}

static MunitResult test_pci_config_read16_basic(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    // Set up a mock device
    setup_mock_device(0, 2, 0, 0x8086, 0x1234, 0x01, 0x06, 0x01);

    // Test reading vendor ID (offset 0)
    uint16_t vendor_id = pci_config_read16(&test_bus_driver, 0, 2, 0, 0);
    munit_assert_uint16(0x8086, ==, vendor_id);

    // Test reading device ID (offset 2)
    uint16_t device_id = pci_config_read16(&test_bus_driver, 0, 2, 0, 2);
    munit_assert_uint16(0x1234, ==, device_id);

    return MUNIT_OK;
}

static MunitResult test_pci_config_read8_basic(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    // Set up a mock device
    setup_mock_device(0, 3, 0, 0x8086, 0x1234, 0x01, 0x06, 0x01);

    // Test reading class code (offset 0x0B)
    uint8_t class_code = pci_config_read8(&test_bus_driver, 0, 3, 0, 0x0B);
    munit_assert_uint8(0x01, ==, class_code);

    // Test reading subclass (offset 0x0A)
    uint8_t subclass = pci_config_read8(&test_bus_driver, 0, 3, 0, 0x0A);
    munit_assert_uint8(0x06, ==, subclass);

    // Test reading prog_if (offset 0x09)
    uint8_t prog_if = pci_config_read8(&test_bus_driver, 0, 3, 0, 0x09);
    munit_assert_uint8(0x01, ==, prog_if);

    return MUNIT_OK;
}

static MunitResult test_pci_config_read_no_device(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Don't set up any device - should read 0xFFFF for vendor ID
    uint16_t vendor_id = pci_config_read16(&test_bus_driver, 0, 5, 0, 0);
    munit_assert_uint16(0xFFFF, ==, vendor_id);

    uint32_t full_read = pci_config_read32(&test_bus_driver, 0, 5, 0, 0);
    munit_assert_uint32(0xFFFFFFFF, ==, full_read);

    return MUNIT_OK;
}

static MunitResult
test_pci_config_read_out_of_bounds(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Test reading from bus outside our range
    uint32_t result = pci_config_read32(&test_bus_driver, 20, 0, 0,
                                        0); // Bus 20 > bus_end (15)
    munit_assert_uint32(0xFFFFFFFF, ==, result);

    result = pci_config_read32(&test_bus_driver, 255, 0, 0,
                               0); // Way out of range
    munit_assert_uint32(0xFFFFFFFF, ==, result);

    return MUNIT_OK;
}

static MunitResult
test_pci_config_read_null_driver(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Test with NULL bus driver
    uint32_t result = pci_config_read32(NULL, 0, 0, 0, 0);
    munit_assert_uint32(0xFFFFFFFF, ==, result);

    return MUNIT_OK;
}

static MunitResult test_pci_config_read_alignment(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Set up device with known pattern
    setup_mock_device(0, 4, 0, 0x1234, 0x5678, 0xAB, 0xCD, 0xEF);

    // The class_d register at 0x08 should contain: revision(08) | prog_if(09) | subclass(0A) | class_code(0B)
    // In little-endian: 0x00 | 0xEF | 0xCD | 0xAB = 0xABCDEF00
    uint32_t class_d = pci_config_read32(&test_bus_driver, 0, 4, 0, 0x08);
    munit_assert_uint32(0xABCDEF00, ==, class_d);

    // Test unaligned 16-bit read
    uint16_t unaligned = pci_config_read16(
            &test_bus_driver, 0, 4, 0, 0x09); // Should read prog_if | subclass
    munit_assert_uint16(0xCDEF, ==, unaligned);

    return MUNIT_OK;
}

// ============================================================================
// PCI DEVICE ENUMERATION TESTS
// ============================================================================

static MunitResult test_pci_device_exists_true(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    // Set up a device
    setup_mock_device(0, 6, 0, 0x8086, 0x1234, 0x01, 0x06, 0x01);

    // Test that device exists
    bool exists = pci_device_exists(&test_bus_driver, 0, 6, 0);
    munit_assert_true(exists);

    return MUNIT_OK;
}

static MunitResult test_pci_device_exists_false(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    // Don't set up any device at this location
    bool exists = pci_device_exists(&test_bus_driver, 0, 7, 0);
    munit_assert_false(exists);

    return MUNIT_OK;
}

static MunitResult
test_pci_device_exists_invalid_vendor_ids(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    // Test vendor ID 0x0000 (invalid)
    setup_mock_device(0, 8, 0, 0x0000, 0x1234, 0x01, 0x06, 0x01);
    bool exists = pci_device_exists(&test_bus_driver, 0, 8, 0);
    munit_assert_false(exists);

    // Test vendor ID 0xFFFF (no device - but we already fill with 0xFF by default)
    // This should already be false since we fill mock_ecam_space with 0xFF
    exists = pci_device_exists(&test_bus_driver, 0, 9, 0);
    munit_assert_false(exists);

    return MUNIT_OK;
}

static MunitResult
test_pci_device_exists_valid_vendor_ids(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    // Test various valid vendor IDs
    setup_mock_device(0, 10, 0, 0x8086, 0x1234, 0x01, 0x06, 0x01); // Intel
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 10, 0));

    setup_mock_device(0, 11, 0, 0x1022, 0x5678, 0x01, 0x06, 0x01); // AMD
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 11, 0));

    setup_mock_device(0, 12, 0, 0x0001, 0x9ABC, 0x01, 0x06,
                      0x01); // Minimum valid
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 12, 0));

    setup_mock_device(0, 13, 0, 0xFFFE, 0xDEF0, 0x01, 0x06,
                      0x01); // Maximum valid
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 13, 0));

    return MUNIT_OK;
}

// ============================================================================
// AHCI CONTROLLER DETECTION TESTS
// ============================================================================

static MunitResult test_ahci_controller_detection(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Set up AHCI controller (class 0x01, subclass 0x06, prog_if 0x01)
    setup_mock_device(0, 14, 0, 0x8086, 0x2922, 0x01, 0x06, 0x01);

    // Set up BAR5 (AHCI Base Address Register) at offset 0x24
    const uint64_t device_offset =
            ((uint64_t)0 << 20) | ((uint64_t)14 << 15) | ((uint64_t)0 << 12);
    uint8_t *device_config = mock_ecam_space + device_offset;
    *(uint32_t *)(device_config + 0x24) = 0xFEBF0000; // BAR5 low
    *(uint32_t *)(device_config + 0x28) = 0x00000000; // BAR5 high

    // Read back the values to verify AHCI detection would work
    uint8_t class_code = pci_config_read8(&test_bus_driver, 0, 14, 0, 0x0B);
    uint8_t subclass = pci_config_read8(&test_bus_driver, 0, 14, 0, 0x0A);
    uint8_t prog_if = pci_config_read8(&test_bus_driver, 0, 14, 0, 0x09);
    uint32_t bar5_low = pci_config_read32(&test_bus_driver, 0, 14, 0, 0x24);

    munit_assert_uint8(0x01, ==, class_code);
    munit_assert_uint8(0x06, ==, subclass);
    munit_assert_uint8(0x01, ==, prog_if);
    munit_assert_uint32(0xFEBF0000, ==, bar5_low);

    return MUNIT_OK;
}

// ============================================================================
// MULTI-FUNCTION DEVICE TESTS
// ============================================================================

static MunitResult
test_multi_function_device_detection(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    // Set up a multi-function device (header type bit 7 set)
    setup_mock_device(0, 15, 0, 0x8086, 0x1000, 0x01, 0x06, 0x01);

    // Set header type to multi-function (bit 7 = 1)
    const uint64_t device_offset =
            ((uint64_t)0 << 20) | ((uint64_t)15 << 15) | ((uint64_t)0 << 12);
    uint8_t *device_config = mock_ecam_space + device_offset;
    *(uint8_t *)(device_config + 0x0E) = 0x80; // Multi-function header type

    // Set up additional functions
    setup_mock_device(0, 15, 1, 0x8086, 0x1001, 0x01, 0x06, 0x01);
    setup_mock_device(0, 15, 2, 0x8086, 0x1002, 0x01, 0x06, 0x01);

    // Test function 0 header type
    uint8_t header_type = pci_config_read8(&test_bus_driver, 0, 15, 0, 0x0E);
    munit_assert_uint8(0x80, ==, header_type);
    munit_assert_true(header_type & 0x80); // Multi-function bit

    // Test that additional functions exist
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 15, 1));
    munit_assert_true(pci_device_exists(&test_bus_driver, 0, 15, 2));
    munit_assert_false(
            pci_device_exists(&test_bus_driver, 0, 15, 3)); // No function 3

    return MUNIT_OK;
}

// ============================================================================
// BRIDGE DEVICE TESTS
// ============================================================================

static MunitResult test_pci_bridge_detection(const MunitParameter params[],
                                             void *data) {
    (void)params;
    (void)data;

    // Set up PCI-to-PCI bridge (class 0x06, subclass 0x04)
    setup_mock_device(1, 0, 0, 0x8086, 0x2448, 0x06, 0x04, 0x00);

    // Set header type to bridge (type 1)
    const uint64_t device_offset =
            ((uint64_t)1 << 20) | ((uint64_t)0 << 15) | ((uint64_t)0 << 12);
    uint8_t *device_config = mock_ecam_space + device_offset;
    *(uint8_t *)(device_config + 0x0E) = 0x01; // Bridge header type
    *(uint8_t *)(device_config + 0x19) = 0x02; // Secondary bus number = 2

    // Test bridge detection
    uint8_t header_type = pci_config_read8(&test_bus_driver, 1, 0, 0, 0x0E);
    munit_assert_uint8(0x01, ==,
                       header_type &
                               0x7F); // Bridge type (ignore multi-function bit)

    uint8_t secondary_bus = pci_config_read8(&test_bus_driver, 1, 0, 0, 0x19);
    munit_assert_uint8(0x02, ==, secondary_bus);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        // Register manipulation macro tests
        {(char *)"/pci/PCI_REG_UU_B", test_PCI_REG_UU_B, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_UM_B", test_PCI_REG_UM_B, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LM_B", test_PCI_REG_LM_B, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LL_B", test_PCI_REG_LL_B, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // PCI configuration space access tests
        {(char *)"/pci/config_read32_basic", test_pci_config_read32_basic,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read16_basic", test_pci_config_read16_basic,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read8_basic", test_pci_config_read8_basic, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read_no_device", test_pci_config_read_no_device,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read_out_of_bounds",
         test_pci_config_read_out_of_bounds, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read_null_driver",
         test_pci_config_read_null_driver, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/pci/config_read_alignment", test_pci_config_read_alignment,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // PCI device enumeration tests
        {(char *)"/pci/device_exists_true", test_pci_device_exists_true, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/device_exists_false", test_pci_device_exists_false,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/device_exists_invalid_vendor_ids",
         test_pci_device_exists_invalid_vendor_ids, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/device_exists_valid_vendor_ids",
         test_pci_device_exists_valid_vendor_ids, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // AHCI controller detection tests
        {(char *)"/pci/ahci_controller_detection",
         test_ahci_controller_detection, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        // Multi-function device tests
        {(char *)"/pci/multi_function_device_detection",
         test_multi_function_device_detection, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Bridge device tests
        {(char *)"/pci/bridge_detection", test_pci_bridge_detection, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
