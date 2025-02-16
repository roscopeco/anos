/*
 * stage3 - Process address space handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef UNIT_TESTS
#include "mock_cpu.h"
#include "mock_recursive.h"
#else
#include "kdrivers/cpu.h"
#include "vmm/recursive.h"
#endif

#include "pmm/pagealloc.h"
#include "spinlock.h"
#include "structs/ref_count_map.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_ADDR_SPACE
#if __STDC_HOSTED__ == 1
#include <stdio.h>
#define debugstr(...) printf(__VA_ARGS__)
#define printhex8(arg, ignore) printf("0x%02x", arg)
#define printhex16(arg, ignore) printf("0x%04x", arg)
#define printhex32(arg, ignore) printf("0x%08x", arg)
#define printhex64(arg, ignore) printf("0x%016x", arg)
#define printdec(arg, ignore) printf("%d", arg)
#else
#include "debugprint.h"
#include "printdec.h"
#include "printhex.h"
#endif
#else
#define debugstr(...)
#define printhex8(...)
#define printhex16(...)
#define printhex32(...)
#define printhex64(...)
#define printdec(...)
#endif

#ifndef NULL
#define NULL (((void *)0))
#endif

#define KERNEL_BEGIN_ENTRY ((RECURSIVE_ENTRY + 2))

static SpinLock address_space_lock;

extern MemoryRegion *physical_region;

bool address_space_init(void) {
    PageTable *pml4 = vmm_recursive_find_pml4();

    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        if ((pml4->entries[i] & PRESENT) == 0) {

            // Allocate a page for this PDPT
            uintptr_t new_pdpt = page_alloc(physical_region);
            if (new_pdpt & 0xff) {
                // failed
                return false;
            }

            // Set up the new PDPT
            pml4->entries[i] = new_pdpt | WRITE | PRESENT;

            // Get a vaddr for this new table and invalidate TLB (just in case)
            uint64_t *vaddr = (uint64_t *)vmm_recursive_find_pdpt(i);
            cpu_invalidate_page((uintptr_t)vaddr);

            // Zero out the new table
            for (int i = 0; i < 512; i++) {
                vaddr[i] = 0;
            }
        }
    }

    return true;
}

uintptr_t address_space_create(uintptr_t shared_space_start,
                               uint64_t shared_space_len) {
    // align shared space start
    shared_space_start &= ~(0xfff);
    uintptr_t shared_space_end = shared_space_start + shared_space_len;

    // Don't let them copy kernel space (even though we are anyhow)
    if (shared_space_end >= VM_KERNEL_SPACE_START) {
        return 0;
    }

    uintptr_t new_pml4_phys = page_alloc(physical_region);

    if (new_pml4_phys & 0xff) {
        return 0;
    }

    spinlock_lock(&address_space_lock);
    uint64_t intr = save_disable_interrupts();

    // Find current pml4
    PageTable *current_pml4 = vmm_recursive_find_pml4();

    // Map in the new one to the "other" spot
    uintptr_t saved_other = current_pml4->entries[RECURSIVE_ENTRY_OTHER];
    current_pml4->entries[RECURSIVE_ENTRY_OTHER] =
            new_pml4_phys | WRITE | PRESENT;

    printhex64(saved_other, debugchar);
    debugstr("\n");
    printhex64((uintptr_t)current_pml4, debugchar);
    debugstr("\n");
    printhex64(current_pml4->entries[RECURSIVE_ENTRY_OTHER], debugchar);
    debugstr("\n");

    // Invalidate the TLB for that table
    PageTable *new_pml4_virt = vmm_recursive_find_pdpt(
            RECURSIVE_ENTRY_OTHER); // Not actually a PDPT, but our new PML4...
    cpu_invalidate_page((uintptr_t)new_pml4_virt);

    debugstr("new_pml4_virt is ");
    printhex64((uintptr_t)new_pml4_virt, debugchar);
    debugstr("\n");

    // Zero out userspace
    for (int i = 0; i < RECURSIVE_ENTRY; i++) {
#ifdef DEBUG_ADDRESS_SPACE_CREATE_COPY_ALL
        new_pml4_virt->entries[i] = current_pml4->entries[i];
    }
#else
        new_pml4_virt->entries[i] = 0;
    }

    // Set up recursive entries - we need both while we're mapping between spaces...
    // NOTE THIS WELL - Because you seem to forget it quite often - the mapping
    // functions always need the other table's 'other' recursive slot to be mapped
    // when working with an address space as the 'other' address space.
    //
    new_pml4_virt->entries[RECURSIVE_ENTRY] = new_pml4_phys | WRITE | PRESENT;
    new_pml4_virt->entries[RECURSIVE_ENTRY_OTHER] =
            new_pml4_phys | WRITE | PRESENT;

    // copy kernel space
    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        new_pml4_virt->entries[i] = current_pml4->entries[i];
    }

    for (uintptr_t ptr = shared_space_start; ptr < shared_space_end;
         ptr += VM_PAGE_SIZE) {
        debugstr("Copying ");
        printhex64(ptr, debugchar);
        debugstr("\n");

        uintptr_t shared_phys = vmm_virt_to_phys_page(ptr);

        if (shared_phys) {
            vmm_map_page_in(new_pml4_virt, ptr, shared_phys, PRESENT);

            // TODO pmm_free_shareable(page) needs implementing to check this and handle appropriately...
            //
            refcount_map_increment(shared_phys);

            debugstr("    Copied a page...\n");
        } else {
            debugstr("    [... skipped, not present]\n");
        }
    }
#endif

    // Zero out other recursive
    new_pml4_virt->entries[RECURSIVE_ENTRY_OTHER] = 0;

    current_pml4->entries[RECURSIVE_ENTRY_OTHER] = saved_other;
    cpu_invalidate_page((uintptr_t)new_pml4_virt);
    spinlock_unlock(&address_space_lock);
    restore_saved_interrupts(intr);

    return new_pml4_phys;
}