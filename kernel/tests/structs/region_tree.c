/*
 * Region Tree Tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Unit tests for AVL-based region tree.
 */

#include <stdint.h>

#include "munit.h"

#include "structs/region_tree.h"

static Region *make_region(uintptr_t start, uintptr_t end) {
    static Region pool[128];
    static size_t used = 0;
    munit_assert(used < 128);
    Region *r = &pool[used++];
    *r = (Region){.start = start,
                  .end = end,
                  .metadata = NULL,
                  .left = NULL,
                  .right = NULL,
                  .height = 1};
    return r;
}

static MunitResult test_insert_and_lookup(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;
    Region *root = NULL;
    root = region_tree_insert(root, make_region(0x1000, 0x2000));
    root = region_tree_insert(root, make_region(0x2000, 0x3000));
    root = region_tree_insert(root, make_region(0x3000, 0x4000));

    munit_assert_ptr_not_null(region_tree_lookup(root, 0x1000));
    munit_assert_ptr_not_null(region_tree_lookup(root, 0x2FFF));
    munit_assert_ptr_null(region_tree_lookup(root, 0x4000));
    munit_assert_ptr_null(region_tree_lookup(root, 0x0FFF));
    return MUNIT_OK;
}

static MunitResult test_resize(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;
    Region *root = NULL;
    Region *r = make_region(0x1000, 0x2000);
    root = region_tree_insert(root, r);

    munit_assert_ptr_not_null(region_tree_lookup(root, 0x1FFF));
    munit_assert_ptr_null(region_tree_lookup(root, 0x2000));

    munit_assert_true(region_tree_resize(r, 0x3000));
    munit_assert_ptr_not_null(region_tree_lookup(root, 0x2FFF));
    munit_assert_false(
            region_tree_resize(r, 0x8000000000001000ULL)); // exceeds limit
    return MUNIT_OK;
}

static MunitResult test_remove(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;
    Region *root = NULL;
    root = region_tree_insert(root, make_region(0x1000, 0x2000));
    root = region_tree_insert(root, make_region(0x2000, 0x3000));
    root = region_tree_insert(root, make_region(0x3000, 0x4000));

    munit_assert_ptr_not_null(region_tree_lookup(root, 0x2000));
    root = region_tree_remove(root, 0x2000);
    munit_assert_ptr_null(region_tree_lookup(root, 0x2000));
    munit_assert_ptr_not_null(region_tree_lookup(root, 0x1000));
    munit_assert_ptr_not_null(region_tree_lookup(root, 0x3000));
    return MUNIT_OK;
}

static MunitResult
test_kernel_space_insert_blocked(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;
    Region *root = NULL;
    root = region_tree_insert(root, make_region(0x1000, 0x2000));
    Region *bad = make_region(0x8000000000000000ULL, 0x8000000000001000ULL);
    Region *res = region_tree_insert(root, bad);
    munit_assert_ptr_equal(res, root);
    munit_assert_ptr_null(region_tree_lookup(root, 0x8000000000000000ULL));
    return MUNIT_OK;
}

static MunitResult test_invalid_region_insert(const MunitParameter params[],
                                              void *data) {
    (void)params;
    (void)data;
    Region *root = NULL;
    Region *bad = make_region(0x3000, 0x1000); // end < start
    Region *res = region_tree_insert(root, bad);
    munit_assert_ptr_equal(res, root);
    return MUNIT_OK;
}

static MunitTest region_tree_tests[] = {
        {"/insert_and_lookup", test_insert_and_lookup, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/resize", test_resize, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove", test_remove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/kernel_blocked", test_kernel_space_insert_blocked, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/invalid_insert", test_invalid_region_insert, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite region_tree_suite = {"/region_tree", region_tree_tests,
                                             NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&region_tree_suite, NULL, argc, argv);
}
