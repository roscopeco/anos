/*
 * Anos system call interface for user-mode code
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_ANOS_SYSCALLS_H
#define __ANOS_ANOS_SYSCALLS_H

#include <stdint.h>

#include "anos/system.h"
#include "anos/types.h"

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
#define anos_map_virtual anos_map_virtual_int
#define anos_send_message anos_send_message_int
#define anos_recv_message anos_recv_message_int
#define anos_reply_message anos_reply_message_int
#define anos_create_channel anos_create_channel_int
#define anos_destroy_channel anos_destroy_channel_int
#else
#define anos_kprint anos_kprint_syscall
#define anos_kputchar anos_kputchar_syscall
#define anos_create_thread anos_create_thread_syscall
#define anos_get_mem_info anos_get_mem_info_syscall
#define anos_task_sleep_current anos_task_sleep_current_syscall
#define anos_create_process anos_create_process_syscall
#define anos_map_virtual anos_map_virtual_syscall
#define anos_send_message anos_send_message_syscall
#define anos_recv_message anos_recv_message_syscall
#define anos_reply_message anos_reply_message_syscall
#define anos_create_channel anos_create_channel_syscall
#define anos_destroy_channel anos_destroy_channel_syscall
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

void *anos_map_virtual_syscall(uint64_t size, uintptr_t base_address);
void *anos_map_virtual_int(uint64_t size, uintptr_t base_address);

uint64_t anos_send_message_syscall(uint64_t channel_cookie, uint64_t tag,
                                   uint64_t arg);
uint64_t anos_send_message_int(uint64_t channel_cookie, uint64_t tag,
                               uint64_t arg);

uint64_t anos_recv_message_syscall(uint64_t channel_cookie, uint64_t *tag,
                                   uint64_t *arg);
uint64_t anos_recv_message_int(uint64_t channel_cookie, uint64_t *tag,
                               uint64_t *arg);

uint64_t anos_reply_message_syscall(uint64_t message_cookie, uint64_t reply);
uint64_t anos_reply_message_int(uint64_t message_cookie, uint64_t reply);

uint64_t anos_create_channel_syscall(void);
uint64_t anos_create_channel_int(void);

int anos_destroy_channel_syscall(uint64_t cookie);
int anos_destroy_channel_int(uint64_t cookie);

#endif //__ANOS_ANOS_SYSCALLS_H
