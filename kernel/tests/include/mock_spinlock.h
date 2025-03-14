/*
 * Mock implementation of the spinlocks for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_SPINLOCK_H
#define __ANOS_TESTS_TEST_SPINLOCK_H

#include <stdint.h>

void mock_spinlock_reset(void);
bool mock_spinlock_is_locked(void);
uint32_t mock_spinlock_get_lock_count(void);
uint32_t mock_spinlock_get_unlock_count(void);

#endif //__ANOS_TESTS_TEST_SPINLOCK_H