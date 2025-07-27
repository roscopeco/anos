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
#include "spinlock.h"

#include "mock_slab.h"
#include "slab/alloc.h"

static ManagedResource *freed_resources_head = NULL;

void process_release_owned_pages(Process *proc) { /* nothing */ }
void task_destroy(Process *proc) { /* nothing */ }

// Minimal fake free func
void dummy_free_func(ManagedResource *resource) { (void)resource; }

// Helper to zero-init a ManagedResource
void init_managed_resource(ManagedResource *mr) {
    memset(mr, 0, sizeof(*mr));
    mr->free_func = dummy_free_func;
}

// Mock spinlock_init
void spinlock_init(SpinLock *lock) { lock->lock = 0; }

void managed_resources_free_all(ManagedResource *head) {
    freed_resources_head = head;
}

void msi_cleanup_process(uint64_t pid) {
    // nothing
}

extern _Atomic volatile uint64_t next_pid;

// Tests
static MunitResult test_process_init_and_create(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    process_init();
    munit_assert_int(next_pid, ==, 1);

    const Process *p = process_create(0x12345000);
    munit_assert_ptr_not_null(p);
    munit_assert_int(p->pid, ==, 1);
    munit_assert_uint64(p->pml4, ==, 0x12345000);
    munit_assert_ptr_null(p->meminfo->res_head);
    munit_assert_ptr_null(p->meminfo->res_tail);

    // 1 for Process, one for ProcessMemoryInfo, one for lock
    munit_assert_int(mock_slab_get_alloc_count(), ==, 3);

    slab_free(p->meminfo->pages_lock);
    slab_free(p->meminfo);
    slab_free((void *)p);

    return MUNIT_OK;
}

static MunitResult test_process_create_failures(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    Process *p;

#ifdef CONSERVATIVE_BUILD
    // Should return NULL if pml4 == 0
    p = process_create(0);
    munit_assert_ptr_null(p);
#endif

    // Simulate allocation failure
    mock_slab_set_should_fail(true);
    p = process_create(0x1000);
    munit_assert_ptr_null(p);

    return MUNIT_OK;
}

static MunitResult test_process_destroy(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    Process *p = process_create(0x12345000);

    const void *resources = p->meminfo->res_head;

    process_destroy(p);

    // Check that resources were freed
    munit_assert_ptr_equal(freed_resources_head, resources);
    munit_assert_int(mock_slab_get_free_count(), ==,
                     3); // 1 for process, 1 for meminfo 1 for task

    return MUNIT_OK;
}

static MunitResult test_add_single_resource(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ProcessMemoryInfo meminfo = {0};
    proc.meminfo = &meminfo;
    ManagedResource res;

    init_managed_resource(&res);

    munit_assert_true(process_add_managed_resource(&proc, &res));
    munit_assert_ptr_equal(proc.meminfo->res_head, &res);
    munit_assert_ptr_equal(proc.meminfo->res_tail, &res);
    munit_assert_ptr_null(res.this.next);

    return MUNIT_OK;
}

static MunitResult test_add_multiple_resources(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ProcessMemoryInfo meminfo = {0};
    proc.meminfo = &meminfo;
    ManagedResource res1, res2, res3;

    init_managed_resource(&res1);
    init_managed_resource(&res2);
    init_managed_resource(&res3);

    munit_assert_true(process_add_managed_resource(&proc, &res1));
    munit_assert_true(process_add_managed_resource(&proc, &res2));
    munit_assert_true(process_add_managed_resource(&proc, &res3));

    munit_assert_ptr_equal(proc.meminfo->res_head, &res1);
    munit_assert_ptr_equal(proc.meminfo->res_tail, &res3);
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
    ProcessMemoryInfo meminfo = {0};
    proc.meminfo = &meminfo;
    ManagedResource res;

    init_managed_resource(&res);

    munit_assert_true(process_add_managed_resource(&proc, &res));
    munit_assert_true(process_remove_managed_resource(&proc, &res));

    munit_assert_ptr_null(proc.meminfo->res_head);
    munit_assert_ptr_null(proc.meminfo->res_tail);

    return MUNIT_OK;
}

static MunitResult test_remove_head_middle_tail(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ProcessMemoryInfo meminfo = {0};
    proc.meminfo = &meminfo;
    ManagedResource res1, res2, res3;

    init_managed_resource(&res1);
    init_managed_resource(&res2);
    init_managed_resource(&res3);

    munit_assert_true(process_add_managed_resource(&proc, &res1));
    munit_assert_true(process_add_managed_resource(&proc, &res2));
    munit_assert_true(process_add_managed_resource(&proc, &res3));

    // Remove head
    munit_assert_true(process_remove_managed_resource(&proc, &res1));
    munit_assert_ptr_equal(proc.meminfo->res_head, &res2);
    munit_assert_ptr_equal(proc.meminfo->res_tail, &res3);

    // Remove middle (res2 now head)
    munit_assert_true(process_remove_managed_resource(&proc, &res2));
    munit_assert_ptr_equal(proc.meminfo->res_head, &res3);
    munit_assert_ptr_equal(proc.meminfo->res_tail, &res3);

    // Remove tail (also head now)
    munit_assert_true(process_remove_managed_resource(&proc, &res3));
    munit_assert_ptr_null(proc.meminfo->res_head);
    munit_assert_ptr_null(proc.meminfo->res_tail);

    return MUNIT_OK;
}

static MunitResult
test_remove_nonexistent_resource(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;
    Process proc = {0};
    ProcessMemoryInfo meminfo = {0};
    proc.meminfo = &meminfo;
    ManagedResource res1, res2;

    init_managed_resource(&res1);
    init_managed_resource(&res2);

    munit_assert_true(process_add_managed_resource(&proc, &res1));

    // Try removing res2, which was never added
    munit_assert_false(process_remove_managed_resource(&proc, &res2));

    // Ensure res1 still intact
    munit_assert_ptr_equal(proc.meminfo->res_head, &res1);
    munit_assert_ptr_equal(proc.meminfo->res_tail, &res1);
    munit_assert_ptr_null(res1.this.next);

    return MUNIT_OK;
}

void *setup(const MunitParameter params[], void *user_data) {
    mock_slab_reset();
    return NULL;
}

static MunitTest tests[] = {
        {"/init_create", test_process_init_and_create, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/create_failures", test_process_create_failures, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/destroy", test_process_destroy, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        {"/add_single", test_add_single_resource, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/add_multiple", test_add_multiple_resources, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_only", test_remove_only_element, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_head_middle_tail", test_remove_head_middle_tail, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_nonexistent", test_remove_nonexistent_resource, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/process", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
