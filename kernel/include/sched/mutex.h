/*
 * stage3 - Scheduler-backed mutexes
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_SCHED_MUTEX_H
#define __ANOS_KERNEL_SCHED_MUTEX_H

#include <stdbool.h>

#include "anos_assert.h"
#include "spinlock.h"
#include "structs/pq.h"
#include "task.h"

typedef struct {
    const Task *owner;
    SpinLock *spin_lock;
    TaskPriorityQueue *wait_queue;
    bool locked;
    uint64_t reserved[4];
} Mutex;

static_assert_sizeof(Mutex, ==, 64);

Mutex *mutex_create(void);
bool mutex_free(Mutex *mutex);

bool mutex_init(Mutex *mutex, SpinLock *spin_lock,
                TaskPriorityQueue *wait_queue);

bool mutex_lock(Mutex *mutex);

bool mutex_unlock(Mutex *mutex);

#endif