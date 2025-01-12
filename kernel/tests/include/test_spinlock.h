/*
 * Mock implementation of the spinlocks for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_TESTS_TEST_SPINLOCK_H
#define __ANOS_TESTS_TEST_SPINLOCK_H

void test_spinlock_reset();
uint32_t test_spinlock_get_lock_count();
uint32_t test_spinlock_get_unlock_count();

#endif //__ANOS_TESTS_TEST_SPINLOCK_H