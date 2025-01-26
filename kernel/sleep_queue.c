/*
 * stage3 - Sleeping task queue
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 */

#include "sleep_queue.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

#include "slab/alloc.h"

bool sleep_queue_enqueue(SleepQueue *queue, Task *task, uint64_t deadline) {
    if (queue == NULL || task == NULL) {
        return false;
    }

    Sleeper *sleeper = slab_alloc_block();

    if (sleeper == NULL) {
        // OOM...?
        return false;
    }

    sleeper->task = task;
    sleeper->wake_at = deadline;

    queue->head = sleeper;
    queue->tail = sleeper;

    return true;
}

Sleeper *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline) {
    return NULL;
}