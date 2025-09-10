/*
 * stage3 - MSI/MSI-X Interrupt Management (cleaned)
 * anos - An Operating System
 */

#include "fba/alloc.h"
#include "kdrivers/timer.h"
#include "kprintf.h"
#include "sched.h"
#include "smp/state.h"
#include "spinlock.h"
#include "task.h"
#include "vmm/vmconfig.h"

#include "x86_64/kdrivers/local_apic.h"
#include "x86_64/kdrivers/msi.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr yet - May 2025
#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr (((void *)0))
#endif
#endif
#endif

#ifdef DEBUG_MSI
#define debugf kprintf
#else
#define debugf(...)
#endif

static MSIManager *msi_manager;
static SpinLock msi_lock;

#define MSI_INDEX(v) ((uint32_t)((v) - MSI_VECTOR_BASE))
#define MSI_VALID(v)                                                           \
    ((uint8_t)(v) >= MSI_VECTOR_BASE && (uint8_t)(v) <= MSI_VECTOR_TOP)

static inline void msi_queue_push(MSIDevice *d, const uint32_t data,
                                  const uint64_t now) {

    MSIEvent *e = &d->queue[d->head];
    e->data = data;
    e->timestamp_ms = now;
    d->head = (d->head + 1) % MSI_QUEUE_SIZE;
    d->count++;
    d->last_event_ms = now;
    d->total_events++;
}

static inline bool msi_queue_pop(MSIDevice *d, uint32_t *out) {

    if (d->count == 0) {
        return false;
    }

    const MSIEvent *e = &d->queue[d->tail];
    *out = e->data;
    d->tail = (d->tail + 1) % MSI_QUEUE_SIZE;
    d->count--;

    return true;
}

void msi_init(void) {
    msi_manager = fba_alloc_blocks((sizeof(MSIManager) + VM_PAGE_SIZE - 1) >>
                                   VM_PAGE_LINEAR_SHIFT);

    if (!msi_manager) {
        kprintf("ERROR: MSI: Failed to allocate manager. Expect an inoperable "
                "system.");
        return;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    for (int i = 0; i < MSI_VECTOR_COUNT; i++) {
        msi_manager->devices[i].vector = 0;
        msi_manager->devices[i].bus_device_func = 0;
        msi_manager->devices[i].owner_pid = 0;
        msi_manager->devices[i].head = 0;
        msi_manager->devices[i].tail = 0;
        msi_manager->devices[i].count = 0;
        msi_manager->devices[i].last_event_ms = 0;
        msi_manager->devices[i].overflow_count = 0;
        msi_manager->devices[i].slow_consumer_detected = false;
        msi_manager->devices[i].waiting_task = nullptr;
        msi_manager->devices[i].total_events = 0;
        msi_manager->allocated_vectors[i] = 0;
    }

    msi_manager->next_vector_hint = 0;

    spinlock_unlock_irqrestore(&msi_lock, flags);

    debugf("MSI: Initialized manager with vectors 0x%02x-0x%02x",
           MSI_VECTOR_BASE, MSI_VECTOR_TOP);
}

uint8_t msi_allocate_vector(const uint32_t bus_device_func,
                            const uint64_t owner_pid, uint64_t *msi_address,
                            uint32_t *msi_data) {
    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t start = msi_manager->next_vector_hint;
    uint32_t i = start;

    do {
        if (!msi_manager->allocated_vectors[i]) {
            MSIDevice *dev = &msi_manager->devices[i];

            dev->vector = (uint8_t)(MSI_VECTOR_BASE + i);
            dev->bus_device_func = bus_device_func;
            dev->owner_pid = owner_pid;
            dev->head = dev->tail = dev->count = 0;
            dev->last_event_ms = get_kernel_upticks();
            dev->overflow_count = 0;
            dev->slow_consumer_detected = false;
            dev->waiting_task = nullptr;
            dev->total_events = 0;

            msi_manager->allocated_vectors[i] = 1;
            msi_manager->next_vector_hint = (i + 1) % MSI_VECTOR_COUNT;

            const uint8_t vector = dev->vector;

            // Pick a target CPU in a simple round-robin based on index.
            const uint32_t cpu_count = state_get_cpu_count();
            const uint32_t target_cpu = cpu_count ? (i % cpu_count) : 0;
            PerCPUState *target_state = state_get_for_any_cpu(target_cpu);
            if (!target_state)
                target_state = state_get_for_this_cpu();
            const uint8_t apic_id = target_state ? target_state->lapic_id : 0;

            // MSI address (physical dest mode, no redirection hint).
            *msi_address = 0xFEE00000ULL | ((uint64_t)apic_id << 12);

            // MSI data: edge-triggered, fixed delivery, vector in [7:0].
            *msi_data = vector;

            spinlock_unlock_irqrestore(&msi_lock, flags);
            debugf("MSI: Allocated vector 0x%02x for BDF %06x to PID %lu on "
                   "CPU %u (addr=0x%016lx data=0x%08x)",
                   vector, bus_device_func, (unsigned long)owner_pid,
                   target_cpu, *msi_addr, *msi_data);
            return vector;
        }

        i = (i + 1) % MSI_VECTOR_COUNT;
    } while (i != start);

    spinlock_unlock_irqrestore(&msi_lock, flags);
    debugf("WARN: MSI: No free vectors available");
    return 0;
}

bool msi_deallocate_vector(const uint8_t vector, const uint64_t owner_pid) {
    if (!MSI_VALID(vector)) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const uint32_t idx = MSI_INDEX(vector);
    MSIDevice *dev = &msi_manager->devices[idx];

    if (!msi_manager->allocated_vectors[idx] || dev->owner_pid != owner_pid) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    Task *to_wake = dev->waiting_task;
    dev->waiting_task = nullptr;

    msi_manager->allocated_vectors[idx] = 0;
    dev->vector = 0;
    dev->owner_pid = 0;

    spinlock_unlock_irqrestore(&msi_lock, flags);

    if (to_wake) {
        const uint64_t s = sched_lock_this_cpu();
        sched_unblock(to_wake);
        sched_unlock_this_cpu(s);
    }

    debugf("MSI: Deallocated vector 0x%02x", vector);
    return true;
}

bool msi_register_handler(const uint8_t vector, Task *task) {
    if (!MSI_VALID(vector) || !task) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const uint32_t idx = MSI_INDEX(vector);
    const MSIDevice *dev = &msi_manager->devices[idx];
    const bool ok = msi_manager->allocated_vectors[idx] &&
                    (dev->owner_pid == task->owner->pid);
    spinlock_unlock_irqrestore(&msi_lock, flags);

    return ok;
}

bool msi_wait_interrupt(const uint8_t vector, Task *task,
                        uint32_t *event_data) {
    if (!MSI_VALID(vector) || !task || !event_data) {
        return false;
    }

    uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const uint32_t idx = MSI_INDEX(vector);
    MSIDevice *dev = &msi_manager->devices[idx];

    if (!msi_manager->allocated_vectors[idx] ||
        dev->owner_pid != task->owner->pid || dev->slow_consumer_detected) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    if (dev->count == 0) {
        dev->waiting_task = task;
        spinlock_unlock_irqrestore(&msi_lock, flags);

        const uint64_t s = sched_lock_this_cpu();
        sched_block(task);
        sched_schedule();
        sched_unlock_this_cpu(s);

        flags = spinlock_lock_irqsave(&msi_lock);
    }

    const bool ok = msi_queue_pop(dev, event_data);
    spinlock_unlock_irqrestore(&msi_lock, flags);
    return ok;
}

void msi_handle_interrupt(const uint8_t vector, const uint32_t data) {
    if (!MSI_VALID(vector)) {
        local_apic_eoe();
        return;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const uint32_t idx = MSI_INDEX(vector);
    MSIDevice *dev = &msi_manager->devices[idx];

    if (!msi_manager->allocated_vectors[idx]) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        local_apic_eoe();
        return;
    }

    const uint64_t now = get_kernel_upticks();
    Task *to_wake = nullptr;

    if (dev->count >= MSI_QUEUE_SIZE) {
        dev->overflow_count++;
        if ((now - dev->last_event_ms) > MSI_TIMEOUT_MS) {
            dev->slow_consumer_detected = true;
            debugf("MSI: Slow consumer on vector 0x%02x (PID %lu)", vector,
                   (unsigned long)dev->owner_pid);
        }
    } else {
        msi_queue_push(dev, data, now);
        if (dev->waiting_task) {
            to_wake = dev->waiting_task;
            dev->waiting_task = nullptr;
        }
    }

    spinlock_unlock_irqrestore(&msi_lock, flags);

    if (to_wake) {
        const uint64_t s = sched_lock_this_cpu();
        sched_unblock(to_wake);
        sched_unlock_this_cpu(s);
    }

    local_apic_eoe();
}

void msi_cleanup_process(const uint64_t pid) {
    Task *wake_list[MSI_VECTOR_COUNT];
    size_t wake_count = 0;

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    for (int i = 0; i < MSI_VECTOR_COUNT; i++) {
        MSIDevice *dev = &msi_manager->devices[i];
        if (msi_manager->allocated_vectors[i] && dev->owner_pid == pid) {
            if (dev->waiting_task) {
                wake_list[wake_count++] = dev->waiting_task;
                dev->waiting_task = nullptr;
            }
            msi_manager->allocated_vectors[i] = 0;
            dev->vector = 0;
            dev->owner_pid = 0;
            debugf("MSI: Cleaned up vector 0x%02x for PID %lu",
                   MSI_VECTOR_BASE + i, (unsigned long)pid);
        }
    }

    spinlock_unlock_irqrestore(&msi_lock, flags);

    if (wake_count) {
        const uint64_t s = sched_lock_this_cpu();

        for (size_t j = 0; j < wake_count; j++) {
            sched_unblock(wake_list[j]);
        }

        sched_unlock_this_cpu(s);
    }
}

bool msi_is_slow_consumer(const uint8_t vector) {
    if (!MSI_VALID(vector)) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const bool slow =
            msi_manager->devices[MSI_INDEX(vector)].slow_consumer_detected;
    spinlock_unlock_irqrestore(&msi_lock, flags);

    return slow;
}

bool msi_verify_ownership(const uint8_t vector, const uint64_t pid) {
    if (!MSI_VALID(vector) || !msi_manager) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);
    const uint32_t idx = MSI_INDEX(vector);

    const MSIDevice *dev = &msi_manager->devices[idx];
    const bool owned =
            msi_manager->allocated_vectors[idx] && (dev->owner_pid == pid);
    spinlock_unlock_irqrestore(&msi_lock, flags);

    return owned;
}
