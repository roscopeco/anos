/*
 * Tests for the sleep queue
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "sleep_queue.h"
#include "munit.h"

#include "fba/alloc.h"
#include "mock_pmm.h"
#include "slab/alloc.h"
#include "vmm/vmalloc.h"

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))

#define INIT_TASK_CLASS TASK_CLASS_NORMAL

static const int PAGES_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

static const uintptr_t TEST_PAGETABLE_ROOT = 0x1234567887654321;
static const uintptr_t TEST_SYS_SP = 0xc0c010c0a1b2c3d4;
static const uintptr_t TEST_SYS_FUNC = 0x2bad3bad4badf00d;

typedef struct {
    SleepQueue *queue;
    void *page_area_ptr;
} Fixture;

void panic_sloc(char *str) {
    munit_errorf("panic_sloc called in sleep_queue: %s\n", str);
}

static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    fba_init(TEST_PML4_ADDR, (uintptr_t)page_area_ptr, 32768);
    slab_alloc_init();

    SleepQueue *queue = malloc(sizeof(SleepQueue));
    queue->head = NULL;
    queue->always0 = 0;
    queue->tail = NULL;

    Fixture *fixture = malloc(sizeof(Fixture));
    fixture->queue = queue;
    fixture->page_area_ptr = page_area_ptr;

    return fixture;
}

static void test_teardown(void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;

    free(fixture->page_area_ptr);
    free(fixture->queue);
    free(fixture);

    mock_pmm_reset();
}

static MunitResult test_enqueue_single(const MunitParameter params[],
                                       void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task = {0};

    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result = sleep_queue_enqueue(queue, &task, 100);

    munit_assert_true(result);
    munit_assert_ptr_not_null(queue->head);
    munit_assert_ptr_equal(queue->head, queue->tail);
    munit_assert_uint64(queue->head->wake_at, ==, 100);
    munit_assert_ptr_equal(queue->head->task, &task);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        initial_allocs + PAGES_PER_SLAB);

    return MUNIT_OK;
}

static MunitResult test_enqueue_multiple_ordered(const MunitParameter params[],
                                                 void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task1 = {0}, task2 = {0};
    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result1 = sleep_queue_enqueue(queue, &task1, 100);
    bool result2 = sleep_queue_enqueue(queue, &task2, 200);

    munit_assert_true(result1);
    munit_assert_true(result2);
    munit_assert_ptr_not_null(queue->head);
    munit_assert_ptr_not_null(queue->tail);
    munit_assert_ptr_equal(queue->head->task, &task1);
    munit_assert_ptr_equal(queue->tail->task, &task2);
    munit_assert_ptr_equal(((ListNode *)queue->head)->next,
                           (ListNode *)queue->tail);
    munit_assert_uint64(queue->head->wake_at, ==, 100);
    munit_assert_uint64(queue->tail->wake_at, ==, 200);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        initial_allocs + PAGES_PER_SLAB);

    return MUNIT_OK;
}

static MunitResult
test_enqueue_multiple_unordered(const MunitParameter params[],
                                void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task1 = {0}, task2 = {0};
    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result1 = sleep_queue_enqueue(queue, &task1, 200);
    bool result2 = sleep_queue_enqueue(queue, &task2, 100);

    munit_assert_true(result1);
    munit_assert_true(result2);
    munit_assert_ptr_not_null(queue->head);
    munit_assert_ptr_not_null(queue->tail);
    munit_assert_ptr_equal(queue->head->task, &task2);
    munit_assert_ptr_equal(queue->tail->task, &task1);
    munit_assert_uint64(queue->head->wake_at, ==, 100);
    munit_assert_uint64(queue->tail->wake_at, ==, 200);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        initial_allocs + PAGES_PER_SLAB);

    return MUNIT_OK;
}

static MunitResult test_dequeue_none(const MunitParameter params[],
                                     void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task = {0};
    uint32_t initial_frees = mock_pmm_get_total_page_frees();

    sleep_queue_enqueue(queue, &task, 200);
    Task *result = sleep_queue_dequeue(queue, 100);

    munit_assert_ptr_null(result);
    munit_assert_ptr_not_null(queue->head);
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, initial_frees);

    return MUNIT_OK;
}

static MunitResult test_dequeue_single(const MunitParameter params[],
                                       void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task = {0};
    uint32_t initial_frees = mock_pmm_get_total_page_frees();

    sleep_queue_enqueue(queue, &task, 100);
    Task *result = sleep_queue_dequeue(queue, 200);

    // result is dequeued
    munit_assert_ptr_not_null(result);
    munit_assert_ptr_equal(result, &task);
    munit_assert_ptr_null(result->this.next);

    // queue is empty
    munit_assert_ptr_null(queue->head);
    munit_assert_ptr_null(queue->tail);

    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==,
                        initial_frees); // Dequeue returns memory to caller

    return MUNIT_OK;
}

static MunitResult test_dequeue_multiple(const MunitParameter params[],
                                         void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task1 = {0}, task2 = {0}, task3 = {0};
    uint32_t initial_frees = mock_pmm_get_total_page_frees();

    sleep_queue_enqueue(queue, &task1, 100);
    sleep_queue_enqueue(queue, &task2, 200);
    sleep_queue_enqueue(queue, &task3, 300);

    Task *result = sleep_queue_dequeue(queue, 250);

    // Result has the two dequeued nodes
    munit_assert_ptr_not_null(result);
    munit_assert_ptr_equal(result, &task1);
    munit_assert_ptr_equal(((ListNode *)result)->next, (ListNode *)&task2);
    munit_assert_null(((ListNode *)result)->next->next);

    munit_assert_ptr_equal(queue->head->task, &task3);
    munit_assert_ptr_equal(queue->tail->task, &task3);

    // TODO really need to be checking actual slab block frees here,
    //      rather than the whole slab, which won't get freed anyhow
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==,
                        initial_frees); // slab not freed

    return MUNIT_OK;
}

static MunitResult test_dequeue_mult_all(const MunitParameter params[],
                                         void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task1 = {0}, task2 = {0}, task3 = {0};
    uint32_t initial_frees = mock_pmm_get_total_page_frees();

    sleep_queue_enqueue(queue, &task1, 100);
    sleep_queue_enqueue(queue, &task2, 200);
    sleep_queue_enqueue(queue, &task3, 300);

    Task *result = sleep_queue_dequeue(queue, 10000);

    // Result has the three dequeued nodes
    munit_assert_ptr_not_null(result);
    munit_assert_ptr_equal(result, &task1);
    munit_assert_ptr_equal(((ListNode *)result)->next, (ListNode *)&task2);
    munit_assert_ptr_equal(((ListNode *)result)->next->next,
                           (ListNode *)&task3);
    munit_assert_null(((ListNode *)result)->next->next->next);

    munit_assert_ptr_equal(queue->head, NULL);
    munit_assert_ptr_equal(queue->tail, NULL);

    // TODO really need to be checking actual slab block frees here,
    //      rather than the whole slab, which won't get freed anyhow
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, initial_frees);

    return MUNIT_OK;
}

static MunitResult test_enqueue_alloc_failure(const MunitParameter params[],
                                              void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task = {0};

    // exhaust the FBA's memory...
    volatile void *waste;
    do {
        waste = fba_alloc_block();
    } while (waste);

    // Now we can't allocate, so we can't enqueue...
    bool result = sleep_queue_enqueue(queue, &task, 100);

    munit_assert_false(result);
    munit_assert_ptr_null(queue->head);
    munit_assert_ptr_null(queue->tail);

    return MUNIT_OK;
}

static MunitResult test_enqueue_null_queue(const MunitParameter params[],
                                           void *fixture_v) {
    Task task = {0};
    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result = sleep_queue_enqueue(NULL, &task, 100);

    munit_assert_false(result);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, initial_allocs);

    return MUNIT_OK;
}

static MunitResult test_enqueue_null_task(const MunitParameter params[],
                                          void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result = sleep_queue_enqueue(queue, NULL, 100);

    munit_assert_false(result);
    munit_assert_ptr_null(queue->head);
    munit_assert_ptr_null(queue->tail);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, initial_allocs);

    return MUNIT_OK;
}

static MunitResult
test_enqueue_single_zero_deadline(const MunitParameter params[],
                                  void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    Task task = {0};

    uint32_t initial_allocs = mock_pmm_get_total_page_allocs();

    bool result = sleep_queue_enqueue(queue, &task, 0);

    munit_assert_true(result);
    munit_assert_ptr_not_null(queue->head);
    munit_assert_ptr_equal(queue->head, queue->tail);
    munit_assert_uint64(queue->head->wake_at, ==, 0);
    munit_assert_ptr_equal(queue->head->task, &task);
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        initial_allocs + PAGES_PER_SLAB);

    return MUNIT_OK;
}

static MunitResult test_dequeue_empty_queue(const MunitParameter params[],
                                            void *fixture_v) {
    Fixture *fixture = (Fixture *)fixture_v;
    SleepQueue *queue = (SleepQueue *)fixture->queue;

    uint32_t initial_frees = mock_pmm_get_total_page_frees();

    Task *result = sleep_queue_dequeue(queue, 100);

    munit_assert_ptr_null(result);
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, initial_frees);

    return MUNIT_OK;
}

static MunitTest sleep_queue_tests[] = {
        {"/enqueue_one", test_enqueue_single, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_mult_ordered", test_enqueue_multiple_ordered, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_mult_unordered", test_enqueue_multiple_unordered, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_alloc_failure", test_enqueue_alloc_failure, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_null_queue", test_enqueue_null_queue, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_null_task", test_enqueue_null_task, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/enqueue_zero_deadline", test_enqueue_single_zero_deadline,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_none", test_dequeue_none, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_single", test_dequeue_single, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_multiple", test_dequeue_multiple, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_mult_all", test_dequeue_mult_all, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/dequeue_empty_queue", test_dequeue_empty_queue, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite sleep_queue_suite = {"/sleep_queue", sleep_queue_tests,
                                             NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&sleep_queue_suite, NULL, argc, argv);
}