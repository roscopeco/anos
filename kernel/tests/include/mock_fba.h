/*
 * Mock implementation of fixed block allocator
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_FBA_H
#define __ANOS_TESTS_TEST_FBA_H

#include <stdint.h>

void mock_fba_reset(void);

void mock_fba_set_should_fail(bool should_fail);

bool mock_fba_get_should_fail(void);

uint64_t mock_fba_get_alloc_count(void);

uint64_t mock_fba_get_free_count(void);

#endif //__ANOS_TESTS_TEST_FBA_H