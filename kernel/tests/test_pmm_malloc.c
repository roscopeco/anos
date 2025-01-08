/*
 * Mock implementation of the pmm for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pmm/pagealloc.h"

#define MAX_PAGES 256

// Parts of the kernel extern this in directly, so we need it here...
MemoryRegion physical_region;

static uint64_t *pages[MAX_PAGES];
static uint8_t page_ptr = 0;
static uint32_t total_page_allocs = 0;
static uint32_t total_page_frees = 0;

uint32_t test_pmm_get_total_page_allocs() { return total_page_allocs; }

void test_pmm_reset() {
    while (page_ptr > 0) {
        free(pages[--page_ptr]);
    }

    total_page_allocs = 0;
}

uint64_t page_alloc(MemoryRegion *region) {
    if (page_ptr == (MAX_PAGES - 1)) {
        fprintf(stderr, "\n\nWARN: Mock page allocator is out of space 😱\n\n");
        return 0;
    }

    total_page_allocs++;
    posix_memalign((void **)&pages[page_ptr], 0x1000, 0x1000);
    return (uint64_t)pages[page_ptr++];
}

void page_free(MemoryRegion *region, uint64_t page) {
    total_page_frees++;

    // don't bother freeing for now...
}
