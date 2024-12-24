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

typedef int64_t SyscallArg;

typedef enum {
    SYSCALL_OK = 0,
    SYSCALL_BAD_NUMBER = -1,
} SyscallResult;

// Set things up for fast syscalls (via `sysenter`)
void syscall_init(void);

#endif //__ANOS_KERNEL_SYSCALLS_H
