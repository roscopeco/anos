/*
 * Mock implementation of the pmm for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdlib.h>

#include "pmm/pagealloc.h"

#define MAX_PAGES 256

// Parts of the kernel extern this in directly, so we need it here...
MemoryRegion physical_region;

static uint32_t total_page_allocs = 0;
static uint64_t next_page_addr = 0x1000;

uint32_t test_pmm_get_total_page_allocs() { return total_page_allocs; }

void test_pmm_reset() {
    total_page_allocs = 0;
    next_page_addr = 0x1000;
}

uint64_t page_alloc(MemoryRegion *region) {
    total_page_allocs++;
    uint64_t result = next_page_addr;
    next_page_addr += 0x1000;
    return result;
}
