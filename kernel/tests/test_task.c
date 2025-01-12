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

void task_switch(Task *next) {
    ListNode *added = list_add(switch_chain, next);

    if (switch_chain == NULL) {
        switch_chain = added;
    }
}