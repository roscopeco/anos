/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * We're fully 64-bit at this point 🎉
 */

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "capabilities.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "ipc/channel.h"
#include "ipc/named.h"
#include "kdrivers/drivers.h"
#include "kprintf.h"
#include "pagefault.h"
#include "panic.h"
#include "pci/enumerate.h"
#include "pmm/pagealloc.h"
#include "process/address_space.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/ipwi.h"
#include "smp/startup.h"
#include "smp/state.h"
#include "std/string.h"
#include "structs/ref_count_map.h"
#include "syscalls.h"
#include "system.h"
#include "task.h"
#include "vmm/vmmapper.h"
#include "x86_64/acpitables.h"
#include "x86_64/kdrivers/cpu.h"
#include "x86_64/kdrivers/hpet.h"
#include "x86_64/kdrivers/local_apic.h"

#ifdef DEBUG_ACPI
#include "printhex.h"
#endif

#define AP_CPUINIT_TIMEOUT 100000000 // 100ms

static ACPI_RSDT *acpi_root_table;

/* Globals */
MemoryRegion *physical_region;
uintptr_t kernel_zero_page;

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

#ifdef DEBUG_MADT
void debug_madt(ACPI_RSDT *rsdt);
#else
#define debug_madt(...)
#endif

static inline uint32_t volatile *init_this_cpu(ACPI_RSDT *rsdt,
                                               uint8_t cpu_num) {
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

static bool zeropage_init() {
    // If you're a retrocomputing fan, zeropage doesn't mean what
    // you think it means here - it's just a page full of zeroes...
    //
    // It shouldn't live in here, but it does for now. It's used by
    // the pagefault handler (so maybe should be over there, but it's
    // easier to init in early boot).

    kernel_zero_page = page_alloc(physical_region);

    if (kernel_zero_page & 0xff) {
        return false;
    }

    vmm_map_page(PER_CPU_TEMP_PAGE_BASE, kernel_zero_page,
                 PG_WRITE | PG_PRESENT);
    uint64_t *temp_map = (uint64_t *)PER_CPU_TEMP_PAGE_BASE;
    memclr(temp_map, VM_PAGE_SIZE);
    vmm_unmap_page(PER_CPU_TEMP_PAGE_BASE);

    return true;
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

// Common entrypoint once bootloader-specific stuff is handled
noreturn void bsp_kernel_entrypoint(uintptr_t rsdp_phys) {
    if (!fba_init((uint64_t *)vmm_find_pml4(), KERNEL_FBA_BEGIN,
                  KERNEL_FBA_SIZE_BLOCKS)) {
        panic("FBA init failed");
    }

    if (!slab_alloc_init()) {
        panic("Slab init failed");
    }

    if (!refcount_map_init()) {
        panic("Refcount map init failed");
    }

    if (!zeropage_init()) {
        panic("Zeropage init failed");
    }

    syscall_init();

#ifdef DEBUG_ACPI
    debugstr("RSDP at ");
    printhex64((uint64_t)rsdp, debugchar);
    debugstr(" (physical): OEM is ");
#endif

    ACPI_RSDP *rsdp = (ACPI_RSDP *)(rsdp_phys + 0xFFFFFFFF80000000);

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

    if (!ipwi_init()) {
        panic("Failed to initialise IPWI subsystem for one or more APs");
    }

#if MAX_CPU_COUNT > 1
    ap_startup_wait = true;
    smp_bsp_start_aps(acpi_root_table, lapic);
#endif

    pci_enumerate();

#ifdef DEBUG_NO_START_SYSTEM
    debugstr("All is well, DEBUG_NO_START_SYSTEM was specified, so halting for "
             "now.\n");
#else
    task_init(get_this_cpu_tss());
    process_init();
    sleep_init();

    if (!capabilities_init()) {
        panic("Capability subsystem initialisation failed");
    };

    ipc_channel_init();
    named_channel_init();

    if (!address_space_init()) {
        panic("Address space initialisation failed");
    }

    // We need to wait for basic CPU initialisation to complete on APs,
    // so we know they'll have their per-CPU state, IPWI, queues etc.
    //
    // We know this if they've reached the "wait for ap_startup_wait" loop.
    //
    wait_for_ap_basic_init_to_complete();

    // Now they're all initialized, we can notify other subsystems
    // that IPWI etc can be used.
    panic_notify_smp_started();
    pagefault_notify_smp_started();

    prepare_system();
    start_system();

    debugstr("Somehow ended up back in entrypoint, that's probably not good - "
             "halting.  ..\n");
#endif

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}
