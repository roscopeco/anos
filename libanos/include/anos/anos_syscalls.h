/*
 * Anos system call interface for user-mode code
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_ANOS_SYSCALLS_H
#define __ANOS_ANOS_SYSCALLS_H

#include "anos/anos_system.h"
#include "anos/anos_types.h"

#include <stdint.h>

typedef struct {
    uintptr_t start;
    uint64_t len_bytes;
} ProcessMemoryRegion;

#ifdef DEBUG_INT_SYSCALLS
#define anos_kprint anos_kprint_int
#define anos_kputchar anos_kputchar_int
#define anos_create_thread anos_create_thread_int
#define anos_get_mem_info anos_get_mem_info_int
#define anos_task_sleep_current anos_task_sleep_current_int
#define anos_create_process anos_create_process_int
#else
#define anos_kprint anos_kprint_syscall
#define anos_kputchar anos_kputchar_syscall
#define anos_create_thread anos_create_thread_syscall
#define anos_get_mem_info anos_get_mem_info_syscall
#define anos_task_sleep_current anos_task_sleep_current_syscall
#define anos_create_process anos_create_process_syscall
#endif

typedef void (*ThreadFunc)(void);

int anos_testcall_int(uint64_t arg0, uint64_t arg1, uint64_t arg2,
                      uint64_t arg3, uint64_t arg4);
int anos_testcall_syscall(uint64_t arg0, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4);

int anos_kprint_int(const char *msg);
int anos_kprint_syscall(const char *msg);

int anos_kputchar_int(char chr);
int anos_kputchar_syscall(char chr);

int anos_create_thread_int(ThreadFunc func, uintptr_t stack_pointer);
int anos_create_thread_syscall(ThreadFunc func, uintptr_t stack_pointer);

int anos_get_mem_info_int(AnosMemInfo *meminfo);
int anos_get_mem_info_syscall(AnosMemInfo *meminfo);

int anos_task_sleep_current_syscall(uint64_t ticks);
int anos_task_sleep_current_int(uint64_t ticks);

int anos_create_process_syscall(uintptr_t stack_base, uint64_t stack_size,
                                uint64_t region_count,
                                ProcessMemoryRegion *regions,
                                uintptr_t entry_point);
int anos_create_process_int(uintptr_t stack_base, uint64_t stack_size,
                            uint64_t region_count, ProcessMemoryRegion *regions,
                            uintptr_t entry_point);

#endif //__ANOS_ANOS_SYSCALLS_H
