/*
 * stage3 - Scheduler idle thread
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is the kernel task that runs when there's nothing
 * else to do. Each CPU scheduler has one of these in the 
 * idle priority class.
 * 
 * It cannot exit, sleep, send messages or otherwise block.
 * 
 * The scheduler itself is responsible for setting this up...
 */

#include <stdnoreturn.h>

#include "debugprint.h"

noreturn void sched_idle_thread(void) {
    while (1) {
        __asm__ volatile("hlt");
    }

    __builtin_unreachable();
}
