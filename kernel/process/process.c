/*
 * stage3 - Process management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "process.h"
#include "process/memory.h"
#include "slab/alloc.h"
#include "spinlock.h"
#include "task.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

#ifdef UNIT_TESTS
#define STATIC_EXCEPT_TESTS
#else
#define STATIC_EXCEPT_TESTS static
#endif

#ifdef CONSERVATIVE_BUILD
#ifdef CONSERVATIVE_PANICKY
#include "panic.h"
#define konservative panic
#else
#include "kprintf.h"
#define konservative kprintf
#endif
#endif

STATIC_EXCEPT_TESTS _Atomic volatile uint64_t next_pid;

void process_init(void) { next_pid = 1; }

Process *process_create(uintptr_t pml4) {
#ifdef CONSERVATIVE_BUILD
    if (pml4 == 0) {
        return NULL;
    }
#endif

    SpinLock *lock = slab_alloc_block();

    if (!lock) {
        return NULL;
    }

    Process *process = slab_alloc_block();

    if (!process) {
        slab_free(lock);
        return NULL;
    }

    process->pid = next_pid++;
    process->pml4 = pml4;
    process->pages_lock = lock;
    process->res_head = process->res_tail = NULL;

    return process;
}

static inline void destroy_process_tasks(Process *process) {
    ProcessTask *task = process->tasks;

    while (task) {
#ifdef CONSERVATIVE_BUILD
        if (!task->task) {
            konservative("[BUG] Destroy NULL Task");
        }
#endif

        task_destroy(task->task);
        ProcessTask *next = (ProcessTask *)task->this.next;
        slab_free(task);
        task = next;
    }
}

void process_destroy(Process *process) {
    destroy_process_tasks(process);
    managed_resources_free_all(process->res_head);
    process_release_owned_pages(process);
    slab_free(process->pages_lock);
    slab_free(process);
}

bool process_add_managed_resource(Process *process,
                                  ManagedResource *managed_resource) {
    if (!process || !managed_resource) {
        return false;
    }

    managed_resource->this.next = NULL;

    if (process->res_tail) {
        process->res_tail->this.next = (ListNode *)managed_resource;
    } else {
        // List was empty
        process->res_head = managed_resource;
    }

    process->res_tail = managed_resource;

    return true;
}

bool process_remove_managed_resource(Process *process,
                                     ManagedResource *managed_resource) {
    if (!process || !managed_resource) {
        return false;
    }

    ManagedResource *prev = NULL;
    ManagedResource *curr = process->res_head;

    while (curr) {
        if (curr == managed_resource) {
            if (prev) {
                prev->this.next = curr->this.next;
            } else {
                process->res_head = (ManagedResource *)curr->this.next;
            }

            if (process->res_tail == managed_resource) {
                process->res_tail = prev;
            }

            managed_resource->this.next = NULL; // clean up
            return true;
        }

        prev = curr;
        curr = (ManagedResource *)curr->this.next;
    }

    return false; // not found
}
