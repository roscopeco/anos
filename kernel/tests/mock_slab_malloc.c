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

static bool should_slab_alloc_fail = false;
static uint64_t slab_alloc_count = 0;
static uint64_t slab_free_count = 0;

void mock_slab_reset(void) {
    should_slab_alloc_fail = false;
    slab_alloc_count = 0;
    slab_free_count = 0;
}

void mock_slab_set_should_fail(bool should_fail) { should_slab_alloc_fail = should_fail; }

bool mock_slab_get_should_fail(void) { return should_slab_alloc_fail; }

uint64_t mock_slab_get_alloc_count(void) { return slab_alloc_count; }

uint64_t mock_slab_get_free_count(void) { return slab_free_count; }

void *slab_alloc_block(void) {
    if (should_slab_alloc_fail)
        return NULL;
    slab_alloc_count++;

    // Allocate 64 bytes and ensure 8-byte alignment
    void *ptr = aligned_alloc(8, 64);
    if (ptr) {
        memset(ptr, 0, 64); // Zero the memory
    }
    return ptr;
}

void slab_free(void *ptr) {
    if (!ptr)
        return;
    slab_free_count++;
    free(ptr);
}
