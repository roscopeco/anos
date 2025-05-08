/*
 * stage3 - Process management
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PROCESS_H
#define __ANOS_KERNEL_PROCESS_H

#include "structs/list.h"
#include <stdint.h>

#include "anos_assert.h"
#include "managed_resources/resources.h"

typedef struct Task Task;

#include "process/memory.h"
#include "spinlock.h"

typedef struct {
    ListNode this;        // 8 bytes
    Task *task;           // 16
    uint64_t reserved[6]; // 64
} ProcessTask;

typedef struct Process {
    uint64_t cap_failures;     // 8 bytes
    SpinLock *pages_lock;      // 16
    uint64_t pid;              // 24
    uintptr_t pml4;            // 32
    ProcessPages *pages;       // 40
    ProcessTask *tasks;        // 48
    ManagedResource *res_head; // 56
    ManagedResource *res_tail; // 64
} Process;

static_assert_sizeof(ProcessTask, ==, 64);
static_assert_sizeof(Process, ==, 64);

void process_init(void);

Process *process_create(uintptr_t pml4);

/* 
 * NOTE! Also frees all the process' managed resources.
 */
void process_destroy(Process *process);

bool process_add_managed_resource(Process *process,
                                  ManagedResource *managed_resource);
bool process_remove_managed_resource(Process *process,
                                     ManagedResource *managed_resource);

#endif