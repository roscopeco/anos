/*
 * stage3 - LAPIC timer ISR (heartbeat)
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "sched.h"
#include "sleep.h"

static volatile uint32_t counter;

// TODO could probably derive this directly from timer count now...?
volatile uint64_t lapic_timer_upticks;

uint64_t get_kernel_upticks(void) { return lapic_timer_upticks; }

void handle_ap_timer_interrupt(void) {
    kernel_timer_eoe();

    const uint64_t lock_flags = sched_lock_this_cpu();
    check_sleepers();
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);
}

void handle_bsp_timer_interrupt(void) {
    lapic_timer_upticks += 1;

    kernel_timer_eoe();

    const uint64_t lock_flags = sched_lock_this_cpu();
    check_sleepers();
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);
}