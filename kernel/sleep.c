/*
 * stage3 - Task sleeping - initial experiment
 * anos - An Operating System
 * 
 * This is just experimental code, it won't be sticking
 * around in this form...
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "config.h"
#include "sched.h"
#include "sleep_queue.h"
#include "structs/list.h"
#include "task.h"

#ifdef DEBUG_SLEEP
#include "debugprint.h"
#include "printhex.h"
#endif

#define NULL (((void *)0))

static SleepQueue sleep_queue;

static uint64_t wake_tick;

uint64_t get_lapic_timer_upticks(void);

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t nanos) {
    if (task != NULL) {
        wake_tick = get_lapic_timer_upticks() + (nanos / NANOS_PER_TICK);
        sleep_queue_enqueue(&sleep_queue, task, wake_tick);

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

    Task *waker = sleep_queue_dequeue(&sleep_queue, get_lapic_timer_upticks());

    while (waker) {
        Task *next = (Task *)waker->this.next;
        sched_unblock(waker);
        waker = next;
    }
}