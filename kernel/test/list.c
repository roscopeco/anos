/**
 * stage3 - Base non-circular singly-linked node list (C implementation for tests)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>

/**
 * Basic linked list node structure
 */
typedef struct node {
    struct node *next;
    // Data would typically follow here in a real implementation
} node_t;

/**
 * Insert a node after the specified target node
 * 
 * @param target Node to insert after (can be NULL)
 * @param subject Node to insert (can be NULL)
 * @return The inserted node
 */
node_t *list_insert_after(node_t *target, node_t *subject) {
    if (target == NULL) {
        // If target is NULL, subject becomes the new head
        if (subject != NULL) {
            subject->next = NULL;
        }
        return subject;
    }

    // Save the current next node
    node_t *next = target->next;

    // Link the target to the subject
    target->next = subject;

    // Link the subject to the next (if subject isn't NULL)
    if (subject != NULL) {
        subject->next = next;
    }

    return subject;
}

/**
 * Add a node to the end of the list
 * 
 * @param head Head of the list (can be NULL)
 * @param subject Node to add (can be NULL)
 * @return The added node
 */
node_t *list_add(node_t *head, node_t *subject) {
    // If head is NULL, just insert after NULL (which makes subject the new head)
    if (head == NULL) {
        return list_insert_after(NULL, subject);
    }

    // Find the end of the list
    node_t *current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    // Insert after the last node
    return list_insert_after(current, subject);
}

/**
 * Delete the node after the specified target
 * 
 * @param target Node whose next node should be deleted (can be NULL)
 * @return The deleted node, or NULL if none was deleted
 */
node_t *list_delete_after(node_t *target) {
    // If target is NULL, nothing to delete
    if (target == NULL) {
        return NULL;
    }

    // Get the node to delete
    node_t *to_delete = target->next;

    // If there's nothing to delete, return NULL
    if (to_delete == NULL) {
        return NULL;
    }

    // Unlink the node to delete
    target->next = to_delete->next;
    to_delete->next = NULL;

    return to_delete;
}

/**
 * Find a node in the list that matches the predicate
 * 
 * @param head Head of the list (can be NULL)
 * @param predicate Function that takes a node and returns true if it matches
 * @return The first matching node, or NULL if none found
 */
node_t *list_find(node_t *head, bool (*predicate)(node_t *)) {
    // If head is NULL or predicate is NULL, nothing to find
    if (head == NULL || predicate == NULL) {
        return NULL;
    }

    // Traverse the list
    node_t *current = head;
    while (current != NULL) {
        // Check if the current node matches
        if (predicate(current)) {
            return current;
        }
        current = current->next;
    }

    // No match found
    return NULL;
}