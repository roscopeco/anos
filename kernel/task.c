/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>
#include "debugprint.h"
#include "task.h"

// not static, ASM needs it...
Task* task_current_ptr;

void task_do_switch(Task* next);

Task* task_current() {
    return task_current_ptr;
}

void task_switch(Task* next) {
    debugstr("Switching task\n");
    task_do_switch(next);
}

#ifdef DEBUG_TEST_TASKS
// TODO remove all this!
#include <stdnoreturn.h>
#include "debugprint.h"
#include "printhex.h"
#include "pmm/pagealloc.h"
#include "vmm/vmmapper.h"
#include "task.h"

extern MemoryRegion *physical_region;

static Task *task1_struct;
static Task *task2_struct;

void task1(void) {
    uint32_t i = 0;

    while (1) {
        debugstr("Task 1: ");
        printhex32(i++, debugchar);
        debugstr("\n");
        task_switch(task2_struct);
    }
 }

 void task2(void) {
    uint32_t i = 0x80000;

    while (1) {
        debugstr("Task 2: ");
        printhex32(i++, debugchar);
        debugstr("\n");
        task_switch(task1_struct);
    }
 }

noreturn void debug_test_tasks() {
    uint64_t p_task1_stack = page_alloc(physical_region);
    uint64_t p_task2_stack = page_alloc(physical_region);

    uint64_t *task1_stack = (uint64_t*)0x10000;
    uint64_t *task2_stack = (uint64_t*)0x20000;

    vmm_map_page(STATIC_PML4, (uint64_t)task1_stack, p_task1_stack, WRITE | PRESENT);
    vmm_map_page(STATIC_PML4, (uint64_t)task2_stack, p_task2_stack, WRITE | PRESENT);

    task1_stack[0] = 0x10;      // tid 0x10
    task1_stack[1] = 0x10f78;   // top of stack - 136 bytes already allocated
    task1_stack[511] = (uint64_t)&task1;

    task2_stack[0] = 0x20;      // tid 0x20
    task2_stack[1] = 0x20f78;   // top of stack - 136 bytes already allocated
    task2_stack[511] = (uint64_t)&task2;

    task1_struct = (Task*)task1_stack;
    task2_struct = (Task*)task2_stack;

    task_switch(task1_struct);

    __builtin_unreachable();
}
#endif//DEBUG_TEST_TASKS
