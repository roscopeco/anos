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

typedef struct ManagedResource ManagedResource;

/*
 * Function called to free ManagedResources.
 *
 * NOTE: This is responsible for freeing the ManagedResource struct
 * itself, as well as the managed resource it references.
 */
typedef void (*ResourceFreeFunc)(ManagedResource *resource);

typedef struct ManagedResource {
    ListNode this;
    ResourceFreeFunc free_func;
    void *resource_ptr;
    uintptr_t data[5];
} ManagedResource;

void managed_resources_free_all(ManagedResource *head);

static_assert_sizeof(ManagedResource, ==, 64);

#endif //__ANOS_KERNEL_MANAGED_RESOURCES_H
