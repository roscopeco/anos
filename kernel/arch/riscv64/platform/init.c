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

#ifndef RISCV_LFG_INIT
#include "panic.h"
#endif

#include "riscv64/kdrivers/cpu.h"

// We set this at startup, and once the APs are started up,
// they'll wait for this to go false before they start
// their own system schedulers.
//
// This way, we can ensure the main one is started and
// everything's initialized before we let them start
// theirs...
volatile bool ap_startup_wait;

bool platform_await_init_complete(void) {
#ifndef RISCV_LFG_INIT
    panic("This is as far as we go right now on RISC-V...\n\n             "
          "Quite a lot of the platform is already up, including memory\n       "
          "      "
          "management, tasks/processes & sleep, channels, IPWI and \n          "
          "   "
          "supporting infrastructure, but there's still plenty more that\n     "
          "        "
          "needs to be done!\n\n             "
          "Visit https://github.com/roscopeco/anos to help with the port! "
          ":)\n\n");
#else
    return true;
#endif
}

bool platform_task_init(void) { return true; }

static bool init_this_cpu(uint64_t hart_id) {
    PerCPUState *cpu_state = fba_alloc_block();

    if (!cpu_state) {
        return false;
    }

    memclr(cpu_state, sizeof(PerCPUState));

    cpu_state->self = cpu_state;
    cpu_state->cpu_id = hart_id;
    memcpy(cpu_state->cpu_brand, "Unknown RISC-V", 15);

    cpu_set_sscratch((uint64_t)cpu_state);
    cpu_set_tp((uint64_t)cpu_state);

    state_register_cpu(0, cpu_state);

    return true;
}

bool platform_init(const uintptr_t platform_data) {
    ap_startup_wait = true;
    return init_this_cpu(0);
}