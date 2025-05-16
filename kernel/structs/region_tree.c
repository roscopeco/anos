/*
 * Region Tree - Memory Region Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Balanced interval tree (AVL) for tracking memory regions,
 * supporting fast lookup by address, insertion, deletion, and resizing.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "slab/alloc.h"
#include "structs/region_tree.h"

static int height(Region *node) { return node ? node->height : 0; }

static int max(int a, int b) { return a > b ? a : b; }

static Region *rotate_right(Region *y) {
    Region *x = y->left;
    Region *T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;
}

static Region *rotate_left(Region *x) {
    Region *y = x->right;
    Region *T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;

    return y;
}

static int get_balance(Region *node) {
    return node ? height(node->left) - height(node->right) : 0;
}

Region *region_tree_insert(Region *node, Region *new_region) {
    if (!new_region || new_region->end <= new_region->start)
        return node; // invalid region

    if (new_region->end > USERSPACE_LIMIT)
        return node; // kernel-space mapping not allowed

    if (!node)
        return new_region;

    if (new_region->start < node->start) {
        node->left = region_tree_insert(node->left, new_region);
    } else {
        node->right = region_tree_insert(node->right, new_region);
    }

    node->height = 1 + max(height(node->left), height(node->right));

    int balance = get_balance(node);

    // Left Left
    if (balance > 1 && new_region->start < node->left->start)
        return rotate_right(node);

    // Right Right
    if (balance < -1 && new_region->start >= node->right->start)
        return rotate_left(node);

    // Left Right
    if (balance > 1 && new_region->start >= node->left->start) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    // Right Left
    if (balance < -1 && new_region->start < node->right->start) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}

Region *region_tree_lookup(Region *node, uintptr_t addr) {
    while (node) {
        if (addr < node->start) {
            node = node->left;
        } else if (addr >= node->end) {
            node = node->right;
        } else {
            return node; // found
        }
    }
    return NULL; // not found
}

void region_tree_visit_all(Region *node, void (*fn)(Region *, void *),
                           void *data) {
    if (!node)
        return;
    region_tree_visit_all(node->left, fn, data);
    fn(node, data);
    region_tree_visit_all(node->right, fn, data);
}

bool region_tree_resize(Region *node, uintptr_t new_end) {
    if (!node || new_end <= node->start || new_end > USERSPACE_LIMIT)
        return false;
    node->end = new_end;
    return true;
}

static Region *min_value_node(Region *node) {
    Region *current = node;
    while (current && current->left)
        current = current->left;
    return current;
}

Region *region_tree_remove(Region *root, uintptr_t start) {
    if (!root)
        return NULL;

    if (start < root->start) {
        root->left = region_tree_remove(root->left, start);
    } else if (start > root->start) {
        root->right = region_tree_remove(root->right, start);
    } else {
        if (!root->left || !root->right) {
            Region *temp = root->left ? root->left : root->right;
            slab_free(root);
            return temp;
        } else {
            Region *temp = min_value_node(root->right);
            root->start = temp->start;
            root->end = temp->end;
            root->flags = temp->flags;
            root->right = region_tree_remove(root->right, temp->start);
        }
    }

    root->height = 1 + max(height(root->left), height(root->right));
    int balance = get_balance(root);

    if (balance > 1 && get_balance(root->left) >= 0)
        return rotate_right(root);

    if (balance > 1 && get_balance(root->left) < 0) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }

    if (balance < -1 && get_balance(root->right) <= 0)
        return rotate_left(root);

    if (balance < -1 && get_balance(root->right) > 0) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }

    return root;
}

void region_tree_free_all(Region **root) {
    if (!root || !*root)
        return;

    Region *node = *root;
    Region *left = node->left;
    Region *right = node->right;

    region_tree_free_all(&left);
    region_tree_free_all(&right);
    slab_free(node);

    *root = nullptr;
}
