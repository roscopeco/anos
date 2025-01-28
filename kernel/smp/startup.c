/*
 * stage3 - SMP startup support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "kdrivers/cpu.h"
#include "kdrivers/local_apic.h"
#include "vmm/recursive.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_SMP_STARTUP
#include "debugprint.h"
#include "printdec.h"
#include "spinlock.h"

static SpinLock debug_output_lock;
#endif

extern void *_binary_kernel_realmode_bin_start,
        *_binary_kernel_realmode_bin_end;

// If you're changing any of these, you'll need to change the real-mode
// link script as well...
//
#define AP_TRAMPOLINE_RUN_PADDR ((0x4000))
#define AP_TRAMPOLINE_BSS_PADDR ((0x8000))

// All these are derived from the two above :)
//
#define AP_TRAMPOLINE_BASE_VADDR                                               \
    (((void *)(0xffffffff80000000 | AP_TRAMPOLINE_RUN_PADDR)))
#define AP_TRAMPOLINE_BSS_VADDR                                                \
    (((void *)(0xffffffff80000000 | AP_TRAMPOLINE_BSS_PADDR)))

#define AP_TRAMPOLINE_BIN_START (((void *)&_binary_kernel_realmode_bin_start))
#define AP_TRAMPOLINE_BIN_LENGTH                                               \
    (((((uintptr_t)&_binary_kernel_realmode_bin_end) -                         \
       ((uintptr_t)&_binary_kernel_realmode_bin_start))))

#define AP_TRAMPOLINE_BSS_LENGTH ((0x1000))

#define AP_TRAMPOLINE_BSS_UID_VADDR ((AP_TRAMPOLINE_BSS_VADDR + 0x00))
#define AP_TRAMPOLINE_BSS_UID                                                  \
    (((uint64_t volatile *)(AP_TRAMPOLINE_BSS_UID_VADDR)))
#define AP_TRAMPOLINE_BSS_PML4_VADDR ((AP_TRAMPOLINE_BSS_VADDR + 0x08))
#define AP_TRAMPOLINE_BSS_PML4                                                 \
    (((uint64_t volatile *)(AP_TRAMPOLINE_BSS_PML4_VADDR)))
#define AP_TRAMPOLINE_BSS_FLAG_VADDR ((AP_TRAMPOLINE_BSS_VADDR + 0x10))
#define AP_TRAMPOLINE_BSS_FLAG                                                 \
    (((uint64_t volatile *)(AP_TRAMPOLINE_BSS_FLAG_VADDR)))

static inline void memcpy(volatile void *dest, volatile void *src,
                          uint64_t count) {
    uint8_t volatile *s = (uint8_t *)src;
    uint8_t volatile *d = (uint8_t *)dest;

    for (int i = 0; i < count; i++) {
        *d++ = *s++;
    }
}

static inline void memclr(volatile void *dest, uint64_t count) {
    uint64_t volatile *d = (uint64_t *)dest;

    for (int i = 0; i < count / 8; i++) {
        *d++ = 0;
    }
}

/*
 * Must only be called by the BSP for now!
 */
void smp_bsp_start_ap(uint8_t ap_id, uint32_t volatile *lapic) {
    // Clear the "alive" flag
    *(AP_TRAMPOLINE_BSS_FLAG) = 0;

    // Send INIT
    *(REG_LAPIC_ICR_HIGH(lapic)) = ap_id << 24;
    *(REG_LAPIC_ICR_LOW(lapic)) = 0x4500;

    cpu_tsc_mdelay(10);

    // Send SIPI
    *(REG_LAPIC_ICR_HIGH(lapic)) = ap_id << 24;
    *(REG_LAPIC_ICR_LOW(lapic)) = 0x4600 | (AP_TRAMPOLINE_RUN_PADDR >> 12);

    // Wait for the "alive" flag
    while (!*AP_TRAMPOLINE_BSS_FLAG)
        ;

#ifdef DEBUG_SMP_STARTUP
    spinlock_lock(&debug_output_lock);

    debugstr("AP #");
    printdec(ap_id, debugchar);
    debugstr(" is up...\n");

    spinlock_unlock(&debug_output_lock);
#endif
}

static void ap_kernel_entrypoint(uint64_t ap_num) {
#ifdef DEBUG_SMP_STARTUP

    spinlock_lock(&debug_output_lock);

    debugstr("AP #");
    printdec(ap_num, debugchar);
    debugstr(" has entered the chat...\n");

    spinlock_unlock(&debug_output_lock);
#endif

    while (1) {
        __asm__ volatile("hlt");
    }
}

void smp_bsp_start_aps(uint32_t volatile *lapic) {

    // copy the AP trampoline code to a fixed address in low conventional memory (to address 0x0400:0x0000)
    memcpy(AP_TRAMPOLINE_BASE_VADDR, AP_TRAMPOLINE_BIN_START,
           AP_TRAMPOLINE_BIN_LENGTH);

    // Clear the AP code BSS
    memclr(AP_TRAMPOLINE_BSS_VADDR, AP_TRAMPOLINE_BSS_LENGTH);

    // Temp identity map the low memory pages so APs can enable paging
    for (int i = 0x4000; i < 0xa000; i += 0x1000) {
        vmm_map_page(i, i, PRESENT | WRITE);
    }

    // Placed return address to ap_kernel_entrypoint on each stack
    for (uintptr_t i = 0x9008; i < 0xa000; i += 0x10) {
        *((uintptr_t *)i) = (uintptr_t)&ap_kernel_entrypoint;
    }

    // Start AP unique ID's at 1 (since BSP is logically 0)
    *(AP_TRAMPOLINE_BSS_UID) = 1;

    // Give APs the same pagetables we have to start with
    *(AP_TRAMPOLINE_BSS_PML4) =
            vmm_recursive_find_pml4()->entries[RECURSIVE_ENTRY];

    // Let's do it!
    smp_bsp_start_ap(1, lapic);
    smp_bsp_start_ap(2, lapic);
    smp_bsp_start_ap(3, lapic);

    // Unmap the low pages, they aren't needed any more...
    for (int i = 0x4000; i < 0xa000; i += 0x1000) {
        vmm_unmap_page(i);
    }
}
