/*
 * Tests for the process memory manager
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "munit.h"

#include "process.h"
#include "process/memory.h"
#include "spinlock.h"

// --- Mocks ---

#define MAX_FAKE_PAGES 128

#define THREADS 4
#define ALLOCS_PER_THREAD 16

typedef struct {
    Process *proc;
    int id;
    bool run_release;
} ThreadArg;

static uint64_t fake_pages[MAX_FAKE_PAGES];
static bool fake_page_allocated[MAX_FAKE_PAGES];
static uint32_t fake_refcount[MAX_FAKE_PAGES];
static pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;

static MemoryRegion dummy_region;

static void reset_fakes(void) {
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        fake_pages[i] = 0x1000 * (i + 1);
        fake_page_allocated[i] = false;
        fake_refcount[i] = 0;
    }
}

void *fba_alloc_block(void) { return calloc(1, 4096); }

void fba_free(void *page) { free(page); }

void *thread_alloc_free(void *arg) {
    ThreadArg *targ = arg;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        uint64_t addr = process_page_alloc(targ->proc, &dummy_region);
        if (addr != 0xFFFFFFFFFFFFFFFF) {
            if (i % 2 == 0) {
                process_page_free(targ->proc, addr);
            } else {
                usleep(100); // slow things down to encourage races
            }
        }
    }
    return NULL;
}

void *thread_stress_release(void *arg) {
    ThreadArg *targ = arg;
    for (int i = 0; i < 8; i++) {
        usleep(150); // Interleave
        process_release_owned_pages(targ->proc);
    }
    return NULL;
}

uintptr_t page_alloc(MemoryRegion *region) {
    (void)region;
    pthread_mutex_lock(&alloc_lock);
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        if (!fake_page_allocated[i]) {
            fake_page_allocated[i] = true;
            pthread_mutex_unlock(&alloc_lock);
            return fake_pages[i];
        }
    }
    pthread_mutex_unlock(&alloc_lock);
    return 0xFFFFFFFFFFFFFFFF;
}

void page_free(MemoryRegion *region, uintptr_t addr) {
    (void)region;
    pthread_mutex_lock(&alloc_lock);
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        if (fake_pages[i] == addr) {
            fake_page_allocated[i] = false;
            break;
        }
    }
    pthread_mutex_unlock(&alloc_lock);
}

uint32_t refcount_map_increment(uintptr_t addr) {
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        if (fake_pages[i] == addr) {
            return ++fake_refcount[i];
        }
    }
    return 0;
}

uint32_t refcount_map_decrement(uintptr_t addr) {
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        if (fake_pages[i] == addr) {
            if (fake_refcount[i] == 0)
                return 0;
            return fake_refcount[i]--;
        }
    }
    return 0;
}

uint64_t spinlock_lock_irqsave(SpinLock *lock) {
    pthread_mutex_lock((pthread_mutex_t *)&lock->lock);
    return 0;
}

void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags) {
    (void)flags;
    pthread_mutex_unlock((pthread_mutex_t *)&lock->lock);
}
static MunitResult test_process_page_alloc_free(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 1,
            .pages_lock = &lock,
            .pages = NULL,
    };

    uint64_t addr = process_page_alloc(&proc, &dummy_region);
    munit_assert_uint64(addr, !=, 0xFFFFFFFFFFFFFFFF);

    bool removed = process_page_free(&proc, addr);
    munit_assert_true(removed);

    process_release_owned_pages(&proc);
    return MUNIT_OK;
}

static MunitResult test_ownership_tracking(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 2,
            .pages_lock = &lock,
            .pages = NULL,
    };

    uint64_t addr1 = process_page_alloc(&proc, &dummy_region);
    uint64_t addr2 = process_page_alloc(&proc, &dummy_region);

    munit_assert_true(process_remove_owned_page(&proc, addr1));
    munit_assert_false(process_remove_owned_page(&proc, 0xdeadbeef));

    process_release_owned_pages(&proc);
    return MUNIT_OK;
}

static MunitResult test_release_frees_all_pages(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 3,
            .pages_lock = &lock,
            .pages = NULL,
    };

    for (int i = 0; i < 10; i++) {
        munit_assert_uint64(process_page_alloc(&proc, &dummy_region), !=,
                            0xFFFFFFFFFFFFFFFF);
    }

    process_release_owned_pages(&proc);

    for (int i = 0; i < 10; i++) {
        munit_assert_false(fake_page_allocated[i]);
    }

    return MUNIT_OK;
}

static MunitResult test_shared_pages_refcounting(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 4,
            .pages_lock = &lock,
            .pages = NULL,
    };

    uint64_t addr = fake_pages[0];
    fake_page_allocated[0] = true;

    munit_assert_true(process_add_owned_page(&proc, &dummy_region, addr, true));
    munit_assert_int(fake_refcount[0], ==, 1);

    munit_assert_true(process_remove_owned_page(&proc, addr));
    munit_assert_int(fake_refcount[0], ==, 0);

    // We don't free the actual memory for the page list until...
    process_release_owned_pages(&proc);

    return MUNIT_OK;
}

static MunitResult test_double_free_is_safe(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 5,
            .pages_lock = &lock,
            .pages = NULL,
    };

    uint64_t addr = process_page_alloc(&proc, &dummy_region);
    munit_assert_true(process_page_free(&proc, addr));
    munit_assert_false(process_page_free(&proc, addr));

    // We don't free the actual memory for the page list until...
    process_release_owned_pages(&proc);

    return MUNIT_OK;
}

static MunitResult test_alloc_failure_handling(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 6,
            .pages_lock = &lock,
            .pages = NULL,
    };

    // Exhaust all pages
    for (int i = 0; i < MAX_FAKE_PAGES; i++) {
        munit_assert_uint64(page_alloc(&dummy_region), !=, 0xFFFFFFFFFFFFFFFF);
    }

    uint64_t addr = process_page_alloc(&proc, &dummy_region);
    munit_assert_uint64(addr, ==, 0xFFFFFFFFFFFFFFFF);

    return MUNIT_OK;
}

static MunitResult test_block_expansion(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    SpinLock lock = {0};
    Process proc = {
            .pid = 7,
            .pages_lock = &lock,
            .pages = NULL,
    };

    const int count = 128;
    for (int i = 0; i < count; i++) {
        munit_assert_uint64(process_page_alloc(&proc, &dummy_region), !=,
                            0xFFFFFFFFFFFFFFFF);
    }

    process_release_owned_pages(&proc);

    for (int i = 0; i < count; i++) {
        munit_assert_false(fake_page_allocated[i]);
    }

    return MUNIT_OK;
}

static MunitResult test_concurrent_allocs(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    SpinLock lock = {.lock = (uint64_t)&mutex};
    Process proc = {
            .pid = 99,
            .pages_lock = &lock,
            .pages = NULL,
    };

    pthread_t threads[THREADS];
    ThreadArg args[THREADS];

    for (int i = 0; i < THREADS; i++) {
        args[i].proc = &proc;
        pthread_create(&threads[i], NULL, thread_alloc_free, &args[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    process_release_owned_pages(&proc);
    return MUNIT_OK;
}

static MunitResult
test_stress_concurrent_alloc_and_release(const MunitParameter params[],
                                         void *data) {
    (void)params;
    (void)data;
    reset_fakes();

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    SpinLock lock = {.lock = (uint64_t)&mutex};
    Process proc = {
            .pid = 100,
            .pages_lock = &lock,
            .pages = NULL,
    };

    pthread_t threads[THREADS];
    ThreadArg args[THREADS];

    for (int i = 0; i < THREADS; i++) {
        args[i].proc = &proc;
        args[i].id = i;
        args[i].run_release = false;
        pthread_create(&threads[i], NULL, thread_alloc_free, &args[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    process_release_owned_pages(&proc);

    process_release_owned_pages(&proc);
    return MUNIT_OK;
}

static MunitTest tests[] = {
        {"/alloc_free", test_process_page_alloc_free, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/ownership_tracking", test_ownership_tracking, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/release_frees_all", test_release_frees_all_pages, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/shared_refcount", test_shared_pages_refcounting, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/double_free", test_double_free_is_safe, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc_failure", test_alloc_failure_handling, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/block_expansion", test_block_expansion, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/concurrent_allocs", test_concurrent_allocs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/stress_concurrent_alloc_release",
         test_stress_concurrent_alloc_and_release, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/process_memory", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
