/*
 * stage3 - Syscalls
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SYSCALLS_H
#define __ANOS_KERNEL_SYSCALLS_H

// This is the vector for slow syscalls (via `int`)
#define SYSCALL_VECTOR 0x69

#define SYSCALL_OK 0x00
#define SYSCALL_BAD_NUMBER 0x01

// Set things up for fast syscalls (via `sysenter`)
void syscall_init(void);

#endif //__ANOS_KERNEL_SYSCALLS_H
