/*
 * stage3 - Basic priority queue for tasks (in scheduler)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Note that none of these routines allocate any memory or
 * copy anything - that's all on the caller.
 */

#ifndef __ANOS_KERNEL_TASK_PQ_H
#define __ANOS_KERNEL_TASK_PQ_H

#include "task.h"
#include <stdbool.h>

typedef struct {
    Task *head;
} TaskPriorityQueue;

// Optional - not needed if static
void task_pq_init(TaskPriorityQueue *pq);

void task_pq_push(TaskPriorityQueue *pq, Task *new_node);
Task *task_pq_pop(TaskPriorityQueue *pq);
Task *task_pq_peek(TaskPriorityQueue *pq);
bool task_pq_empty(TaskPriorityQueue *pq);

#endif //__ANOS_KERNEL_TASK_PQ_H
