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

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr yet - May 2025
#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr (((void *)0))
#endif
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
        return nullptr;
    }

    ProcessMemoryInfo *meminfo = slab_alloc_block();

    if (!meminfo) {
        slab_free(lock);
        return nullptr;
    }

    Process *process = slab_alloc_block();

    if (!process) {
        slab_free(lock);
        slab_free(meminfo);
        return nullptr;
    }

    process->pid = next_pid++;
    process->pml4 = pml4;

    meminfo->pages = nullptr;
    meminfo->pages_lock = lock;
    meminfo->res_head = meminfo->res_tail = nullptr;
    meminfo->regions = nullptr;

    process->meminfo = meminfo;

    return process;
}

static inline void destroy_process_tasks(Process *process) {
    ProcessTask *process_task = process->tasks;

    while (process_task) {
#ifdef CONSERVATIVE_BUILD
        if (!process_task->task) {
            konservative("[BUG] Destroy NULL Task");
        }
#endif

        task_destroy(process_task->task);
        ProcessTask *next = (ProcessTask *)process_task->this.next;
        slab_free(process_task);

        // updating process->tasks at the same time here in case
        // we panic and want to use what's left of the list...
        //
        process->tasks = process_task = next;
    }
}

void process_destroy(Process *process) {
    destroy_process_tasks(process);
    managed_resources_free_all(process->meminfo->res_head);
    process_release_owned_pages(process);
    region_tree_free_all(&process->meminfo->regions);
    slab_free(process->meminfo->pages_lock);
    slab_free(process->meminfo);
    slab_free(process);
}

bool process_add_managed_resource(Process *process,
                                  ManagedResource *managed_resource) {
    if (!process || !managed_resource) {
        return false;
    }

    managed_resource->this.next = nullptr;

    if (process->meminfo->res_tail) {
        process->meminfo->res_tail->this.next = (ListNode *)managed_resource;
    } else {
        // List was empty
        process->meminfo->res_head = managed_resource;
    }

    process->meminfo->res_tail = managed_resource;

    return true;
}

bool process_remove_managed_resource(Process *process,
                                     ManagedResource *managed_resource) {
    if (!process || !managed_resource) {
        return false;
    }

    ManagedResource *prev = nullptr;
    ManagedResource *curr = process->meminfo->res_head;

    while (curr) {
        if (curr == managed_resource) {
            if (prev) {
                prev->this.next = curr->this.next;
            } else {
                process->meminfo->res_head = (ManagedResource *)curr->this.next;
            }

            if (process->meminfo->res_tail == managed_resource) {
                process->meminfo->res_tail = prev;
            }

            managed_resource->this.next = nullptr; // clean up
            return true;
        }

        prev = curr;
        curr = (ManagedResource *)curr->this.next;
    }

    return false; // not found
}
