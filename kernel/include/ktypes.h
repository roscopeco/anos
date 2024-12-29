/*
 * stage3 - Kernel type tags (for use in linked lists)
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PROCESS_H
#define __ANOS_KERNEL_PROCESS_H

#include <stdint.h>

typedef enum {
    KTYPE_SLAB_HEADER = 1,
} KType;

#endif //__ANOS_KERNEL_PROCESS_H
