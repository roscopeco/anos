/*
* stage3 - Platform initialisation for riscv64
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "smp/state.h"
#include "std/string.h"

#include "panic.h"

#include "riscv64/kdrivers/cpu.h"
#include "riscv64/kdrivers/sbi.h"

// We set this at startup, and once the APs are started up,
// they'll wait for this to go false before they start
// their own system schedulers.
//
// This way, we can ensure the main one is started and
// everything's initialized before we let them start
// theirs...
volatile bool ap_startup_wait;

bool platform_await_init_complete(void) { return true; }

bool platform_task_init(void) { return true; }

static inline void enable_timer_interrupts(void) {
    uint64_t sie;
    __asm__ volatile("csrr %0, sie" : "=r"(sie));
    sie |= (1 << 5); // STIE
    __asm__ volatile("csrw sie, %0" ::"r"(sie));
}

static bool init_this_cpu(uint64_t hart_id) {
    PerCPUState *cpu_state = fba_alloc_block();

    if (!cpu_state) {
        return false;
    }

    memclr(cpu_state, sizeof(PerCPUState));

    cpu_state->self = cpu_state;
    cpu_state->cpu_id = hart_id;
    memcpy(cpu_state->cpu_brand, "Unknown RISC-V", 15);

    // We set this to 0 at startup. This allows us to handle
    // stack switches on kernel entry/exit in a sane way.
    // See docs/RISC-V-Specifics.md for details.
    cpu_set_sscratch(0);
    cpu_set_tp((uint64_t)cpu_state);

    state_register_cpu(0, cpu_state);

    sbi_set_timer(cpu_read_rdtime());
    enable_timer_interrupts();

    return true;
}

bool platform_init(const uintptr_t platform_data) {
    ap_startup_wait = true;
    return init_this_cpu(0);
}