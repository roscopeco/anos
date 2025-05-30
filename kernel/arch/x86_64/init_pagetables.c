/*
 * stage3 - Kernel page table initialisation for x86_64
 * anos - An Operating System
 *
 * Now we're into the kernel proper, we no longer need the identity
 * mapping that was set up by the early-boot. We also need to set
 * up some mappings to support the PMM stack and other kernel
 * things.
 *
 * This expects that the page-tables are currently the mimimal ones
 * set up by the bootloader (see stage2/init_pagetables.asm) or our
 * UEFI entrypoint code (arch/x86_64/entrypoints/limine_entrypoint.c).
 *
 * This doesn't move (or replace entirely) the tables - the PML4,
 * PDPT and PD for the top 2GiB will stay where they are. They
 * will be changed, however:
 *
 * * The PML4 entry mapping the bottom of the address space will be removed
 * * The PDPT entry mapping the bottom of the address space will be removed
 *
 * In both the above, the entries that map the first 2MiB physical
 * to the start of the top 2GiB will be left alone, since that's where
 * the kernel is going to live (and where this code is running from, so
 * changing it would be ... a mistake 😅).
 *
 * * A new PDPT entry mapping the bottom part of the top PML4 will be added
 *
 * This is where the PMM stack will live - I'm reserving the bottom 128GiB
 * of the top 512GiB (so, the top PML4) for this - starting at
 * 0xFFFFFF8000000000.
 *
 * This seems excessive, but will allow supporting up to 128TiB RAM, even in
 * the worst case of it being fully fragmented. Address-space is cheap,
 * so why not 🤷‍♂️ (and to be fair, this might not be a long-term
 * solution, but it does the job for now...)
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "vmm/vmmapper.h"

uint64_t *pagetables_init() {
    // These are the static pagetables that were set up during init.
    // They'll become the tables that belong to the SYSTEM process...
    //
    uint64_t *pml4 = (uint64_t *)(STATIC_KERNEL_SPACE + 0x9c000);
    uint64_t *pdpt = (uint64_t *)(STATIC_KERNEL_SPACE + 0x9d000);
    uint64_t *pd = (uint64_t *)(STATIC_KERNEL_SPACE + 0x9e000);

    // Remove the bottom of the address-space identity mapping that
    // was set up by the bootloader, it's no longer needed...
    *pml4 = 0;
    *pdpt = 0;

    // Map the second 2MiB at the bottom of RAM into kernel space,
    // immediately following the 2MiB mapped by stage2.
    //
    // This should only be temporary, to allow us to easily access
    // that bit of RAM during kernel init...
    //
    // Use 0x98000 as the page table
    uint64_t *newpt = (uint64_t *)(STATIC_KERNEL_SPACE + 0x98000);
    for (int i = 0; i < 0x200; i++) {
        newpt[i] = ((i << 12) + 0x200000) | PG_PRESENT | PG_WRITE;
    }

    // And hook it into the page directory as the second 2MiB
    pd[1] = 0x98000 | PG_PRESENT | PG_WRITE;

    // Set up initial page directory and table for the PMM stack.
    // Might as well use the 8KiB below the existing page tables,
    // and only mapping one page for now, just to give the PMM
    // room to start - once it's running additional mapping will be
    // done by the page fault handler as needed...
    uint64_t *pmm_pd = (uint64_t *)(STATIC_KERNEL_SPACE + 0x9a000);
    uint64_t *pmm_pt = (uint64_t *)(STATIC_KERNEL_SPACE + 0x9b000);

    // Zero them out
    for (int i = 0; i < 0x400; i++) {
        pmm_pd[i] = 0;
    }

    // Map the new table into the directory, with physical address
    pmm_pd[0] = 0x9b000 | PG_PRESENT | PG_WRITE;

    // Map the physical page below these page tables as the PMM bootstrap
    // page - this will contain the region struct and first bit of the
    // stack.
    pmm_pt[0] = 0x99000 | PG_PRESENT | PG_WRITE;

    // Hook this into the PDPT
    pdpt[0] = 0x9a000 | PG_PRESENT | PG_WRITE;

    // Just load cr3 to dump the TLB...
    __asm__ volatile("mov %%cr3, %%rax\n\t"
                     "mov %%rax, %%cr3\n\t"
                     :
                     :
                     : "rax", "memory");

    return pml4;
}