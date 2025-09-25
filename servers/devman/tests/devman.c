/*
 * Tests for DEVMAN (Device Manager)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_types.h"
#include "devman_internal.h"
#include "munit.h"

// Mock syscalls for testing
#include <anos/syscalls.h>

// Mock syscall implementations
SyscallResult anos_create_channel(void) {
    static uint64_t next_channel_id = 100;
    SyscallResult result = {.result = SYSCALL_OK, .value = next_channel_id++};
    return result;
}

SyscallResult anos_register_named_channel(const char *name, uint64_t channel_id) {
    (void)name;
    (void)channel_id;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

SyscallResult anos_find_named_channel(const char *name) {
    (void)name;
    SyscallResult result = {.result = SYSCALL_ERR_NOT_FOUND};
    return result;
}

SyscallResult anos_send_message(uint64_t channel_id, void *buffer, size_t msg_size, void *msg_buffer) {
    (void)channel_id;
    (void)buffer;
    (void)msg_size;
    (void)msg_buffer;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

SyscallResult anos_recv_message(uint64_t channel_id, void *buffer, size_t buffer_size, size_t *actual_size) {
    (void)channel_id;
    (void)buffer;
    (void)buffer_size;
    (void)actual_size;
    SyscallResult result = {.result = SYSCALL_FAILURE};
    return result;
}

SyscallResult anos_map_physical(uint64_t physical_addr, void *virtual_addr, size_t size, uint32_t flags) {
    (void)physical_addr;
    (void)virtual_addr;
    (void)size;
    (void)flags;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

SyscallResultA anos_alloc_physical_pages(size_t size) {
    (void)size;
    static uintptr_t next_addr = 0x90000000;
    SyscallResultA result = {.result = SYSCALL_OK, .value = next_addr};
    next_addr += size;
    return result;
}

SyscallResult anos_unmap_virtual(uint64_t virtual_addr, size_t size) {
    (void)virtual_addr;
    (void)size;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

SyscallResult anos_task_sleep_current(uint32_t ms) {
    (void)ms;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

SyscallResult anos_kprint(const char *message) {
    (void)message;
    SyscallResult result = {.result = SYSCALL_OK};
    return result;
}

// Test setup and teardown
static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    // Reset device registry state
    memset(device_registry, 0, sizeof(device_registry));
    device_count = 0;
    next_device_id = 1;

    return NULL;
}

// ============================================================================
// DEVICE REGISTRATION TESTS
// ============================================================================

static MunitResult test_register_device_basic(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    DeviceInfo device = {.device_id = 0, // Should be assigned by register_device
                         .parent_id = 0,
                         .device_type = DEVICE_TYPE_STORAGE,
                         .hardware_type = STORAGE_HW_AHCI,
                         .capabilities = DEVICE_CAP_READ | DEVICE_CAP_WRITE,
                         .name = "Test Storage Device",
                         .driver_name = "ahcidrv",
                         .driver_channel = 123};

    uint64_t assigned_id = register_device(&device);

    munit_assert_uint64(assigned_id, ==, 1); // First device should get ID 1
    munit_assert_uint32(device_count, ==, 1);
    munit_assert_uint64(next_device_id, ==, 2);

    // Check device was stored correctly
    munit_assert_uint64(device_registry[0].device_id, ==, 1);
    munit_assert_uint32(device_registry[0].device_type, ==, DEVICE_TYPE_STORAGE);
    munit_assert_string_equal(device_registry[0].name, "Test Storage Device");

    return MUNIT_OK;
}

static MunitResult test_register_device_multiple(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    DeviceInfo device1 = {.device_id = 0,
                          .parent_id = 0,
                          .device_type = DEVICE_TYPE_PCI,
                          .hardware_type = 0,
                          .capabilities = 0,
                          .name = "PCI Bridge",
                          .driver_name = "pcidrv",
                          .driver_channel = 100};

    DeviceInfo device2 = {.device_id = 0,
                          .parent_id = 1, // Child of device1
                          .device_type = DEVICE_TYPE_STORAGE,
                          .hardware_type = STORAGE_HW_NVME,
                          .capabilities = DEVICE_CAP_READ | DEVICE_CAP_WRITE | DEVICE_CAP_TRIM,
                          .name = "NVMe SSD",
                          .driver_name = "nvmedrv",
                          .driver_channel = 101};

    uint64_t id1 = register_device(&device1);
    uint64_t id2 = register_device(&device2);

    munit_assert_uint64(id1, ==, 1);
    munit_assert_uint64(id2, ==, 2);
    munit_assert_uint32(device_count, ==, 2);

    // Verify parent-child relationship
    munit_assert_uint64(device_registry[1].parent_id, ==, 1);

    return MUNIT_OK;
}

static MunitResult test_register_device_max_capacity(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Fill up the device registry to maximum capacity
    for (int i = 0; i < MAX_DEVICES; i++) {
        DeviceInfo device = {.device_id = 0,
                             .parent_id = 0,
                             .device_type = DEVICE_TYPE_UNKNOWN,
                             .hardware_type = 0,
                             .capabilities = 0,
                             .name = "Test Device",
                             .driver_name = "testdrv",
                             .driver_channel = 0};

        uint64_t assigned_id = register_device(&device);
        munit_assert_uint64(assigned_id, ==, i + 1);
    }

    munit_assert_uint32(device_count, ==, MAX_DEVICES);

    // Try to register one more - should fail (return 0)
    DeviceInfo overflow_device = {.device_id = 0,
                                  .parent_id = 0,
                                  .device_type = DEVICE_TYPE_UNKNOWN,
                                  .hardware_type = 0,
                                  .capabilities = 0,
                                  .name = "Overflow Device",
                                  .driver_name = "testdrv",
                                  .driver_channel = 0};

    uint64_t overflow_id = register_device(&overflow_device);
    munit_assert_uint64(overflow_id, ==, 0);            // Should fail
    munit_assert_uint32(device_count, ==, MAX_DEVICES); // Count unchanged

    return MUNIT_OK;
}

static MunitResult test_register_device_null_info(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    uint64_t result = register_device(NULL);

    munit_assert_uint64(result, ==, 0);       // Should fail
    munit_assert_uint32(device_count, ==, 0); // No devices registered

    return MUNIT_OK;
}

// ============================================================================
// DEVICE UNREGISTRATION TESTS
// ============================================================================

static MunitResult test_unregister_device_basic(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // First register a device
    DeviceInfo device = {.device_id = 0,
                         .parent_id = 0,
                         .device_type = DEVICE_TYPE_NETWORK,
                         .hardware_type = 0,
                         .capabilities = 0,
                         .name = "Network Card",
                         .driver_name = "netdrv",
                         .driver_channel = 200};

    uint64_t device_id = register_device(&device);
    munit_assert_uint64(device_id, ==, 1);
    munit_assert_uint32(device_count, ==, 1);

    // Now unregister it
    bool success = unregister_device(device_id);

    munit_assert_true(success);
    munit_assert_uint32(device_count, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_unregister_device_nonexistent(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Try to unregister a device that doesn't exist
    bool success = unregister_device(999);

    munit_assert_false(success);
    munit_assert_uint32(device_count, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_unregister_device_with_gaps(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Register three devices
    DeviceInfo device1 = {.device_type = DEVICE_TYPE_PCI, .name = "Device1"};
    DeviceInfo device2 = {.device_type = DEVICE_TYPE_STORAGE, .name = "Device2"};
    DeviceInfo device3 = {.device_type = DEVICE_TYPE_DISPLAY, .name = "Device3"};

    uint64_t id1 = register_device(&device1);
    uint64_t id2 = register_device(&device2);
    uint64_t id3 = register_device(&device3);

    munit_assert_uint32(device_count, ==, 3);

    // Unregister the middle device
    bool success = unregister_device(id2);
    munit_assert_true(success);
    munit_assert_uint32(device_count, ==, 2);

    // Verify device3 moved to position 1 (compaction)
    munit_assert_uint64(device_registry[1].device_id, ==, id3);
    munit_assert_string_equal(device_registry[1].name, "Device3");

    return MUNIT_OK;
}

// ============================================================================
// DEVICE QUERY TESTS
// ============================================================================

static MunitResult test_query_devices_all(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Register several devices
    DeviceInfo devices[] = {{.device_type = DEVICE_TYPE_PCI, .name = "PCI Device"},
                            {.device_type = DEVICE_TYPE_STORAGE, .name = "Storage Device"},
                            {.device_type = DEVICE_TYPE_NETWORK, .name = "Network Device"}};

    for (int i = 0; i < 3; i++) {
        register_device(&devices[i]);
    }

    // Query all devices
    DeviceInfo results[10];
    uint32_t count = query_devices(QUERY_ALL, DEVICE_TYPE_UNKNOWN, 0, results, 10);

    munit_assert_uint32(count, ==, 3);
    munit_assert_string_equal(results[0].name, "PCI Device");
    munit_assert_string_equal(results[1].name, "Storage Device");
    munit_assert_string_equal(results[2].name, "Network Device");

    return MUNIT_OK;
}

static MunitResult test_query_devices_by_type(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Register devices of different types
    DeviceInfo storage1 = {.device_type = DEVICE_TYPE_STORAGE, .name = "SSD"};
    DeviceInfo storage2 = {.device_type = DEVICE_TYPE_STORAGE, .name = "HDD"};
    DeviceInfo network = {.device_type = DEVICE_TYPE_NETWORK, .name = "Ethernet"};

    register_device(&storage1);
    register_device(&storage2);
    register_device(&network);

    // Query only storage devices
    DeviceInfo results[10];
    uint32_t count = query_devices(QUERY_BY_TYPE, DEVICE_TYPE_STORAGE, 0, results, 10);

    munit_assert_uint32(count, ==, 2);
    munit_assert_uint32(results[0].device_type, ==, DEVICE_TYPE_STORAGE);
    munit_assert_uint32(results[1].device_type, ==, DEVICE_TYPE_STORAGE);

    return MUNIT_OK;
}

static MunitResult test_query_devices_by_id(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Register a device
    DeviceInfo device = {.device_type = DEVICE_TYPE_DISPLAY, .name = "Graphics Card"};
    uint64_t device_id = register_device(&device);

    // Query by specific ID
    DeviceInfo results[10];
    uint32_t count = query_devices(QUERY_BY_ID, DEVICE_TYPE_UNKNOWN, device_id, results, 10);

    munit_assert_uint32(count, ==, 1);
    munit_assert_uint64(results[0].device_id, ==, device_id);
    munit_assert_string_equal(results[0].name, "Graphics Card");

    return MUNIT_OK;
}

static MunitResult test_query_devices_children(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Create parent-child hierarchy
    DeviceInfo parent = {.device_type = DEVICE_TYPE_PCI, .parent_id = 0, .name = "PCI Bridge"};
    DeviceInfo child1 = {.device_type = DEVICE_TYPE_STORAGE, .name = "Storage Child"};
    DeviceInfo child2 = {.device_type = DEVICE_TYPE_NETWORK, .name = "Network Child"};

    uint64_t parent_id = register_device(&parent);

    child1.parent_id = parent_id;
    child2.parent_id = parent_id;

    register_device(&child1);
    register_device(&child2);

    // Query children of parent
    DeviceInfo results[10];
    uint32_t count = query_devices(QUERY_CHILDREN, DEVICE_TYPE_UNKNOWN, parent_id, results, 10);

    munit_assert_uint32(count, ==, 2);
    munit_assert_uint64(results[0].parent_id, ==, parent_id);
    munit_assert_uint64(results[1].parent_id, ==, parent_id);

    return MUNIT_OK;
}

static MunitResult test_query_devices_buffer_limit(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Register 5 devices
    for (int i = 0; i < 5; i++) {
        DeviceInfo device = {.device_type = DEVICE_TYPE_UNKNOWN, .name = "Test Device"};
        register_device(&device);
    }

    // Query with buffer that can only hold 3 devices
    DeviceInfo results[3];
    uint32_t count = query_devices(QUERY_ALL, DEVICE_TYPE_UNKNOWN, 0, results, 3);

    munit_assert_uint32(count, ==, 3); // Should be limited by buffer size

    return MUNIT_OK;
}

// Test array
static MunitTest test_suite_tests[] = {
        // Device registration tests
        {(char *)"/devman/register_device_basic", test_register_device_basic, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/devman/register_device_multiple", test_register_device_multiple, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/devman/register_device_max_capacity", test_register_device_max_capacity, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/devman/register_device_null_info", test_register_device_null_info, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Device unregistration tests
        {(char *)"/devman/unregister_device_basic", test_unregister_device_basic, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/devman/unregister_device_nonexistent", test_unregister_device_nonexistent, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/devman/unregister_device_with_gaps", test_unregister_device_with_gaps, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Device query tests
        {(char *)"/devman/query_devices_all", test_query_devices_all, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/devman/query_devices_by_type", test_query_devices_by_type, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/devman/query_devices_by_id", test_query_devices_by_id, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/devman/query_devices_children", test_query_devices_children, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/devman/query_devices_buffer_limit", test_query_devices_buffer_limit, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"devman", argc, argv);
}