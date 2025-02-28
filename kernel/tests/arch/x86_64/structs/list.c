/*
 * Tests for the base linked node list
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stddef.h>
#include <stdlib.h>

#include "munit.h"
#include "structs/list.h"

#define NEW_NODE_TYPE 42
#define SECOND_NODE_TYPE 99
#define THIRD_NODE_TYPE 141

static ListNode new_node;
static ListNode second_node;
static ListNode third_node;

static MunitResult test_insert_null_into_null(const MunitParameter params[],
                                              void *param) {
    ListNode *node = list_insert_after(NULL, NULL);

    munit_assert_null(node);

    return MUNIT_OK;
}

static MunitResult test_insert_into_null(const MunitParameter params[],
                                         void *param) {
    ListNode *node = list_insert_after(NULL, &new_node);

    munit_assert_ptr(node, ==, &new_node);

    // This should also null out any next (i.e. create a new list)
    node->next = &second_node;

    node = list_insert_after(NULL, &new_node);

    munit_assert_ptr(node, ==, &new_node);
    munit_assert_null(node->next);

    return MUNIT_OK;
}

static MunitResult test_insert_null_into_list(const MunitParameter params[],
                                              void *param) {
    new_node.next = &new_node;

    ListNode *node = list_insert_after(&new_node, NULL);

    munit_assert_null(node);
    munit_assert_null(new_node.next);

    return MUNIT_OK;
}

static MunitResult test_insert_node_into_self(const MunitParameter params[],
                                              void *param) {
    // This should be a no-op, or it would create a circular list, which this
    // lib doesn't support
    //
    // If next was previously null, it should remain so (no-op)
    ListNode *node = list_insert_after(&new_node, &new_node);

    munit_assert_ptr(node, ==, &new_node);
    munit_assert_null(node->next);

    // If next was previously non-null, should still be the same after (so
    // again, no-op).
    new_node.next = &second_node;
    node = list_insert_after(&new_node, &new_node);

    munit_assert_ptr(node, ==, &new_node);
    munit_assert_ptr(node->next, ==, &second_node);

    return MUNIT_OK;
}

static MunitResult test_insert_node_into_list(const MunitParameter params[],
                                              void *param) {
    munit_assert_null(new_node.next);

    ListNode *node = list_insert_after(&new_node, &second_node);

    munit_assert_ptr(node, ==, &second_node);

    munit_assert_ptr(new_node.next, ==, &second_node);
    munit_assert_null(second_node.next);

    return MUNIT_OK;
}

static MunitResult test_insert_node_into_middle(const MunitParameter params[],
                                                void *param) {
    new_node.next = &second_node;

    ListNode *node = list_insert_after(&new_node, &third_node);

    munit_assert_ptr(node, ==, &third_node);

    munit_assert_ptr(new_node.next, ==, &third_node);
    munit_assert_ptr(third_node.next, ==, &second_node);
    munit_assert_null(second_node.next);

    return MUNIT_OK;
}

static MunitResult test_add_null_to_null(const MunitParameter params[],
                                         void *param) {
    ListNode *node = list_add(NULL, NULL);

    munit_assert_null(node);

    return MUNIT_OK;
}

static MunitResult test_add_node_to_null(const MunitParameter params[],
                                         void *param) {
    ListNode *node = list_add(NULL, &new_node);

    munit_assert_ptr(node, ==, &new_node);
    munit_assert_null(new_node.next);

    // Should also clear any existing next on adding to null (new list)
    new_node.next = &second_node;

    node = list_add(NULL, &new_node);

    munit_assert_ptr(node, ==, &new_node);
    munit_assert_null(new_node.next);

    return MUNIT_OK;
}

static MunitResult test_add_null_to_list_one(const MunitParameter params[],
                                             void *param) {
    ListNode *node = list_add(&new_node, NULL);

    munit_assert_null(node);
    munit_assert_null(new_node.next);

    return MUNIT_OK;
}

static MunitResult test_add_null_to_list_multi(const MunitParameter params[],
                                               void *param) {
    new_node.next = &second_node;

    ListNode *node = list_add(&new_node, NULL);

    munit_assert_null(node);
    munit_assert_ptr(new_node.next, ==, &second_node);
    munit_assert_null(second_node.next);

    return MUNIT_OK;
}

static MunitResult test_add_node_to_list_one(const MunitParameter params[],
                                             void *param) {
    munit_assert_null(new_node.next);

    ListNode *node = list_add(&new_node, &second_node);

    munit_assert_ptr(node, ==, &second_node);
    munit_assert_ptr(new_node.next, ==, &second_node);

    return MUNIT_OK;
}

static MunitResult test_add_node_to_list_multi(const MunitParameter params[],
                                               void *param) {
    new_node.next = &second_node;

    ListNode *node = list_add(&new_node, &second_node);

    munit_assert_ptr(node, ==, &second_node);
    munit_assert_ptr(new_node.next, ==, &second_node);
    munit_assert_null(second_node.next);

    return MUNIT_OK;
}

static MunitResult test_delete_null(const MunitParameter params[],
                                    void *param) {
    ListNode *node = list_delete_after(NULL);

    munit_assert_null(node);

    return MUNIT_OK;
}

static MunitResult test_delete_list_end(const MunitParameter params[],
                                        void *param) {
    munit_assert_null(new_node.next);

    ListNode *node = list_delete_after(&new_node);

    munit_assert_null(node);
    munit_assert_null(new_node.next);

    return MUNIT_OK;
}

static MunitResult test_delete_middle_node(const MunitParameter params[],
                                           void *param) {
    new_node.next = &second_node;
    second_node.next = &third_node;

    ListNode *node = list_delete_after(&new_node);

    munit_assert_ptr(node, ==, &second_node);
    munit_assert_ptr(new_node.next, ==, &third_node);

    // link here is removed
    munit_assert_null(second_node.next);

    return MUNIT_OK;
}

static MunitResult test_delete_last_node(const MunitParameter params[],
                                         void *param) {
    new_node.next = &second_node;

    ListNode *node = list_delete_after(&new_node);

    munit_assert_ptr(node, ==, &second_node);
    munit_assert_null(new_node.next);

    return MUNIT_OK;
}

static MunitResult test_find_null_null(const MunitParameter params[],
                                       void *param) {

    ListNode *node = list_find(NULL, NULL);

    munit_assert_null(node);

    return MUNIT_OK;
}

static MunitResult test_find_list_null(const MunitParameter params[],
                                       void *param) {

    ListNode *node = list_find(&new_node, NULL);

    munit_assert_null(node);

    return MUNIT_OK;
}

static bool always_true(ListNode *candidate) { return true; }

static MunitResult test_find_null_pred(const MunitParameter params[],
                                       void *param) {

    ListNode *node = list_find(NULL, always_true);

    munit_assert_null(node);

    return MUNIT_OK;
}

static bool match_none(ListNode *candidate) { return false; }

static bool match_head(ListNode *candidate) {
    return candidate->type == NEW_NODE_TYPE;
}

static bool match_middle(ListNode *candidate) {
    return candidate->type == SECOND_NODE_TYPE;
}

static bool match_last(ListNode *candidate) {
    return candidate->type == THIRD_NODE_TYPE;
}

static MunitResult test_find_match_none(const MunitParameter params[],
                                        void *param) {
    new_node.next = &second_node;
    second_node.next = &third_node;

    ListNode *node = list_find(&new_node, match_none);

    munit_assert_null(node);

    return MUNIT_OK;
}

static MunitResult test_find_match_head(const MunitParameter params[],
                                        void *param) {
    new_node.next = &second_node;
    second_node.next = &third_node;

    ListNode *node = list_find(&new_node, match_head);

    munit_assert_ptr(node, ==, &new_node);

    return MUNIT_OK;
}

static MunitResult test_find_match_middle(const MunitParameter params[],
                                          void *param) {
    new_node.next = &second_node;
    second_node.next = &third_node;

    ListNode *node = list_find(&new_node, match_middle);

    munit_assert_ptr(node, ==, &second_node);

    return MUNIT_OK;
}

static MunitResult test_find_match_last(const MunitParameter params[],
                                        void *param) {
    new_node.next = &second_node;
    second_node.next = &third_node;

    ListNode *node = list_find(&new_node, match_last);

    munit_assert_ptr(node, ==, &third_node);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    new_node.type = NEW_NODE_TYPE;
    new_node.next = NULL;

    second_node.type = SECOND_NODE_TYPE;
    second_node.next = NULL;

    third_node.type = THIRD_NODE_TYPE;
    third_node.next = NULL;

    return NULL;
}

static void teardown(void *param) {
    // Nothing
}

static MunitTest test_suite_tests[] = {
        {(char *)"/structs/list/insert_null_into_null",
         test_insert_null_into_null, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/insert_into_null", test_insert_into_null, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/insert_null_into_list",
         test_insert_null_into_list, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/insert_node_into_self",
         test_insert_node_into_self, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/insert_node_into_list",
         test_insert_node_into_list, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/insert_node_into_middle",
         test_insert_node_into_middle, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},

        {(char *)"/structs/list/add_null_to_null", test_add_null_to_null, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/add_node_to_null", test_add_node_to_null, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/add_null_to_list_one",
         test_add_null_to_list_one, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/add_null_to_list_multi",
         test_add_null_to_list_multi, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/add_node_to_list_one",
         test_add_node_to_list_one, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/structs/list/add_node_to_list_multi",
         test_add_node_to_list_multi, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},

        {(char *)"/structs/list/delete_null", test_delete_null, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/delete_list_end", test_delete_list_end, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/delete_last", test_delete_last_node, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/delete_middle", test_delete_middle_node, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/structs/list/find_null_null", test_find_null_null, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_list_null", test_find_list_null, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_null_pred", test_find_null_pred, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_match_none", test_find_match_none, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_match_head", test_find_match_head, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_match_middle", test_find_match_middle,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/structs/list/find_match_last", test_find_match_last, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
