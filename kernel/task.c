/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "task.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "ktypes.h"
#include "printhex.h"
#include "slab/alloc.h"

#include <stdint.h>

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

// not static, ASM needs it...
Task *task_current_ptr;
void *task_tss_ptr; // opaque, we only access from assembly...

static uint64_t next_tid;

void task_init(void *tss) {
    // start with no initial task, task_switch can handle this...
    // TODO debugging would be nicer if we kept the original stack,
    // it'd give nicer backtraces at least...
    //
    // Look into that at some point...
    task_current_ptr = (Task *)0;

    tdebug("TSS is at ");
    tdbgx64((uint64_t)tss);
    tdebug("\n");

    task_tss_ptr = tss;

    next_tid = 2;
}

Task *task_current() { return task_current_ptr; }

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
