/*
 * stage3 - Managed resource handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "managed_resources/resources.h"
#include "structs/list.h"

void managed_resources_free_all(ManagedResource *head) {
    while (head) {
        head->free_func(head->resource_ptr, head->free_data);
        head = (ManagedResource *)head->this.next;
    }
}
