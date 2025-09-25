/*
 * Mock anos/syscalls.h for DEVMAN testing
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

typedef struct {
    uint64_t result;
    uintptr_t value;
} SyscallResultA;

// Syscall results
#define SYSCALL_OK 0
#define SYSCALL_ERR_NOT_FOUND -1
#define SYSCALL_FAILURE -1

// Memory mapping flags
#define ANOS_MAP_PHYSICAL_FLAG_READ 0x1
#define ANOS_MAP_PHYSICAL_FLAG_WRITE 0x2
#define ANOS_MAP_PHYSICAL_FLAG_EXEC 0x4
#define ANOS_MAP_PHYSICAL_FLAG_NOCACHE 0x8

#define ANOS_MAP_VIRTUAL_FLAG_READ 0x1
#define ANOS_MAP_VIRTUAL_FLAG_WRITE 0x2
#define ANOS_MAP_VIRTUAL_FLAG_EXEC 0x4
#define ANOS_MAP_VIRTUAL_FLAG_NOCACHE 0x8

// Mock syscall function declarations
SyscallResult anos_create_channel(void);
SyscallResult anos_register_named_channel(const char *name, uint64_t channel_id);
SyscallResult anos_find_named_channel(const char *name);
SyscallResult anos_send_message(uint64_t channel_id, void *buffer, size_t msg_size, void *msg_buffer);
SyscallResult anos_recv_message(uint64_t channel_id, void *buffer, size_t buffer_size, size_t *actual_size);
SyscallResult anos_map_physical(uint64_t physical_addr, void *virtual_addr, size_t size, uint32_t flags);
SyscallResultA anos_alloc_physical_pages(size_t size);
SyscallResult anos_unmap_virtual(uint64_t virtual_addr, size_t size);
SyscallResult anos_task_sleep_current(uint32_t ms);
SyscallResult anos_kprint(const char *message);

#endif /* __ANOS_SYSCALLS_H */