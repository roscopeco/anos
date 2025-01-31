/*
 * stage3 - Sleeping task queue
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 */

#include "sleep_queue.h"
#include "ktypes.h"
#include "slab/alloc.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

// Optional - not needed if static
void sleep_queue_init(SleepQueue *queue) {
    queue->always0 = 0;
    queue->head = 0;
    queue->tail = 0;
}

bool sleep_queue_enqueue(SleepQueue *queue, Task *task, uint64_t deadline) {
    if (queue == NULL || task == NULL) {
        return false;
    }

    Sleeper *sleeper = slab_alloc_block();

    if (sleeper == NULL) {
        // OOM...?
        return false;
    }

    sleeper->task = task;
    sleeper->wake_at = deadline;
    sleeper->this.next = NULL;
    sleeper->this.type = KTYPE_SLEEPER;

    Sleeper *current = (Sleeper *)queue;

    while (current->this.next &&
           sleeper->wake_at > ((Sleeper *)current->this.next)->wake_at) {
        current = (Sleeper *)current->this.next;
    }

    sleeper->this.next = current->this.next;
    current->this.next = (ListNode *)sleeper;
    if (sleeper->this.next == NULL) {
        queue->tail = sleeper;
    }

    return true;
}

Task *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline) {
    if (!queue || !queue->head) {
        return NULL;
    }

    // Find first sleeper to keep
    Sleeper *curr = queue->head;
    Sleeper *new_head = NULL;
    while (curr && curr->wake_at <= deadline) {
        curr = (Sleeper *)curr->this.next;
    }
    new_head = curr;

    // No sleepers to dequeue
    if (new_head == queue->head) {
        return NULL;
    }

    // Build task list and free sleepers
    Task *task_list = NULL;
    Task *last_task = NULL;
    curr = queue->head;

    while (curr != new_head) {
        Task *task = curr->task;
        if (!task_list) {
            task_list = task;
        } else {
            last_task->this.next = (ListNode *)task;
        }
        last_task = task;

        Sleeper *to_free = curr;
        curr = (Sleeper *)curr->this.next;
        slab_free_block(to_free);
    }

    if (last_task) {
        last_task->this.next = NULL;
    }

    // Update queue
    queue->head = new_head;
    if (!new_head) {
        queue->tail = NULL;
    }

    return task_list;
}