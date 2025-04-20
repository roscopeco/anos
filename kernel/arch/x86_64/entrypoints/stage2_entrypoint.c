/*
 * stage3 - Kernel entry point from STAGE2 bootloader
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "machine.h"
#include "x86_64/acpitables.h"
#include "x86_64/entrypoints/common.h"
#include "x86_64/init_pagetables.h"
#include "x86_64/pmm/config.h"

#include "debugprint.h"

#ifdef DEBUG_MEMMAP
void debug_memmap_e820(E820h_MemMap *memmap);
#else
#define debug_memmap_e820(...)
#endif

#ifndef VRAM_VIRT_BASE
#define VRAM_VIRT_BASE ((char *const)0xffffffff800b8000)
#endif

noreturn void bsp_kernel_entrypoint(uintptr_t rsdp_phys);

noreturn void bsp_kernel_entrypoint_bios(uintptr_t rsdp_phys,
                                         E820h_MemMap *memmap) {
    debugterm_init(VRAM_VIRT_BASE, 0, 0);

    init_kernel_gdt();
    install_interrupts();
    pagetables_init();

    physical_region =
            page_alloc_init_e820(memmap, PMM_PHYS_BASE, STATIC_PMM_VREGION);

    debug_memmap_e820(memmap);

    bsp_kernel_entrypoint(rsdp_phys);
}
