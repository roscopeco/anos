/*
* stage3 - Inter-processor work-item (IPWI) execution
* anos - An Operating System
*
* Copyright (c) 2025 Ross Bamford
*/

#include <stdbool.h>

#include "spinlock.h"

#include "smp/ipwi.h"

#include "smp/state.h"
#include "std/string.h"

void arch_ipwi_notify_all_except_current(void);

bool ipwi_init(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();

    if (!cpu_state) {
        return false;
    }

    spinlock_init(&cpu_state->ipwi_queue_lock_this_cpu);

    if (!shift_array_init(&cpu_state->ipwi_queue, sizeof(IpwiWorkItem), 16)) {
        return false;
    }

    return true;
}

bool ipwi_enqueue(const IpwiWorkItem *item, const uint8_t cpu_num) {
    if (cpu_num > state_get_cpu_count()) {
        return false;
    }

    PerCPUState *target_state = state_get_for_any_cpu(cpu_num);

    if (!target_state) {
        return false;
    }

    spinlock_lock(&target_state->ipwi_queue_lock_this_cpu);
    shift_array_insert_tail(&target_state->ipwi_queue, item);
    spinlock_unlock(&target_state->ipwi_queue_lock_this_cpu);

    return true;
}

bool ipwi_enqueue_all_except_current(IpwiWorkItem *item) {
    const PerCPUState *this_state = state_get_for_this_cpu();

    for (int i = 0; i < state_get_cpu_count(); i++) {
        if (i != this_state->cpu_id) {
            if (!ipwi_enqueue(item, i)) {
                return false;
            }
        }
    }

    return true;
}

void ipwi_notify_all_except_current(void) {
    arch_ipwi_notify_all_except_current();
}

bool ipwi_dequeue_this_cpu(IpwiWorkItem *out_item) {
    PerCPUState *this_state = state_get_for_this_cpu();
    bool result = false;

    if (!this_state) {
        return result;
    }

    const uint64_t flags =
            spinlock_lock_irqsave(&this_state->ipwi_queue_lock_this_cpu);
    const IpwiWorkItem *item = shift_array_get_head(&this_state->ipwi_queue);

    if (item) {
        result = true;
        memcpy(out_item, item, sizeof(*out_item));
    }

    shift_array_remove_head(&this_state->ipwi_queue);
    spinlock_unlock_irqrestore(&this_state->ipwi_queue_lock_this_cpu, flags);

    return result;
}

void ipwi_ipi_handler(void) {
    IpwiWorkItem item;
    while (ipwi_dequeue_this_cpu(&item)) {
        // we have an item!
        switch (item.type) {
        case IPWI_TYPE_TLB_SHOOTDOWN:
        case IPWI_TYPE_REMOTE_EXEC:
            // TODO not yet implemented
            break;
        case IPWI_TYPE_PANIC_HALT:
            halt_and_catch_fire();
        default:
            // TODO warn or something, ignore for now
            break;
        }
    }
}