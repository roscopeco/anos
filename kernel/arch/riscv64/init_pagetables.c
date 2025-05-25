/*
 * stage3 - Kernel page table initialisation for RISC-V
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 *
 * This expects that the page-tables are currently the mimimal ones
 * set up by the RISC-V entrypoint code (see 
 * arch/riscv64/entrypoints/limine_entrypoint.c).
 *
 * This doesn't move (or replace entirely) the tables - the PML4,
 * PDPT and PD for the top 2GiB will stay where they are, and will
 * remain the ones that are statically allocated in reserved memory.
 *
 * The entries that map that physical memory to the kernel's VMA will
 * be left alone, since that's where the kernel is going to live (and 
 * where this code is running from, so* changing it would be ... a mistake 😅).
 *
 * A new PDPT entry mapping the bottom part of the top PML4 will be added -
 * this is where the PMM stack will live. This is compatible with the 
 * x86_64 layout, see notes in the init_pagetables.c for that arch, and 
 * in the MemoryMap.md, for a few notes on the design and tradeoffs etc.
 */

#include <stdint.h>

#include "vmm/vmmapper.h"

uint64_t *pagetables_init(void) {

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

    return (uint64_t *)(STATIC_KERNEL_SPACE + 0x9c000);
}