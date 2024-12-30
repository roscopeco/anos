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
    ListNode this;
    uint64_t pid;
} Process;

#endif