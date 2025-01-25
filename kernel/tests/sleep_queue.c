/*
 * Tests for the sleep queue
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "sleep_queue.h"
#include "munit.h"

static void *test_setup(const MunitParameter params[], void *user_data) {
    SleepQueue *queue = malloc(sizeof(SleepQueue));
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

static void test_teardown(void *fixture) { free(fixture); }

static MunitResult test_enqueue_single(const MunitParameter params[],
                                       void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper = {0};
    Task task = {0};
    sleeper.task = &task;

    sleep_queue_enqueue(queue, &sleeper, 100);

    munit_assert_ptr_equal(queue->head, &sleeper);
    munit_assert_ptr_equal(queue->tail, &sleeper);
    munit_assert_uint64(sleeper.wake_at, ==, 100);

    return MUNIT_OK;
}

static MunitResult test_enqueue_multiple_ordered(const MunitParameter params[],
                                                 void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper1 = {0}, sleeper2 = {0};
    Task task1 = {0}, task2 = {0};
    sleeper1.task = &task1;
    sleeper2.task = &task2;

    sleep_queue_enqueue(queue, &sleeper1, 100);
    sleep_queue_enqueue(queue, &sleeper2, 200);

    munit_assert_ptr_equal(queue->head, &sleeper1);
    munit_assert_ptr_equal(queue->tail, &sleeper2);
    munit_assert_ptr_equal(sleeper1.this.next, &sleeper2.this);
    munit_assert_uint64(sleeper1.wake_at, ==, 100);
    munit_assert_uint64(sleeper2.wake_at, ==, 200);

    return MUNIT_OK;
}

static MunitResult
test_enqueue_multiple_unordered(const MunitParameter params[], void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper1 = {0}, sleeper2 = {0};
    Task task1 = {0}, task2 = {0};
    sleeper1.task = &task1;
    sleeper2.task = &task2;

    sleep_queue_enqueue(queue, &sleeper1, 200);
    sleep_queue_enqueue(queue, &sleeper2, 100);

    munit_assert_ptr_equal(queue->head, &sleeper2);
    munit_assert_ptr_equal(queue->tail, &sleeper1);
    munit_assert_ptr_equal(sleeper2.this.next, &sleeper1.this);
    munit_assert_uint64(sleeper2.wake_at, ==, 100);
    munit_assert_uint64(sleeper1.wake_at, ==, 200);

    return MUNIT_OK;
}

static MunitResult test_dequeue_none(const MunitParameter params[],
                                     void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper = {0};
    Task task = {0};
    sleeper.task = &task;

    sleep_queue_enqueue(queue, &sleeper, 200);
    Sleeper *result = sleep_queue_dequeue(queue, 100);

    munit_assert_ptr_null(result);
    munit_assert_ptr_equal(queue->head, &sleeper);

    return MUNIT_OK;
}

static MunitResult test_dequeue_single(const MunitParameter params[],
                                       void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper = {0};
    Task task = {0};
    sleeper.task = &task;

    sleep_queue_enqueue(queue, &sleeper, 100);
    Sleeper *result = sleep_queue_dequeue(queue, 200);

    munit_assert_ptr_equal(result, &sleeper);
    munit_assert_ptr_null(queue->head);
    munit_assert_ptr_null(queue->tail);

    return MUNIT_OK;
}

static MunitResult test_enqueue_same_deadline(const MunitParameter params[],
                                              void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper1 = {0}, sleeper2 = {0};
    Task task1 = {0}, task2 = {0};
    sleeper1.task = &task1;
    sleeper2.task = &task2;

    sleep_queue_enqueue(queue, &sleeper1, 100);
    sleep_queue_enqueue(queue, &sleeper2, 100);

    munit_assert_ptr_equal(queue->head, &sleeper1);
    munit_assert_ptr_equal(queue->tail, &sleeper2);
    munit_assert_ptr_equal(sleeper1.this.next, &sleeper2.this);
    munit_assert_uint64(sleeper1.wake_at, ==, 100);
    munit_assert_uint64(sleeper2.wake_at, ==, 100);

    return MUNIT_OK;
}

static MunitResult test_enqueue_null_queue(const MunitParameter params[],
                                           void *fixture) {
    Sleeper sleeper = {0};
    Task task = {0};
    sleeper.task = &task;

    // Should not crash
    sleep_queue_enqueue(NULL, &sleeper, 100);
    return MUNIT_OK;
}

static MunitResult test_enqueue_null_sleeper(const MunitParameter params[],
                                             void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;

    // Should not crash
    sleep_queue_enqueue(queue, NULL, 100);
    munit_assert_ptr_null(queue->head);
    munit_assert_ptr_null(queue->tail);
    return MUNIT_OK;
}

static MunitResult test_dequeue_empty_queue(const MunitParameter params[],
                                            void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;

    Sleeper *result = sleep_queue_dequeue(queue, 100);
    munit_assert_ptr_null(result);
    return MUNIT_OK;
}

static MunitResult test_dequeue_multiple(const MunitParameter params[],
                                         void *fixture) {
    SleepQueue *queue = (SleepQueue *)fixture;
    Sleeper sleeper1 = {0}, sleeper2 = {0}, sleeper3 = {0};
    Task task1 = {0}, task2 = {0}, task3 = {0};
    sleeper1.task = &task1;
    sleeper2.task = &task2;
    sleeper3.task = &task3;

    sleep_queue_enqueue(queue, &sleeper1, 100);
    sleep_queue_enqueue(queue, &sleeper2, 200);
    sleep_queue_enqueue(queue, &sleeper3, 300);

    Sleeper *result = sleep_queue_dequeue(queue, 250);

    munit_assert_ptr_equal(result, &sleeper1);
    munit_assert_ptr_equal(result->this.next, &sleeper2.this);
    munit_assert_ptr_equal(queue->head, &sleeper3);
    munit_assert_ptr_equal(queue->tail, &sleeper3);

    return MUNIT_OK;
}

static MunitTest sleep_queue_tests[] = {
        {"/enqueue_single", test_enqueue_single, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_multiple_ordered", test_enqueue_multiple_ordered, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_multiple_unordered", test_enqueue_multiple_unordered,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_none", test_dequeue_none, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_single", test_dequeue_single, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_multiple", test_dequeue_multiple, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_same_deadline", test_enqueue_same_deadline, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_null_queue", test_enqueue_null_queue, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_null_sleeper", test_enqueue_null_sleeper, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_empty_queue", test_dequeue_empty_queue, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite sleep_queue_suite = {"/sleep_queue", sleep_queue_tests,
                                             NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&sleep_queue_suite, NULL, argc, argv);
}