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

#include "acpitables.h"
#include "cpuid.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "gdt.h"
#include "init_pagetables.h"
#include "interrupts.h"
#include "kdrivers/cpu.h"
#include "kdrivers/drivers.h"
#include "kdrivers/local_apic.h"
#include "ktypes.h"
#include "machine.h"
#include "panic.h"
#include "pci/enumerate.h"
#include "pmm/pagealloc.h"
#include "printdec.h"
#include "printhex.h"
#include "process/address_space.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/startup.h"
#include "smp/state.h"
#include "structs/ref_count_map.h"
#include "syscalls.h"
#include "system.h"
#include "task.h"
#include "vmm/recursive.h"
#include "vmm/vmmapper.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

// This is the static virtual address region (128GB from this base)
// that is reserved for PMM structures and stack.
#ifndef STATIC_PMM_VREGION
#define STATIC_PMM_VREGION ((void *)0xFFFFFF8000000000)
#endif

// The base address of the physical region this allocator should manage.
#ifndef PMM_PHYS_BASE
#define PMM_PHYS_BASE 0x200000
#endif

#ifndef VRAM_VIRT_BASE
#define VRAM_VIRT_BASE ((char *const)0xffffffff800b8000)
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static char *MSG = VERSION "\n";

static ACPI_RSDT *acpi_root_table;

/* Globals */
MemoryRegion *physical_region;

// We set this at startup, and once the APs are started up,
// they'll wait for this to go false before they start
// their own system schedulers.
//
// This way, we can ensure the main one is started and
// everything's initialized before we let them start
// theirs...
volatile bool ap_startup_wait;

#ifdef DEBUG_MEMMAP
void debug_memmap(E820h_MemMap *memmap);
#else
#define debug_memmap(...)
#endif

#ifdef DEBUG_MADT
void debug_madt(ACPI_RSDT *rsdt);
#else
#define debug_madt(...)
#endif

static inline void banner() {
    debugattr(0x0B);
    debugstr("STAGE");
    debugattr(0x03);
    debugchar('3');
    debugattr(0x08);
    debugstr(" #");
    debugattr(0x0F);
    debugstr(MSG);
    debugattr(0x07);
}

static inline void install_interrupts() { idt_install(0x08); }

static inline uint32_t volatile *init_this_cpu(ACPI_RSDT *rsdt,
                                               uint8_t cpu_num) {
    cpu_init_this();
    cpu_debug_info(cpu_num);

    // Allocate our per-CPU data
    uint64_t *state_block = fba_alloc_block();

    if (!state_block) {
        panic("Failed to allocate CPU state");
    }

    for (int i = 0; i < sizeof(PerCPUState) / 8; i++) {
        state_block[i] = 0;
    }

    PerCPUState *cpu_state = (PerCPUState *)state_block;

    cpu_state->self = cpu_state;
    cpu_state->cpu_id = cpu_num;
    cpu_state->lapic_id = cpu_read_local_apic_id();
    cpu_get_brand_str(cpu_state->cpu_brand);

    cpu_write_msr(MSR_KernelGSBase, (uint64_t)cpu_state);
    cpu_write_msr(MSR_GSBase, 0);
    cpu_swapgs();

    // Init local APIC on this CPU
    ACPI_MADT *madt = acpi_tables_find_madt(rsdt);

    if (madt == NULL) {
        debugstr("No MADT; Halting\n");
        halt_and_catch_fire();
    }

    return init_local_apic(madt, cpu_num == 0);
}

// Replace the bootstrap 32-bit pages with 64-bit user pages.
//
// TODO we should remap the memory as read-only after this since they
// won't be changing again, accessed bit is already set ready for this...
//
static inline void init_kernel_gdt() {
    GDTR gdtr;
    GDTEntry *user_code;
    GDTEntry *user_data;

    cpu_store_gdtr(&gdtr);

    // Reverse ordered because SYSRET is fucking weird...
    user_data = get_gdt_entry(&gdtr, 3);
    user_code = get_gdt_entry(&gdtr, 4);

    init_gdt_entry(
            user_code, 0, 0,
            GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(3) |
                    GDT_ENTRY_ACCESS_NON_SYSTEM | GDT_ENTRY_ACCESS_EXECUTABLE |
                    GDT_ENTRY_ACCESS_READ_WRITE | GDT_ENTRY_ACCESS_ACCESSED,
            GDT_ENTRY_FLAGS_64BIT);

    init_gdt_entry(user_data, 0, 0,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(3) |
                           GDT_ENTRY_ACCESS_NON_SYSTEM |
                           GDT_ENTRY_ACCESS_READ_WRITE |
                           GDT_ENTRY_ACCESS_ACCESSED,
                   GDT_ENTRY_FLAGS_64BIT);
}

static inline void *get_this_cpu_tss() {
    PerCPUState *cpu_state = state_get_per_cpu();
    return gdt_per_cpu_tss(cpu_state->cpu_id);
}

noreturn void ap_kernel_entrypoint(uint64_t ap_num) {
#ifdef DEBUG_SMP_STARTUP
#ifdef VERY_NOISY_SMP_STARTUP
    spinlock_lock(&debug_output_lock);

    debugstr("AP #");
    printdec(ap_num, debugchar);
    debugstr(" has entered the chat...\n");

    spinlock_unlock(&debug_output_lock);
#endif
#endif

    syscall_init();

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, ap_num);

    task_init(get_this_cpu_tss());
    sleep_init();

    while (ap_startup_wait) {
        // just busy right now, but should hlt and wait for an IPI or something...?
    }

    start_system_ap(ap_num);

    panic("Somehow ended up back in AP entrypoint. This is a bad thing...");
}

noreturn void bsp_kernel_entrypoint(ACPI_RSDP *rsdp, E820h_MemMap *memmap) {
    debugterm_init(VRAM_VIRT_BASE);
    banner();

    init_kernel_gdt();
    install_interrupts();
    pagetables_init();

    physical_region =
            page_alloc_init(memmap, PMM_PHYS_BASE, STATIC_PMM_VREGION);

    if (!fba_init((uint64_t *)vmm_recursive_find_pml4(), KERNEL_FBA_BEGIN,
                  KERNEL_FBA_SIZE_BLOCKS)) {
        panic("FBA init failed");
    }

    if (!slab_alloc_init()) {
        panic("Slab init failed");
    }

    if (!refcount_map_init()) {
        panic("Refcount map init failed");
    }

    syscall_init();

#ifdef DEBUG_ACPI
    debugstr("RSDP at ");
    printhex64((uint64_t)rsdp, debugchar);
    debugstr(" (physical): OEM is ");
#endif

    rsdp = (ACPI_RSDP *)(((uint64_t)rsdp) | 0xFFFFFFFF80000000);

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

    debug_memmap(memmap);
    debug_madt(acpi_root_table);
    kernel_drivers_init(acpi_root_table);

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, 0);

#if MAX_CPU_COUNT > 1
    ap_startup_wait = true;
    smp_bsp_start_aps(acpi_root_table, lapic);
#endif

    pci_enumerate();

#ifdef DEBUG_FORCE_HANDLED_PAGE_FAULT
    debugstr("\nForcing a (handled) page fault with write to "
             "0xFFFFFFFF80600000...\n");

    // Force a page fault for testing purpose. This one should get handled and a
    // page mapped...
    uint32_t *bad = (uint32_t *)0xFFFFFFFF80600000;
    *bad = 0x0A11C001;
    debugstr("Continued after fault, write succeeded. Value is ");
    debugattr(0x02);
    printhex32(*bad, debugchar);
    debugattr(0x07);
    debugstr("\n");
#endif

#ifdef DEBUG_FORCE_UNHANDLED_PAGE_FAULT
    // Map a page, write to it, read it back, then unmap and attempt to read
    // again. Should force an unhandled page fault.
    debugstr("Forcing unhandled page fault and testing vmm_map / vmm_unmap at "
             "the same time...\n");
    volatile uint64_t *volatile ptr = (uint64_t *)0x400000000;
    uint64_t tpage = page_alloc(physical_region);
    vmm_map_page(0x400000000, tpage, WRITE | PRESENT);

    *ptr = 0x12345678;

    debugstr("Mapped & set: now ");
    printhex64(*ptr, debugchar);
    debugstr("\n");

    uintptr_t old = vmm_unmap_page(0x400000000);
    debugstr("Unmapped - phys was ");
    printhex64(old, debugchar);
    debugstr("; should equal ");
    printhex64(tpage, debugchar);
    debugstr("\n");

    debugstr("Attempting read - should panic here...\n");
    printhex64(*ptr, debugchar);
    debugstr("\n");
#endif

#ifdef DEBUG_NO_START_SYSTEM
    debugstr("All is well, DEBUG_NO_START_SYSTEM was specified, so halting for "
             "now.\n");
#else
    task_init(get_this_cpu_tss());
    process_init();
    sleep_init();

    if (!address_space_init()) {
        panic("Address space initialisation failed");
    }

    prepare_system();
    start_system();
    debugstr("Somehow ended up back in entrypoint, that's probably not good - "
             "halting.  ..\n");
#endif

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}
