/*
 * stage3 - SMP per-CPU state access funcs
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "smp/state.h"
#include "kdrivers/cpu.h"

#ifdef CONSERVATIVE_KERNEL
#include "debugstr.h"
#include "printdec.h"
#endif

#ifndef NULL
#define NULL (((void *)0))
#endif

static PerCPUState *cpu_states[MAX_CPU_COUNT];
static uint8_t cpu_count;

void state_register_cpu(uint8_t cpu_num, PerCPUState *state) {
#ifdef CONSERVATIVE_KERNEL
    if (state == NULL) {
#ifdef CONSERVATIVE_PANICKY
        panic("[BUG] Attempted to register NULL CPU state");
#else
        debugstr("!!! WARN: [BUG] Registering NULL state for CPU #");
        printdec(cpu_num);
        debugstr("; This is unlikely to end well...\n");
#endif
    }

    if (cpu_num >= MAX_CPU_COUNT) {
#ifdef CONSERVATIVE_PANICKY
        panic("[BUG] Attempted to register CPU >= MAX_CPU_COUNT");
#else
        debugstr("!!! WARN: [BUG] Registering CPU #");
        printdec(cpu_num);
        debugstr(" which is above MAX_CPU_COUNT");
        debugstr("; This is unlikely to end well...\n");
#endif
    }

    if (cpu_states[cpu_num] != 0) {
#ifdef CONSERVATIVE_PANICKY
        panic("[BUG] Per-CPU state block reused");
#else
        debugstr("!!! WARN: [BUG] State block for CPU #");
        printdec(cpu_num);
        debugstr(" reused");
        debugstr("; This is unlikely to end well...\n");
#endif
    }
#endif

    cpu_states[cpu_num] = state;
    cpu_count++;
}

uint8_t state_get_cpu_count(void) { return cpu_count; }

PerCPUState *state_get_for_any_cpu(uint8_t cpu_num) {
#ifdef CONSERVATIVE_KERNEL
    if (cpu_num >= cpu_count) {
#ifdef CONSERVATIVE_PANICKY
        panic("[BUG] Attempted to fetch CPU state for CPU >= cpu_count");
#else
        debugstr("!!! WARN: [BUG] Fetching NULL state for CPU #");
        printdec(cpu_num);
        debugstr(" which is >= cpu_count");
        debugstr("; This is unlikely to end well...\n");
#endif
    }
#endif

    return cpu_states[cpu_num];
}
