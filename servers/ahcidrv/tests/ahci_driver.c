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
#include <stdio.h>
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

static MunitResult
test_ahci_port_read_uninitialized(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    uint8_t buffer[512];
    AHCIPort port;
    memset(&port, 0, sizeof(port));

    // Port not initialized
    bool result = ahci_port_read(&port, 0, 1, buffer);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_write_uninitialized(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    uint8_t buffer[512] = {0x42};
    AHCIPort port;
    memset(&port, 0, sizeof(port));

    // Port not initialized
    bool result = ahci_port_write(&port, 0, 1, buffer);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_ahci_port_read_success(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;
    uint8_t buffer[1024]; // 2 sectors

    // Set up full controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));
    memset(buffer, 0, sizeof(buffer));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Initialize port
    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Identify device to set up sector info
    result = ahci_port_identify(&port);
    munit_assert_true(result);

    // Test single sector read
    result = ahci_port_read(&port, 0, 1, buffer);
    munit_assert_true(result);

    // Test multi-sector read
    result = ahci_port_read(&port, 100, 2, buffer);
    munit_assert_true(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult test_ahci_port_write_success(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;
    uint8_t buffer[1024]; // 2 sectors

    // Set up test data
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));
    memset(buffer, 0xAB, sizeof(buffer)); // Fill with test pattern

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Initialize port
    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Identify device to set up sector info
    result = ahci_port_identify(&port);
    munit_assert_true(result);

    // Test single sector write
    result = ahci_port_write(&port, 0, 1, buffer);
    munit_assert_true(result);

    // Test multi-sector write
    result = ahci_port_write(&port, 200, 2, buffer);
    munit_assert_true(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_read_dma_allocation_failure(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;
    uint8_t buffer[512];

    // Set up controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller and port
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    result = ahci_port_identify(&port);
    munit_assert_true(result);

    // Force DMA allocation to fail during read
    mock_alloc_physical_set_fail(true);

    // Read should fail due to DMA allocation failure
    result = ahci_port_read(&port, 0, 1, buffer);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// DEVICE STATE AND ERROR HANDLING TESTS
// ============================================================================

static MunitResult
test_ahci_port_init_no_device_present(const MunitParameter params[],
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

    // Modify mock AHCI registers to simulate no device present
    extern AHCIRegs mock_ahci_registers;
    mock_ahci_registers.ports[0].ssts =
            0x0; // No device present (DET field = 0)

    // Port init should fail with no device
    memset(&port, 0, sizeof(port));
    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_identify_memory_allocation_failure(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Set up controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller and port
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Force memory allocation to fail during identify
    mock_alloc_physical_set_fail(true);

    // Identify should fail due to buffer allocation failure
    result = ahci_port_identify(&port);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// MULTIPLE PORT SCENARIOS
// ============================================================================

static MunitResult test_ahci_multiple_ports_init(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort ports[4];

    // Initialize controller
    memset(&ctrl, 0, sizeof(ctrl));
    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Set up mock to have 4 active ports
    extern AHCIRegs mock_ahci_registers;
    mock_ahci_registers.host.cap =
            0x3; // 4 ports (bits 0-4 = 3, so port_count = 3+1 = 4)
    mock_ahci_registers.host.pi = 0xF; // Ports 0-3 active

    // Update controller state to reflect new CAP register
    ctrl.port_count = (mock_ahci_registers.host.cap & 0x1F) + 1;
    ctrl.active_ports = mock_ahci_registers.host.pi;

    // Initialize all 4 ports
    for (int i = 0; i < 4; i++) {
        // Set each port to have a device present
        mock_ahci_registers.ports[i].ssts = 0x3;       // Device present
        mock_ahci_registers.ports[i].sig = 0x00000101; // SATA drive

        memset(&ports[i], 0, sizeof(ports[i]));
        result = ahci_port_init(&ports[i], &ctrl, i);
        munit_assert_true(result);
        munit_assert_uint8(i, ==, ports[i].port_num);
        munit_assert_true(ports[i].initialized);
    }

    // Test that port 4 fails (out of range)
    AHCIPort invalid_port;
    memset(&invalid_port, 0, sizeof(invalid_port));
    result = ahci_port_init(&invalid_port, &ctrl, 4);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// EDGE CASE AND BOUNDARY TESTS
// ============================================================================

static MunitResult test_ahci_port_read_large_lba(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;
    uint8_t buffer[512];

    // Set up controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    result = ahci_port_identify(&port);
    munit_assert_true(result);

    // Test read with large LBA (48-bit addressing)
    const uint64_t large_lba = 0x123456789ABCULL;
    result = ahci_port_read(&port, large_lba, 1, buffer);
    munit_assert_true(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult test_ahci_port_cleanup(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Initialize controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Enable MSI to test cleanup
    port.msi_enabled = true;
    port.msi_vector = 0x40;

    // Test port cleanup
    ahci_port_cleanup(&port);

    // Verify cleanup
    munit_assert_false(port.initialized);
    munit_assert_false(port.connected);
    munit_assert_false(port.msi_enabled);
    munit_assert_uint8(0, ==, port.msi_vector);

    // Clean up controller
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// INTERRUPT VS POLLING TESTS
// ============================================================================

static MunitResult
test_ahci_port_msi_interrupt_configuration(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Initialize controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Verify MSI was configured during port init
    munit_assert_true(port.msi_enabled);
    munit_assert_uint8(0x40, ==, port.msi_vector);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult
test_ahci_port_init_without_msi_capability(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Initialize controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    // Disable MSI capability in controller
    ctrl.msi_cap_offset = 0;

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Verify MSI was NOT configured (should fall back to polling)
    munit_assert_false(port.msi_enabled);
    munit_assert_uint8(0, ==, port.msi_vector);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// COMMAND SETUP AND FIS CONSTRUCTION TESTS
// ============================================================================

static MunitResult
test_ahci_identify_command_setup(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Set up controller and port
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    // Initialize controller and port
    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Test identify command - this will test FIS construction internally
    result = ahci_port_identify(&port);
    munit_assert_true(result);

    // Verify device info was populated from mock identify data
    munit_assert_uint64(0x1000, ==, port.sector_count); // From mock setup
    munit_assert_uint16(512, ==, port.sector_size);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

static MunitResult
test_ahci_identify_unsupported_device_types(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;
    AHCIPort port;

    // Test port multiplier rejection
    memset(&ctrl, 0, sizeof(ctrl));
    memset(&port, 0, sizeof(port));

    mock_map_physical_set_fail(false);
    mock_map_virtual_set_fail(false);
    mock_alloc_physical_set_fail(false);

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result);

    result = ahci_port_init(&port, &ctrl, 0);
    munit_assert_true(result);

    // Set device signature to port multiplier
    extern AHCIRegs mock_ahci_registers;
    mock_ahci_registers.ports[0].sig = 0x96690101; // AHCI_SIG_PM

    // Identify should fail for unsupported device types
    result = ahci_port_identify(&port);
    munit_assert_false(result);

    // Test enclosure management bridge rejection
    mock_ahci_registers.ports[0].sig = 0xC33C0101; // AHCI_SIG_SEMB
    result = ahci_port_identify(&port);
    munit_assert_false(result);

    // Clean up
    ahci_controller_cleanup(&ctrl);

    return MUNIT_OK;
}

// ============================================================================
// REGISTER ACCESS PATTERNS TESTS
// ============================================================================

static MunitResult
test_ahci_controller_register_sanity_check(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    AHCIController ctrl;

    memset(&ctrl, 0, sizeof(ctrl));
    mock_map_physical_set_fail(false);

    // Set up mock with invalid CAP register values
    extern AHCIRegs mock_ahci_registers;
    mock_ahci_registers.host.cap = 0xFFFFFFFF; // Invalid value

    bool result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result); // Should still succeed but with warning

    // Reset to all zeros (also invalid)
    mock_ahci_registers.host.cap = 0x00000000;
    ahci_controller_cleanup(&ctrl);

    memset(&ctrl, 0, sizeof(ctrl));
    result = ahci_controller_init(&ctrl, 0xFEBF0000ULL, 0xC0000000ULL);
    munit_assert_true(result); // Should still succeed but with warning

    ahci_controller_cleanup(&ctrl);

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
        {"/io/read_uninitialized", test_ahci_port_read_uninitialized, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/io/write_uninitialized", test_ahci_port_write_uninitialized, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/io/read_success", test_ahci_port_read_success, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/io/write_success", test_ahci_port_write_success, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/io/read_dma_failure", test_ahci_port_read_dma_allocation_failure,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // Device state and error handling tests
        {"/error/no_device_present", test_ahci_port_init_no_device_present,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/error/identify_alloc_failure",
         test_ahci_port_identify_memory_allocation_failure, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Multiple port tests
        {"/ports/multiple_init", test_ahci_multiple_ports_init, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Edge case and boundary tests
        {"/edge/read_large_lba", test_ahci_port_read_large_lba, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/edge/port_cleanup", test_ahci_port_cleanup, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Interrupt vs polling tests
        {"/interrupt/msi_configuration",
         test_ahci_port_msi_interrupt_configuration, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/interrupt/no_msi_capability",
         test_ahci_port_init_without_msi_capability, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Command setup and FIS construction tests
        {"/command/identify_setup", test_ahci_identify_command_setup, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/command/unsupported_devices",
         test_ahci_identify_unsupported_device_types, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Register access pattern tests
        {"/register/sanity_check", test_ahci_controller_register_sanity_check,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/ahci/driver", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}