/*
 * stage3 - Kernel type tags (for use in linked lists)
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_KTYPES_H
#define __ANOS_KERNEL_KTYPES_H

#include <stdint.h>

typedef enum {
    KTYPE_SLAB_HEADER = 1,
    KTYPE_TASK,
    KTYPE_PROCESS,
    KTYPE_SLEEPER,
    KTYPE_IPC_MESSAGE,
} KType;

#endif //__ANOS_KERNEL_KTYPES_H
