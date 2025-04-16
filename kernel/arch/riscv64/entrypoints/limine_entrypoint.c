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
#include "kprintf.h"
#include "machine.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static char *MSG = VERSION;

#define MAX_MEMMAP_ENTRIES 64

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

extern uint64_t _kernel_vma_start;
extern uint64_t _kernel_vma_end;
extern uint64_t _bss_end;
extern uint64_t _code;

static Limine_MemMap static_memmap;
static Limine_MemMapEntry *static_memmap_pointers[MAX_MEMMAP_ENTRIES];
static Limine_MemMapEntry static_memmap_entries[MAX_MEMMAP_ENTRIES];

// TODO this should come from common code...
static void banner(char *arch) {
    debugattr(0x0B);
    debugstr("STAGE");
    debugattr(0x03);
    debugchar('3');
    debugattr(0x08);
    debugstr(" #");
    debugattr(0x0F);
    debugstr(MSG);
    debugattr(0x08);
    debugstr(" [");
    debugattr(0x07);
    debugstr(arch);
    debugattr(0x08);
    debugstr("]\n");
    debugattr(0x07);
}

noreturn void bsp_kernel_entrypoint_limine() {
    if (limine_framebuffer_request.response &&
        limine_framebuffer_request.response->framebuffer_count > 0) {
        uintptr_t fb_phys =
                (uintptr_t)limine_framebuffer_request.response->framebuffers[0]
                        ->address -
                limine_hhdm_request.response->offset;

        uint16_t fb_width =
                limine_framebuffer_request.response->framebuffers[0]->width;
        uint16_t fb_height =
                limine_framebuffer_request.response->framebuffers[0]->height;

        // We're now on our own pagetables, and have essentially the same setup as
        // we do on entry from STAGE2 when BIOS booting.
        //
        // IOW we have a baseline environment.
        //
        debugterm_init((char *)fb_phys, fb_width, fb_height);

        banner("riscv64");

        kprintf("\n\nThis is as far as we go right now...\n");
    }

    while (1)
        ;
}
