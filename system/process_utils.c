/*
 * Process utility functions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stddef.h>
#include <stdint.h>

#define VM_PAGE_SIZE ((0x1000))

unsigned int round_up_to_page_size(const size_t size) { return (size + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1); }

unsigned int round_up_to_machine_word_size(const size_t size) {
    return (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
}