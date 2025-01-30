/*
 * stage3 - Sleeping task queue
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SLEEP_QUEUE_H
#define __ANOS_KERNEL_SLEEP_QUEUE_H

#include <stdint.h>

#include "structs/list.h"
#include "task.h"

typedef struct {
    ListNode this;    // 16
    uint64_t wake_at; // 32
    Task *task;       // 24
    uint64_t res[4];  // 64
} Sleeper;

/**
 * NOTE ON first `reserved` and `always0` fields:
 * 
 * There's some weirdness here, so listen up.
 * 
 * These fields exist to allow me to cast `(SleepQueue*)` to 
 * `(Sleeper*)` and use it as a sentinel node when searching 
 * the list.
 * 
 * When cast as such, the sentinel node will always:
 * 
 *   * Be non-null (if the queue is non-null ofc)
 *   * Have a deadline of zero (i.e. less than all others)
 * 
 * which means I can avoid some special cases and branching
 * when traversing the list, but it does also mean that 
 * callers must make sure this field is zero or there'll 
 * probably be some odd behaviour...
 */
typedef struct {
    Sleeper *head;     // ..8
    uint64_t reserved; // ..16
    uint64_t always0;  // ..24 [see note above]
    Sleeper *tail;     // ..32
    uint64_t res[4];   // ..64
} SleepQueue;

static_assert_sizeof(Sleeper, ==, 64);
static_assert_sizeof(SleepQueue, ==, 64);

/*
 * Enqueue a single sleeper.
 */
bool sleep_queue_enqueue(SleepQueue *queue, Task *task, uint64_t deadline);

/*
 * Dequeue sleepers given the specified deadline. 
 *
 * This will return NULL, or a linked-list of sleepers
 * that were removed based on their `wake_at` vs
 * the given deadline. 
 */
Task *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline);

#endif //__ANOS_KERNEL_SLEEP_QUEUE_H