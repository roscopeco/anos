/*
 * region_tree.h - AVL-based memory region tracking
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This header defines the RegionTree structure and API for managing
 * memory regions within user address spaces. It uses a self-balancing AVL tree
 * to allow fast (O(log n)) lookups, insertions, and deletions of memory
 * regions, with minimal overhead and no heap usage required.
 *
 * Intended use cases include:
 *   - Tracking heap or mmap'd memory regions in a process
 *   - Validating user pointers during syscalls
 *   - Efficiently locating regions on page faults
 *
 * Regions are defined by a start and end (exclusive) address, and may
 * carry optional metadata. Regions must lie entirely within user space
 * (i.e., below USERSPACE_LIMIT), or insertion/resizing will be rejected.
 */

#ifndef __ANOS_KERNEL_REGION_TREE_H
#define __ANOS_KERNEL_REGION_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "anos_assert.h"

#define USERSPACE_LIMIT 0x8000000000000000ULL

/* Region node - managed in AVL tree */
typedef struct Region {
    uintptr_t start; // Start of region (inclusive)
    uintptr_t end;   // End of region (exclusive)
    uint64_t flags;  // type etc

    struct Region *left;  // Left child (lower addresses)
    struct Region *right; // Right child (higher addresses)
    uint64_t height;      // AVL tree node height

    uint64_t reserved[2];
} Region;

static_assert_sizeof(Region, ==, 64);

/*
 * region_tree_insert - Insert a region into the tree
 * Returns the new root of the tree. If the region is invalid (e.g., kernel space
 * or end <= start), the original tree is returned unchanged.
 */
Region *region_tree_insert(Region *node, Region *new_region);

/*
 * region_tree_lookup - Find the region containing a given address
 * Returns the region node, or NULL if not found.
 */
Region *region_tree_lookup(Region *node, uintptr_t addr);

/*
 * region_tree_remove - Remove the region with the specified start address
 * Returns the new root of the tree. No-op if not found.
 */
Region *region_tree_remove(Region *root, uintptr_t start);

/*
 * region_tree_visit_all - In-order traversal of all regions
 * Calls the provided function on each region.
 */
void region_tree_visit_all(Region *node, void (*fn)(Region *, void *),
                           void *data);

/*
 * region_tree_resize - Update the end of an existing region
 * Returns true on success, or false if new_end is invalid
 * (e.g., <= start or exceeds USERSPACE_LIMIT).
 */
bool region_tree_resize(Region *node, uintptr_t new_end);

/*
 * region_tree_free_all - Free a whole region tree.
 */
void region_tree_free_all(Region **root);

/*
 * region_tree_maybe_coalesce - (Optional) Merge adjacent compatible regions
 * This function is not implemented yet, but may be used in future if user
 * syscall patterns lead to excessive fragmentation.
 */
// void region_tree_maybe_coalesce(Region **root); // (not yet implemented)

#endif // __ANOS_KERNEL_REGION_TREE_H
