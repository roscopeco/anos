/*
* Bootstrap process loader
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This serves as the entrypoint for a new process loaded
 * from the RAMFS by SYSTEM.
 */

// clang-format Language: C

#ifndef __ANOS_SYSTEM_LOADER_H
#define __ANOS_SYSTEM_LOADER_H

#include <stdnoreturn.h>

#define VM_PAGE_SIZE ((0x1000))

#define SYS_VFS_TAG_GET_SIZE ((0x1))
#define SYS_VFS_TAG_LOAD_PAGE ((0x2))

#define INIT_STACK_CAP_SIZE_LONGS ((2))
#define INIT_STACK_STATIC_VALUE_COUNT ((4))

#define STACK_TOP ((0x80000000))
#define MIN_STACK_SIZE ((0x40000)) // 256KiB minimum stack
#define MAX_ARG_LENGTH ((256))
#define MAX_ARG_COUNT ((512))

// TODO these need keeping in sync with address_space.h from the kernel!
// We allow up to 33 pages (128KiB) at the top of the stack for initial
// arg values etc.
#define INIT_STACK_ARG_PAGES_COUNT ((33))

#define MAX_STACK_VALUE_COUNT                                                  \
    ((INIT_STACK_ARG_PAGES_COUNT - 1) * VM_PAGE_SIZE / (sizeof(uintptr_t)))

noreturn void initial_server_loader(void);

#endif
