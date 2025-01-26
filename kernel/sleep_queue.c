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

// Sleeper *sleep_queue_dequeue(SleepQueue *queue, uint64_t deadline) {
//     if (queue == NULL) {
//         return false;
//     }

//     Sleeper* last = queue->head;

//     while (last != NULL && last->wake_at <= deadline) {
//         last = (Sleeper*)last->this.next;
//     }

//     if (last->wake_at >= deadline) {
//         Sleeper *first = queue->head;

//         // Dequeue, make sure tail stays in sync
//         queue->head = last->this.next;
//         if (queue->tail == last) {
//             queue->tail = NULL;
//         }

//         Task *result, *current;

//         do {
//             current = first->task;

//             if (result == NULL) {
//                 result = current;
//             }

//             result->this.next = (ListNode*)current;

//             first = first->this.next;
//             slab_free_block(first);
//         } while (first);

//         return result;

//     } else {
//         return NULL;
//     }
// }

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