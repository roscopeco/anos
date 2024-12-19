#include <stdbool.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "pmm/pagealloc.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"

#define NULL (((void *)0))

static uintptr_t _fba_begin;
static uint64_t _fba_size_blocks;
static uint64_t _fba_bitmap_size_blocks;

#ifdef UNIT_TESTS
uintptr_t test_fba_check_begin() { return _fba_begin; }

uint64_t test_fba_check_size() { return _fba_size_blocks; }
#endif

extern MemoryRegion *physical_region;

bool fba_init(uint64_t *pml4, uintptr_t fba_begin, uint64_t fba_size_blocks) {
    if ((fba_begin & 0xfff) != 0) { // begin must be page aligned
        return false;
    }

    if ((fba_size_blocks & 0x7fff) !=
        0) { // block count must be multiple of full-page bitmap size
        return false;
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

    return true;
}

void *fba_alloc_blocks(uint32_t count) { return NULL; }

void *fba_alloc_block() { return NULL; }

void fba_free(void *block) {}