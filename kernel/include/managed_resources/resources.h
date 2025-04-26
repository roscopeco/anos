/*
 * stage3 - Process resource management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_MANAGED_RESOURCES_H
#define __ANOS_KERNEL_MANAGED_RESOURCES_H

#include <stdint.h>

#include "anos_assert.h"
#include "structs/list.h"

typedef void (*ResourceFreeFunc)(void *, uint64_t data);

typedef struct ManagedResource {
    ListNode this;
    ResourceFreeFunc free_func;
    void *resource_ptr;
    uint64_t free_data;
    uint64_t reserved[4];
} ManagedResource;

void managed_resources_free_all(ManagedResource *head);

static_assert_sizeof(ManagedResource, ==, 64);

#endif //__ANOS_KERNEL_MANAGED_RESOURCES_H
