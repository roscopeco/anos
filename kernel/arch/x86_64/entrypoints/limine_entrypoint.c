/*
 * stage3 - Kernel entry point from Limine bootloader
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO neither BIOS nor UEFI boot should really be making the assumptions
 * we make about low physical memory layout... This'll undoubtedly need
 * to be redone in the future...
 */

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "framebuffer.h"
#include "machine.h"
#include "std/string.h"
#include "vmm/vmmapper.h"

#include "platform/acpi/acpitables.h"
#include "platform/bootloaders/limine.h"

#include "x86_64/entrypoints/common.h"
#include "x86_64/init_pagetables.h"
#include "x86_64/kdrivers/cpu.h"
#include "x86_64/pmm/config.h"

#ifdef DEBUG_VMM
#include "kprintf.h"
#endif

#define MAX_MEMMAP_ENTRIES 128

// NOTE: These **must** be kept in-step with STAGE2's `init_pagetables.asm`!
#define PM4_START ((0x9c000))
#define PDP_START ((0x9d000))
#define PD_START ((0x9e000))
#define PT_START ((0x9f000))

#define KERNEL_BSS_PHYS ((0x110000))
#define KERNEL_CODE_PHYS ((0x120000))

#define KERNEL_INIT_STACK_TOP ((STATIC_KERNEL_SPACE + KERNEL_BSS_PHYS))

#define KERNEL_FRAMEBUFFER ((0xffffffff82000000))

#ifdef DEBUG_MEMMAP
void debug_memmap_limine(Limine_MemMap *memmap);
#else
#define debug_memmap_limine(...)
#endif

// TODO migrate to revision 3. We'll need a new way to sort out the
//      ACPI tables though, since at r3 they don't get mapped into the HHDM
//      and the identity map isn't available any more - we only get the
//      phys of the RSDP so will need to not copy and just map...
//
LIMINE_BASE_REVISION(2);

static volatile Limine_MemMapRequest limine_memmap_request __attribute__((__aligned__(8))) = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 3,
};

static volatile Limine_RsdpRequest limine_rsdp_request __attribute__((__aligned__(8))) = {
        .id = LIMINE_RSDP_REQUEST,
        .revision = 3,
};

static volatile Limine_FrameBufferRequest limine_framebuffer_request __attribute__((__aligned__(8))) = {
        .id = LIMINE_FRAMEBUFFER_REQUEST,
        .revision = 3,
};

static volatile Limine_HHDMRequest limine_hhdm_request __attribute__((__aligned__(8))) = {
        .id = LIMINE_HHDM_REQUEST,
        .revision = 3,
};

static volatile Limine_ModuleRequest limine_module_request __attribute__((__aligned__(8))) = {
        .id = LIMINE_MODULE_REQUEST,
        .revision = 3,
        .internal_module_count = 0,
};

extern uint64_t _kernel_vma_start;
extern uint64_t _kernel_vma_end;
extern uint64_t _bss_end;
extern uint64_t _code;

// Defined by the linker.
extern void *_system_bin_start;

// We'll need this later when we come to map the SYSTEM image. On x86_64,
// we still keep the direct map of the bottom 2MiB, so for now we can
// just infer this...
uintptr_t _system_bin_start_phys;

// We need this later when starting system, we set it here because it's
// based on the size of the module limine loads for us...
size_t _system_bin_size;

// Global framebuffer info for bootstrap
// TODO doesn't actually need to be global any more AFAIK...
static uintptr_t g_fb_phys = 0;
static uint16_t g_fb_width = 0;
static uint16_t g_fb_height = 0;

static Limine_MemMap static_memmap;
static Limine_MemMapEntry *static_memmap_pointers[MAX_MEMMAP_ENTRIES];
static Limine_MemMapEntry static_memmap_entries[MAX_MEMMAP_ENTRIES];

// We only need an addressable rsdp for the kernel, the
// rest of the tables are mapped dynamically and the kernel
// doesn't care where they are...
static ACPI_RSDP static_rsdp;

// Externals
noreturn void bsp_kernel_entrypoint(uintptr_t rsdp_phys);
noreturn void bootstrap_trampoline(size_t system_size, uint16_t fb_width, uint16_t fb_height, uintptr_t new_stack,
                                   uintptr_t new_pt_phys, void *boing);

// Forwards
static noreturn void bootstrap_continue(size_t system_size, uint16_t fb_width, uint16_t fb_height);

noreturn void bsp_kernel_entrypoint_limine() {
    // grab stuff we need - memmap first. We'll copy it into a static buffer for ease...
    static_memmap.entries = (Limine_MemMapEntry **)&static_memmap_pointers;
    static_memmap.entry_count = limine_memmap_request.memmap->entry_count;
    if (static_memmap.entry_count > MAX_MEMMAP_ENTRIES) {
        // TODO really Ross??
        static_memmap.entry_count = MAX_MEMMAP_ENTRIES;
    }

    for (int i = 0; i < static_memmap.entry_count; i++) {
        static_memmap_pointers[i] = &static_memmap_entries[i];
        static_memmap_entries[i].base = limine_memmap_request.memmap->entries[i]->base;
        static_memmap_entries[i].length = limine_memmap_request.memmap->entries[i]->length;
        static_memmap_entries[i].type = limine_memmap_request.memmap->entries[i]->type;
    }

    // framebuffer - assume it's direct mapped so we can just subtract the offset to get its phys...
    g_fb_phys = (uintptr_t)limine_framebuffer_request.response->framebuffers[0]->address -
                limine_hhdm_request.response->offset;
    g_fb_width = limine_framebuffer_request.response->framebuffers[0]->width;
    g_fb_height = limine_framebuffer_request.response->framebuffers[0]->height;

    // RSDP
    const ACPI_RSDP *limine_rsdp = (ACPI_RSDP *)limine_rsdp_request.rsdp->address;

    for (int i = 0; i < 8; i++) {
        static_rsdp.signature[i] = limine_rsdp->signature[i];
    }

    for (int i = 0; i < 6; i++) {
        static_rsdp.oem_id[i] = limine_rsdp->oem_id[i];
    }
    static_rsdp.checksum = limine_rsdp->checksum;
    static_rsdp.extended_checksum = limine_rsdp->extended_checksum;
    static_rsdp.length = limine_rsdp->length;
    static_rsdp.revision = limine_rsdp->revision;
    static_rsdp.rsdt_address = limine_rsdp->rsdt_address;
    static_rsdp.xsdt_address = limine_rsdp->xsdt_address;

    // copy kernel (yep, we're doing that - we want a known environment and phys
    // layout, though do see comment at the top of this file about assumptions
    // about low phys memory...)
    //
    // BSS first...
    uint64_t *new_base = (uint64_t *)(limine_hhdm_request.response->offset + KERNEL_BSS_PHYS);

    memcpy(new_base, &_kernel_vma_start, ((uintptr_t)&_bss_end) - ((uintptr_t)&_kernel_vma_start));

    //
    // ... then code and data...
    new_base = (uint64_t *)(limine_hhdm_request.response->offset + KERNEL_CODE_PHYS);
    memcpy(new_base, &_code, ((uintptr_t)&_kernel_vma_end) - ((uintptr_t)&_code));

    //
    // .. and finally the system/ramfs binary. This is expected to be
    // right at the end of the kernel, per the link script.
    uint64_t module_count = 0;
    size_t system_size = 0;

    if (limine_module_request.response) {
        module_count = limine_module_request.response->module_count;
    }

    if (module_count == 1) {
        // TODO check name to make sure it's our module / we only have one!
        //
        if (limine_module_request.response->modules[0] && limine_module_request.response->modules[0]->address) {
            system_size = limine_module_request.response->modules[0]->size;
            new_base = (uint64_t *)(((uintptr_t)new_base + ((uintptr_t)&_system_bin_start) - ((uintptr_t)&_code)));

            memcpy(new_base, limine_module_request.response->modules[0]->address, system_size);
        }
    }

    // Set up the static pagetables the kernel expects to exist...
    uint64_t volatile *new_pml4 = (uint64_t *)(limine_hhdm_request.response->offset + PM4_START);
    uint64_t volatile *new_pdpt = (uint64_t *)(limine_hhdm_request.response->offset + PDP_START);
    uint64_t volatile *new_pd = (uint64_t *)(limine_hhdm_request.response->offset + PD_START);
    uint64_t volatile *new_pt = (uint64_t *)(limine_hhdm_request.response->offset + PT_START);

    // Set up the initial tables
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = 0;                                        // zero out the PML4
        new_pdpt[i] = 0;                                        // ... and the PDPT
        new_pd[i] = 0;                                          // ... as well as the PD
        new_pt[i] = (i * VM_PAGE_SIZE) | PG_PRESENT | PG_WRITE; // ... and map low mem into the PT
    }
    new_pml4[0x1ff] = PDP_START | PG_PRESENT | PG_WRITE; // Set up the entries we need for
    new_pdpt[0x1fe] = PD_START | PG_PRESENT | PG_WRITE;  // the mappings in kernel space...
    new_pd[0] = PT_START | PG_PRESENT | PG_WRITE;

    // Initialize PAT with write-combining in upper 4 entries
    cpu_write_pat(PAT_WRITE_BACK, PAT_WRITE_THROUGH, PAT_UNCACHED_MINUS, PAT_UNCACHEABLE, PAT_WRITE_COMBINING,
                  PAT_WRITE_COMBINING, PAT_WRITE_COMBINING, PAT_WRITE_COMBINING);

    // map framebuffer, as four 2MiB large pages at 0xffffffff82000000 - 0xffffffff827fffff
    // Use PAT bit for write-combining (maps to PAT entry 4+ which we set to WC above)
    new_pd[0x10] = g_fb_phys | PG_PRESENT | PG_WRITE | PG_PAGESIZE | PG_PAT_LARGE;
    new_pd[0x11] = (g_fb_phys + 0x200000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE | PG_PAT_LARGE;
    new_pd[0x12] = (g_fb_phys + 0x400000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE | PG_PAT_LARGE;
    new_pd[0x13] = (g_fb_phys + 0x600000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE | PG_PAT_LARGE;

    bootstrap_trampoline(system_size, g_fb_width, g_fb_height, KERNEL_INIT_STACK_TOP, PM4_START, bootstrap_continue);
}

static noreturn void bootstrap_continue(const size_t system_size, const uint16_t fb_width, const uint16_t fb_height) {
    // We're now on our own pagetables, and have essentially the same setup as
    // we do on entry from STAGE2 when BIOS booting.
    //
    // IOW we have a baseline environment.
    //
    // Stash this for later - because we're still mapping the kernel code space,
    // we can just infer it for now...
    _system_bin_start_phys = (uint64_t)&_system_bin_start & ~0xFFFFFFFF80000000;
    _system_bin_size = system_size;

    debugterm_init((char *)KERNEL_FRAMEBUFFER, g_fb_width, g_fb_height);

    // Store framebuffer info for syscalls
    framebuffer_set_info(g_fb_phys, KERNEL_FRAMEBUFFER, g_fb_width, g_fb_height, 32);

    init_kernel_gdt();
    install_interrupts();

    uint64_t *pml4_virt = pagetables_init();

    debug_memmap_limine(&static_memmap);

    physical_region = page_alloc_init_limine(&static_memmap, PMM_PHYS_BASE, STATIC_PMM_VREGION, true);

#ifdef DEBUG_VMM
    extern uint64_t vmm_direct_mapping_terapages_used, vmm_direct_mapping_gigapages_used,
            vmm_direct_mapping_megapages_used, vmm_direct_mapping_pages_used;

    const size_t pre_direct_free = physical_region->free;
#endif
    vmm_init_direct_mapping(pml4_virt, &static_memmap);
#ifdef DEBUG_VMM
    const size_t post_direct_free = physical_region->free;
    kprintf("\nPage tables for VMM Direct Mapping: %ld bytes of physical "
            "memory\n",
            pre_direct_free - post_direct_free);
    kprintf("    Mapping types: %ld tera; %ld giga; %ld mega; %ld small\n\n", vmm_direct_mapping_terapages_used,
            vmm_direct_mapping_gigapages_used, vmm_direct_mapping_megapages_used, vmm_direct_mapping_pages_used);
#endif

    if (system_size == 0) {
        // No system module passed, fail early for now.
        debugstr("No system module loaded - check bootloader config. Halting\n");
        halt_and_catch_fire();
    }

    bsp_kernel_entrypoint(((uintptr_t)&static_rsdp) - STATIC_KERNEL_SPACE);
}
