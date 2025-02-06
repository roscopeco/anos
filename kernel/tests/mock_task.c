/*
 * Mock implementation of the task_switch routine for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "structs/list.h"
#include "task.h"

static ListNode *switch_chain;
static Task *current_task;

static Process *last_create_new_owner;
static uintptr_t last_create_new_sp;
static uintptr_t last_create_new_sys_ssp;
static uintptr_t last_create_new_bootstrap;
static uintptr_t last_create_new_func;
static TaskClass last_create_new_class;

void mock_task_set_curent(Task *new_current) { current_task = new_current; }

Process *mock_task_get_last_create_new_owner(void) {
    return last_create_new_owner;
}

uintptr_t mock_task_get_last_create_new_sp(void) { return last_create_new_sp; }

uintptr_t mock_task_get_last_create_new_sys_ssp(void) {
    return last_create_new_sys_ssp;
}

uintptr_t mock_task_get_last_create_new_bootstrap(void) {
    return last_create_new_bootstrap;
}

uintptr_t mock_task_get_last_create_new_func(void) {
    return last_create_new_func;
}

TaskClass mock_task_get_last_create_new_class(void) {
    return last_create_new_class;
}

void task_init(void *tss) { /* noop */ }

Task *task_current(void) { return current_task; }

void task_switch(Task *next) {
    ListNode *added = list_add(switch_chain, (ListNode *)next);

    if (switch_chain == NULL) {
        switch_chain = added;
    }

    current_task = next;
}

static Task new_task;

Task *task_create_new(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                      uintptr_t bootstrap, uintptr_t func, TaskClass class) {

    last_create_new_owner = owner;
    last_create_new_sp = sp;
    last_create_new_sys_ssp = sys_ssp;
    last_create_new_bootstrap = bootstrap;
    last_create_new_func = func;
    last_create_new_class = class;

    new_task.owner = owner;
    return &new_task;
}

void task_do_switch(void) { /* noop */ }