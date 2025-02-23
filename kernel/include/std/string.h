/*
 * stage3 - Std routines we need to supply...
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_STD_STRING_H
#define __ANOS_KERNEL_STD_STRING_H

#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
void *memset(void *dest, int val, size_t count);
void *memclr(void *dest, size_t count);

#endif //__ANOS_KERNEL_STD_STRING_H
