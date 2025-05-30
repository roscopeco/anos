/*
 * stage3 - Platform initialisation for x86_64
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "fba/alloc.h"
#include "kdrivers/drivers.h"
#include "kprintf.h"
#include "panic.h"
#include "sleep.h"
#include "smp/ipwi.h"
#include "smp/startup.h"
#include "smp/state.h"
#include "std/string.h"
#include "syscalls.h"
#include "system.h"
#include "task.h"

#include "platform/acpi/acpitables.h"
#include "platform/pci/enumerate.h"

#include "x86_64/kdrivers/cpu.h"
#include "x86_64/kdrivers/hpet.h"
#include "x86_64/kdrivers/local_apic.h"

#define AP_CPUINIT_TIMEOUT 100000000 // 100ms

#ifdef DEBUG_MADT
void debug_madt(ACPI_RSDT *rsdt);
#else
#define debug_madt(...)
#endif

static ACPI_RSDT *acpi_root_table;

static uint32_t volatile *init_this_cpu(ACPI_RSDT *rsdt,
                                        const uint8_t cpu_num) {
    cpu_init_this();
    cpu_debug_info(cpu_num);

    // Allocate our per-CPU data
    PerCPUState *cpu_state = fba_alloc_block();

    if (!cpu_state) {
        panic("Failed to allocate CPU state");
    }

    memclr(cpu_state, sizeof(PerCPUState));

    cpu_state->self = cpu_state;
    cpu_state->cpu_id = cpu_num;
    cpu_state->lapic_id = cpu_read_local_apic_id();
    cpu_get_brand_str(cpu_state->cpu_brand);

    // NOTE: Locks and queues etc initialized by their respective subsystems!

    cpu_write_msr(MSR_KernelGSBase, (uint64_t)cpu_state);
    cpu_write_msr(MSR_GSBase, 0);
    cpu_swapgs();

    state_register_cpu(cpu_num, cpu_state);

    // Init local APIC on this CPU
    ACPI_MADT *madt = acpi_tables_find_madt(rsdt);

    if (madt == NULL) {
        kprintf("No MADT; Halting\n");
        halt_and_catch_fire();
    }

    return init_local_apic(madt, cpu_num == 0);
}

static inline void *get_this_cpu_tss(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();
    return gdt_per_cpu_tss(cpu_state->cpu_id);
}

// We set this at startup, and once the APs are started up,
// they'll wait for this to go false before they start
// their own system schedulers.
//
// This way, we can ensure the main one is started and
// everything's initialized before we let them start
// theirs...
volatile bool ap_startup_wait;

// This is the number of CPUs waiting on ap_startup_wait.
// We'll wait for this to equal the number of APs (or timeout)
// to ensure basic CPU init (and IPWI etc) is done on all
// CPUs before proceeding.
static volatile int ap_waiting_count;

noreturn void ap_kernel_entrypoint(uint64_t ap_num) {
#ifdef DEBUG_SMP_STARTUP
#ifdef VERY_NOISY_SMP_STARTUP
    kprintf("AP #%d  has entered the chat...\n", ap_num);
#endif
#endif

    syscall_init();

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, ap_num);

    if (!ipwi_init()) {
        panic("Failed to initialise IPWI subsystem for one or more APs");
    }

    ap_waiting_count += 1;

    while (ap_startup_wait) {
        // just busy right now, but should hlt and wait for an IPI or something...?

        // NOTE: The soft barrier **is** necessary - without it, under certain
        //       conditions, the optimizer will assume the value doesn't change
        //       (even with the volatile) and end up sending _some_ of the APs into
        //       an infinite loop here.
        //
        //       I'm not 100% certain _why_ this happens (and especially why it
        //       _only_ happens when building a kernel with UBSAN), but it does,
        //       so this is here to fix it.
        //
        //       We could also just go with making `ap_startup_wait` an `_Atomic` I
        //       imagine, but as far as I understand right now, we don't need a
        //       hard barrier, the soft barrier is good enough.
        //
        //       The pause hint is really just here because this is a spin loop...
        //
        __asm__ volatile("pause" : : : "memory");
    }

    task_init(get_this_cpu_tss());
    sleep_init();
    start_system_ap(ap_num);

    panic("Somehow ended up back in AP entrypoint. This is a bad thing...");
}

static void wait_for_ap_basic_init_to_complete(void) {
    KernelTimer volatile *hpet = hpet_as_timer();

    uint64_t end = hpet->current_ticks() +
                   (AP_CPUINIT_TIMEOUT / hpet->nanos_per_tick());

    while (hpet->current_ticks() < end) {
        __asm__ __volatile__("pause" : : : "memory");

        if (ap_waiting_count == state_get_cpu_count() - 1) {
            break;
        }
    }

#ifdef DEBUG_SMP_STARTUP
    if (ap_waiting_count != state_get_cpu_count() - 1) {
        kprintf("WARN: One or more APs have gone rogue!\n");
    }
#endif
}

bool platform_await_init_complete(void) {
    // We need to wait for basic CPU initialisation to complete on APs,
    // so we know they'll have their per-CPU state, IPWI, queues etc.
    //
    // We know this if they've reached the "wait for ap_startup_wait" loop.
    //
    wait_for_ap_basic_init_to_complete();

    return true;
}

bool platform_task_init(void) {
    task_init(get_this_cpu_tss());
    return true;
}

bool platform_init(const uintptr_t platform_data) {
#ifdef DEBUG_ACPI
    debugstr("RSDP at ");
    printhex64((uint64_t)rsdp, debugchar);
    debugstr(" (physical): OEM is ");
#endif

    ACPI_RSDP *rsdp = (ACPI_RSDP *)(platform_data + 0xFFFFFFFF80000000);

#ifdef DEBUG_ACPI
    debugstr_len(rsdp->oem_id, 6);
    debugstr("\nRSDP revision is ");
    printhex8(rsdp->revision, debugchar);

    if (rsdp->revision > 1) {
        debugstr("\nXSDT at ");
        printhex64(rsdp->xsdt_address, debugchar);
    } else {
        debugstr("\nRSDT at ");
        printhex32(rsdp->rsdt_address, debugchar);
    }
    debugstr("\n");
#endif

    acpi_root_table = acpi_tables_init(rsdp);
    if (acpi_root_table == NULL) {
        panic("ACPI table mapping failed");
    }

    debug_madt(acpi_root_table);
    kernel_drivers_init(acpi_root_table);

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, 0);

#if MAX_CPU_COUNT > 1
    ap_startup_wait = true;
    smp_bsp_start_aps(acpi_root_table, lapic);
#endif

    pci_enumerate();

    syscall_init();

    return true;
}