/*
 * stage3 - Debug printing for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DEBUGPRINT_H
#define __ANOS_KERNEL_DEBUGPRINT_H

#include <stdint.h>

void debugchar(char chr);
void debugstr(char *str);
void debugattr(uint8_t new_attr);

#endif//__ANOS_KERNEL_DEBUGPRINT_H
