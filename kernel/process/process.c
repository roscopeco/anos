/*
 * stage3 - Process management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "process.h"
#include "slab/alloc.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

static _Atomic volatile uint64_t next_pid;

void process_init(void) { next_pid = 1; }

Process *process_create(uintptr_t pml4) {
#ifdef CONSERVATIVE_BUILD
    if (pml4 == 0) {
        return NULL;
    }
#endif

    Process *process = slab_alloc_block();

    if (!process) {
        return NULL;
    }

    process->pid = next_pid++;
    process->pml4 = pml4;

    return process;
}

void process_destroy(Process *process) { slab_free(process); }
