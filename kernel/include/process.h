/*
 * stage3 - Process management
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PROCESS_H
#define __ANOS_KERNEL_PROCESS_H

#include <stdint.h>

#include "anos_assert.h"
#include "managed_resources/resources.h"
#include "structs/list.h"
#include "structs/region_tree.h"

typedef struct Task Task;

#include "process/memory.h"
#include "spinlock.h"

typedef struct {
    ListNode this;        // 8 bytes
    Task *task;           // 16
    uint64_t reserved[6]; // 64
} ProcessTask;

typedef struct {
    SpinLock *pages_lock;      // 8
    ProcessPages *pages;       // 16
    ManagedResource *res_head; // 24
    ManagedResource *res_tail; // 32
    Region *regions;           // 40
    uint64_t reserved[3];      // 64
} ProcessMemoryInfo;

typedef struct Process {
    uint64_t cap_failures;      // 8 bytes
    uint64_t pid;               // 16
    uintptr_t pml4;             // 24
    ProcessTask *tasks;         // 32
    ProcessMemoryInfo *meminfo; // 40
    uint64_t reserved[3];       // 64
} Process;

static_assert_sizeof(ProcessTask, ==, 64);
static_assert_sizeof(ProcessMemoryInfo, ==, 64);
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