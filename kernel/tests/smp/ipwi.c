/*
* stage3 - Inter-processor work-item (IPWI) tests
* anos - An Operating System
*
* Copyright (c) 2025 Ross Bamford
*/

#include "munit.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "smp/ipwi.h"
#include "smp/state.h"
#include "spinlock.h"
#include "structs/shift_array.h"

// === Mocks ===

Task mock_task;
Process mock_owner;

static PerCPUState mock_states[4];
static int last_halt_called = 0;
static int shift_array_init_should_fail = 0;
static int enqueue_fail_cpu = -1;
static int dequeue_has_item = 0;
static IpwiWorkItem mocked_item;
static int invalidate_page_called = 0;
static uintptr_t invalidate_page_addrs[16];

Task *task_current(void) { return &mock_task; }

void arch_ipwi_notify_all_except_current(void) {
    // Called from notify test
}

void cpu_invalidate_tlb_addr(const uintptr_t addr) {
    if (invalidate_page_called < 16) {
        invalidate_page_addrs[invalidate_page_called++] = addr;
    }
}

void halt_and_catch_fire(void) { last_halt_called++; }

bool shift_array_init(ShiftToMiddleArray *arr, size_t elem_size,
                      int initial_capacity) {
    return shift_array_init_should_fail ? false : true;
}

bool shift_array_insert_tail(ShiftToMiddleArray *arr, const void *item) {
    memcpy(&mocked_item, item, sizeof(IpwiWorkItem));
    return true;
}

void *shift_array_get_head(const ShiftToMiddleArray *arr) {
    return dequeue_has_item ? &mocked_item : NULL;
}

void shift_array_remove_head(ShiftToMiddleArray *arr) { dequeue_has_item = 0; }

void spinlock_init(SpinLock *lock) {}
void spinlock_lock(SpinLock *lock) {}
void spinlock_unlock(SpinLock *lock) {}
uint64_t spinlock_lock_irqsave(SpinLock *lock) { return 0x42; }
void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags) {}

void ipwi_ipi_handler(void);

// === Tests ===

static MunitResult test_ipwi_init_success(const MunitParameter params[],
                                          void *data) {
    shift_array_init_should_fail = 0;
    munit_assert_true(ipwi_init());
    return MUNIT_OK;
}

static MunitResult
test_ipwi_init_fail_on_shift_array(const MunitParameter params[], void *data) {
    shift_array_init_should_fail = 1;
    munit_assert_false(ipwi_init());
    return MUNIT_OK;
}

static MunitResult test_ipwi_enqueue_success(const MunitParameter params[],
                                             void *data) {
    __test_cpu_count = 2;

    IpwiWorkItem item = {.type = IPWI_TYPE_REMOTE_EXEC};
    munit_assert_true(ipwi_enqueue(&item, 2));
    munit_assert_int(mocked_item.type, ==, IPWI_TYPE_REMOTE_EXEC);

    __test_cpu_count = 1;
    return MUNIT_OK;
}

static MunitResult
test_ipwi_enqueue_fail_invalid_cpu(const MunitParameter params[], void *data) {
    IpwiWorkItem item = {.type = IPWI_TYPE_REMOTE_EXEC};
    munit_assert_false(ipwi_enqueue(&item, 99));
    return MUNIT_OK;
}

static MunitResult
test_ipwi_enqueue_all_except_current(const MunitParameter params[],
                                     void *data) {
    IpwiWorkItem item = {.type = IPWI_TYPE_TLB_SHOOTDOWN};
    enqueue_fail_cpu = -1;
    munit_assert_true(ipwi_enqueue_all_except_current(&item));
    return MUNIT_OK;
}

static MunitResult test_ipwi_dequeue_success(const MunitParameter params[],
                                             void *data) {
    dequeue_has_item = 1;
    mocked_item.type = IPWI_TYPE_REMOTE_EXEC;
    IpwiWorkItem out;
    munit_assert_true(ipwi_dequeue_this_cpu(&out));
    munit_assert_int(out.type, ==, IPWI_TYPE_REMOTE_EXEC);
    return MUNIT_OK;
}

static MunitResult test_ipwi_dequeue_empty(const MunitParameter params[],
                                           void *data) {
    dequeue_has_item = 0;
    IpwiWorkItem out;
    munit_assert_false(ipwi_dequeue_this_cpu(&out));
    return MUNIT_OK;
}

static MunitResult test_ipwi_notify_calls_arch(const MunitParameter params[],
                                               void *data) {
    // Just a smoke test for now
    ipwi_notify_all_except_current();
    return MUNIT_OK;
}

static MunitResult test_ipwi_ipi_handler_panic(const MunitParameter params[],
                                               void *data) {
    dequeue_has_item = 1;
    mocked_item.type = IPWI_TYPE_PANIC_HALT;
    last_halt_called = 0;
    ipwi_ipi_handler();
    munit_assert_int(last_halt_called, ==, 1);
    return MUNIT_OK;
}

static MunitResult
test_ipwi_ipi_handler_tlb_shootdown(const MunitParameter params[], void *data) {
    IpwiPayloadTLBShootdown *payload =
            (IpwiPayloadTLBShootdown *)&mocked_item.payload;
    mocked_item.type = IPWI_TYPE_TLB_SHOOTDOWN;
    payload->start_vaddr = 0x4000;
    payload->page_count = 3;
    payload->target_pid = 42;

    mock_owner.pid = 42;
    mock_task.owner = &mock_owner;

    invalidate_page_called = 0;
    dequeue_has_item = 1;

    ipwi_ipi_handler();

    munit_assert_int(invalidate_page_called, ==, 3);
    munit_assert_uint64(invalidate_page_addrs[0], ==, 0x4000);
    munit_assert_uint64(invalidate_page_addrs[1], ==, 0x5000);
    munit_assert_uint64(invalidate_page_addrs[2], ==, 0x6000);
    return MUNIT_OK;
}

static MunitTest ipwi_tests[] = {
        {"/init_success", test_ipwi_init_success, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/init_fail", test_ipwi_init_fail_on_shift_array, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_success", test_ipwi_enqueue_success, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_invalid", test_ipwi_enqueue_fail_invalid_cpu, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_all", test_ipwi_enqueue_all_except_current, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_success", test_ipwi_dequeue_success, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_empty", test_ipwi_dequeue_empty, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/notify", test_ipwi_notify_calls_arch, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ipi_handler_panic", test_ipwi_ipi_handler_panic, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ipi_handler_tlb_shootdown", test_ipwi_ipi_handler_tlb_shootdown,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite ipwi_test_suite = {"/ipwi", ipwi_tests, NULL, 1,
                                           MUNIT_SUITE_OPTION_NONE};

int main(const int argc, char *argv[]) {
    return munit_suite_main(&ipwi_test_suite, NULL, argc, argv);
}
