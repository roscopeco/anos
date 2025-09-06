/*
 * Mock anos/syscalls.h for PCI driver testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SYSCALLS_H
#define __ANOS_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support constexpr yet - Sept 2025
#ifndef constexpr
#define constexpr const
#endif
#ifndef nullptr
#define nullptr NULL
#endif
#endif

// Syscall result type
typedef struct {
    uint64_t result;
    uint64_t value;
} SyscallResult;

// Syscall results
#define SYSCALL_OK 0
#define SYSCALL_ERR_NOT_FOUND -1
#define SYSCALL_FAILURE -1

// Mock syscall function declarations
SyscallResult anos_find_named_channel(const char *name);
SyscallResult anos_send_message(uint64_t channel_id, void *buffer,
                                size_t msg_size, void *msg_buffer);

#endif /* __ANOS_SYSCALLS_H */