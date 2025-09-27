/*
 * Mock anos/syscalls.h for SYSTEM testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SYSCALLS_H
#define __ANOS_SYSCALLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support constexpr yet - Sept 2025
#ifndef constexpr
#define constexpr const
#endif
#ifndef nullptr
#define nullptr NULL
#endif
#endif

// Syscall result types
typedef struct {
    int64_t result;
    uint64_t value;
} SyscallResult;

typedef struct {
    int64_t result;
    void *value;
} SyscallResultP;

// Syscall results
#define SYSCALL_OK 0
#define SYSCALL_FAILURE -1

// Memory mapping flags
#define ANOS_MAP_VIRTUAL_FLAG_READ 0x1
#define ANOS_MAP_VIRTUAL_FLAG_WRITE 0x2
#define ANOS_MAP_VIRTUAL_FLAG_EXEC 0x4

// VFS tags (avoid redefinition with loader.h)
#ifndef SYS_VFS_TAG_GET_SIZE
#define SYS_VFS_TAG_GET_SIZE 1
#endif
#ifndef SYS_VFS_TAG_LOAD_PAGE
#define SYS_VFS_TAG_LOAD_PAGE 2
#endif

// IPC buffer size
#define MAX_IPC_BUFFER_SIZE 4096

// Syscall IDs for capabilities
#define SYSCALL_ID_END 26

// Mock syscall function declarations (implemented in test file)
SyscallResult anos_find_named_channel(const char *name);
SyscallResultP anos_map_virtual(size_t size, uintptr_t addr, int flags);
SyscallResult anos_send_message(uint64_t cookie, uint64_t tag, size_t size, void *buffer);
SyscallResult anos_unmap_virtual(size_t size, uintptr_t addr);

#endif /* __ANOS_SYSCALLS_H */