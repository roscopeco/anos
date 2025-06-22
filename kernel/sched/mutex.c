/*
 * stage3 - Scheduler-backed mutexes
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "task.h"

#include "sched/mutex.h"

#include "sched.h"
#include "slab/alloc.h"
#include "spinlock.h"
#include "structs/pq.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr yet - May 2025
#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr (((void *)0))
#endif
#endif
#endif

Mutex *mutex_create(void) {
    Mutex *mutex = slab_alloc_block();

    if (!mutex) {
        return nullptr;
    }

    SpinLock *spin_lock = slab_alloc_block();

    if (!spin_lock) {
        slab_free(mutex);
        return nullptr;
    }

    TaskPriorityQueue *wait_queue = slab_alloc_block();

    if (!wait_queue) {
        slab_free(spin_lock);
        slab_free(mutex);
        return nullptr;
    }

    if (!mutex_init(mutex, spin_lock, wait_queue)) {
        slab_free(wait_queue);
        slab_free(spin_lock);
        slab_free(mutex);
        return nullptr;
    }

    return mutex;
}

bool mutex_free(Mutex *mutex) {
    if (!mutex) {
        return false;
    }

    if (mutex->locked) {
        return false;
    }

    if (mutex->spin_lock) {
        slab_free(mutex->spin_lock);
    }

    if (mutex->wait_queue) {
        slab_free(mutex->wait_queue);
    }

    slab_free(mutex);

    return true;
}

bool mutex_init(Mutex *mutex, SpinLock *spin_lock,
                TaskPriorityQueue *wait_queue) {
    if (!mutex) {
        return false;
    }

    if (!spin_lock) {
        return false;
    }

    if (!wait_queue) {
        return false;
    }

    spinlock_init(spin_lock);
    task_pq_init(wait_queue);

    mutex->owner = nullptr;
    mutex->spin_lock = spin_lock;
    mutex->wait_queue = wait_queue;
    mutex->locked = false;

    return true;
}

bool mutex_lock(Mutex *mutex) {
    if (!mutex) {
        return false;
    }

    Task *task = task_current();

    if (!task) {
        return false;
    }

    while (true) {
        const uintptr_t lock_flags = spinlock_lock_irqsave(mutex->spin_lock);

        if (mutex->locked == false) {
            // we can lock
            mutex->owner = task;
            mutex->locked = true;

            spinlock_unlock_irqrestore(mutex->spin_lock, lock_flags);
            return true;
        }

        // Here, mutex must be locked...
        if (mutex->owner == task) {
            // mutex is reentrant...

            spinlock_unlock_irqrestore(mutex->spin_lock, lock_flags);
            return true;
        }

        // We need to queue...
        sched_lock_this_cpu();
        task_pq_push(mutex->wait_queue, task);
        spinlock_unlock(mutex->spin_lock);
        sched_block(task_current());
        sched_schedule();
        sched_unlock_this_cpu(lock_flags);
    }
}

#define CHATGPT

#ifdef CHATGPT
bool mutex_unlock(Mutex *mutex) {
    if (!mutex) {
        return false;
    }

    Task *task = task_current();
    if (!task || mutex->owner != task) {
        return false;
    }

    const uintptr_t lock_flags = spinlock_lock_irqsave(mutex->spin_lock);

    Task *next = task_pq_pop(mutex->wait_queue);
    if (!next) {
        mutex->locked = false;
        mutex->owner = NULL;
        spinlock_unlock_irqrestore(mutex->spin_lock, lock_flags);
        return true;
    }

    mutex->owner = next;

    sched_lock_this_cpu();
    spinlock_unlock(mutex->spin_lock);
    sched_unblock(next);
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);
    return true;
}
#else
bool mutex_unlock(Mutex *mutex) {
    if (!mutex) {
        return false;
    }

    Task *task = task_current();

    if (!task || mutex->owner != task) {
        return false;
    }

    const uintptr_t lock_flags = spinlock_lock_irqsave(mutex->spin_lock);

    Task *next = task_pq_pop(mutex->wait_queue);

    if (!next) {
        mutex->locked = false;
        mutex->owner = nullptr;
        spinlock_unlock_irqrestore(mutex->spin_lock, lock_flags);
        return true;
    } else {
        sched_lock_this_cpu();
        task_pq_push(mutex->wait_queue, task);
        spinlock_unlock(mutex->spin_lock);
        sched_block(task_current());
        sched_schedule();
        sched_unlock_this_cpu(lock_flags);
        return true;
    }
}
#endif