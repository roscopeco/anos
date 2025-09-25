/*
 * stage3 - Process address space handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "pmm/pagealloc.h"
#include "process/address_space.h"
#include "sched.h"
#include "smp/state.h"
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

#define KERNEL_BEGIN_ENTRY FIRST_KERNEL_PML4E

static SpinLock address_space_lock;

extern MemoryRegion *physical_region;

bool address_space_init(void) {
    PageTable *pml4 = vmm_find_pml4();

    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        if ((pml4->entries[i] & PG_PRESENT) == 0) {

            // Allocate a page for this PDPT
            const uintptr_t new_pdpt = page_alloc(physical_region);
            if (new_pdpt & 0xff) {
                // failed
                return false;
            }

            // Set up the new PDPT
            pml4->entries[i] = vmm_phys_and_flags_to_table_entry(new_pdpt, PG_READ | PG_WRITE | PG_PRESENT);

            // Get a vaddr for this new table and invalidate TLB (just in case)
            uint64_t *vaddr = (uint64_t *)vmm_phys_to_virt(new_pdpt);
            cpu_invalidate_tlb_addr((uintptr_t)vaddr);

            // Zero out the new table
            for (int j = 0; j < 512; j++) {
                vaddr[j] = 0;
            }
        }
    }

    return true;
}

uintptr_t address_space_create(uintptr_t init_stack_vaddr, const size_t init_stack_len, const int region_count,
                               AddressSpaceRegion regions[], const int stack_value_count,
                               const uint64_t *stack_values) {

    // align stack vaddr
    init_stack_vaddr &= ~(0xfff);
    const uintptr_t init_stack_end = init_stack_vaddr + init_stack_len;

#ifdef CONSERVATIVE_BUILD
    // Only doing these checks in conservative build, syscall.c checks args
    // coming from userspace anyhow...
    //

    // Don't let them explicitly map kernel space (even though we are anyhow)
    if (init_stack_vaddr >= VM_KERNEL_SPACE_START || init_stack_end >= VM_KERNEL_SPACE_START) {
        return 0;
    }

    // Don't let them pass more than our allowed number of initial stack
    // values
    if (stack_value_count > init_stack_len / sizeof(uintptr_t) || stack_value_count > MAX_STACK_VALUE_COUNT) {
        return 0;
    }

    // align shared regions
    for (int i = 0; i < region_count; i++) {
        const AddressSpaceRegion *ptr = &regions[i];

        if (((uintptr_t)ptr->start) > VM_KERNEL_SPACE_START) {
            return 0;
        }

        if (((uintptr_t)ptr->start) + sizeof(AddressSpaceRegion) > VM_KERNEL_SPACE_START) {
            return 0;
        }

        if (ptr->start & 0xfff) {
            return 0;
        }

        if (ptr->len_bytes & 0xfff) {
            return 0;
        }
    }
#endif

    // NOTE: pagetable memory is **not** process-owned.
    const uintptr_t new_pml4_phys = page_alloc(physical_region);

    if (new_pml4_phys & 0xff) {
        debugstr("Unable to allocate new PML4");
        return 0;
    }

    const uint64_t lock_flags = spinlock_lock_irqsave(&address_space_lock);

    // Find current pml4
    PageTable *current_pml4 = vmm_find_pml4();

    // Invalidate the TLB for that table
    PageTable *new_pml4_virt = (PageTable *)vmm_phys_to_virt(new_pml4_phys);
    cpu_invalidate_tlb_addr((uintptr_t)new_pml4_virt);

    debugstr("new_pml4_virt is ");
    printhex64((uintptr_t)new_pml4_virt, debugchar);
    debugstr("\n");

    // Zero out userspace
    for (int i = 0; i < FIRST_KERNEL_PML4E; i++) {
#ifdef DEBUG_ADDRESS_SPACE_CREATE_COPY_ALL
        new_pml4_virt->entries[i] = current_pml4->entries[i];
    }
#else
        new_pml4_virt->entries[i] = 0;
    }

    // copy kernel space
    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        new_pml4_virt->entries[i] = current_pml4->entries[i];
    }

    // map shared regions
    debugstr("There are ");
    printhex8(region_count, debugchar);
    debugstr(" shared region(s)\n");
    for (int i = 0; i < region_count; i++) {
        const uintptr_t region_end = regions[i].start + regions[i].len_bytes;

        for (uintptr_t ptr = regions[i].start; ptr < region_end; ptr += VM_PAGE_SIZE) {

            debugstr("Copying ");
            printhex64(ptr, debugchar);
            debugstr("\n");

            const uintptr_t shared_phys = vmm_virt_to_phys_page(ptr);

            if (shared_phys) {
                // TODO what if this fails (to alloc table pages)?
                //
                // TODO don't always map these PG_EXEC, pass flags instead!
                vmm_map_page_in((uint64_t *)new_pml4_virt, ptr, shared_phys,
                                PG_PRESENT | PG_READ | PG_USER | PG_EXEC | PG_COPY_ON_WRITE);

                // TODO pmm_free_shareable(page) needs implementing to check this and handle appropriately...
                //
                refcount_map_increment(shared_phys);

                debugstr("    Copied a page mapping as COW...\n");
            } else {
                debugstr("    [... skipped, not present]\n");
            }
        }
    }
#endif

    // We track the (up to) 32 physical pages at the top of the stack,
    // so we can use them below when copying stack initial values...
    //
    // We have 33 pages since argc data is allowed to be 128KiB,
    // or 32 pages, so we keep an extra one for the capabilities etc...
    //
    uint64_t top_phys_stack_pages[INIT_STACK_ARG_PAGES_COUNT];

    // sort out the requested initial stack, we allocate it in reverse so
    // that the ARG_PAGES pages we store in the top_phys_stack_pages array will
    // actually be the ones at the top of the stack...
    //
    // TODO should only allocate one here, and let #PF handler sort the rest...
    if (init_stack_len) {
        uint8_t current_top_phys_page_idx = 0;

        for (uintptr_t ptr = init_stack_end - VM_PAGE_SIZE; ptr >= init_stack_vaddr; ptr -= VM_PAGE_SIZE) {

            const uintptr_t stack_page = page_alloc(physical_region);

            if (stack_page & 0xff) {
                debugstr("Failed to allocate stack page for ");
                printhex64(ptr, debugchar);
                debugstr("\n");

                // TODO there's a bit to sort out here...
                //
                //   * Free the pages we've allocated so far
                //   * Free the page tables for the address space
                //   * Free the address space itself
                //
                // This will need a proper free_address_space routine, which I don't
                // have yet, so we'll just fail and leak the memory for now...

                cpu_invalidate_tlb_addr((uintptr_t)new_pml4_virt);
                spinlock_unlock_irqrestore(&address_space_lock, lock_flags);

                return 0;
            }

            // record this if we're still in arg pages, since we might need it for
            // copying stack init values later...
            if (current_top_phys_page_idx < INIT_STACK_ARG_PAGES_COUNT) {
                top_phys_stack_pages[current_top_phys_page_idx++] = stack_page;
            }

            vmm_map_page_in((uint64_t *)new_pml4_virt, ptr, stack_page, PG_READ | PG_WRITE | PG_PRESENT | PG_USER);
        }
    }

    // TODO this is the wrong place to do this, really...
    //
    // Copy in the requested initial stack values to the bottom
    // stack page. We'll need to temporarily map it.

    // TODO potential race condition here, if we get rescheduled onto
    // a different CPU, and this CPU then goes on to do stuff that
    // needs the temp mapping, this will go wrong.
    //
    // Could be done with locking or a crititcal section of some
    // sort, but I dislike the whole per-CPU temp mapping idea tbh, need
    // to come up with something better...
    //
    // (it's actually worse than that - all it needs is for us to get
    // interrupted by some other thread in this process that also uses the temp
    // mapping page, and we're done - it's global (to the address space) state.

    const PerCPUState *state = state_get_for_this_cpu();
    const uintptr_t per_cpu_temp_page = vmm_per_cpu_temp_page_addr(state->cpu_id);

    uint64_t volatile *temp_stack_bottom = (uint64_t *)per_cpu_temp_page;

    for (int i = stack_value_count - 1; i >= 0; i--) {
        if (temp_stack_bottom == (uint64_t *)per_cpu_temp_page) {
            // reached bottom of temp page, need to map the next one
            const uintptr_t phys = top_phys_stack_pages[i >> 9];
            vmm_map_page(per_cpu_temp_page, phys, PG_READ | PG_WRITE | PG_PRESENT);
            temp_stack_bottom = (uint64_t *)(per_cpu_temp_page + VM_PAGE_SIZE);
        }

        *--temp_stack_bottom = stack_values[i];
    }

    vmm_unmap_page(per_cpu_temp_page);

    cpu_invalidate_tlb_addr((uintptr_t)new_pml4_virt);
    spinlock_unlock_irqrestore(&address_space_lock, lock_flags);

    return new_pml4_phys;
}