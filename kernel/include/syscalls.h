/*
 * stage3 - Syscalls
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_SYSCALLS_H
#define __ANOS_KERNEL_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

#include "capabilities.h"

// This is the vector for slow syscalls (via `int`)
#define SYSCALL_VECTOR 0x69

// Maximum number of regions when creating a new process
#define MAX_PROCESS_REGIONS ((16))

typedef int64_t SyscallArg;

typedef enum {
    SYSCALL_OK = 0ULL,
    SYSCALL_FAILURE = -1ULL,
    SYSCALL_BAD_NUMBER = -2ULL,
    SYSCALL_NOT_IMPL = -3ULL,
    SYSCALL_BADARGS = -4ULL,
    SYSCALL_BAD_NAME = -5ULL,

    /* reserved */
    SYSCALL_INCAPABLE = -254ULL
} SyscallResult;

typedef struct {
    uint64_t physical_total;
    uint64_t physical_avail;
} AnosMemInfo;

typedef struct {
    uintptr_t start;
    uint64_t len_bytes;
} __attribute__((packed)) ProcessMemoryRegion;

typedef void(ProcessEntrypoint)(void);

typedef struct {
    ProcessEntrypoint *entry_point; // 8
    uintptr_t stack_base;           // 16
    size_t stack_size;              // 24
    uint8_t region_count;           // 25
    uint8_t reserved0[7];           // 32
    ProcessMemoryRegion *regions;   // 40
    uint16_t stack_value_count;     // 42
    uint16_t reserved1[3];          // 48
    uint64_t *stack_values;         // 56
    uint64_t reserved;              // 64
} __attribute__((packed)) ProcessCreateParams;

static_assert_sizeof(ProcessCreateParams, ==, 64);

typedef SyscallResult (*SyscallHandler)(SyscallArg, SyscallArg, SyscallArg,
                                        SyscallArg, SyscallArg);

typedef enum {
    SYSCALL_ID_INVALID = 0,
    SYSCALL_ID_DEBUG_PRINT,
    SYSCALL_ID_DEBUG_CHAR,
    SYSCALL_ID_CREATE_THREAD,
    SYSCALL_ID_MEMSTATS,
    SYSCALL_ID_SLEEP,
    SYSCALL_ID_CREATE_PROCESS,
    SYSCALL_ID_MAP_VIRTUAL,
    SYSCALL_ID_SEND_MESSAGE,
    SYSCALL_ID_RECV_MESSAGE,
    SYSCALL_ID_REPLY_MESSAGE,
    SYSCALL_ID_CREATE_CHANNEL,
    SYSCALL_ID_DESTROY_CHANNEL,
    SYSCALL_ID_REGISTER_NAMED_CHANNEL,
    SYSCALL_ID_DEREGISTER_NAMED_CHANNEL,
    SYSCALL_ID_FIND_NAMED_CHANNEL,
    SYSCALL_ID_KILL_CURRENT_TASK,
    SYSCALL_ID_UNMAP_VIRTUAL,
    SYSCALL_ID_CREATE_REGION,
    SYSCALL_ID_DESTROY_REGION,

    // sentinel
    SYSCALL_ID_END,
} __attribute__((packed)) SyscallId;

typedef struct {
    Capability this;
    SyscallId syscall_id;
    uint32_t flags;
    SyscallHandler handler;
    uint64_t reserved[6];
} SyscallCapability;

static_assert_sizeof(SyscallCapability, ==, 64);

#define VALID_SYSCALL_ID(id)                                                   \
    (((id > SYSCALL_ID_INVALID) && (id < SYSCALL_ID_COUNT)))

// Set things up for fast syscalls (via `sysenter`)
void syscall_init(void);

// Init syscall capabilities and stack them for SYSTEM
uint64_t *syscall_init_capabilities(uint64_t *stack);

#endif //__ANOS_KERNEL_SYSCALLS_H
