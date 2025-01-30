/*
 * stage3 - Spinlocks
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SPINLOCK_H
#define __ANOS_KERNEL_SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "anos_assert.h"

typedef struct {
    uint64_t lock;
    uint64_t fill_cache_line[7];
} SpinLock;

typedef struct {
    uint64_t lock;
    uint64_t ident;
    uint64_t fill_cache_line[6];
} ReentrantSpinLock;

static_assert_sizeof(SpinLock, ==, 64);
static_assert_sizeof(ReentrantSpinLock, ==, 64);

/*
 * Init (zero) a spinlock. Note that this is optional,
 * it doesn't need to be called if the lock will be
 * zeroed automatically (e.g. if it's static).
 */
void spinlock_init(SpinLock *lock);

void spinlock_lock(SpinLock *lock);

void spinlock_unlock(SpinLock *lock);

/*
 * Init (zero) a reentrant spinlock. Note that this
 * is optional, it doesn't need to be called if the
 * lock will be zeroed automatically (e.g. if it's
 * static).
 */
void spinlock_reentrant_init(ReentrantSpinLock *lock);

/*
 * Lock a reentrant lock with the given caller identifier. This will
 * usually be some identifier that's unique to the calling thread...
 *
 * Returns `true` if the lock was successful, false otherwise.
 */
bool spinlock_reentrant_lock(ReentrantSpinLock *lock, uint64_t ident);

/*
 * Unlock a reentrant lock with the given caller identifier.
 *
 * Returns `true` if the unlock was successful, false otherwise.
 */
bool spinlock_reentrant_unlock(ReentrantSpinLock *lock, uint64_t ident);

#endif // __ANOS_KERNEL_SPINLOCK_H