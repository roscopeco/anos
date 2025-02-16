/*
 * stage3 - Syscalls
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SYSCALLS_H
#define __ANOS_KERNEL_SYSCALLS_H

#include <stdint.h>

// This is the vector for slow syscalls (via `int`)
#define SYSCALL_VECTOR 0x69

#define MAX_PROCESS_REGIONS 16

typedef int64_t SyscallArg;

typedef enum {
    SYSCALL_OK = 0,
    SYSCALL_FAILURE = -1,
    SYSCALL_BAD_NUMBER = -2,
    SYSCALL_NOT_IMPL = -3,
    SYSCALL_BADARGS = -4,
} SyscallResult;

// TODO this is duplicated into libanos headers...
typedef struct {
    uint64_t physical_total;
    uint64_t physical_avail;
} AnosMemInfo;

// TODO this is duplicated into libanos headers...
typedef struct {
    uintptr_t start;
    uint64_t len_bytes;
} ProcessMemoryRegion;

// Set things up for fast syscalls (via `sysenter`)
void syscall_init(void);

#endif //__ANOS_KERNEL_SYSCALLS_H
