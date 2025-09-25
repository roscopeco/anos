/*
 * Mock implementation of fixed block allocator that mallocs as needed
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool should_fba_alloc_fail = false;
static uint64_t fba_alloc_count = 0;
static uint64_t fba_free_count = 0;

void mock_fba_reset(void) {
    should_fba_alloc_fail = false;
    fba_alloc_count = 0;
    fba_free_count = 0;
}

void mock_fba_set_should_fail(const bool should_fail) { should_fba_alloc_fail = should_fail; }

bool mock_fba_get_should_fail(void) { return should_fba_alloc_fail; }

uint64_t mock_fba_get_alloc_count(void) { return fba_alloc_count; }

uint64_t mock_fba_get_free_count(void) { return fba_free_count; }

void *fba_alloc_block(void) {
    if (should_fba_alloc_fail) {
        return NULL;
    }

    fba_alloc_count++;

    // Allocate 4096 bytes and ensure it's page-aligned
    void *ptr = aligned_alloc(4096, 4096);
    if (ptr) {
        memset(ptr, 0, 4096);
    }
    return ptr;
}

void *fba_alloc_blocks(const uint64_t count) {
    if (should_fba_alloc_fail) {
        return NULL;
    }

    fba_alloc_count += count;

    // Allocate 4096 bytes and ensure it's page-aligned
    void *ptr = aligned_alloc(4096, 4096 * count);

    if (ptr) {
        memset(ptr, 0, 4096 * count);
    }

    return ptr;
}

void fba_free(void *ptr) {
    if (!ptr)
        return;
    fba_free_count++;
    free(ptr);
}
