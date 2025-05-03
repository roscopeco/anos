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

#include "process/memory.h"
#include "spinlock.h"

typedef struct Process {
    uint64_t reserved0[2]; // 16 bytes
    uint64_t pid;          // 24
    uintptr_t pml4;        // 32
    SpinLock *pages_lock;  // 40
    ProcessPages *pages;   // 48
    uint64_t reserved[2];  // 64
} Process;

static_assert_sizeof(Process, ==, 64);

void process_init(void);

Process *process_create(uintptr_t pml4);

void process_destroy(Process *process);

#endif