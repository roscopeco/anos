/*
 * stage3 - Abstract timer interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_DRIVERS_TIMER_H
#define __ANOS_KERNEL_DRIVERS_TIMER_H

#include <stdint.h>

#include "anos_assert.h"

typedef uint64_t (*KernelTimerNanosPerTickFunc)(void);
typedef uint64_t (*KernelCurrentTicksFunc)(void);
typedef void (*KernelTimerBusyDelayNanosFunc)(uint64_t);

typedef struct {
    KernelCurrentTicksFunc current_ticks;
    KernelTimerNanosPerTickFunc nanos_per_tick;
    KernelTimerBusyDelayNanosFunc delay_nanos;
    uint64_t reserved[5];
} KernelTimer;

static_assert_sizeof(KernelTimer, ==, 64);

#endif //__ANOS_KERNEL_DRIVERS_TIMER_H
