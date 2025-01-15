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

void task_init(void *abused_to_be_init_task_instead) {
    current_task = (Task *)abused_to_be_init_task_instead;
}

Task *task_current(void) { return current_task; }

void task_switch(Task *next) {
    ListNode *added = list_add(switch_chain, (ListNode *)next);

    if (switch_chain == NULL) {
        switch_chain = added;
    }

    current_task = next;
}