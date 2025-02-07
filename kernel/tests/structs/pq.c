/*
 * stage3 - Tests for task priority queue
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "munit.h"
#include "structs/pq.h"

typedef struct {
    TaskPriorityQueue pq;
    Task nodes[10]; // Pre-allocated nodes for testing
    TaskSched scheds[10];
} PQFixture;

static void *pq_setup(const MunitParameter params[], void *user_data) {
    PQFixture *fixture = munit_new(PQFixture);
    task_pq_init(&fixture->pq);
    // Initialize nodes with some default values
    for (int i = 0; i < 10; i++) {
        fixture->nodes[i].sched = &fixture->scheds[i];
        fixture->nodes[i].sched->prio = 0;
        fixture->nodes[i].this.next = NULL;
    }
    return fixture;
}

static void pq_tear_down(void *fixture) { free(fixture); }

// Test functions
static MunitResult test_empty_queue(const MunitParameter params[],
                                    void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    munit_assert_true(task_pq_empty(&f->pq));
    munit_assert_null(task_pq_peek(&f->pq));
    munit_assert_null(task_pq_pop(&f->pq));

    return MUNIT_OK;
}

static MunitResult test_single_element(const MunitParameter params[],
                                       void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    f->nodes[0].sched->prio = 5;
    task_pq_push(&f->pq, &f->nodes[0]);

    munit_assert_false(task_pq_empty(&f->pq));
    munit_assert_ptr_equal(task_pq_peek(&f->pq), &f->nodes[0]);
    munit_assert_int(task_pq_peek(&f->pq)->sched->prio, ==, 5);

    Task *task_pq_popped = task_pq_pop(&f->pq);
    munit_assert_ptr_equal(task_pq_popped, &f->nodes[0]);
    munit_assert_true(task_pq_empty(&f->pq));

    return MUNIT_OK;
}

static MunitResult test_priority_ordering(const MunitParameter params[],
                                          void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    // Insert in non-priority order
    f->nodes[0].sched->prio = 5;
    f->nodes[1].sched->prio = 3;
    f->nodes[2].sched->prio = 7;
    f->nodes[3].sched->prio = 1;

    task_pq_push(&f->pq, &f->nodes[0]); // 5
    task_pq_push(&f->pq, &f->nodes[1]); // 3
    task_pq_push(&f->pq, &f->nodes[2]); // 7
    task_pq_push(&f->pq, &f->nodes[3]); // 1

    // Should come out in priority order
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 1);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 3);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 5);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 7);
    munit_assert_true(task_pq_empty(&f->pq));

    return MUNIT_OK;
}

static MunitResult test_duplicate_priorities(const MunitParameter params[],
                                             void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    f->nodes[0].sched->prio = 5;
    f->nodes[1].sched->prio = 5;
    f->nodes[2].sched->prio = 3;
    f->nodes[3].sched->prio = 3;

    task_pq_push(&f->pq, &f->nodes[0]);
    task_pq_push(&f->pq, &f->nodes[1]);
    task_pq_push(&f->pq, &f->nodes[2]);
    task_pq_push(&f->pq, &f->nodes[3]);

    // Should maintain FIFO order within same priority
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 3);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 3);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 5);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 5);

    return MUNIT_OK;
}

static MunitResult test_null_node(const MunitParameter params[],
                                  void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    // Pushing NULL should be safe
    task_pq_push(&f->pq, NULL);
    munit_assert_true(task_pq_empty(&f->pq));

    return MUNIT_OK;
}

static MunitResult test_reused_node(const MunitParameter params[],
                                    void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    f->nodes[0].sched->prio = 5;
    task_pq_push(&f->pq, &f->nodes[0]);

    Task *popped = task_pq_pop(&f->pq);
    munit_assert_ptr_equal(popped, &f->nodes[0]);

    // Reuse the popped node
    popped->sched->prio = 3; // Change priority
    task_pq_push(&f->pq, popped);

    // Should be able to pop it again
    Task *repopped = task_pq_pop(&f->pq);
    munit_assert_ptr_equal(repopped, &f->nodes[0]);
    munit_assert_int(repopped->sched->prio, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_extreme_priorities(const MunitParameter params[],
                                           void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    f->nodes[0].sched->prio = UINT8_MAX;
    f->nodes[1].sched->prio = 0;
    f->nodes[2].sched->prio = 0;
    f->nodes[3].sched->prio = 1;

    // Insert in random order
    task_pq_push(&f->pq, &f->nodes[0]); // UINT8_MAX
    task_pq_push(&f->pq, &f->nodes[2]); // 0
    task_pq_push(&f->pq, &f->nodes[1]); // 0
    task_pq_push(&f->pq, &f->nodes[3]); // 1

    // Should come out in priority order
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 0);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 0);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 1);
    munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, UINT8_MAX);

    return MUNIT_OK;
}

static MunitResult test_alternating_priorities(const MunitParameter params[],
                                               void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    // Create alternating high/low priorities
    for (int i = 0; i < 8; i++) {
        f->nodes[i].sched->prio = (i % 2 == 0) ? 100 : 1;
    }

    // Push in alternating order
    for (int i = 0; i < 8; i++) {
        task_pq_push(&f->pq, &f->nodes[i]);
    }

    // Should come out grouped by priority
    for (int i = 0; i < 4; i++) {
        munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 1);
    }
    for (int i = 0; i < 4; i++) {
        munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 100);
    }

    return MUNIT_OK;
}

static MunitResult test_empty_refill(const MunitParameter params[],
                                     void *fixture) {
    PQFixture *f = (PQFixture *)fixture;

    // Fill and empty multiple times
    for (int cycle = 0; cycle < 3; cycle++) {
        munit_assert_true(task_pq_empty(&f->pq));

        // Fill
        f->nodes[0].sched->prio = 3;
        f->nodes[1].sched->prio = 1;
        task_pq_push(&f->pq, &f->nodes[0]);
        task_pq_push(&f->pq, &f->nodes[1]);

        munit_assert_false(task_pq_empty(&f->pq));

        // Empty
        munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 1);
        munit_assert_int(task_pq_pop(&f->pq)->sched->prio, ==, 3);
    }

    return MUNIT_OK;
}

// Test suite
static MunitTest tests[] = {
        {"/empty_queue", test_empty_queue, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/single_element", test_single_element, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/priority_ordering", test_priority_ordering, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/duplicate_priorities", test_duplicate_priorities, pq_setup,
         pq_tear_down, MUNIT_TEST_OPTION_NONE, NULL},
        {"/null_node", test_null_node, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/reused_node", test_reused_node, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/extreme_priorities", test_extreme_priorities, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/alternating_priorities", test_alternating_priorities, pq_setup,
         pq_tear_down, MUNIT_TEST_OPTION_NONE, NULL},
        {"/empty_refill", test_empty_refill, pq_setup, pq_tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/pq", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}