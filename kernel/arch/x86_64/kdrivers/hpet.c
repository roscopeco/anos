/*
 * stage3 - HPET Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>

#include "acpitables.h"
#include "debugprint.h"
#include "kdrivers/drivers.h"
#include "kdrivers/hpet.h"
#include "kdrivers/timer.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_HPET
#if __STDC_HOSTED__ == 1
#include <stdio.h>
#define debugstr(...) printf(__VA_ARGS__)
#define printhex8(arg, ignore) printf("0x%02x", arg)
#define printhex16(arg, ignore) printf("0x%04x", arg)
#define printhex32(arg, ignore) printf("0x%08x", arg)
#define printhex64(arg, ignore) printf("0x%016x", arg)
#else
#include "printdec.h"
#include "printhex.h"
#endif
#endif

#ifndef NULL
#define NULL (((void *)0))
#endif

/*
QEMU caps & id: 0x009896808086a201

00000000100110001001011010000000 1000000010000110 1 0 1 00010 00000001
         = 10000000                   = 8086      Y N Y  = 2    = 1

So period = 10000000
   vendor = 8086
   flags  = 64-bit legacy-capable
   tmax   = 2
   rev    = 1
*/

static volatile HPETRegs *regs;
static KernelTimer timer;

static uint64_t nanos_per_tick(void) {
    if (regs) {
        return hpet_period(regs->caps_and_id) / 1000000;
    } else {
        return 0;
    }
}

static uint64_t current_ticks(void) {
    if (regs) {
        return regs->counter_value;
    } else {
        return 0;
    }
}

static void delay_nanos(uint64_t nanos) {
    uint64_t start = current_ticks();
    uint64_t end = start + (nanos / nanos_per_tick());

    while (current_ticks() < end)
        ;
}

KernelTimer *hpet_as_timer(void) { return &timer; }

bool hpet_init(ACPI_RSDT *rsdt) {
    if (!rsdt) {
        return false;
    }

    ACPI_HPET *hpet = acpi_tables_find_hpet(rsdt);

    if (hpet) {
        void volatile *vaddr = kernel_drivers_alloc_pages(1);

        if (vaddr == NULL) {
            debugstr("WARN: Failed to allocate MMIO vm space for HPET\n");
            return false;
        }

        // per-spec, HPET **must** be in memory...
        if (!vmm_map_page_containing((uintptr_t)vaddr, hpet->address.address,
                                     PG_PRESENT | PG_WRITE)) {
            debugstr("WARN: Failed to map MMIO vm space for HPET\n");
            return false;
        }

        regs = (HPETRegs *)vaddr;
        timer.current_ticks = current_ticks;
        timer.nanos_per_tick = nanos_per_tick;
        timer.delay_nanos = delay_nanos;
        regs->counter_value = 10;
        regs->flags = 1;

#ifdef DEBUG_HPET

        debugstr("Found HPET ");
        printhex8(hpet->hpet_number, debugchar);
        debugstr(" with ");
        printhex8(hpet->comparator_count, debugchar);
        debugstr(" comparators [PCI Vendor ");
        printhex16(hpet->pci_vendor_id, debugchar);
        debugstr("]\n");

        debugstr("  Address: ");
        printhex64(hpet->address.address, debugchar);
        debugstr("\n");

        debugstr("  Counter size: ");
        printhex8(hpet->counter_size, debugchar);
        debugstr("\n");

        debugstr("  Minimum tick: ");
        printhex16(hpet->minimum_tick, debugchar);
        debugstr("\n");

        debugstr("  Page protection: ");
        printhex8(hpet->page_protection, debugchar);
        debugstr("\n");

        debugstr("  HW rev ID: ");
        printhex8(hpet->hardware_rev_id, debugchar);
        debugstr("\n");

        debugstr("  CAPS: ");
        printhex64(regs->caps_and_id, debugchar);
        debugstr("\n");

        debugstr("  Vendor ID: ");
        printhex16(hpet_vendor(regs->caps_and_id), debugchar);
        debugstr("\n");

        debugstr("  Clock period: ");
        printhex16(hpet_period(regs->caps_and_id), debugchar);
        debugstr("  (");
        printdec(hpet_period(regs->caps_and_id), debugchar);
        debugstr(" femtosecs)\n");

        debugstr("  Timer count: ");
        printdec(hpet_timer_count(regs->caps_and_id), debugchar);
        debugstr("\n");

        debugstr("  Capabilities: ");
        debugstr(hpet_is_64_bit(regs->caps_and_id) ? " [64BIT]" : " [32BIT]");
        debugstr(hpet_can_legacy(regs->caps_and_id) ? " [LRM]" : " [NO LRM]");
        debugstr("\n");
#endif

        return true;
    } else {
#ifdef DEBUG_HPET
        debugstr("No HPET...\n");
#endif
        return false;
    }
}
