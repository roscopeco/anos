/*
 * stage3 - Inter-processor work-item (IPWI) execution
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This framework supports inter-processor execution of work items,
 * enabling in-kernel code running on a given core to interrupt
 * other cores and have them perform some task (from a predefined
 * subset of tasks).
 *
 * The classic, obvious use-case for this is TLB shootdown, but
 * the design is generic enough that it can be extended to support
 * other arbitrary task types.
 *
 * Architecture-specific notes for x86_64:
 *
 * It should be noted that very modern (i.e. Alder Lake and later)
 * Intel CPUs have hardware support specifically for the TLB shootdown
 * case in the form of Remote Action Requests[1], but since Anos
 * targets micro-architectures as early as Haswell, we continue to
 * use IPIs since these are well supported everywhere.
 *
 * We may, in the future, extend this support to take advantage of RAR
 * where available.
 *
 * [1] https://www.intel.com/content/dam/develop/external/us/en/documents/341431-remote-action-request-white-paper.pdf
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_SMP_REMOTE_EXEC_H__
#define __ANOS_KERNEL_SMP_REMOTE_EXEC_H__

#include <stddef.h>
#include <stdint.h>

#include "anos_assert.h"

#define IPWI_IPI_VECTOR ((0x02)) // Use NMI for Panic IPI

typedef enum {
    IPWI_TYPE_REMOTE_EXEC = 1,
    IPWI_TYPE_TLB_SHOOTDOWN,
    IPWI_TYPE_PANIC_HALT, // no payload

    /* ... */

    IPWI_TYPE_LIMIT
} IpwiType;

typedef void (*IpwiRemoteFunc)(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                               uint64_t arg5);

typedef struct {
    IpwiRemoteFunc func;
    uint64_t args[6];
} IpwiPayloadRemoteExec;

typedef struct {
    uint64_t reserved0;
    uintptr_t start_vaddr;
    size_t page_count;
    uint64_t target_pid;   // Only PID, **or** PML4,
    uintptr_t target_pml4; // never both!
    uint64_t reserved1[2];
} IpwiPayloadTLBShootdown;

typedef struct {
    uint32_t type;
    uint32_t flags;

    uint8_t payload[56];
} IpwiWorkItem;

static_assert_sizeof(IpwiWorkItem, ==, 64);
static_assert_sizeof(IpwiPayloadRemoteExec, ==, 56);
static_assert_sizeof(IpwiPayloadTLBShootdown, ==, 56);

/* Initialize the IPWI subsystem on the current CPU.
 *
 * MUST call this _on each CPU_ before using any other IPWI funcs!
 */
bool ipwi_init(void);

/*
 * Enqueue the given work item for the given CPU. The item will be copied
 * into the target CPU's queue so can be changed after this returns.
 */
bool ipwi_enqueue(const IpwiWorkItem *item, uint8_t cpu_num);

/*
 * Enqueue the given work item for all CPUs except the current one.
 *
 * The item will be copied into the target CPU queues so can be changed after
 * this returns.
 */
bool ipwi_enqueue_all_except_current(IpwiWorkItem *item);

/*
 * Send an interprocessor notification to all CPUs except the current one.
 *
 * This will cause them to drain their queues and take action on any items.
 */
void ipwi_notify_all_except_current(void);

/*
 * Dequeue the next item from this CPU's queue, if available.
 *
 * Returns true if an item was dequeued (into `out_item`) and false otherwise.
 *
 * The item is copied into the provided structure.
 */
bool ipwi_dequeue_this_cpu(IpwiWorkItem *out_item);

#endif //__ANOS_KERNEL_SMP_REMOTE_EXEC_H__