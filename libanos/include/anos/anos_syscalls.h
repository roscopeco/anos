/*
 * Anos system call interface for user-mode code
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_ANOS_SYSCALLS_H
#define __ANOS_ANOS_SYSCALLS_H

#include "anos/anos_system.h"
#include <stdint.h>

#ifdef DEBUG_INT_SYSCALLS
#define kprint kprint_int
#define kputchar kputchar_int
#define anos_create_thread anos_create_thread_int
#else
#define kprint kprint_syscall
#define kputchar kputchar_syscall
#define anos_create_thread anos_create_thread_syscall
#endif

typedef void (*ThreadFunc)(void);

int testcall_int(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                 uint64_t arg4);
int testcall_syscall(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                     uint64_t arg4);

int kprint_int(const char *msg);
int kprint_syscall(const char *msg);

int kputchar_int(char chr);
int kputchar_syscall(char chr);

int anos_create_thread_int(ThreadFunc func, uintptr_t stack_pointer);
int anos_create_thread_syscall(ThreadFunc func, uintptr_t stack_pointer);

#endif //__ANOS_ANOS_SYSCALLS_H
