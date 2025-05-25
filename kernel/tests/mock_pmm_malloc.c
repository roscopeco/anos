/*
 * Mock implementation of the pmm for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mock_pmm.h"
#include "pmm/pagealloc.h"

// Parts of the kernel extern this in directly, so we need it here...
MemoryRegion physical_region;

static void *pages[MOCK_PMM_MAX_PAGES];
static uint16_t page_ptr = 0;
static uint32_t total_page_allocs = 0;
static uint32_t total_page_frees = 0;

uint32_t mock_pmm_get_total_page_allocs() { return total_page_allocs; }
uint32_t mock_pmm_get_total_page_frees() { return total_page_frees; }

void mock_pmm_reset() {
    while (page_ptr > 0) {
        free(pages[--page_ptr]);
    }

    total_page_allocs = 0;
}

uintptr_t page_alloc(MemoryRegion *region) {
    if (page_ptr == (MOCK_PMM_MAX_PAGES - 1)) {
        fprintf(stderr, "\n\nWARN: Mock page allocator is out of space 😱\n\n");
        return 0xff;
    }

    total_page_allocs++;
    posix_memalign((void **)&pages[page_ptr], 0x1000, 0x1000);
    return (uint64_t)pages[page_ptr++];
}

void page_free(MemoryRegion *region, uintptr_t page) {
    total_page_frees++;

    // don't bother freeing for now...
}
