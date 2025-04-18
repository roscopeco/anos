/*
 * stage3 - Spinlocks
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * This header defines the spinlock interface which is implemented
 * for both x86_64 and RISC-V architectures.
 */

// clang-format Language: C

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

/*
 * Lock a spinlock, without touching interrupts.
 *
 * This is only safe outside of interrupt contexts.
 */
void spinlock_lock(SpinLock *lock);

/*
 * Lock a spinlock and disable interrupts.
 *
 * Interrupts are unconditonally disabled once the lock
 * is acquired - they are left in their current state
 * while the lock spins.
 * 
 * The return value is suitable for passing to the 
 * related `spinlock_unlock_irqrestore` routine.
 */
uint64_t spinlock_lock_irqsave(SpinLock *lock);

/*
 * Unlock a spinlock, without touching interrupts.
 *
 * This is only safe outside of interrupt contexts.
 */
void spinlock_unlock(SpinLock *lock);

/*
 * Unlock a spinlock and restore previously-saved
 * interrupt state (e.g. from `spinlock_lock_irqsave`).
 */
void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags);

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