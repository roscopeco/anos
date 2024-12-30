/*
 * stage3 - The slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "slab/alloc.h"
#include "fba/alloc.h"
#include "ktypes.h"
#include "spinlock.h"
#include "structs/bitmap.h"
#include <stdbool.h>
#include <stdint.h>

#define NULL (((void *)0))

#define SPIN_LOCK()                                                            \
    do {                                                                       \
        spinlock_lock(&slab_lock);                                             \
    } while (0)

#define SPIN_UNLOCK_RET(retval)                                                \
    do {                                                                       \
        spinlock_unlock(&slab_lock);                                           \
        return (retval);                                                       \
    } while (0)

static ListNode *partial;
static ListNode *full;

static SpinLock slab_lock;

static const uint8_t FBA_BLOCKS_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

bool slab_alloc_init() { return true; }

static inline uint8_t first_set_bit_64(uint64_t nonzero_uint64) {
#if defined(__GNUC__) || defined(__clang__)
    // GCC & Clang have a nice intrinsic for this, as long as value is never
    // zero...
    return __builtin_ctzll(nonzero_uint64);
#else
    // Otherwise, fallback to De Bruijn sequence...
    static const int DeBruijnTable[64] = {
            0,  1,  48, 2,  57, 49, 28, 3,  61, 58, 50, 42, 38, 29, 17, 4,
            62, 55, 59, 36, 53, 51, 43, 22, 45, 39, 33, 30, 24, 18, 12, 5,
            63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
            46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9,  13, 8,  7,  6};

    // Isolate least significant set bit and multiply by De Bruijn constant
    return DeBruijnTable[((nonzero_uint64 & -nonzero_uint64) *
                          ((uint64_t)(0x03f79d71b4cb0a89))) >>
                         58];
#endif
}

void *slab_alloc_block() {
    Slab *target = NULL;

    SPIN_LOCK();

    if (partial == NULL) {
        // No partial slabs - allocate a new one..
        target = (Slab *)fba_alloc_blocks_aligned(FBA_BLOCKS_PER_SLAB,
                                                  FBA_BLOCKS_PER_SLAB);

        if (target == NULL) {
            // alloc failed
            SPIN_UNLOCK_RET(NULL);
        }

        // zero out header
        target->this.next = NULL;
        target->this.type = KTYPE_SLAB_HEADER;
        target->bitmap0 = 1; // first block always allocated
        target->bitmap1 = 0;
        target->bitmap2 = 0;
        target->bitmap3 = 0;

        partial = list_insert_after(NULL, (ListNode *)target);
    } else {
        // Use the first partial slab...
        target = (Slab *)partial;
    }

    // find free block
    int free_block = 0;
    if (target->bitmap0 != 0xffffffffffffffff) {
        free_block = first_set_bit_64(~(target->bitmap0));
    } else if (target->bitmap1 != 0xffffffffffffffff) {
        free_block = first_set_bit_64(~(target->bitmap1)) + 64;
    } else if (target->bitmap2 != 0xffffffffffffffff) {
        free_block = first_set_bit_64(~(target->bitmap2)) + 128;
    } else if (target->bitmap3 != 0xffffffffffffffff) {
        free_block = first_set_bit_64(~(target->bitmap3)) + 192;
    } else {
        // TODO warn about this, full block in partial list...
        // (or maybe just panic, since we're going to be in an indeterminite state now anyway...)
        SPIN_UNLOCK_RET(NULL);
    }

    bitmap_set(&target->bitmap0, free_block);

    if (target->bitmap0 == 0xffffffffffffffff &&
        target->bitmap1 == 0xffffffffffffffff &&
        target->bitmap2 == 0xffffffffffffffff &&
        target->bitmap3 == 0xffffffffffffffff) {
        // TODO probably ought to just implement push / pop (front) in list.c...
        partial = target->this.next;
        target->this.next = full;
        full = (ListNode *)target;
    }

    SPIN_UNLOCK_RET((void *)(target + free_block));
}

void slab_free_block(void *block) {
    Slab *slab = slab_base(block);

    if (!slab) {
        // Not in FBA, so not a slab.
        // TODO warn or something, this should always be a bug...
        return;
    }

    uint64_t block_num = ((Slab *)block) - slab;

    if (block_num == 0) {
        // we can't free the bitmap!
        return;
    }

    spinlock_lock(&slab_lock);

    if (slab->bitmap0 == 0xffffffffffffffff &&
        slab->bitmap1 == 0xffffffffffffffff &&
        slab->bitmap2 == 0xffffffffffffffff &&
        slab->bitmap3 == 0xffffffffffffffff) {

        // This slab is in the full list, we need to find it and remove it
        // TODO this is stupid inefficient in the worst case if the lists get big...

        if (full == (ListNode *)slab) {
            // simple case - just popping head and add to partial list
            full = slab->this.next;
            slab->this.next = partial;
            partial = (ListNode *)slab;
        } else {
            // this is the inefficient bit... I don't especially want to
            // go doubly-linked though...
            ListNode *prev = NULL;
            ListNode *test = full;

            while (test) {
                if (test->next == (ListNode *)slab) {
                    prev = test;
                    break;
                }
                test = test->next;
            }

            if (prev) {
                prev->next = test->next;
                test->next = partial;
                partial = test;
            } else {
                // TODO warn about this, full slab wasn't in the full list!
                // (or maybe just panic, since we're going to be in an indeterminite state now anyway...)
            }
        }
    }

    bitmap_clear(&slab->bitmap0, block_num);
    spinlock_unlock(&slab_lock);
}
