/*
 * Tests for process handling
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "process.h"

// Mock slab allocator for testing
static Process fake_process_storage;
static bool simulate_alloc_fail = false;
static int slab_alloc_calls = 0;
static int slab_free_calls = 0;
static ManagedResource *freed_resources_head = NULL;

void process_release_owned_pages(Process *proc) { /* nothing */ }
void task_destroy(Process *proc) { /* nothing */ }

Process *slab_alloc_block(void) {
    slab_alloc_calls++;
    if (simulate_alloc_fail) {
        return NULL;
    }
    memset(&fake_process_storage, 0, sizeof(fake_process_storage));
    return &fake_process_storage;
}

void slab_free(Process *process) {
    (void)process;
    slab_free_calls++;
}

// Minimal fake free func
void dummy_free_func(ManagedResource *resource) { (void)resource; }

// Helper to zero-init a ManagedResource
void init_managed_resource(ManagedResource *mr) {
    memset(mr, 0, sizeof(*mr));
    mr->free_func = dummy_free_func;
}

void managed_resources_free_all(ManagedResource *head) {
    freed_resources_head = head;
}

extern _Atomic volatile uint64_t next_pid;

// Tests
static MunitResult test_process_init_and_create(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    extern void process_init(
            void); // you have to extern if it's static otherwise
    extern Process *process_create(uintptr_t pml4);

    slab_alloc_calls = 0;
    simulate_alloc_fail = false;

    process_init();
    munit_assert_int(next_pid, ==, 1);

    Process *p = process_create(0x12345000);
    munit_assert_ptr_not_null(p);
    munit_assert_int(p->pid, ==, 1);
    munit_assert_ptr_equal(p, &fake_process_storage);
    munit_assert_uint64(p->pml4, ==, 0x12345000);
    munit_assert_ptr_null(p->res_head);
    munit_assert_ptr_null(p->res_tail);
    munit_assert_int(slab_alloc_calls, ==, 2); // 1 for Process, one for lock

    return MUNIT_OK;
}

static MunitResult test_process_create_failures(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    extern Process *process_create(uintptr_t pml4);

    slab_alloc_calls = 0;
    Process *p;

#ifdef CONSERVATIVE_BUILD
    // Should return NULL if pml4 == 0
    p = process_create(0);
    munit_assert_ptr_null(p);
#endif

    // Simulate allocation failure
    simulate_alloc_fail = true;
    p = process_create(0x1000);
    munit_assert_ptr_null(p);
    simulate_alloc_fail = false;

    return MUNIT_OK;
}

static MunitResult test_process_destroy(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    extern void process_destroy(Process * process);

    Process dummy_proc = {0};
    ManagedResource dummy_res = {0};
    dummy_proc.res_head = &dummy_res;

    freed_resources_head = NULL;
    slab_free_calls = 0;

    process_destroy(&dummy_proc);

    // Check that resources were freed
    munit_assert_ptr_equal(freed_resources_head, &dummy_res);
    munit_assert_int(slab_free_calls, ==, 2); // 1 for process, 1 for task

    return MUNIT_OK;
}

static MunitResult test_add_single_resource(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ManagedResource res;

    init_managed_resource(&res);

    munit_assert_true(process_add_managed_resource(&proc, &res));
    munit_assert_ptr_equal(proc.res_head, &res);
    munit_assert_ptr_equal(proc.res_tail, &res);
    munit_assert_ptr_null(res.this.next);

    return MUNIT_OK;
}

static MunitResult test_add_multiple_resources(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ManagedResource res1, res2, res3;

    init_managed_resource(&res1);
    init_managed_resource(&res2);
    init_managed_resource(&res3);

    munit_assert_true(process_add_managed_resource(&proc, &res1));
    munit_assert_true(process_add_managed_resource(&proc, &res2));
    munit_assert_true(process_add_managed_resource(&proc, &res3));

    munit_assert_ptr_equal(proc.res_head, &res1);
    munit_assert_ptr_equal(proc.res_tail, &res3);
    munit_assert_ptr_equal(res1.this.next, &res2.this);
    munit_assert_ptr_equal(res2.this.next, &res3.this);
    munit_assert_ptr_null(res3.this.next);

    return MUNIT_OK;
}

static MunitResult test_remove_only_element(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ManagedResource res;

    init_managed_resource(&res);

    munit_assert_true(process_add_managed_resource(&proc, &res));
    munit_assert_true(process_remove_managed_resource(&proc, &res));

    munit_assert_ptr_null(proc.res_head);
    munit_assert_ptr_null(proc.res_tail);

    return MUNIT_OK;
}

static MunitResult test_remove_head_middle_tail(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ManagedResource res1, res2, res3;

    init_managed_resource(&res1);
    init_managed_resource(&res2);
    init_managed_resource(&res3);

    munit_assert_true(process_add_managed_resource(&proc, &res1));
    munit_assert_true(process_add_managed_resource(&proc, &res2));
    munit_assert_true(process_add_managed_resource(&proc, &res3));

    // Remove head
    munit_assert_true(process_remove_managed_resource(&proc, &res1));
    munit_assert_ptr_equal(proc.res_head, &res2);
    munit_assert_ptr_equal(proc.res_tail, &res3);

    // Remove middle (res2 now head)
    munit_assert_true(process_remove_managed_resource(&proc, &res2));
    munit_assert_ptr_equal(proc.res_head, &res3);
    munit_assert_ptr_equal(proc.res_tail, &res3);

    // Remove tail (also head now)
    munit_assert_true(process_remove_managed_resource(&proc, &res3));
    munit_assert_ptr_null(proc.res_head);
    munit_assert_ptr_null(proc.res_tail);

    return MUNIT_OK;
}

static MunitResult
test_remove_nonexistent_resource(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ManagedResource res1, res2;

    init_managed_resource(&res1);
    init_managed_resource(&res2);

    munit_assert_true(process_add_managed_resource(&proc, &res1));

    // Try removing res2, which was never added
    munit_assert_false(process_remove_managed_resource(&proc, &res2));

    // Ensure res1 still intact
    munit_assert_ptr_equal(proc.res_head, &res1);
    munit_assert_ptr_equal(proc.res_tail, &res1);
    munit_assert_ptr_null(res1.this.next);

    return MUNIT_OK;
}

// Test suite setup

static MunitTest tests[] = {
        {"/init_create", test_process_init_and_create, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/create_failures", test_process_create_failures, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/destroy", test_process_destroy, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        {"/add_single", test_add_single_resource, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/add_multiple", test_add_multiple_resources, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_only", test_remove_only_element, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_head_middle_tail", test_remove_head_middle_tail, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_nonexistent", test_remove_nonexistent_resource, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/process", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
