/*
 * Test interface to mock pmm implementation for tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_PMM_H
#define __ANOS_TESTS_TEST_PMM_H

#include <stdbool.h>
#include <stdint.h>

#define TEST_PMM_NOALLOC_START_ADDRESS ((0x1000))
#define MOCK_PMM_MAX_PAGES 10240

void mock_pmm_reset();

uint32_t mock_pmm_get_total_page_allocs();
uint32_t mock_pmm_get_total_page_frees();

#endif //__ANOS_TESTS_TEST_PMM_H