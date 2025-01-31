/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdatomic.h>
#include <stdint.h>

#include "anos_assert.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "ktypes.h"
#include "printhex.h"
#include "slab/alloc.h"
#include "smp/state.h"
#include "task.h"

#ifdef DEBUG_TASK_SWITCH
#include "debugprint.h"
#include "printhex.h"
#ifdef VERY_NOISY_TASK_SWITCH
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#else
#define vdebug(...)
#define vdbgx64(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#else
#define tdebug(...)
#define tdbgx64(...)
#define vdebug(...)
#define vdbgx64(...)
#endif

typedef per_cpu struct {
    Task *task_current_ptr;
    void *task_tss_ptr; // opaque, we only access from assembly...
} PerCPUTaskState;

static_assert_sizeof(PerCPUTaskState, <=, STATE_TASK_DATA_MAX);

static _Atomic volatile uint64_t next_tid;

static inline PerCPUTaskState *get_cpu_task_state(void) {
    PerCPUState *cpu_state = state_get_per_cpu();
    return (PerCPUTaskState *)cpu_state->task_data;
}

static inline PerCPUTaskState *init_cpu_task_state(void *tss) {
    PerCPUTaskState *state = get_cpu_task_state();

    state->task_current_ptr = (Task *)0;
    state->task_tss_ptr = tss;

    return state;
}

void task_init(void *tss) {
    PerCPUTaskState *cpu_state = init_cpu_task_state(tss);

    // start with no initial task, task_switch can handle this...
    // TODO debugging would be nicer if we kept the original stack,
    // it'd give nicer backtraces at least...
    //
    // Look into that at some point...

    tdebug("TSS is at ");
    tdbgx64((uint64_t)tss);
    tdebug("\n");

    next_tid = 2;
}

Task *task_current() { return get_cpu_task_state()->task_current_ptr; }

// See task_switch.asm
void task_do_switch(Task *next);

void task_switch(Task *next) {
    tdebug("Switching task: ");
    tdbgx64((uint64_t)next);
    tdebug("\n");

    task_do_switch(next);
}

void user_thread_entrypoint(void);

Task *task_create_new(Process *owner, uintptr_t sp, uintptr_t func) {
    Task *task = slab_alloc_block();

    task->tid = next_tid++;
    task->rsp0 = task->ssp = ((uintptr_t)fba_alloc_block()) +
                             0x1000; // 4KiB kernel stack should be enough...?

    // push address of entrypoint func as first place this task will "return" to...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = (uint64_t)user_thread_entrypoint;

    // space for initial registers except rdi, rsi, values don't care...
    task->ssp -= 104;

    // push address of thread user stack, this will get popped into rsi...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = sp;

    // push address of thread func, this will get popped into rdi...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = func;

    task->owner = owner;
    task->pml4 = owner->pml4;
    task->ts_remain = DEFAULT_TIMESLICE;
    task->state = TASK_STATE_READY;

    // TODO pass these in, or inherit from owner
    //      if the latter, have a separate call to change them...
    task->class = TASK_CLASS_NORMAL;
    task->prio = 0;

    task->this.next = (void *)0;
    task->this.type = KTYPE_TASK;

    return task;
}
