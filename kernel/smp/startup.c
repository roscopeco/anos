/*
 * stage3 - SMP startup support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include "acpitables.h"
#include "cpuid.h"
#include "gdt.h"
#include "interrupts.h"
#include "kdrivers/cpu.h"
#include "kdrivers/local_apic.h"
#include "vmm/recursive.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_SMP_STARTUP
#include "debugprint.h"
#include "printdec.h"
#include "printhex.h"
#include "spinlock.h"

static SpinLock debug_output_lock;
#endif

extern void *_binary_kernel_realmode_bin_start,
        *_binary_kernel_realmode_bin_end;

// If you're changing any of these, you'll need to change the real-mode
// link script as well...
//
#define AP_TRAMPOLINE_RUN_PADDR ((0x1000))
#define AP_TRAMPOLINE_BSS_PADDR ((0x5000))
#define AP_TRAMPOLINE_STK_PADDR ((0x6000))
#define AP_TRAMPOLINE_CPU_STK_SIZE ((0x800))
#define AP_TRAMPOLINE_STK_TOTAL_SIZE ((0x8000))

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

#define AP_TRAMPOLINE_BSS_GDT_VADDR ((AP_TRAMPOLINE_BSS_VADDR + 0x18))
#define AP_TRAMPOLINE_BSS_GDT (((GDTR *)(AP_TRAMPOLINE_BSS_GDT_VADDR)))

#define AP_TRAMPOLINE_BSS_IDT_VADDR ((AP_TRAMPOLINE_BSS_VADDR + 0x28))
#define AP_TRAMPOLINE_BSS_IDT (((IDTR *)(AP_TRAMPOLINE_BSS_IDT_VADDR)))

noreturn void ap_kernel_entrypoint(uint64_t ap_num);

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
static void smp_bsp_start_ap(uint8_t ap_id, uint32_t volatile *lapic) {
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

void smp_bsp_start_aps(ACPI_RSDT *rsdt, uint32_t volatile *lapic) {
    // copy the AP trampoline code to a fixed address in low conventional memory
    memcpy(AP_TRAMPOLINE_BASE_VADDR, AP_TRAMPOLINE_BIN_START,
           AP_TRAMPOLINE_BIN_LENGTH);

    // Clear the AP code BSS
    memclr(AP_TRAMPOLINE_BSS_VADDR, AP_TRAMPOLINE_BSS_LENGTH);

    // Temp identity map the low memory pages so APs can enable paging
    for (int i = AP_TRAMPOLINE_RUN_PADDR; i < 0x10000; i += 0x1000) {
        vmm_map_page(i, i, PRESENT | WRITE);
    }

    // Placed return address to ap_kernel_entrypoint on each stack
    for (uintptr_t i = AP_TRAMPOLINE_STK_PADDR + AP_TRAMPOLINE_CPU_STK_SIZE - 8;
         i < (AP_TRAMPOLINE_STK_PADDR + AP_TRAMPOLINE_STK_TOTAL_SIZE);
         i += AP_TRAMPOLINE_CPU_STK_SIZE) {
        *((uintptr_t *)i) = (uintptr_t)&ap_kernel_entrypoint;
    }

    // Start AP unique ID's at 1 (since BSP is logically 0)
    *(AP_TRAMPOLINE_BSS_UID) = 1;

    // Give APs the same pagetables we have to start with
    *(AP_TRAMPOLINE_BSS_PML4) =
            vmm_recursive_find_pml4()->entries[RECURSIVE_ENTRY];

    // Once in long-mode, we'll want APs to use our GDT & IDT...
    cpu_store_gdtr(AP_TRAMPOLINE_BSS_GDT);
    cpu_store_idtr(AP_TRAMPOLINE_BSS_IDT);

    ACPI_MADT *madt = acpi_tables_find_madt(rsdt);

    if (madt) {
        uint16_t remain = madt->header.length - sizeof(ACPI_MADT);
        uint8_t *ptr = ((uint8_t *)madt) + sizeof(ACPI_MADT);
        uint8_t bsp_local_apic_id = cpu_read_local_apic_id();

        while (remain > 0) {
            uint8_t *type = ptr++;
            uint8_t *len = ptr++;

            switch (*type) {
            case 0: // Processor local APIC
                uint8_t cpu_id = *ptr++;
                uint8_t lapic_id = *ptr++;
                uint32_t *flags32 = (uint32_t *)ptr;
                uint32_t flags = *flags32;

                if (lapic_id != bsp_local_apic_id && (flags & 0x03) == 0x1) {
                    // can enable!
#ifdef DEBUG_SMP_STARTUP
#ifdef VERY_NOISY_SMP_STARTUP
                    spinlock_lock(&debug_output_lock);
                    debugstr("Will enable CPU ID  ");
                    printhex8(cpu_id, debugchar);
                    debugstr(" [LAPIC ");
                    printhex8(lapic_id, debugchar);
                    debugstr("; Flags: ");
                    printhex32(flags, debugchar);
                    debugstr("]\n");
                    spinlock_unlock(&debug_output_lock);
#endif
#endif
                    smp_bsp_start_ap(cpu_id, lapic);
                } else {
#ifdef DEBUG_SMP_STARTUP
#ifdef VERY_NOISY_SMP_STARTUP
                    if (lapic_id == bsp_local_apic_id) {
                        spinlock_lock(&debug_output_lock);
                        debugstr("Skipping CPU ID  ");
                        printhex8(cpu_id, debugchar);
                        debugstr(" - it is the BSP\n");
                        spinlock_unlock(&debug_output_lock);
                    } else {
                        spinlock_lock(&debug_output_lock);
                        debugstr("Cannot enable CPU ID  ");
                        printhex8(cpu_id, debugchar);
                        debugstr(" [LAPIC ");
                        printhex8(lapic_id, debugchar);
                        debugstr("; Flags: ");
                        printhex32(flags, debugchar);
                        debugstr("]\n");
                        spinlock_unlock(&debug_output_lock);
                    }
#endif
#endif
                }

                ptr += 4;
                break;
            default:
                ptr += *len - 2;
            }

            remain -= *len;
        }
    }

    // Unmap the low pages, they aren't needed any more...
    for (int i = AP_TRAMPOLINE_RUN_PADDR; i < 0x10000; i += 0x1000) {
        vmm_unmap_page(i);
    }
}
