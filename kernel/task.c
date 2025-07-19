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
#include "printhex.h"
#include "slab/alloc.h"
#include "smp/state.h"
#include "task.h"

#include "printdec.h"
#include "std/string.h"

#ifdef DEBUG_TASK_SWITCH
#include "debugprint.h"
#include "kprintf.h"
#include "printhex.h"
#ifdef VERY_NOISY_TASK_SWITCH
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#define vdebugf(...) kprintf(__VA_ARGS__)
#else
#define vdebugf(...)
#define vdebug(...)
#define vdbgx64(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#define tdebugf(...) kprintf(__VA_ARGS__)
#else
#define tdebugf(...)
#define tdebug(...)
#define tdbgx64(...)
#define vdebug(...)
#define vdbgx64(...)
#define vdebugf(...)
#endif

#ifndef NULL
#define NULL (((void *)0))
#endif

#ifdef CONSERVATIVE_BUILD
#include "panic.h"
#ifdef CONSERVATIVE_PANICKY
#define konservative panic
#else
#ifdef UNIT_TESTS
void mock_kprintf(const char *msg);
#define konservative mock_kprintf
#else
#include "kprintf.h"
#define konservative kprintf
#endif
#endif
#endif

typedef per_cpu struct {
    Task *task_current_ptr;
    void *task_tss_ptr; // opaque, we only access from assembly...
} PerCPUTaskState;

static_assert_sizeof(PerCPUTaskState, <=, STATE_TASK_DATA_MAX);

static _Atomic volatile uint64_t next_tid;

void user_thread_entrypoint(void);
void kernel_thread_entrypoint(void);
void thread_exitpoint(void);

static inline PerCPUTaskState *get_cpu_task_state(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();
    return (PerCPUTaskState *)cpu_state->task_data;
}

static inline PerCPUTaskState *init_cpu_task_state(void *tss) {
    PerCPUTaskState *state = get_cpu_task_state();

    state->task_current_ptr = (Task *)0;
    state->task_tss_ptr = tss;

    return state;
}

#ifdef UNIT_TESTS
void task_do_switch(Task *next) {
    get_cpu_task_state()->task_current_ptr = next;
}
#else
// See task_switch.asm
void task_do_switch(Task *next);
#endif

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

void task_switch(Task *next) {
    vdebug("Switching task: ");
    vdbgx64((uint64_t)next);
    vdebug("\n");

    task_do_switch(next);
}

Task *task_create_new(Process *owner, const uintptr_t sp,
                      const uintptr_t sys_ssp, const uintptr_t bootstrap,
                      const uintptr_t func, const TaskClass class) {

    Task *task = fba_alloc_block();

    if (task == NULL) {
        return NULL;
    }

    task->data = &task->sdata;
    task->sched = &task->ssched;

    // clear out data and sched data
    memset(task->sdata, 0, TASK_DATA_SIZE);
    memset(&task->ssched, 0, sizeof(TaskSched));
    tdebugf("sdata @ 0x%016lx; ssched @ 0x%016lx\n", (uintptr_t)&task->sdata,
            (uintptr_t)&task->sched);

    task->sched->tid = next_tid++;

    if (sys_ssp) {
        task->rsp0 = task->ssp = sys_ssp;
    } else {
        task->rsp0 = task->ssp =
                ((uintptr_t)fba_alloc_block()) +
                0x1000; // default 4KiB kernel stack should be enough...?

        vdebug("Created kernel stack for 0 thread @ ");
        vdbgx64(task->rsp0);
        vdebug("\n");
    }

#ifdef ARCH_RISCV64
    // Push zero at bottom of stack. On RISC-V, this becomes initial SEPC
    // of new task (the entrypoint will deal with that properly anwyay)
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = 0;
#endif

    // push address of entrypoint func as first place this task will "return" to...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = bootstrap;

    // space for initial registers except first two arg regs, values don't care...
    task->ssp -= 8 * (TASK_SAVED_REGISTER_COUNT - 2);

    // push address of thread user stack, this will get popped into second
    // arg reg (rsi/a1)...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = sp;

    // push address of thread func, this will get popped into first arg
    // reg (rdi/a0)...
    task->ssp -= 8;
    *((uint64_t *)task->ssp) = func;

    task->owner = owner;
    task->pml4 = owner->pml4;
    task->sched->ts_remain = DEFAULT_TIMESLICE;
    task->sched->state = TASK_STATE_READY;
    task->sched->status_flags = 0;

    // TODO pass these in, or inherit from owner
    //      if the latter, have a separate call to change them...
    task->sched->class = class;
    task->sched->prio = 0;

    task->this.next = (void *)0;

    // Add to process' task list
    // TODO add at end, to ensure destruction in reverse order?
    ProcessTask *process_task = slab_alloc_block();
    process_task->this.next = (ListNode *)owner->tasks;
    process_task->task = task;
    owner->tasks = process_task;

    return task;
}

void task_destroy(Task *task) {
#ifdef CONSERVATIVE_BUILD
    if (!task) {
        konservative("[BUG] Destroy task with NULL task");
        return;
    }

    if (!task->sched) {
        konservative("[BUG] Destroy task with NULL sched");
    }

    if (!task->data) {
        konservative("[BUG] Destroy task with NULL data area");
    }

    if (task->sched && (task->sched->state != TASK_STATE_TERMINATED)) {
        // always panic in this case, we're going to crash soon anyway
        // if we wipe out a task that's running or queued....
        panic("[BUG] Destroy task with state other than TASK_STATE_TERMINATED");
    }
#endif

    if (task) {
        PerCPUTaskState *task_state = get_cpu_task_state();

        if (task == task_state->task_current_ptr) {
            task_state->task_current_ptr = NULL;
        }

        fba_free(task);
    }
}

Task *task_create_user(Process *owner, const uintptr_t sp,
                       const uintptr_t sys_ssp, const uintptr_t func,
                       const TaskClass class) {
    return task_create_new(owner, sp, sys_ssp,
                           (uintptr_t)user_thread_entrypoint, func, class);
}

Task *task_create_kernel(Process *owner, const uintptr_t sp,
                         const uintptr_t sys_ssp, const uintptr_t func,
                         const TaskClass class) {
    return task_create_new(owner, sp, sys_ssp,
                           (uintptr_t)kernel_thread_entrypoint, func, class);
}

void task_remove_from_process(Task *task) {
    if (!task || !task->owner)
        return;

    ProcessTask **curr = (ProcessTask **)&task->owner->tasks;

    while (*curr) {
        if ((*curr)->task == task) {
            ProcessTask *to_remove = *curr;

            *curr = (ProcessTask *)(*curr)->this.next;
            slab_free(to_remove);

            return;
        }
        curr = (ProcessTask **)&(*curr)->this.next;
    }
}

#include "kprintf.h"
#include "sched.h"

noreturn void task_current_exitpoint(void) {
    Task *task = task_current();
    Process *owner = task->owner;

    tdebugf("Thread %ld (process %ld) is exiting..\n", task->sched->tid,
            owner->pid);

    task_remove_from_process(task);

    // Things are about to get... interesting.
    //
    // We need to destroy this task (and its stack) while we're still running.
    // This means we're very limited in the things we can do after this point -
    // and we need to make sure nothing jumps in while we're doing it and reuses
    // the stack we're about to free but are still using...
    //
    // The scheduler **must** remain locked throughout obviously...

    if (owner->tasks == NULL) {
        tdebugf("Last thread for process %ld exited, killing process\n",
                owner->pid);
        process_destroy(owner);
    }

    task_destroy(task);
    sched_schedule();

    __builtin_unreachable();
}