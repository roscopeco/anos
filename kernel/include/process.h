/*
 * stage3 - Process management
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PROCESS_H
#define __ANOS_KERNEL_PROCESS_H

#include "structs/list.h"
#include <stdint.h>

typedef struct {
    uint64_t reserved0[2]; // 16 bytes
    uint64_t pid;          // 24
    uintptr_t pml4;        // 32
    uint64_t reserved[4];  // 64
} Process;

#endif