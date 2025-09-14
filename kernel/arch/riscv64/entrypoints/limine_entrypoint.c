/*
 * stage3 - Kernel entry point from Limine bootloader on RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#define ANOS_NO_DECL_REGIONS

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "kprintf.h"
#include "machine.h"
#include "pmm/pagealloc.h"
#include "vmm/vmconfig.h"

#include "platform/bootloaders/limine.h"

#include "riscv64/interrupts.h"
#include "riscv64/kdrivers/cpu.h"
#include "riscv64/pmm/config.h"
#include "riscv64/sbi.h"
#include "riscv64/vmm/vmmapper.h"

#define MAX_MEMMAP_ENTRIES 64

#define KERNEL_BSS_PHYS ((0x110000))
#define KERNEL_CODE_PHYS ((0x120000))

#define KERNEL_INIT_STACK_TOP ((STATIC_KERNEL_SPACE + KERNEL_BSS_PHYS))

#define KERNEL_FRAMEBUFFER ((0xffffffff82000000))

#ifdef DEBUG_VMM
#define vmm_debugf(...) kprintf(__VA_ARGS__)
#ifdef VERY_NOISY_VMM
#define vmm_vdebugf(...) kprintf(__VA_ARGS__)
#else
#define vmm_vdebugf(...)
#endif
#else
#define vmm_debugf(...)
#define vmm_vdebugf(...)
#endif

#ifdef DEBUG_MEMMAP
void debug_memmap_limine(Limine_MemMap *memmap);
#else
#define debug_memmap_limine(...)
#endif

static volatile Limine_MemMapRequest limine_memmap_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_MEMMAP_REQUEST,
                .revision = 3,
                .memmap = NULL,
};

static volatile Limine_FrameBufferRequest limine_framebuffer_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_FRAMEBUFFER_REQUEST,
                .revision = 3,
                .response = NULL,
};

static volatile Limine_HHDMRequest limine_hhdm_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_HHDM_REQUEST,
                .revision = 3,
                .response = NULL,
};

/* Defined by the linker */
extern uint64_t _kernel_vma_start;
extern uint64_t _kernel_vma_end;
extern uint64_t _bss_end;
extern uint64_t _code;

extern void *_system_bin_start;
extern void *_system_bin_end;

uintptr_t _system_bin_start_phys;

// We can infer this on RISC-V currently since the system binary is linked
// into the kernel, but for compatibility with the non-arch-specific code,
// let's just define it. We'll need it later anyhow when we separate the
// system bin from the kernel on this arch too...
size_t _system_bin_size;

static Limine_MemMap static_memmap;
static Limine_MemMapEntry *static_memmap_pointers[MAX_MEMMAP_ENTRIES];
static Limine_MemMapEntry static_memmap_entries[MAX_MEMMAP_ENTRIES];

/* Static initial pagetables */
static uint64_t new_pml4[512] __attribute__((aligned(4096)));
static uint64_t new_pdpt[512] __attribute__((aligned(4096)));

// One PD, two PTs to cover the full kernel code/data space...
static uint64_t code_data_pd[512] __attribute__((aligned(4096)));
static uint64_t code_data_pt_1[512] __attribute__((aligned(4096)));
static uint64_t code_data_pt_2[512] __attribute__((aligned(4096)));

// One PD, one PT for the PMM bootstrap mappings
static uint64_t pmm_pd[512] __attribute__((aligned(4096)));
static uint64_t pmm_pt[512] __attribute__((aligned(4096)));

/* Bootstrap page for the PMM */
static uint64_t pmm_bootstrap_page[512] __attribute__((aligned(4096)));

/* Globals */
extern MemoryRegion *physical_region;

static Limine_MemMap static_memmap;
static Limine_MemMapEntry *static_memmap_pointers[MAX_MEMMAP_ENTRIES];
static Limine_MemMapEntry static_memmap_entries[MAX_MEMMAP_ENTRIES];

// Externals
noreturn void bootstrap_trampoline(uint16_t fb_width, uint16_t fb_height,
                                   uintptr_t new_stack, uintptr_t new_pt_phys,
                                   void *boing);

noreturn void bsp_kernel_entrypoint(uintptr_t platform_data);

// Forwards
static noreturn void bootstrap_continue(uint16_t fb_width, uint16_t fb_height);

typedef struct {
    uintptr_t phys_addr;
    uint16_t page_flags;
    uint64_t page_size;
} VmmPage;

static inline int8_t vmm_page_size_to_level(uint64_t page_size) {
    // Valid page sizes are powers of 2 that are multiples of 4KiB
    if (page_size < 0x1000 || (page_size & (page_size - 1)) != 0) {
        return -1; // Not a power of 2 or too small
    }

    uint8_t level = 0;
    uint64_t size = 0x1000;
    while (page_size > size) {
        size <<= 9;
        level++;
        if (level > SATP_MODE_MAX_LEVELS) {
            return -1;
        }
    }

    // Verify the page size matches exactly
    return (page_size == size) ? level : -1;
}

static inline bool vmm_table_walk_levels(uintptr_t virt_addr,
                                         uint8_t table_levels,
                                         uintptr_t root_table_phys,
                                         uintptr_t direct_map_base,
                                         VmmPage *out) {
    uint64_t *current_table_virt =
            (uint64_t *)(root_table_phys + direct_map_base);
    vmm_vdebugf("vmm_table_walk_levels: PML4 virt: %p\n",
                (void *)current_table_virt);

    uint8_t levels_remaining = table_levels;
    uint64_t current_entry;

    while (levels_remaining > 0) {
        uint16_t entry_index =
                vmm_virt_to_table_index(virt_addr, levels_remaining);
        current_entry = current_table_virt[entry_index];

        if ((current_entry & PG_PRESENT) == 0) {
            vmm_vdebugf("  * Level %d entry is not present\n",
                        levels_remaining);
            return false;
        }

        if (current_entry & (PG_READ | PG_WRITE | PG_EXEC)) {
            vmm_vdebugf("vmm_table_walk_levels: Level %d entry %d represents a "
                        "%ld page at %p\n",
                        levels_remaining, entry_index,
                        vmm_level_page_size(levels_remaining),
                        (void *)vmm_table_entry_to_phys(current_entry));
            out->phys_addr = vmm_table_entry_to_phys(current_entry);
            out->page_flags = vmm_table_entry_to_page_flags(current_entry);
            out->page_size = vmm_level_page_size(levels_remaining);
            return true;
        }

        current_table_virt =
                (uint64_t *)(vmm_table_entry_to_phys(current_entry) +
                             direct_map_base);
        vmm_vdebugf("  * Level %d entry points to level %d table at %p\n",
                    levels_remaining, levels_remaining - 1,
                    (void *)vmm_table_entry_to_phys(current_entry));
        levels_remaining--;
    }

    vmm_vdebugf("vmm_table_walk_levels: No page found\n");
    return false;
}

static inline int vmm_mmu_mode_to_table_levels(uint8_t mmu_mode) {
    switch (mmu_mode) {
    case SATP_MODE_BARE:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: MMU is in BARE mode\n");
        return SATP_MODE_BARE_LEVELS;
    case SATP_MODE_SV64:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: MMU is in SV64 mode\n");
        return SATP_MODE_SV64_LEVELS;
    case SATP_MODE_SV57:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: MMU is in SV57 mode\n");
        return SATP_MODE_SV57_LEVELS;
    case SATP_MODE_SV48:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: MMU is in SV48 mode\n");
        return SATP_MODE_SV48_LEVELS;
    case SATP_MODE_SV39:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: MMU is in SV39 mode\n");
        return SATP_MODE_SV39_LEVELS;
    default:
        vmm_vdebugf("vmm_mmu_mode_to_table_levels: Unknown SATP mode!\n");
        return -1;
    }
}

static inline bool vmm_table_walk_mode(uintptr_t virt_addr, uint8_t mmu_mode,
                                       uintptr_t root_table_phys,
                                       uintptr_t direct_map_base,
                                       VmmPage *out) {
    if (!out) {
        return false;
    }

    uint8_t table_levels = vmm_mmu_mode_to_table_levels(mmu_mode);

    if (unlikely(table_levels == 0)) {
        vmm_vdebugf("vmm_table_walk_mode: Returning virt_addr\n");
        out->phys_addr = virt_addr;
        out->page_flags = PG_PRESENT | PG_READ | PG_WRITE | PG_EXEC;
        out->page_size = 0x1000;
        return true;
    }

    if (unlikely(table_levels == -1)) {
        vmm_vdebugf("vmm_table_walk_mode: Unknown SATP mode!\n");
        return false;
    }

    return vmm_table_walk_levels(virt_addr,
                                 vmm_mmu_mode_to_table_levels(mmu_mode),
                                 root_table_phys, direct_map_base, out);
}

static inline bool vmm_map_page_no_alloc(uintptr_t virt_addr, uint8_t mmu_mode,
                                         uintptr_t root_table_phys,
                                         uintptr_t direct_map_base,
                                         VmmPage *page) {
    if (!page) {
        return false;
    }

    uint8_t table_levels = vmm_mmu_mode_to_table_levels(mmu_mode);

    vmm_vdebugf("vmm_map_page_no_alloc: mode %d, table levels %d\n", mmu_mode,
                table_levels);

    if (unlikely(table_levels == 0)) {
        vmm_vdebugf(
                "vmm_map_page_no_alloc: No mapping required; returning true\n");
        return true;
    }

    if (unlikely(table_levels == -1)) {
        vmm_vdebugf("vmm_map_page_no_alloc: Unknown SATP mode!\n");
        return false;
    }

    // Calculate which level we need to map at based on the page size
    int8_t target_level = vmm_page_size_to_level(page->page_size);
    if (target_level < 0) {
        vmm_vdebugf("vmm_map_page_no_alloc: Invalid page size %ld\n",
                    page->page_size);
        return false;
    }

    if (target_level >= table_levels) {
        vmm_vdebugf("vmm_map_page_no_alloc: Page size %ld requires level %d, "
                    "but MMU only supports %d levels\n",
                    page->page_size, target_level, table_levels);
        return false;
    }

    // Get the current table level count
    uint64_t *current_table = (uint64_t *)(root_table_phys + direct_map_base);
    uint8_t levels_remaining = table_levels;
    uint16_t entry_index;

    vmm_vdebugf("vmm_map_page_no_alloc: direct_map_base is 0x%016lx\n",
                direct_map_base);
    vmm_vdebugf("vmm_map_page_no_alloc: Mapping no alloc in root table at %p\n",
                (void *)current_table);
    vmm_vdebugf("vmm_map_page_no_alloc: Attempting to map phys page %p at "
                "level %d in %d level table at %p virtual\n",
                (void *)page->phys_addr, target_level, levels_remaining,
                (void *)virt_addr);

    // Walk through the page tables until we reach the target level
    while (levels_remaining > target_level + 1) {
        vmm_vdebugf("vmm_map_page_no_alloc: at level %d\n", levels_remaining);
        entry_index = vmm_virt_to_table_index(virt_addr, levels_remaining);
        uint64_t current_entry = current_table[entry_index];
        vmm_vdebugf("vmm_map_page_no_alloc: entry %d is 0x%016lx ( => P: "
                    "0x%016lx / V: 0x%016lx)\n",
                    entry_index, current_entry,
                    vmm_table_entry_to_phys(current_entry),
                    vmm_table_entry_to_phys(current_entry) + direct_map_base);

        // If the entry is not present, we can't map the page
        if ((current_entry & PG_PRESENT) == 0) {
            vmm_debugf("vmm_map_page_no_alloc: Level %d entry is not present\n",
                       levels_remaining);
            return false;
        }

        // if the entry is a leaf page, we can't map the page because there won't be subtables
        if (current_entry & (PG_READ | PG_WRITE | PG_EXEC)) {
            vmm_debugf("vmm_map_page_no_alloc: Level %d entry is a leaf page\n",
                       levels_remaining);
            return false;
        }

        current_table = (uint64_t *)(vmm_table_entry_to_phys(current_entry) +
                                     direct_map_base);
        vmm_vdebugf("vmm_map_page_no_alloc: Move to level %d table at %p\n",
                    levels_remaining - 1, current_table);
        levels_remaining--;
    }

    // We've reached the target level, map the page
    entry_index = vmm_virt_to_table_index(virt_addr, target_level + 1);
    current_table[entry_index] = vmm_phys_and_flags_to_table_entry(
            page->phys_addr, page->page_flags | PG_PRESENT);
    cpu_invalidate_tlb_addr(virt_addr);

    vmm_vdebugf("Target level %d entry %d is now 0x%016lx\n", target_level,
                entry_index, current_table[entry_index]);
    return true;
}

noreturn void bsp_kernel_entrypoint_limine() {
    uintptr_t fb_phys;
    uint16_t fb_width;
    uint16_t fb_height;

    // Early init the debugterm since we need to be able to report errors early.
    // We'll reinit it later with the correct framebuffer in our usual kernel mappings.
    if (limine_framebuffer_request.response &&
        limine_framebuffer_request.response->framebuffer_count > 0) {
        fb_phys =
                (uintptr_t)limine_framebuffer_request.response->framebuffers[0]
                        ->address -
                limine_hhdm_request.response->offset;

        fb_width = limine_framebuffer_request.response->framebuffers[0]->width;
        fb_height =
                limine_framebuffer_request.response->framebuffers[0]->height;

        debugterm_init((char *)fb_phys, fb_width, fb_height);
    } else {
        halt_and_catch_fire();
    }

    install_interrupts();

    // For ease, just copy the memmap into a static buffer, so we can use it later
    // when the time comes to initialize the PMM...
    if (!limine_memmap_request.memmap) {
        debugattr(0x0C);
        kprintf("HALT: No memmap found; Cannot continue.\n");
        halt_and_catch_fire();
    }

    if (!limine_hhdm_request.response) {
        debugattr(0x0C);
        kprintf("HALT: No HHDM found; Cannot continue.\n");
        halt_and_catch_fire();
    }

    static_memmap.entries = (Limine_MemMapEntry **)&static_memmap_pointers;
    static_memmap.entry_count = limine_memmap_request.memmap->entry_count;
    if (static_memmap.entry_count > MAX_MEMMAP_ENTRIES) {
        // TODO really Ross??
        static_memmap.entry_count = MAX_MEMMAP_ENTRIES;
    }

    for (int i = 0; i < static_memmap.entry_count; i++) {
        static_memmap_pointers[i] = &static_memmap_entries[i];
        static_memmap_entries[i].base =
                limine_memmap_request.memmap->entries[i]->base;
        static_memmap_entries[i].length =
                limine_memmap_request.memmap->entries[i]->length;
        static_memmap_entries[i].type =
                limine_memmap_request.memmap->entries[i]->type;
    }

    // Now, we need to set up our initial pagetables.
    //
    // On x86_64, we copy the kernel to suit our expected physical layout, and then
    // set up the pagetables to map the kernel at the correct virtual addresses.
    //
    // On RISC-V, we can't be that lazy since we don't have any guarantees about
    // low memory layout*, so we'll need to do some table walking to get the physical
    // addresses of our static pagetables, and then copy entries from Limine's tables
    // into ours. This should be good enough to get the kernel started and let us
    // do our proper pagetable setup after we trampoline.
    //
    // We'll also need to set up the framebuffer mapping while we're at it.
    //
    // * to be fair, we don't have any cast-iron guarantees about the low memory
    // layout on x86_64 either, but it's a fair assumption over there because of
    // backwards-compatibility...
    //

    uint64_t satp = cpu_read_satp();
    uintptr_t pml4_phys, pdpt_phys, pd_phys, pt_1_phys, pt_2_phys;
    VmmPage page;

    // These are static, so part of the kernel's bss, and not in the HHDM area. We need to table walk
    // to get the physical addresses, just subtracting the HHDM offset from the virtual addresses
    // won't work...
    //
    if (vmm_table_walk_mode((uintptr_t)new_pml4, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pml4_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PML4 physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)new_pdpt, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pdpt_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PDPT physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)code_data_pd, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pd_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PD physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)code_data_pt_1, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pt_1_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PT1 physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)code_data_pt_2, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pt_2_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PT2 physical address; Halting\n");
        halt_and_catch_fire();
    }

    new_pml4[0x1ff] =
            (pdpt_phys >> 2) | PG_PRESENT; // Set up the entries we need for
    new_pdpt[0x1fe] =
            (pd_phys >> 2) | PG_PRESENT; // the mappings in kernel space...
    code_data_pd[0] = (pt_1_phys >> 2) | PG_PRESENT;
    code_data_pd[1] = (pt_2_phys >> 2) | PG_PRESENT;

    // map framebuffer, as four 2MiB large pages at 0xffffffff82000000 - 0xffffffff827fffff
    // TODO write-combining!
    code_data_pd[0x10] = (fb_phys >> 2) | PG_PRESENT | PG_READ | PG_WRITE;
    code_data_pd[0x11] =
            ((fb_phys + 0x200000) >> 2) | PG_PRESENT | PG_READ | PG_WRITE;
    code_data_pd[0x12] =
            ((fb_phys + 0x400000) >> 2) | PG_PRESENT | PG_READ | PG_WRITE;
    code_data_pd[0x13] =
            ((fb_phys + 0x600000) >> 2) | PG_PRESENT | PG_READ | PG_WRITE;

    // Find the phys address of the embedded SYSTEM image as well, we'll need
    // that later on when we come to map it in...
    if (vmm_table_walk_mode((uintptr_t)&_system_bin_start, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        _system_bin_start_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PT physical address; Halting\n");
        halt_and_catch_fire();
    }

    // setup PMM stack area and bootstrap page
    uintptr_t pmm_pd_phys, pmm_pt_phys, pmm_bootstrap_phys;

    if (vmm_table_walk_mode((uintptr_t)pmm_pd, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pmm_pd_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PMM PD physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)pmm_pt, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pmm_pt_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PMM PD physical address; Halting\n");
        halt_and_catch_fire();
    }
    if (vmm_table_walk_mode((uintptr_t)pmm_bootstrap_page, cpu_satp_mode(satp),
                            cpu_satp_to_root_table_phys(satp),
                            limine_hhdm_request.response->offset, &page)) {
        pmm_bootstrap_phys = page.phys_addr;
    } else {
        kprintf("Failed to detemine PMM PD physical address; Halting\n");
        halt_and_catch_fire();
    }

    // ... map bootstrap page
    pmm_pd[0] = (pmm_pt_phys >> 2) | PG_PRESENT;
    pmm_pt[0] = (pmm_bootstrap_phys >> 2) | PG_PRESENT | PG_READ | PG_WRITE;

    // ... then hook this into the kernel-space mapping
    new_pdpt[0] = (pmm_pd_phys >> 2) | PG_PRESENT;

    // Copy BSS mappings....
    for (uintptr_t page_virt = (uintptr_t)&_kernel_vma_start;
         page_virt < (uintptr_t)&_bss_end; page_virt += 0x1000) {
        if (vmm_table_walk_mode(page_virt, cpu_satp_mode(satp),
                                cpu_satp_to_root_table_phys(satp),
                                limine_hhdm_request.response->offset, &page)) {
            if (!vmm_map_page_no_alloc(page_virt, SATP_MODE_SV48, pml4_phys,
                                       limine_hhdm_request.response->offset,
                                       &page)) {
                kprintf("Failed to map kernel data page %p; Halting\n",
                        (void *)page_virt);
                halt_and_catch_fire();
            }
        } else {
            kprintf("Kernel data page %p not found in bootloader mapping; "
                    "Halting\n",
                    (void *)page_virt);
            halt_and_catch_fire();
        }
    }

    // Copy code mappings....
    for (uintptr_t page_virt = (uintptr_t)&_code;
         page_virt < (uintptr_t)&_kernel_vma_end; page_virt += 0x1000) {
        if (vmm_table_walk_mode(page_virt, cpu_satp_mode(satp),
                                cpu_satp_to_root_table_phys(satp),
                                limine_hhdm_request.response->offset, &page)) {
            if (!vmm_map_page_no_alloc(page_virt, SATP_MODE_SV48, pml4_phys,
                                       limine_hhdm_request.response->offset,
                                       &page)) {
                kprintf("Failed to map kernel code page %p; Halting\n",
                        (void *)page_virt);
                halt_and_catch_fire();
            }
        } else {
            kprintf("Kernel code page %p not found in bootloader mapping; "
                    "Halting\n",
                    (void *)page_virt);
            halt_and_catch_fire();
        }
    }

    bootstrap_trampoline(fb_width, fb_height, KERNEL_INIT_STACK_TOP,
                         ((pml4_phys >> 12) | ((uint64_t)SATP_MODE_SV48 << 60)),
                         bootstrap_continue);
}

static noreturn void bootstrap_continue(uint16_t fb_width, uint16_t fb_height) {
    // We're now on our own pagetables, and have essentially the same setup as
    // we do on x86_64 at this point...
    //
    // IOW we have a baseline environment.
    //
    _system_bin_size =
            (uintptr_t)&_system_bin_end - (uintptr_t)&_system_bin_start;

    debugterm_reinit((char *)KERNEL_FRAMEBUFFER, fb_width, fb_height);

    if (_system_bin_size == 0) {
        // No system module passed, fail early for now.
        debugstr(
                "No system module loaded - check bootloader config. Halting\n");
        halt_and_catch_fire();
    }

    sbi_debug_info();

    debug_memmap_limine(&static_memmap);

    physical_region = page_alloc_init_limine(&static_memmap, 0,
                                             STATIC_PMM_VREGION, false);

#ifdef DEBUG_PMM
    kprintf("\n\nphysical_region allocated at %p : %ld bytes total / %ld bytes "
            "free\n",
            physical_region, physical_region->size, physical_region->free);
#endif

#ifdef DEBUG_VMM
    extern uint64_t vmm_direct_mapping_terapages_used,
            vmm_direct_mapping_gigapages_used,
            vmm_direct_mapping_megapages_used, vmm_direct_mapping_pages_used;

    size_t pre_direct_free = physical_region->free;
#endif
    vmm_init_direct_mapping(new_pml4, &static_memmap);
#ifdef DEBUG_VMM
    size_t post_direct_free = physical_region->free;
    kprintf("\nPage tables for VMM Direct Mapping: %ld bytes of physical "
            "memory\n",
            pre_direct_free - post_direct_free);
    kprintf("    Mapping types: %ld tera; %ld giga; %ld mega; %ld small\n\n",
            vmm_direct_mapping_terapages_used,
            vmm_direct_mapping_gigapages_used,
            vmm_direct_mapping_megapages_used, vmm_direct_mapping_pages_used);
#endif

    bsp_kernel_entrypoint(0);
}
