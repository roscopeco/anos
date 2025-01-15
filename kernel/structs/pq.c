/*
 * stage3 - Basic priority queue for tasks (in scheduler)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Note that none of these routines allocate any memory or
 * copy anything - that's all on the caller.
 * 
 * O(n) enqueue, O(1) peek / dequeue
 * (The prr scheduler does the latter multiple times...)
 */

#include <stdbool.h>

#include "structs/pq.h"
#include "task.h"

#define NULL (((void *)0))

#ifdef CONSERVATIVE_BUILD
#ifdef __STDC_HOSTED__
#include <stdio.h>
#define debugstr(...) printf(__VA_ARGS__)
#else
#include "debugprint.h"
#endif

static bool check_invariants(TaskPriorityQueue *pq) {
    if (pq->head == NULL) {
        return true; // Empty queue is valid
    }

    // Check first node
    if (pq->head->this.next == (ListNode *)pq->head) {
        debugstr("Error: Cycle detected at head\n");
        return false;
    }

    // Detect cycles & priority violation
    Task *slow = pq->head;
    Task *fast = (Task *)pq->head->this.next;

    int prev_priority = pq->head->prio;

    while (fast != NULL && fast->this.next != NULL) {
        // (Floyd's algorithm)
        if (fast == slow || ((Task *)fast->this.next) == slow) {
            debugstr("Error: Cycle detected in queue\n");
            return false;
        }

        // priority ordering
        if (slow->prio > ((Task *)slow->this.next)->prio) {
            debugstr("Error: Priority ordering violation: %d > %d\n",
                     slow->prio, ((Task *)slow->this.next)->prio);
            return false;
        }

        slow = (Task *)slow->this.next;
        fast = (Task *)((Task *)fast->this.next)->this.next;
    }

    return true;
}
#endif

void task_pq_init(TaskPriorityQueue *pq) { pq->head = NULL; }

void task_pq_push(TaskPriorityQueue *pq, Task *new_node) {
    if (!new_node) {
        return;
    }

    if (pq->head == NULL || new_node->prio < pq->head->prio) {
        new_node->this.next = (ListNode *)pq->head;
        pq->head = new_node;
        return;
    }

    Task *current = pq->head;
    while (current->this.next != NULL &&
           ((Task *)current->this.next)->prio <= new_node->prio) {
        current = (Task *)current->this.next;
    }

    new_node->this.next = current->this.next;
    current->this.next = (ListNode *)new_node;

#ifdef CONSERVATIVE_BUILD
    if (!check_invariants(pq)) {
        debugstr("WARN: Invariant violation after push\n");
    }
#endif
}

Task *task_pq_pop(TaskPriorityQueue *pq) {
    if (pq->head == NULL) {
        return NULL;
    }

    Task *min_node = pq->head;
    pq->head = (Task *)pq->head->this.next;
    min_node->this.next = NULL; // Detach from list

#ifdef CONSERVATIVE_BUILD
    if (!check_invariants(pq)) {
        debugstr("WARN: Invariant violation after pop\n");
    }
#endif

    return min_node;
}

Task *task_pq_peek(TaskPriorityQueue *pq) { return pq->head; }

bool task_pq_empty(TaskPriorityQueue *pq) { return pq->head == NULL; }
