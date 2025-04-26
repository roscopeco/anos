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
        // free_func is responsible for freeing the struct,
        // so stash this away first...
        ManagedResource *next = (ManagedResource *)head->this.next;

        head->free_func(head);
        head = next;
    }
}
