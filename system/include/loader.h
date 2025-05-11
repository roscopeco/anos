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

#define INIT_STACK_CAP_SIZE_LONGS (2)
#define INIT_STACK_STATIC_VALUE_COUNT (4)

noreturn void initial_server_loader(void *initial_sp);

#endif
