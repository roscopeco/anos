/*
* stage3 - Platform initialisation for riscv64
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "machine.h"
#include "panic.h"

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

bool platform_init(const uintptr_t platform_data) { return true; }