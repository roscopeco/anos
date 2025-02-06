/*
 * stage3 - User-mode supervisor startup
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SYSTEM_H
#define __ANOS_KERNEL_SYSTEM_H

#include <stdint.h>
#include <stdnoreturn.h>

void prepare_system(void);

noreturn void start_system(void);
noreturn void start_system_ap(uint8_t cpu_id);

#endif //__ANOS_KERNEL_SYSTEM_H