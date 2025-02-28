/*
 * Mock implementation of the spinlocks for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "spinlock.h"

static uint32_t init_count;
static uint32_t lock_count;
static uint32_t unlock_count;

void mock_spinlock_reset() {
    init_count = 0;
    lock_count = 0;
    unlock_count = 0;
}

bool mock_spinlock_is_locked(void) { return lock_count > unlock_count; }

uint32_t mock_spinlock_get_lock_count() { return lock_count; }

uint32_t mock_spinlock_get_unlock_count() { return unlock_count; }

void spinlock_init(SpinLock *lock) { init_count++; }

void spinlock_lock(SpinLock *lock) { ++lock_count; }

void spinlock_unlock(SpinLock *lock) { ++unlock_count; }

uint64_t spinlock_lock_irqsave(SpinLock *lock) {
    ++lock_count;
    return 1234;
}

void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags) {
    ++unlock_count;
}
