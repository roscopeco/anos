/*
 * stage3 - MSI/MSI-X Interrupt Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
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

#ifdef DEBUG_MSI
#define debugf kprintf
#else
#define debugf(...)
#endif

static MSIManager *msi_manager;
static SpinLock msi_lock;

void msi_init(void) {
    msi_manager = fba_alloc_blocks((sizeof(MSIManager) + VM_PAGE_SIZE - 1) >>
                                   VM_PAGE_LINEAR_SHIFT);

    if (!msi_manager) {
        kprintf("ERROR: MSI: Failed to allocate manager. Expect an inoperable "
                "system.\n");
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
        msi_manager->devices[i].waiting_task = NULL;
        msi_manager->devices[i].total_events = 0;
        msi_manager->allocated_vectors[i] = 0;
    }

    msi_manager->next_vector_hint = 0;

    spinlock_unlock_irqrestore(&msi_lock, flags);

    debugf("MSI: Initialized manager with vectors 0x%02x-0x%02x\n",
           MSI_VECTOR_BASE, MSI_VECTOR_TOP);
}

uint8_t msi_allocate_vector(const uint32_t bus_device_func,
                            const uint64_t owner_pid, uint64_t *msi_address,
                            uint32_t *msi_data) {

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t start_hint = msi_manager->next_vector_hint;
    uint32_t i = start_hint;

    do {
        if (!msi_manager->allocated_vectors[i]) {
            MSIDevice *device = &msi_manager->devices[i];

            device->vector = MSI_VECTOR_BASE + i;
            device->bus_device_func = bus_device_func;
            device->owner_pid = owner_pid;
            device->head = 0;
            device->tail = 0;
            device->count = 0;
            device->last_event_ms = get_kernel_upticks();
            device->overflow_count = 0;
            device->slow_consumer_detected = false;
            device->waiting_task = NULL;
            device->total_events = 0;

            msi_manager->allocated_vectors[i] = 1;
            msi_manager->next_vector_hint = (i + 1) % MSI_VECTOR_COUNT;

            const uint8_t vector = device->vector;

            // Generate MSI address and data for this vector
            // Select target CPU using round-robin for load balancing
            const uint32_t cpu_count = state_get_cpu_count();
            uint32_t target_cpu =
                    i % cpu_count; // Round-robin based on vector index
            PerCPUState *target_cpu_state = state_get_for_any_cpu(target_cpu);

            if (!target_cpu_state) {
                // Fallback to current if CPU state not available
#ifdef CONSERVATIVE_BUILD
                kprintf("WARN: CPU state not available in msi_allocate_vector; "
                        "a crash is likely imminent!");
#endif
                target_cpu = 0;
                target_cpu_state = state_get_for_this_cpu();
            }

            const uint8_t target_apic_id =
                    target_cpu_state ? target_cpu_state->lapic_id : 0;

            // MSI Address format for x86_64:
            // Bits 31-20: 0xFEE (MSI address prefix)
            // Bits 19-12: Destination APIC ID
            // Bits 11-4: Reserved (0)
            // Bits 3: Redirection hint (0 = directed)
            // Bits 2: Destination mode (0 = physical)
            *msi_address = 0xFEE00000ULL | ((uint64_t)target_apic_id << 12);

            // MSI Data format:
            // Bits 15: Trigger mode (0 = edge)
            // Bits 14: Level (0 for edge triggered)
            // Bits 10-8: Delivery mode (000 = fixed)
            // Bits 7-0: Vector number
            *msi_data = vector; // Edge triggered, fixed delivery, our vector

            spinlock_unlock_irqrestore(&msi_lock, flags);

            debugf("MSI: Allocated vector 0x%02x for device %06x to PID %lu "
                   "on CPU %u (addr=0x%016lx data=0x%08x)\n",
                   vector, bus_device_func, owner_pid, target_cpu, *msi_address,
                   *msi_data);
            return vector;
        }

        i = (i + 1) % MSI_VECTOR_COUNT;
    } while (i != start_hint);

    spinlock_unlock_irqrestore(&msi_lock, flags);
    debugf("WARN: MSI: No free vectors available\n");
    return 0;
}

bool msi_deallocate_vector(const uint8_t vector, const uint64_t owner_pid) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t index = vector - MSI_VECTOR_BASE;
    MSIDevice *device = &msi_manager->devices[index];

    if (!msi_manager->allocated_vectors[index] ||
        device->owner_pid != owner_pid) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    if (device->waiting_task) {
        const uint64_t lock_flags = sched_lock_this_cpu();
        sched_unblock(device->waiting_task);
        sched_unlock_this_cpu(lock_flags);
        device->waiting_task = NULL;
    }

    msi_manager->allocated_vectors[index] = 0;
    device->vector = 0;
    device->owner_pid = 0;

    spinlock_unlock_irqrestore(&msi_lock, flags);

    debugf("MSI: Deallocated vector 0x%02x\n", vector);
    return true;
}

bool msi_register_handler(const uint8_t vector, Task *task) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP || !task) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t index = vector - MSI_VECTOR_BASE;
    const MSIDevice *device = &msi_manager->devices[index];

    if (!msi_manager->allocated_vectors[index] ||
        device->owner_pid != task->owner->pid) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    spinlock_unlock_irqrestore(&msi_lock, flags);
    return true;
}

bool msi_wait_interrupt(const uint8_t vector, Task *task,
                        uint32_t *event_data) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP || !task ||
        !event_data) {
        return false;
    }

    uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t index = vector - MSI_VECTOR_BASE;
    MSIDevice *device = &msi_manager->devices[index];

    if (!msi_manager->allocated_vectors[index] ||
        device->owner_pid != task->owner->pid) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    if (device->slow_consumer_detected) {
        spinlock_unlock_irqrestore(&msi_lock, flags);
        return false;
    }

    if (device->count == 0) {
        device->waiting_task = task;
        spinlock_unlock_irqrestore(&msi_lock, flags);

        const uint64_t sched_flags = sched_lock_this_cpu();
        sched_block(task);
        sched_schedule();
        sched_unlock_this_cpu(sched_flags);

        flags = spinlock_lock_irqsave(&msi_lock);
    }

    if (device->count > 0) {
        const MSIEvent *event = &device->queue[device->tail];
        *event_data = event->data;

        device->tail = (device->tail + 1) % MSI_QUEUE_SIZE;
        device->count--;

        spinlock_unlock_irqrestore(&msi_lock, flags);
        return true;
    }

    spinlock_unlock_irqrestore(&msi_lock, flags);
    return false;
}

void msi_handle_interrupt(const uint8_t vector, const uint32_t data) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP) {
        local_apic_eoe();
        return;
    }

    const uint32_t index = vector - MSI_VECTOR_BASE;
    MSIDevice *device = &msi_manager->devices[index];

    spinlock_lock(&msi_lock);

    if (!msi_manager->allocated_vectors[index]) {
        spinlock_unlock(&msi_lock);
        local_apic_eoe();
        return;
    }

    const uint64_t current_time = get_kernel_upticks();

    if (device->count >= MSI_QUEUE_SIZE) {
        device->overflow_count++;

        if ((current_time - device->last_event_ms) > MSI_TIMEOUT_MS) {
            device->slow_consumer_detected = true;
            debugf("MSI: Slow consumer detected on vector 0x%02x, PID %lu "
                   "killed\n",
                   vector, device->owner_pid);
        }

        spinlock_unlock(&msi_lock);
        local_apic_eoe();
        return;
    }

    MSIEvent *event = &device->queue[device->head];
    event->data = data;
    event->timestamp_ms = current_time;

    device->head = (device->head + 1) % MSI_QUEUE_SIZE;
    device->count++;
    device->last_event_ms = current_time;
    device->total_events++;

    if (device->waiting_task) {
        const uint64_t sched_flags = sched_lock_this_cpu();
        sched_unblock(device->waiting_task);
        sched_unlock_this_cpu(sched_flags);
        device->waiting_task = NULL;
    }

    spinlock_unlock(&msi_lock);
    local_apic_eoe();
}

void msi_cleanup_process(uint64_t pid) {
    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    for (int i = 0; i < MSI_VECTOR_COUNT; i++) {
        MSIDevice *device = &msi_manager->devices[i];

        if (msi_manager->allocated_vectors[i] && device->owner_pid == pid) {
            if (device->waiting_task) {
                const uint64_t sched_flags = sched_lock_this_cpu();
                sched_unblock(device->waiting_task);
                sched_unlock_this_cpu(sched_flags);
                device->waiting_task = NULL;
            }

            msi_manager->allocated_vectors[i] = 0;
            device->vector = 0;
            device->owner_pid = 0;

            debugf("MSI: Cleaned up vector 0x%02x for terminated PID %lu\n",
                   MSI_VECTOR_BASE + i, pid);
        }
    }

    spinlock_unlock_irqrestore(&msi_lock, flags);
}

bool msi_is_slow_consumer(const uint8_t vector) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP) {
        return false;
    }

    const uint32_t index = vector - MSI_VECTOR_BASE;
    return msi_manager->devices[index].slow_consumer_detected;
}

bool msi_verify_ownership(const uint8_t vector, const uint64_t pid) {
    if (vector < MSI_VECTOR_BASE || vector > MSI_VECTOR_TOP || !msi_manager) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&msi_lock);

    const uint32_t index = vector - MSI_VECTOR_BASE;
    const MSIDevice *device = &msi_manager->devices[index];

    const bool owned =
            msi_manager->allocated_vectors[index] && device->owner_pid == pid;

    spinlock_unlock_irqrestore(&msi_lock, flags);
    return owned;
}
