/*
 * Test interface to mock vmm implementation for tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_TESTS_TEST_VMM_H
#define __ANOS_TESTS_TEST_VMM_H

#include <stdbool.h>
#include <stdint.h>

uint32_t test_vmm_get_total_page_maps();

uint64_t test_vmm_get_last_page_map_pml4();
uint64_t test_vmm_get_last_page_map_paddr();
uint64_t test_vmm_get_last_page_map_vaddr();
uint64_t test_vmm_get_last_page_map_flags();

void test_vmm_reset();

#endif //__ANOS_TESTS_TEST_VMM_H