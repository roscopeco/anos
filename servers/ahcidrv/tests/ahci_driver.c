/*
 * AHCI driver implementation tests - testing of real driver functions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "ahci.h"
#include "munit.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Mock syscall control functions
void mock_syscalls_reset(void);
void mock_map_physical_set_fail(bool should_fail);
void mock_map_virtual_set_fail(bool should_fail);
void mock_alloc_physical_set_fail(bool should_fail);
uint64_t mock_get_last_physical_addr(void);
uint64_t mock_get_last_virtual_addr(void);
size_t mock_get_last_size(void);
uint32_t mock_get_last_flags(void);

// AHCI driver test state reset function
void ahci_reset_test_state(void);

static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    mock_syscalls_reset();
    ahci_reset_test_state();
    return NULL;
}

// ============================================================================
// AHCI CONTROLLER TESTS
// ============================================================================

static MunitResult
test_ahci_controller_init_invalid_args(const MunitParameter params[],
                                       void *data) {
    (void)params;
    (void)data;

    // Test NULL controller
    bool result = ahci_controller_init(NULL, 0x12345000ULL, 0x23456000ULL);
    munit_assert_false(result);

    // Test zero AHCI base
    AHCIController ctrl;
    result = ahci_controller_init(&ctrl, 0, 0x23456000ULL);
    munit_assert_false(result);

    // Test zero PCI config base
    result = ahci_controller_init(&ctrl, 0x12345000ULL, 0);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_controller_init_map_failure(const MunitParameter params[],
                                      void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;

    // Force map_physical to fail
    mock_map_physical_set_fail(true);

    bool result = ahci_controller_init(&ctrl, 0x12345000ULL, 0x23456000ULL);
    munit_assert_false(result);

    // Verify it tried to map the PCI config base first
    munit_assert_uint64(0x23456000ULL, ==, mock_get_last_physical_addr());
    munit_assert_size(0x1000, ==, mock_get_last_size()); // Single page mapping

    return MUNIT_OK;
}

static MunitResult
test_ahci_controller_init_success(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    memset(&ctrl, 0, sizeof(ctrl));

    // Allow mapping to succeed
    mock_map_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Verify controller state - pci_base should be the mapped virtual PCI config address
    munit_assert_uint64(0xC000000000ULL, ==, ctrl.pci_base);
    munit_assert_ptr_not_null(ctrl.mapped_regs);
    munit_assert_ptr_not_null(ctrl.regs);
    munit_assert_true(ctrl.initialized);

    // Verify it mapped the correct address and size
    munit_assert_uint64(0xFEBF0000ULL, ==, mock_get_last_physical_addr());

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult test_ahci_controller_cleanup(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    memset(&ctrl, 0, sizeof(ctrl));

    // Initialize first
    mock_map_physical_set_fail(false);
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Now cleanup
    ahci_controller_cleanup(&ctrl);

    // Verify cleanup
    munit_assert_uint64(0, ==, ctrl.pci_base);
    munit_assert_ptr_null(ctrl.mapped_regs);
    munit_assert_ptr_null(ctrl.regs);
    munit_assert_false(ctrl.initialized);

    return MUNIT_OK;
}

// ============================================================================
// AHCI PORT TESTS
// ============================================================================

static MunitResult
test_ahci_port_init_invalid_args(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;
    memset(&ctrl, 0, sizeof(ctrl));

    // Test NULL port
    bool result = ahci_port_init(NULL, &ctrl, 0);
    munit_assert_false(result);

    // Test NULL controller
    result = ahci_port_init(&port, NULL, 0);
    munit_assert_false(result);

    // Test invalid port number
    result = ahci_port_init(&port, &ctrl, 32);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_init_controller_not_initialized(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    memset(&ctrl, 0, sizeof(ctrl)); // Not initialized

    bool result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_init_memory_allocation_failure(const MunitParameter params[],
                                              void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Initialize controller
    memset(&ctrl, 0, sizeof(ctrl));
    mock_map_physical_set_fail(false);
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Force memory allocation to fail
    mock_map_virtual_set_fail(true);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult test_ahci_port_init_success(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Initialize controller
    memset(&ctrl, 0, sizeof(ctrl));
    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Initialize port
    memset(&port, 0, sizeof(port));
    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Verify port state
    munit_assert_ptr_equal(&ctrl, port.controller);
    munit_assert_uint8(0, ==, port.port_num);
    munit_assert_true(port.initialized);
    munit_assert_ptr_not_null(port.cmd_list);
    munit_assert_ptr_not_null(port.fis_base);
    munit_assert_ptr_not_null(port.cmd_tables);

    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// AHCI DEVICE IDENTIFICATION TESTS
// ============================================================================

static MunitResult
test_ahci_port_identify_invalid_args(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    // Test NULL port
    bool result = ahci_port_identify(NULL);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_identify_uninitialized(const MunitParameter params[],
                                      void *data) {
    (void)params;
    (void)data;

    AHCIPort port = {0};

    const bool result = ahci_port_identify(&port);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_identify_success(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Set up full controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));
    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Initialize port
    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Test the actual identify function
    result = ahci_port_identify(&port);

    munit_assert_true(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// AHCI I/O OPERATION TESTS
// ============================================================================

static MunitResult
test_ahci_port_read_invalid_args(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    uint8_t buffer[512];

    // Test NULL port
    bool result = ahci_port_read(NULL, 0, 1, buffer);
    munit_assert_false(result);

    AHCIPort port;
    memset(&port, 0, sizeof(port));

    // Test NULL buffer
    result = ahci_port_read(&port, 0, 1, NULL);
    munit_assert_false(result);

    // Test zero count
    result = ahci_port_read(&port, 0, 0, buffer);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_write_invalid_args(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    uint8_t buffer[512] = {0};

    // Test NULL port
    bool result = ahci_port_write(NULL, 0, 1, buffer);
    munit_assert_false(result);

    AHCIPort port;
    memset(&port, 0, sizeof(port));

    // Test NULL buffer
    result = ahci_port_write(&port, 0, 1, NULL);
    munit_assert_false(result);

    // Test zero count
    result = ahci_port_write(&port, 0, 0, buffer);
    munit_assert_false(result);

    return MUNIT_OK;
}

// ============================================================================
// TEST SUITE DEFINITION
// ============================================================================

static MunitTest test_suite_tests[] = {
        // Controller tests
        {"/controller/init_invalid_args",
         test_ahci_controller_init_invalid_args, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/controller/init_map_failure", test_ahci_controller_init_map_failure,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/controller/init_success", test_ahci_controller_init_success, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/controller/cleanup", test_ahci_controller_cleanup, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Port tests
        {"/port/init_invalid_args", test_ahci_port_init_invalid_args, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/port/init_controller_not_initialized",
         test_ahci_port_init_controller_not_initialized, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/port/init_memory_failure",
         test_ahci_port_init_memory_allocation_failure, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/port/init_success", test_ahci_port_init_success, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Device identification tests
        {"/identify/invalid_args", test_ahci_port_identify_invalid_args, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/identify/uninitialized", test_ahci_port_identify_uninitialized,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/identify/success", test_ahci_port_identify_success, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // I/O operation tests
        {"/io/read_invalid_args", test_ahci_port_read_invalid_args, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/io/write_invalid_args", test_ahci_port_write_invalid_args, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/ahci/driver", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}