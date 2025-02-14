/*
 * Mock implementation of fixed block allocator
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_TESTS_TEST_SLAB_H
#define __ANOS_TESTS_TEST_SLAB_H

#include <stdbool.h>
#include <stdint.h>

void mock_slab_reset(void);

void mock_slab_set_should_fail(bool should_fail);

bool mock_slab_get_should_fail(void);

uint64_t mock_slab_get_alloc_count(void);

uint64_t mock_slab_get_free_count(void);

#endif //__ANOS_TESTS_TEST_SLAB_H