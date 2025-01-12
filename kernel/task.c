/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "task.h"
#include "debugprint.h"
#include "printhex.h"
#include <stdint.h>

// not static, ASM needs it...
Task *task_current_ptr;
void *task_tss_ptr; // opaque, we only access from assembly...

void task_init(void *tss) {
    // start with no initial task, task_switch can handle this...
    // TODO debugging would be nicer if we kept the original stack,
    // it'd give nicer backtraces at least...
    //
    // Look into that at some point...
    task_current_ptr = (Task *)0;

    task_tss_ptr = tss;
}

Task *task_current() { return task_current_ptr; }

// See task_switch.asm
void task_do_switch(Task *next);

void task_switch(Task *next) {
#ifdef DEBUG_TASK_SWITCH
    debugstr("Switching task: ");
    printhex64((uint64_t)next, debugchar);
    debugstr("\n");
#endif
    task_do_switch(next);
}
