/*
 * Shift-to-middle Array for ANOS
 * anos - An Operating System
 * 
 * Copyright (c)2025 Ross Bamford. See LICENSE for details.
 *
 * This is a C port of:
 * 
 *     https://github.com/attilatorda/Shift-To-Middle_Array
 * 
 * which provides a high-performance (amortized O(1) insertion 
 * & deletion at both head & tail, and constant-time index)
 * vector / deque implementation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "std/string.h"
#include "vmm/vmconfig.h"

typedef struct {
    void *data;
    size_t elem_size;
    int head;
    int tail;
    int capacity;
} ShiftToMiddleArray;

static inline void *fba_calloc_blocks(size_t bytes) {
    size_t num_blocks = (bytes + VM_PAGE_SIZE - 1) / VM_PAGE_SIZE;

    void *base = fba_alloc_block();

    if (!base) {
        return NULL;
    }

    memset(base, 0, VM_PAGE_SIZE);

    for (size_t i = 1; i < num_blocks; ++i) {
        void *next = fba_alloc_block();

        if (!next) {
            // Free already-allocated blocks on failure
            for (size_t j = 0; j < i; ++j)
                fba_free((char *)base + j * VM_PAGE_SIZE);
            return NULL;
        }

        memset(next, 0, VM_PAGE_SIZE);
    }

    return base;
}

static inline void *kernel_realloc(void *old_data, size_t old_bytes,
                                   size_t new_bytes) {
    void *new_data = fba_calloc_blocks(new_bytes);

    if (!new_data) {
        return NULL;
    }

    memcpy(new_data, old_data, old_bytes);

    size_t old_blocks = (old_bytes + VM_PAGE_SIZE - 1) / VM_PAGE_SIZE;

    for (size_t i = 0; i < old_blocks; ++i) {
        fba_free((char *)old_data + i * VM_PAGE_SIZE);
    }

    return new_data;
}

bool shift_array_init(ShiftToMiddleArray *arr, size_t elem_size,
                      int initial_capacity) {
    arr->elem_size = elem_size;
    arr->capacity = initial_capacity;
    arr->data = fba_calloc_blocks(elem_size * initial_capacity);

    if (!arr->data) {
        return false;
    }

    arr->head = initial_capacity / 2;
    arr->tail = arr->head;

    return true;
}

void shift_array_free(ShiftToMiddleArray *arr) {
    size_t blocks =
            (arr->elem_size * arr->capacity + VM_PAGE_SIZE - 1) / VM_PAGE_SIZE;

    for (size_t i = 0; i < blocks; ++i) {
        fba_free((char *)arr->data + i * VM_PAGE_SIZE);
    }
}

bool shift_array_resize(ShiftToMiddleArray *arr) {
    int new_capacity = arr->capacity * 2;
    size_t new_bytes = new_capacity * arr->elem_size;

    void *new_data = fba_calloc_blocks(new_bytes);

    if (!new_data) {
        return false;
    }

    int new_head = (new_capacity - (arr->tail - arr->head)) / 2;

    for (int i = arr->head, j = new_head; i < arr->tail; ++i, ++j) {
        void *src = (char *)arr->data + i * arr->elem_size;
        void *dst = (char *)new_data + j * arr->elem_size;
        memcpy(dst, src, arr->elem_size);
    }

    shift_array_free(arr);

    arr->data = new_data;
    arr->tail = new_head + (arr->tail - arr->head);
    arr->head = new_head;
    arr->capacity = new_capacity;

    return true;
}

bool shift_array_insert_head(ShiftToMiddleArray *arr, const void *value) {
    if (arr->head == 0 && !shift_array_resize(arr)) {
        return false;
    }

    arr->head--;
    memcpy((char *)arr->data + arr->head * arr->elem_size, value,
           arr->elem_size);

    return true;
}

bool shift_array_insert_tail(ShiftToMiddleArray *arr, const void *value) {
    if (arr->tail == arr->capacity && !shift_array_resize(arr)) {
        return false;
    }

    memcpy((char *)arr->data + arr->tail * arr->elem_size, value,
           arr->elem_size);
    arr->tail++;

    return true;
}

void shift_array_remove_head(ShiftToMiddleArray *arr) {
    if (arr->head < arr->tail) {
        arr->head++;
    }
}

void shift_array_remove_tail(ShiftToMiddleArray *arr) {
    if (arr->tail > arr->head) {
        arr->tail--;
    }
}

void *shift_array_get_head(const ShiftToMiddleArray *arr) {
    if (arr->head < arr->tail) {
        return (char *)arr->data + arr->head * arr->elem_size;
    }

    return NULL;
}

void *shift_array_get_tail(const ShiftToMiddleArray *arr) {
    if (arr->tail > arr->head) {
        return (char *)arr->data + (arr->tail - 1) * arr->elem_size;
    }

    return NULL;
}

bool shift_array_is_empty(const ShiftToMiddleArray *arr) {
    return arr->head == arr->tail;
}
