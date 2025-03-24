#include "structs/shift_array.h"
#include "munit.h"

#include <stdlib.h>
#include <string.h>

/** Mock allocator **/
void *fba_alloc_block(void) { return calloc(1, 4096); }

void fba_free(void *ptr) { free(ptr); }

static MunitResult test_insert_head(const MunitParameter params[], void *data) {
    ShiftToMiddleArray arr;
    munit_assert_true(shift_array_init(&arr, sizeof(int), 4));

    for (int i = 0; i < 5; i++) {
        munit_assert_true(shift_array_insert_head(&arr, &i));
    }

    for (int i = 4; i >= 0; i--) {
        int *val = (int *)shift_array_get_head(&arr);
        munit_assert_not_null(val);
        munit_assert_int(*val, ==, i);
        shift_array_remove_head(&arr);
    }

    munit_assert_true(shift_array_is_empty(&arr));

    shift_array_free(&arr);
    return MUNIT_OK;
}

static MunitResult test_insert_tail(const MunitParameter params[], void *data) {
    ShiftToMiddleArray arr;
    munit_assert_true(shift_array_init(&arr, sizeof(int), 4));

    for (int i = 0; i < 6; i++) {
        munit_assert_true(shift_array_insert_tail(&arr, &i));
    }

    for (int i = 0; i < 6; i++) {
        int *val = (int *)shift_array_get_head(&arr);
        munit_assert_not_null(val);
        munit_assert_int(*val, ==, i);
        shift_array_remove_head(&arr);
    }

    shift_array_free(&arr);
    return MUNIT_OK;
}

static MunitResult test_head_tail_access(const MunitParameter params[],
                                         void *data) {
    ShiftToMiddleArray arr;
    munit_assert_true(shift_array_init(&arr, sizeof(int), 2));

    int a = 10, b = 20, c = 30;
    munit_assert_true(shift_array_insert_tail(&arr, &a));
    munit_assert_true(shift_array_insert_tail(&arr, &b));
    munit_assert_true(shift_array_insert_tail(&arr, &c)); // force resize

    int *head = (int *)shift_array_get_head(&arr);
    int *tail = (int *)shift_array_get_tail(&arr);

    munit_assert_not_null(head);
    munit_assert_not_null(tail);
    munit_assert_int(*head, ==, 10);
    munit_assert_int(*tail, ==, 30);

    shift_array_free(&arr);
    return MUNIT_OK;
}

static MunitResult test_remove_behavior(const MunitParameter params[],
                                        void *data) {
    ShiftToMiddleArray arr;
    munit_assert_true(shift_array_init(&arr, sizeof(int), 4));

    int x = 1, y = 2, z = 3;
    shift_array_insert_tail(&arr, &x);
    shift_array_insert_tail(&arr, &y);
    shift_array_insert_tail(&arr, &z);

    shift_array_remove_tail(&arr);
    int *tail = (int *)shift_array_get_tail(&arr);
    munit_assert_int(*tail, ==, 2);

    shift_array_remove_head(&arr);
    int *head = (int *)shift_array_get_head(&arr);
    munit_assert_int(*head, ==, 2);

    shift_array_remove_head(&arr);
    munit_assert_true(shift_array_is_empty(&arr));
    munit_assert_null(shift_array_get_head(&arr));
    munit_assert_null(shift_array_get_tail(&arr));

    shift_array_free(&arr);
    return MUNIT_OK;
}

static MunitTest tests[] = {
        {"/insert_head", test_insert_head, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/insert_tail", test_insert_tail, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/head_tail_access", test_head_tail_access, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/remove_behavior", test_remove_behavior, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/shift_array", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
