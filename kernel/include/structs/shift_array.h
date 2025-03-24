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

// clang-format Language: C

#ifndef __ANOS_KERNEL_SHIFT_TO_MIDDLE_ARRAY_H
#define __ANOS_KERNEL_SHIFT_TO_MIDDLE_ARRAY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void *data;
    size_t elem_size;
    int head;
    int tail;
    int capacity;
} ShiftToMiddleArray;

bool shift_array_init(ShiftToMiddleArray *arr, size_t elem_size,
                      int initial_capacity);

void shift_array_free(ShiftToMiddleArray *arr);

bool shift_array_resize(ShiftToMiddleArray *arr);

bool shift_array_insert_head(ShiftToMiddleArray *arr, const void *value);

bool shift_array_insert_tail(ShiftToMiddleArray *arr, const void *value);

void shift_array_remove_head(ShiftToMiddleArray *arr);

void shift_array_remove_tail(ShiftToMiddleArray *arr);

void *shift_array_get_head(const ShiftToMiddleArray *arr);

void *shift_array_get_tail(const ShiftToMiddleArray *arr);

bool shift_array_is_empty(const ShiftToMiddleArray *arr);

#endif //__ANOS_KERNEL_SHIFT_TO_MIDDLE_ARRAY_H
