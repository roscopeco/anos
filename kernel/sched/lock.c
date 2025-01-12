/*
 * stage3 - Scheduler locking
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO this needs making properly safe for SMP...
 */

#include <stdbool.h>
#include <stdint.h>

#include "machine.h"
#include "spinlock.h"
#include "task.h"

static SpinLock lock;
static uint8_t irq_disable_count;

void sched_lock(void) {
    disable_interrupts();

    if (irq_disable_count == 0) {
        spinlock_lock(&lock);
    }

    irq_disable_count++;
}

void sched_unlock() {
    if (irq_disable_count <= 1) {
        irq_disable_count = 0;
        spinlock_unlock(&lock);
        enable_interrupts();
    } else {
        irq_disable_count--;
    }
}