#include "munit.h"

#include "managed_resources/resources.h"

// Dummy counter to track calls
static int dummy_free_calls = 0;

// Dummy free function
void dummy_free_func(ManagedResource *resource) {
    (void)resource;
    dummy_free_calls++;
}

// Helper to init a resource
void init_managed_resource(ManagedResource *res) {
    memset(res, 0, sizeof(*res));
    res->free_func = dummy_free_func;
}

// Tests

static MunitResult test_free_no_resources(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    dummy_free_calls = 0;
    managed_resources_free_all(NULL);

    munit_assert_int(dummy_free_calls, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_free_single_resource(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    ManagedResource res;
    init_managed_resource(&res);

    dummy_free_calls = 0;
    managed_resources_free_all(&res);

    munit_assert_int(dummy_free_calls, ==, 1);

    return MUNIT_OK;
}

static MunitResult test_free_multiple_resources(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    ManagedResource res1, res2, res3;
    init_managed_resource(&res1);
    init_managed_resource(&res2);
    init_managed_resource(&res3);

    res1.this.next = &res2.this;
    res2.this.next = &res3.this;
    res3.this.next = NULL;

    dummy_free_calls = 0;
    managed_resources_free_all(&res1);

    munit_assert_int(dummy_free_calls, ==, 3);

    return MUNIT_OK;
}

// Test suite setup

static MunitTest tests[] = {{"/free_none", test_free_no_resources, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {"/free_single", test_free_single_resource, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {"/free_multiple", test_free_multiple_resources, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/managed_resources", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) { return munit_suite_main(&suite, NULL, argc, argv); }
