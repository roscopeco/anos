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

void sleep_queue_enqueue(SleepQueue *queue, Sleeper *sleeper,
                         uint64_t deadline) {}

Sleeper *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline) {
    return NULL;
}