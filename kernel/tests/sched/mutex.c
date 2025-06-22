#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "munit.h"

#include "sched/mutex.h"
#include "task.h"

static Task *current_task = NULL;

Task *task_current(void) { return current_task; }

void task_set_current(Task *task) { current_task = task; }

Task *task_create_test(const char *name, uint8_t prio) {
    static Task tasks[8];
    static int count = 0;
    Task *t = &tasks[count++];
    memset(t, 0, sizeof(Task));
    t->sched = &t->ssched;
    t->ssched.prio = prio;
    t->ssched.state = TASK_STATE_READY;
    return t;
}

void sched_block(Task *task) { task->sched->state = TASK_STATE_BLOCKED; }

void sched_unblock(Task *task) { task->sched->state = TASK_STATE_READY; }

void sched_schedule(void) {}
void sched_lock_this_cpu(void) {}
void sched_unlock_this_cpu(uintptr_t flags) {}

uint64_t spinlock_lock_irqsave(SpinLock *ignored) { return 0; }
void spinlock_unlock_irqrestore(SpinLock *ignored, uint64_t ignored2) {}
void spinlock_unlock(SpinLock *ignored) {}
void spinlock_init(SpinLock *ignored) {}

void *slab_alloc_block(void) {
    static uint8_t arena[4096 * 8];
    static size_t offset = 0;
    if (offset + 4096 > sizeof(arena))
        return NULL;
    void *block = &arena[offset];
    offset += 4096;
    return block;
}

void slab_free(void *ptr) {
    // no-op in this mock
}

// === Tests === //

static MunitResult test_basic_lock_unlock(const MunitParameter params[],
                                          void *data) {
    Task *me = task_create_test("task1", 1);
    task_set_current(me);
    Mutex *m = mutex_create();
    munit_assert_true(mutex_lock(m));
    munit_assert_true(mutex_unlock(m));
    return MUNIT_OK;
}

static MunitResult test_recursive_lock(const MunitParameter params[],
                                       void *data) {
    Task *me = task_create_test("task2", 1);
    task_set_current(me);
    Mutex *m = mutex_create();
    munit_assert_true(mutex_lock(m));
    munit_assert_true(mutex_lock(m));
    munit_assert_true(mutex_unlock(m));
    return MUNIT_OK;
}

static MunitResult test_wrong_owner_unlock_fails(const MunitParameter params[],
                                                 void *data) {
    Task *t1 = task_create_test("task3", 1);
    Task *t2 = task_create_test("task4", 2);
    task_set_current(t1);
    Mutex *m = mutex_create();
    munit_assert_true(mutex_lock(m));
    task_set_current(t2);
    munit_assert_false(mutex_unlock(m));
    return MUNIT_OK;
}

static MunitTest tests[] = {
        {"/basic", test_basic_lock_unlock, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/recursive", test_recursive_lock, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/wrong_owner", test_wrong_owner_unlock_fails, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/mutex", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
