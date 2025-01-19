/*
 * stage3 - Task sleeping - initial experiment
 * anos - An Operating System
 * 
 * This is just experimental code, it won't be sticking
 * around in this form...
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "sched.h"
#include "structs/list.h"
#include "task.h"
#include <stdint.h>

#ifdef DEBUG_SLEEP
#include "debugprint.h"
#include "printhex.h"
#endif

#define NULL (((void *)0))

static Task *sleeper;
static uint64_t wake_tick;

uint64_t get_lapic_timer_upticks(void);

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t ticks) {
    if (sleeper == NULL) {
        // only one sleeper at a time right now...
        sleeper = task;
        wake_tick = get_lapic_timer_upticks() + ticks;

#ifdef DEBUG_SLEEP
        debugstr("Ticks now is ");
        printhex64(get_lapic_timer_upticks(), debugchar);
        debugstr(" - Waking at ");
        printhex64(wake_tick, debugchar);
        debugstr("\n");
#endif
        sched_block(task);
        sched_schedule();
    }
}

/* Caller MUST lock the scheduler! */
void check_sleepers() {
#ifdef DEBUG_SLEEP
    if (get_lapic_timer_upticks() % 100 == 0) {
        debugstr("Ticks now is ");
        printhex64(get_lapic_timer_upticks(), debugchar);
        debugstr(" - Next wakeup at ");
        printhex64(wake_tick, debugchar);
        debugstr("\n");
    }
#endif

    if (sleeper && get_lapic_timer_upticks() >= wake_tick) {
#ifdef DEBUG_SLEEP
        debugstr("Waking sleeper!\n");
#endif
        sched_unblock(sleeper);
        sleeper = NULL;
    }
}