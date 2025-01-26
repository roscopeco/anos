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
    Task *task;       // 24
    uint64_t wake_at; // 32
    uint64_t res[4];  // 64
} Sleeper;

typedef struct {
    Sleeper *head;   // 8
    Sleeper *tail;   // 16
    uint64_t res[6]; // 64
} SleepQueue;

static_assert_sizeof(Sleeper, 64);
static_assert_sizeof(SleepQueue, 64);

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
Sleeper *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline);

#endif //__ANOS_KERNEL_SLEEP_QUEUE_H