/*
 * stage3 - The fixed-block allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "pmm/pagealloc.h"
#include "spinlock.h"
#include "structs/bitmap.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"

#define tprintf(...)
#define NULL (((void *)0))

#define SPIN_LOCK()                                                            \
    do {                                                                       \
        spinlock_lock(&fba_lock);                                              \
    } while (0)

#define SPIN_UNLOCK_RET(retval)                                                \
    do {                                                                       \
        spinlock_unlock(&fba_lock);                                            \
        return (retval);                                                       \
    } while (0)

static uint64_t *_pml4;
static uintptr_t _fba_begin;
static uint64_t _fba_size_blocks;
static uint64_t _fba_bitmap_size_blocks;
static uint64_t *_fba_bitmap, *_fba_bitmap_end;
static SpinLock fba_lock;

#ifdef UNIT_TESTS
uintptr_t test_fba_check_begin() { return _fba_begin; }
uint64_t test_fba_check_size() { return _fba_size_blocks; }
uint64_t *test_fba_bitmap() { return _fba_bitmap; }
uint64_t *test_fba_bitmap_end() { return _fba_bitmap_end; }

#ifdef DEBUG_UNIT_TESTS
#include <stdio.h>
#undef tprintf
#define tprintf(...) tprintf(__VA_ARGS__)
#endif
#endif

extern MemoryRegion *physical_region;

bool fba_init(uint64_t *pml4, uintptr_t fba_begin, uint64_t fba_size_blocks) {
    if ((fba_begin & 0xfff) != 0) { // begin must be page aligned
        return false;
    }

    if ((fba_size_blocks & 0x7fff) != 0) {
        // block count must be multiple of full-page bitmap size
        return false;
    }

    if (fba_size_blocks == 0) {
        // valid, but noop
        return true;
    }

    uint64_t bitmap_page_count = fba_size_blocks >> 15;
    uint64_t bitmap_page_end = fba_begin + (bitmap_page_count << 12);

    for (uintptr_t virt = fba_begin; virt < bitmap_page_end; virt += 0x1000) {
        uint64_t phys = page_alloc(physical_region);

        if (phys & 0xfff) {
            // not page aligned, signals error.
            //
            // TODO we maybe should free all allocated pages so far if this
            // happens, but the expectation is that we'll panic if this fails so
            // not bothering for now...
            return false;
        }

        vmm_map_page(pml4, virt, phys, PRESENT | WRITE);
    }

    _fba_begin = fba_begin;
    _fba_size_blocks = fba_size_blocks;
    _fba_bitmap_size_blocks = bitmap_page_count;

    // Mark blocks used by the bitmap as in-use
    for (int i = 0; i < bitmap_page_count; i++) {
        bitmap_set((uint64_t *)fba_begin, i);
    }

    _fba_bitmap = (uint64_t *)_fba_begin;
    _fba_bitmap_end = _fba_bitmap + (bitmap_page_count << 9);

    _pml4 = pml4;

    return true;
}

static inline void *do_alloc(uintptr_t block_address) {
    uint64_t phys = page_alloc(physical_region);

    if (phys & 0xfff) {
        // not page aligned, signals error.
        return NULL;
    }

    vmm_map_page(_pml4, block_address, phys, PRESENT | WRITE);

    // TODO invalidate TLB...

    return (void *)block_address;
}

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
                          ((uint64_t)(0x03F79D71B4CB0A89))) >>
                         58];
#endif
}

void *fba_alloc_blocks(uint32_t count) {
    // tprintf("bmp     = %p\n", _fba_bitmap);
    // tprintf("bmp_end = %p\n", _fba_bitmap_end);

    // SPIN_LOCK();

    // uint64_t *bmp;
    // for (bmp = _fba_bitmap; bmp < _fba_bitmap_end; bmp++) {
    //     if (*bmp != 0xffffffffffffffff) {
    //         tprintf("Block %p has space [0x%016lx]!\n", bmp, *bmp);
    //         break;
    //     }
    // }

    // if (bmp == _fba_bitmap_end) {
    //     tprintf("All blocks are full\n");
    //     SPIN_UNLOCK_RET(NULL);
    // }

    // for (int bit = 0; bit < 64; bit++) {
    //     if (!(*bmp & (1ULL << bit))) {
    //         // Found the first unset bit
    //         bitmap_set(bmp, bit);
    //         uintptr_t block_address = _fba_begin + ((bmp - _fba_bitmap) * 64
    //         + bit) * VM_PAGE_SIZE;

    //         SPIN_UNLOCK_RET(do_alloc(block_address));
    //     }
    // }

    // SPIN_UNLOCK_RET(NULL);

    return NULL;
}

void *fba_alloc_block() {
    tprintf("bmp     = %p\n", _fba_bitmap);
    tprintf("bmp_end = %p\n", _fba_bitmap_end);

    SPIN_LOCK();

    uint64_t *bmp;
    for (bmp = _fba_bitmap; bmp < _fba_bitmap_end; bmp++) {
        if (*bmp != 0xffffffffffffffff) {
            tprintf("Block %p has space [0x%016lx]!\n", bmp, *bmp);
            break;
        }
    }

    if (bmp == _fba_bitmap_end) {
        tprintf("All blocks are full\n");
        SPIN_UNLOCK_RET(NULL);
    }

    int bit = first_set_bit_64(~(*bmp));
    bitmap_set(bmp, bit);
    uintptr_t block_address =
            _fba_begin + ((bmp - _fba_bitmap) * 64 + bit) * VM_PAGE_SIZE;
    SPIN_UNLOCK_RET(do_alloc(block_address));
}

void fba_free(void *block) {}