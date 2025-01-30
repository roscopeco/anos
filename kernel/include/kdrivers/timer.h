/*
 * stage3 - Abstract timer interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DRIVERS_TIMER_H
#define __ANOS_KERNEL_DRIVERS_TIMER_H

#include <stdint.h>

#include "anos_assert.h"

typedef uint64_t (*KernelTimerNanosPerTickFunc)(void);
typedef uint64_t (*KernelCurrentTicksFunc)(void);

typedef struct {
    KernelCurrentTicksFunc current_ticks;
    KernelTimerNanosPerTickFunc nanos_per_tick;
    uint64_t reserved[6];
} KernelTimer;

static_assert_sizeof(KernelTimer, ==, 64);

#endif //__ANOS_KERNEL_DRIVERS_TIMER_H
