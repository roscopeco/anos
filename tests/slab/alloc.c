/*
 * Tests for the slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdlib.h>

#include "munit.h"
#include "slab/alloc.h"
#include "test_pmm.h"
#include "test_vmm.h"

static void *test_setup(const MunitParameter params[], void *user_data) {
    return NULL;
}

static void test_teardown(void *page_area_ptr) {
    test_pmm_reset();
    test_vmm_reset();
}

static MunitResult test_slab_init(const MunitParameter params[], void *param) {
    bool result = slab_alloc_init();

    // succeeds
    munit_assert_true(result);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init", test_slab_init, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/slab", test_suite_tests, NULL,
                                      1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
