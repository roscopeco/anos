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

#ifdef WITH_KERNEL_HEART
// TODO Obviously doesn't belong here, just a hack for proof of life...
#define VRAM_VIRTUAL_HEART 0xffffffff800b809e
static bool heart_state = false;
#endif

static volatile uint32_t counter;

// TODO could probably derive this directly from timer count now...?
volatile uint64_t lapic_timer_upticks;

uint64_t get_kernel_upticks(void) { return lapic_timer_upticks; }

void handle_ap_timer_interrupt(void) {
    kernel_timer_eoe();

    uint64_t lock_flags = sched_lock_this_cpu();
    check_sleepers();
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);
}

void handle_bsp_timer_interrupt(void) {
    lapic_timer_upticks += 1;

#ifdef WITH_KERNEL_HEART
    if (counter++ == (KERNEL_HZ / 2)) /* one full cycle / sec */ {
        counter = 0;

        uint8_t *vram = (uint8_t *)VRAM_VIRTUAL_HEART;

        vram[0] = 0x03; // heart

        if (heart_state) {
            vram[1] = 0x0C; // red
        } else {
            vram[1] = 0x08; // "light black"
        }

        heart_state = !heart_state;
    }
#endif

    kernel_timer_eoe();

    uint64_t lock_flags = sched_lock_this_cpu();
    check_sleepers();
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);
}