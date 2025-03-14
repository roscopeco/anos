/*
 * stage3 - Basic non-circular singly-linked node list
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Note that none of these routines allocate any memory or
 * copy anything - that's all on the caller.
 *
 * Don't try and do circular lists with this - it deliberately
 * doesn't support them (since that would needlessly slow down
 * all operations).
 *
 * If we need circular lists (or doubly-linked ones for that matter)
 * they'll be supported separately - this is just a nice quick
 * singly-linked list for general kernel usage.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_LIST_H
#define __ANOS_KERNEL_LIST_H

#include <stdbool.h>
#include <stdint.h>

/*
 * A list node. The intention is that this struct will
 * be used as the first entry in other structs (hence
 * the size and type fields, which aren't used by the
 * list implementation itself 😊).
 */
typedef struct _ListNode {
    struct _ListNode *next;
    uint64_t type;
} ListNode;

/*
 * A predicate function, passed to the `list_find` function.
 */
typedef bool (*ListPredicate)(ListNode *candidate);

/*
 * Insert a new subject node after the specified target node.
 *
 * Target can be null, in which case this creates a new list,
 * removing any `next` pointer in the subject node.
 *
 * Subject can be null, in which case this essentially deletes
 * the list after the target.
 *
 * If both are null, this does nothing much, and will just
 * return null.
 *
 * Arguments:
 *  target      The node to insert after
 *  subject     The node to insert
 *
 * Returns:
 *  The inserted node
 */
ListNode *list_insert_after(ListNode *target, ListNode *subject);

/*
 * Add the subject node to the end of the list headed by the
 * specified head node.
 *
 * head can be null, in which case this creates a new list,
 * removing any `next` pointer in the subject node.
 *
 * Subject can be null, in which case this won't really do
 * anything, other than wasting some compute.
 *
 * If both are null, this does nothing much, and will just
 * return null.
 *
 * Arguments:
 *  head        The node at the head of the list
 *  subject     The node to insert
 *
 * Returns:
 *  The inserted node
 */
// TODO in hindsight, it might be more convenient for
//      this to return the list head...
//
ListNode *list_add(ListNode *head, ListNode *subject);

/*
 * Delete the node immediately following the specified target.
 *
 * If target is null, just returns null.
 *
 * Otherwise, will remove the node following the target, and
 * change the next pointer in the target to be the next from
 * the removed node.
 *
 * Arguments:
 *  target      The node to delete the following node from
 *
 * Returns:
 *  The deleted node
 */
ListNode *list_delete_after(ListNode *target);

/*
 * Find a node matching a given predicate.
 *
 * If head is null, just returns null.
 *
 * Otherwise, will pass each list element to the predicate
 * function in turn, returning the first one for which the
 * predicate returns `true`.
 *
 * This function guarantees null will never be passed to
 * the predicate.
 *
 * Arguments:
 *  head        The head of the list to search
 *  predicate   The predicate function
 *
 * Returns:
 *  The first matching node, or NULL if no match
 */
ListNode *list_find(ListNode *head, ListPredicate predicate);

#endif //__ANOS_KERNEL_LIST_H
