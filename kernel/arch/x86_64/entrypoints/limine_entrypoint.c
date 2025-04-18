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

#include "acpitables.h"
#include "debugprint.h"
#include "entrypoints/common.h"
#include "init_pagetables.h"
#include "kdrivers/cpu.h"
#include "pmm/config.h"
#include "std/string.h"
#include "vmm/vmmapper.h"

#define MAX_MEMMAP_ENTRIES 64

// NOTE: These **must** be kept in-step with STAGE2's `init_pagetables.asm`!
#define PM4_START ((0x9c000))
#define PDP_START ((0x9d000))
#define PD_START ((0x9e000))
#define PT_START ((0x9f000))

#define KERNEL_BSS_PHYS ((0x110000))
#define KERNEL_CODE_PHYS ((0x120000))

#define KERNEL_INIT_STACK_TOP ((STATIC_KERNEL_SPACE + KERNEL_BSS_PHYS))

#define KERNEL_FRAMEBUFFER ((0xffffffff82000000))

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b
#define LIMINE_MEMMAP_REQUEST                                                  \
    {LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62}
#define LIMINE_RSDP_REQUEST                                                    \
    {LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c}
#define LIMINE_FRAMEBUFFER_REQUEST                                             \
    {LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b}
#define LIMINE_HHDM_REQUEST                                                    \
    {LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b}

#ifdef DEBUG_MEMMAP
void debug_memmap_limine(Limine_MemMap *memmap);
#else
#define debug_memmap_limine(...)
#endif

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_MemMap *memmap;
} __attribute__((packed)) Limine_MemMapRequest;

typedef struct {
    uint64_t revision;
    ACPI_RSDP *address;
} __attribute__((packed)) Limine_Rsdp;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_Rsdp *rsdp;
} __attribute__((packed)) Limine_RsdpRequest;

typedef struct {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} __attribute__((packed)) Limine_VideoMode;

typedef struct {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp; // Bits per pixel
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;

    /* Response revision 1 */
    uint64_t mode_count;
    Limine_VideoMode **modes;
} __attribute__((packed)) Limine_FrameBuffer;

typedef struct {
    uint64_t revision;
    uint64_t framebuffer_count;
    Limine_FrameBuffer **framebuffers;
} __attribute__((packed)) Limine_FrameBuffers;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_FrameBuffers *response;
} __attribute__((packed)) Limine_FrameBufferRequest;

typedef struct {
    uint64_t revision;
    uint64_t offset;
} __attribute__((packed)) Limine_HHDM;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_HHDM *response;
} __attribute__((packed)) Limine_HHDMRequest;

static volatile Limine_MemMapRequest limine_memmap_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_MEMMAP_REQUEST,
                .revision = 3,
};

static volatile Limine_RsdpRequest limine_rsdp_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_RSDP_REQUEST,
                .revision = 3,
};

static volatile Limine_FrameBufferRequest limine_framebuffer_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_FRAMEBUFFER_REQUEST,
                .revision = 3,
};

static volatile Limine_HHDMRequest limine_hhdm_request
        __attribute__((__aligned__(8))) = {
                .id = LIMINE_HHDM_REQUEST,
                .revision = 3,
};

extern uint64_t _kernel_vma_start;
extern uint64_t _kernel_vma_end;
extern uint64_t _bss_end;
extern uint64_t _code;

static Limine_MemMap static_memmap;
static Limine_MemMapEntry *static_memmap_pointers[MAX_MEMMAP_ENTRIES];
static Limine_MemMapEntry static_memmap_entries[MAX_MEMMAP_ENTRIES];

// We only need an addressable rsdp for the kernel, the
// rest of the tables are mapped dynamically and the kernel
// doesn't care where they are...
static ACPI_RSDP static_rsdp;

// Externals
noreturn void bsp_kernel_entrypoint(uintptr_t rsdp_phys);
noreturn void bootstrap_trampoline(uint16_t fb_width, uint16_t fb_height,
                                   uintptr_t new_stack, uintptr_t new_pt_phys,
                                   void *boing);

// Forwards
static noreturn void bootstrap_continue(uint16_t fb_width, uint16_t fb_height);

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
        static_memmap_entries[i].base =
                limine_memmap_request.memmap->entries[i]->base;
        static_memmap_entries[i].length =
                limine_memmap_request.memmap->entries[i]->length;
        static_memmap_entries[i].type =
                limine_memmap_request.memmap->entries[i]->type;
    }

    // framebuffer - assume it's direct mapped so we can just subtract the offset to get its phys...
    uintptr_t fb_phys =
            (uintptr_t)limine_framebuffer_request.response->framebuffers[0]
                    ->address -
            limine_hhdm_request.response->offset;
    uint16_t fb_width =
            limine_framebuffer_request.response->framebuffers[0]->width;
    uint16_t fb_height =
            limine_framebuffer_request.response->framebuffers[0]->height;

    // RSDP
    for (int i = 0; i < 8; i++) {
        static_rsdp.signature[i] =
                limine_rsdp_request.rsdp->address->signature[i];
    }
    for (int i = 0; i < 6; i++) {
        static_rsdp.oem_id[i] = limine_rsdp_request.rsdp->address->oem_id[i];
    }
    static_rsdp.checksum = limine_rsdp_request.rsdp->address->checksum;
    static_rsdp.extended_checksum =
            limine_rsdp_request.rsdp->address->extended_checksum;
    static_rsdp.length = limine_rsdp_request.rsdp->address->length;
    static_rsdp.revision = limine_rsdp_request.rsdp->address->revision;
    static_rsdp.rsdt_address = limine_rsdp_request.rsdp->address->rsdt_address;
    static_rsdp.xsdt_address = limine_rsdp_request.rsdp->address->xsdt_address;

    // copy kernel (yep, we're doing that - we want a known environment and phys
    // layout, though do see comment at the top of this file about assumptions
    // about low phys memory...)
    //
    // BSS first...
    uint64_t *new_base = (uint64_t *)(limine_hhdm_request.response->offset +
                                      KERNEL_BSS_PHYS);

    memcpy(new_base, &_kernel_vma_start,
           ((uintptr_t)&_bss_end) - ((uintptr_t)&_kernel_vma_start));

    //
    // ... then code and data...
    new_base = (uint64_t *)(limine_hhdm_request.response->offset +
                            KERNEL_CODE_PHYS);
    memcpy(new_base, &_code,
           ((uintptr_t)&_kernel_vma_end) - ((uintptr_t)&_code));

    // Set up the static pagetables the kernel expects to exist...
    uint64_t volatile *new_pml4 =
            (uint64_t *)(limine_hhdm_request.response->offset + PM4_START);
    uint64_t volatile *new_pdpt =
            (uint64_t *)(limine_hhdm_request.response->offset + PDP_START);
    uint64_t volatile *new_pd =
            (uint64_t *)(limine_hhdm_request.response->offset + PD_START);
    uint64_t volatile *new_pt =
            (uint64_t *)(limine_hhdm_request.response->offset + PT_START);

    // Set up the initial tables
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = 0; // zero out the PML4
        new_pdpt[i] = 0; // ... and the PDPT
        new_pd[i] = 0;   // ... as well as the PD
        new_pt[i] = (i * VM_PAGE_SIZE) | PG_PRESENT |
                    PG_WRITE; // ... and map low mem into the PT
    }
    new_pml4[0x1ff] =
            PDP_START | PG_PRESENT | PG_WRITE; // Set up the entries we need for
    new_pdpt[0x1fe] =
            PD_START | PG_PRESENT | PG_WRITE; // the mappings in kernel space...
    new_pd[0] = PT_START | PG_PRESENT | PG_WRITE;

    // map framebuffer, as four 2MiB large pages at 0xffffffff82000000 - 0xffffffff827fffff
    // TODO write-combining!
    new_pd[0x10] = fb_phys | PG_PRESENT | PG_WRITE | PG_PAGESIZE;
    new_pd[0x11] = (fb_phys + 0x200000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE;
    new_pd[0x12] = (fb_phys + 0x400000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE;
    new_pd[0x13] = (fb_phys + 0x600000) | PG_PRESENT | PG_WRITE | PG_PAGESIZE;

    bootstrap_trampoline(fb_width, fb_height, KERNEL_INIT_STACK_TOP, PM4_START,
                         bootstrap_continue);
}

static noreturn void bootstrap_continue(uint16_t fb_width, uint16_t fb_height) {
    // We're now on our own pagetables, and have essentially the same setup as
    // we do on entry from STAGE2 when BIOS booting.
    //
    // IOW we have a baseline environment.
    //
    debugterm_init((char *)KERNEL_FRAMEBUFFER, fb_width, fb_height);

    init_kernel_gdt();
    install_interrupts();

    pagetables_init();

    physical_region = page_alloc_init_limine(&static_memmap, PMM_PHYS_BASE,
                                             STATIC_PMM_VREGION);

    debug_memmap_limine(&static_memmap);

    bsp_kernel_entrypoint(((uintptr_t)&static_rsdp) - STATIC_KERNEL_SPACE);
}
