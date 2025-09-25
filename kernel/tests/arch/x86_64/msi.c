/*
 * Tests for MSI/MSI-X Interrupt Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "munit.h"
#include "process.h"
#include "smp/state.h"
#include "task.h"

#include "x86_64/kdrivers/msi.h"

static int eoe_call_count = 0;

void local_apic_eoe(void) { eoe_call_count++; }

static Task mock_task;
static Process mock_process;

Task *task_current(void) { return &mock_task; }

uint64_t sched_lock_this_cpu(void) { return 0; }
void sched_unlock_this_cpu(uint64_t flags) {}
void sched_block(Task *task) {}
void sched_schedule(void) {}
void sched_unblock(Task *task) {}

static uint64_t mock_time = 1000;
uint64_t get_kernel_upticks(void) { return mock_time; }

static void *setup(const MunitParameter params[], void *user_data) {
    mock_time = 1000;
    eoe_call_count = 0; // Reset EOI counter for each test

    for (int i = 0; i < __test_cpu_count; i++) {
        __test_cpu_state[i].cpu_id = i;
        __test_cpu_state[i].lapic_id = i + 1; // LAPIC IDs start at 1
    }

    mock_process.pid = 123;
    mock_task.owner = &mock_process;

    msi_init();

    return NULL;
}

static void teardown(void *ignored) {
    // nothing
}

static MunitResult test_msi_allocate_vector(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x01020A; // Bus 1, Device 2, Function 2
    const uint64_t pid = 123;

    const uint8_t vector = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);

    // Should allocate a valid vector
    munit_assert_uint8(vector, >=, MSI_VECTOR_BASE);
    munit_assert_uint8(vector, <=, MSI_VECTOR_TOP);

    // MSI address should have correct format
    munit_assert_uint64(msi_address & 0xFFF00000ULL, ==, 0xFEE00000ULL);

    // MSI data should contain the vector
    munit_assert_uint32(msi_data & 0xFF, ==, vector);

    return MUNIT_OK;
}

static MunitResult test_msi_allocate_vector_exhaustion(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x010000;
    const uint64_t pid = 123;

    // Allocate all available vectors
    for (int i = 0; i < MSI_VECTOR_COUNT; i++) {
        const uint8_t vector = msi_allocate_vector(bus_device_func + i, pid, &msi_address, &msi_data);
        munit_assert_uint8(vector, !=, 0); // Should succeed
    }

    // Next allocation should fail
    const uint8_t vector = msi_allocate_vector(bus_device_func + MSI_VECTOR_COUNT, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector, ==, 0); // Should fail

    return MUNIT_OK;
}

static MunitResult test_msi_cpu_load_balancing(const MunitParameter params[], void *fixture) {
    uint64_t msi_addresses[4];
    uint32_t msi_data[4];

    // Allocate vectors and check they target different CPUs
    for (int i = 0; i < 4; i++) {
        const uint64_t pid = 123;
        const uint32_t bus_device_func = 0x020000;
        const uint8_t vector = msi_allocate_vector(bus_device_func + i, pid, &msi_addresses[i], &msi_data[i]);
        munit_assert_uint8(vector, !=, 0);

        // Extract APIC ID from MSI address (bits 19-12)
        const uint8_t target_apic_id = (msi_addresses[i] >> 12) & 0xFF;
        const uint8_t expected_apic_id = (i % __test_cpu_count) + 1;

        munit_assert_uint8(target_apic_id, ==, expected_apic_id);
    }

    return MUNIT_OK;
}

static MunitResult test_msi_deallocate_vector(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x030000;
    const uint64_t pid = 123;

    // Allocate a vector
    const uint8_t vector = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector, !=, 0);

    // Deallocate it
    const bool result = msi_deallocate_vector(vector, pid);
    munit_assert_true(result);

    // Should be able to allocate a vector again (might not be the same vector due to hint system)
    const uint8_t vector2 = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector2, !=, 0); // Should get a valid vector

    return MUNIT_OK;
}

static MunitResult test_msi_verify_ownership(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x040000;
    const uint64_t pid1 = 123;
    const uint64_t pid2 = 456;

    // Allocate vector to PID1
    const uint8_t vector = msi_allocate_vector(bus_device_func, pid1, &msi_address, &msi_data);
    munit_assert_uint8(vector, !=, 0);

    // PID1 should own it
    munit_assert_true(msi_verify_ownership(vector, pid1));

    // PID2 should not own it
    munit_assert_false(msi_verify_ownership(vector, pid2));

    // Invalid vector should return false
    munit_assert_false(msi_verify_ownership(0x30, pid1)); // Below range
    munit_assert_false(msi_verify_ownership(0xE0, pid1)); // Above range

    return MUNIT_OK;
}

static MunitResult test_msi_register_handler(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x050000;
    const uint64_t pid = 123;

    // Allocate vector
    const uint8_t vector = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector, !=, 0);

    // Register handler should succeed for owner
    munit_assert_true(msi_register_handler(vector, &mock_task));

    // Create different task with different owner
    Process other_process = {.pid = 456};
    Task other_task = {.owner = &other_process};

    // Should fail for non-owner
    munit_assert_false(msi_register_handler(vector, &other_task));

    return MUNIT_OK;
}

static MunitResult test_msi_slow_consumer(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x060000;
    const uint64_t pid = 123;

    // Allocate vector
    const uint8_t vector = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector, !=, 0);

    // Initially should not be slow consumer
    munit_assert_false(msi_is_slow_consumer(vector));

    // Fill up the queue by simulating many interrupts
    for (int i = 0; i < MSI_QUEUE_SIZE + 5; i++) {
        msi_handle_interrupt(vector, 0xDEADBEEF);
    }

    // Advance time past the timeout threshold to trigger slow consumer detection
    mock_time += MSI_TIMEOUT_MS + 1;

    // Send one more interrupt to trigger the timeout check
    msi_handle_interrupt(vector, 0xDEADBEEF);

    // Should detect slow consumer after queue overflow and timeout
    munit_assert_true(msi_is_slow_consumer(vector));

    return MUNIT_OK;
}

// Test msi_handle_interrupt
static MunitResult test_msi_handle_interrupt(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint32_t bus_device_func = 0x080000;
    const uint64_t pid = 123;

    // Allocate vector
    const uint8_t vector = msi_allocate_vector(bus_device_func, pid, &msi_address, &msi_data);
    munit_assert_uint8(vector, !=, 0);

    // Reset EOI counter before testing
    eoe_call_count = 0;

    // Test handling valid interrupt
    msi_handle_interrupt(vector, 0xDEADBEEF);
    munit_assert_int(eoe_call_count, ==, 1); // Should call EOI once

    // Test handling invalid vector (too low)
    msi_handle_interrupt(MSI_VECTOR_BASE - 1, 0x12345678);
    munit_assert_int(eoe_call_count, ==,
                     2); // Should call EOI for invalid vector

    // Test handling invalid vector (too high)
    msi_handle_interrupt(MSI_VECTOR_TOP + 1, 0x87654321);
    munit_assert_int(eoe_call_count, ==,
                     3); // Should call EOI for invalid vector

    // Test handling interrupt for unallocated vector
    uint8_t unallocated_vector = MSI_VECTOR_BASE + 50; // Should be unallocated
    msi_handle_interrupt(unallocated_vector, 0xABCDEF00);
    munit_assert_int(eoe_call_count, ==,
                     4); // Should call EOI for unallocated vector

    // Test that valid interrupt was properly queued
    uint32_t event_data;
    const bool result = msi_wait_interrupt(vector, &mock_task, &event_data);
    munit_assert_true(result);
    munit_assert_uint32(event_data, ==, 0xDEADBEEF);

    // Test queue overflow EOI handling
    eoe_call_count = 0;

    // Fill up the queue completely
    for (int i = 0; i < MSI_QUEUE_SIZE; i++) {
        msi_handle_interrupt(vector, 0x1000 + i);
    }
    munit_assert_int(eoe_call_count, ==,
                     MSI_QUEUE_SIZE); // EOI for each queued interrupt

    // Now send one more to trigger overflow
    msi_handle_interrupt(vector, 0xBADC0FFE);
    munit_assert_int(eoe_call_count, ==,
                     MSI_QUEUE_SIZE + 1); // EOI must still be called for overflow

    return MUNIT_OK;
}

static MunitResult test_msi_cleanup_process(const MunitParameter params[], void *fixture) {
    uint64_t msi_address;
    uint32_t msi_data;
    const uint64_t pid = 123;

    // Allocate several vectors to the process
    uint8_t vectors[3];
    for (int i = 0; i < 3; i++) {
        const uint32_t bus_device_func = 0x070000;
        vectors[i] = msi_allocate_vector(bus_device_func + i, pid, &msi_address, &msi_data);
        munit_assert_uint8(vectors[i], !=, 0);
    }

    // All should be owned by the process
    for (int i = 0; i < 3; i++) {
        munit_assert_true(msi_verify_ownership(vectors[i], pid));
    }

    // Cleanup the process
    msi_cleanup_process(pid);

    // None should be owned by the process anymore
    for (int i = 0; i < 3; i++) {
        munit_assert_false(msi_verify_ownership(vectors[i], pid));
    }

    return MUNIT_OK;
}

static MunitTest msi_tests[] = {
        {"/allocate_vector", test_msi_allocate_vector, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/allocate_vector_exhaustion", test_msi_allocate_vector_exhaustion, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/cpu_load_balancing", test_msi_cpu_load_balancing, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/deallocate_vector", test_msi_deallocate_vector, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/verify_ownership", test_msi_verify_ownership, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/register_handler", test_msi_register_handler, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/slow_consumer", test_msi_slow_consumer, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/handle_interrupt", test_msi_handle_interrupt, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/cleanup_process", test_msi_cleanup_process, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/msi", msi_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(const int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, "µnit", argc, argv);
}