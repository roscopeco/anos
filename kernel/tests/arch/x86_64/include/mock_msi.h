/*
 * Mock definitions for MSI testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __MOCK_MSI_H
#define __MOCK_MSI_H

#include <stdbool.h>
#include <stdint.h>

// Mock PerCPUState structure for testing
typedef struct {
    uint32_t cpu_id;
    uint8_t lapic_id;
    // Add other fields as needed for testing
} PerCPUState;

// Mock Task structure
typedef struct Task {
    struct Process *owner;
} Task;

// Mock Process structure
typedef struct Process {
    uint64_t pid;
} Process;

// Mock SMP state functions
uint32_t state_get_cpu_count(void);
PerCPUState *state_get_for_any_cpu(uint32_t cpu_id);
PerCPUState *state_get_for_this_cpu(void);

// Mock task functions
Task *task_current(void);

// Mock scheduler functions
uint64_t sched_lock_this_cpu(void);
void sched_unlock_this_cpu(uint64_t flags);
void sched_block(Task *task);
void sched_schedule(void);
void sched_unblock(Task *task);

// Mock timer function
uint64_t get_kernel_upticks(void);

// Mock slab allocator for testing
static inline void *slab_alloc_block(void) {
    // Simple malloc-based implementation for testing
    return malloc(4096); // Assume 4KB blocks
}

static inline void slab_free(void *ptr) { free(ptr); }

#include <stdlib.h>

#endif // __MOCK_MSI_H