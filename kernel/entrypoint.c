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
#include "process.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/startup.h"
#include "smp/state.h"
#include "syscalls.h"
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

extern void *_system_bin_start;
extern void *_system_bin_end;

static char *MSG = VERSION "\n";

void user_thread_entrypoint(void);
void kernel_thread_entrypoint(void);

#ifdef DEBUG_MEMMAP
static char *MEM_TYPES[] = {"INVALID",  "AVAILABLE",  "RESERVED",
                            "ACPI",     "NVS",        "UNUSABLE",
                            "DISABLED", "PERSISTENT", "UNKNOWN"};

void debug_memmap(E820h_MemMap *memmap) {
    debugstr("\nThere are ");
    printhex16(memmap->num_entries, debugchar);
    debugstr(" memory map entries\n");

    for (int i = 0; i < memmap->num_entries; i++) {
        E820h_MemMapEntry *entry = &memmap->entries[i];

        debugstr("Entry ");
        printhex16(i, debugchar);
        debugstr(": ");
        printhex64(entry->base, debugchar);
        debugstr(" -> ");
        printhex64(entry->base + entry->length, debugchar);

        if (entry->type < 8) {
            debugstr(" (");
            debugstr(MEM_TYPES[entry->type]);
            debugstr(")\n");
        } else {
            debugstr(" (");
            debugstr(MEM_TYPES[8]);
            debugstr(")\n");
        }
    }
}
#else
#define debug_memmap(...)
#endif

#ifdef DEBUG_MADT
void debug_madt(ACPI_RSDT *rsdt) {
    ACPI_MADT *madt = acpi_tables_find_madt(rsdt);

    if (madt == NULL) {
        debugstr("(ACPI MADT table not found)\n");
        return;
    }

    debugstr("MADT length    : ");
    printhex32(madt->header.length, debugchar);
    debugstr("\n");

    debugstr("LAPIC address  : ");
    printhex32(madt->lapic_address, debugchar);
    debugstr("\n");
    debugstr("Flags          : ");
    printhex32(madt->lapic_address, debugchar);
    debugstr("\n");

    uint16_t remain = madt->header.length - 0x2C;
    uint8_t *ptr = ((uint8_t *)madt) + 0x2C;

    while (remain > 0) {
        uint8_t *type = ptr++;
        uint8_t *len = ptr++;

        uint16_t *flags16;
        uint32_t *flags32;

        switch (*type) {
        case 0: // Processor local APIC
            debugstr("  CPU            [ID: ");
            printhex8(*ptr++, debugchar);
            debugstr("; LAPIC ");
            printhex8(*ptr++, debugchar);
            flags32 = (uint32_t *)ptr;
            debugstr("; Flags: ");
            printhex32(*flags32, debugchar);
            debugstr("]\n");
            ptr += 4;
            break;
        case 1: // IO APIC
            debugstr("  IOAPIC         [ID: ");
            printhex8(*ptr++, debugchar);

            ptr++; // skip reserved

            uint32_t *apicaddr = (uint32_t *)ptr;
            ptr += 4;
            uint32_t *gsibase = (uint32_t *)ptr;
            ptr += 4;

            debugstr("; Addr: ");
            printhex32(*apicaddr, debugchar);
            debugstr("; GSIBase: ");
            printhex32(*gsibase, debugchar);
            debugstr("]\n");
            break;
        case 2: // IP APIC Source Override
            debugstr("  IOAPIC Src O/R [Bus: ");
            printhex8(*ptr++, debugchar);
            debugstr("; IRQ: ");
            printhex8(*ptr++, debugchar);

            uint32_t *gsi = (uint32_t *)ptr;
            ptr += 4;
            debugstr("; GSI: ");
            printhex32(*gsi, debugchar);

            flags16 = (uint16_t *)ptr;
            ptr += 2;
            debugstr("; Flags: ");
            printhex16(*flags16, debugchar);
            debugstr("]\n");

            break;
        case 4: // LAPIC NMI
            debugstr("  LAPIC NMI      [Processor: ");
            printhex8(*ptr++, debugchar);

            flags16 = (uint16_t *)ptr;
            ptr += 2;
            debugstr("; Flags: ");
            printhex16(*flags16, debugchar);

            debugstr("; LINT#: ");
            printhex8(*ptr++, debugchar);
            debugstr("]\n");

            break;
        default:
            // Just skip over
            debugstr("  #TODO UNKNOWN  [Type: ");
            printhex8(*type, debugchar);
            debugstr("; Len: ");
            printhex8(*len, debugchar);
            debugstr("]\n");
            ptr += *len - 2;
        }

        remain -= *len;
    }
}
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
    cpu_debug_info();

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

MemoryRegion *physical_region;
ACPI_RSDT *acpi_root_table;

noreturn void start_system(void) {
    const uint64_t system_start_virt = 0x1000000;
    const uint64_t system_start_phys =
            (uint64_t)&_system_bin_start & ~(0xFFFFFFFF80000000);
    const uint64_t system_len_bytes =
            (uint64_t)&_system_bin_end - (uint64_t)&_system_bin_start;
    const uint64_t system_len_pages = system_len_bytes >> 12;

    const uint16_t flags = PRESENT | USER;

    // Map pages for the user code
    for (int i = 0; i < system_len_pages; i++) {
        vmm_map_page(system_start_virt + (i << 12),
                     system_start_phys + (i << 12), flags);
    }

    // TODO the way this is set up currently, there's no way to know how much
    // BSS/Data we need... We'll just map a couple pages for now...

    // Set up four pages for the user bss / data
    uint64_t user_bss = 0x0000000080000000;
    uint64_t user_bss_phys = page_alloc(physical_region);
    vmm_map_page(user_bss, user_bss_phys, flags | WRITE);
    user_bss_phys = page_alloc(physical_region);
    vmm_map_page(user_bss + 0x1000, user_bss_phys, flags | WRITE);
    user_bss_phys = page_alloc(physical_region);
    vmm_map_page(user_bss + 0x2000, user_bss_phys, flags | WRITE);
    user_bss_phys = page_alloc(physical_region);
    vmm_map_page(user_bss + 0x3000, user_bss_phys, flags | WRITE);

    // ... and a page below that for the user stack
    uint64_t user_stack = user_bss - 0x1000;
    uint64_t user_stack_phys = page_alloc(physical_region);
    vmm_map_page(user_stack, user_stack_phys, flags | WRITE);

    // ... the FBA can give us a kernel stack...
    void *kernel_stack = fba_alloc_blocks(4); // 16KiB

    // create a process and task for system
    sched_init((uintptr_t)user_stack + 0x1000, (uintptr_t)kernel_stack + 0x4000,
               (uintptr_t)0x0000000001000000, (uintptr_t)user_thread_entrypoint,
               TASK_CLASS_NORMAL);

    // We can just get away with disabling here, no need to save/restore flags
    // because we know we're currently the only thread...
    __asm__ volatile("cli");

    // Kick off scheduling...
    sched_schedule();

    // ... which will never return to here.
    __builtin_unreachable();
}

static noreturn void idle() {
    while (1) {
        __asm__ volatile("hlt");
    }

    __builtin_unreachable();
}

noreturn void start_idle(void) {
    // ... the FBA can give us runtime and kernel stacks...
    void *runtime_stack = fba_alloc_block(); // 4KiB
    void *kernel_stack = fba_alloc_block();  // 4KiB

    // create a process and task for idle
    sched_init((uintptr_t)runtime_stack + 0x1000,
               (uintptr_t)kernel_stack + 0x1000, (uintptr_t)idle,
               (uintptr_t)kernel_thread_entrypoint, TASK_CLASS_IDLE);

    // We can just get away with disabling here, no need to save/restore flags
    // because we know we're currently the only thread on this CPU...
    __asm__ volatile("cli");

    // Kick off scheduling...
    sched_schedule();

    // ... which will never return to here.
    __builtin_unreachable();
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

    init_cpuid();

    syscall_init();

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, ap_num);

    task_init(get_this_cpu_tss());
    sleep_init();

    start_idle();

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
        debugstr("ACPI table mapping failed; halting\n");
        halt_and_catch_fire();
    }

    debug_memmap(memmap);
    debug_madt(acpi_root_table);
    kernel_drivers_init(acpi_root_table);

    uint32_t volatile *lapic = init_this_cpu(acpi_root_table, 0);

#ifndef NO_SMP
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
    sleep_init();

    start_system();
    debugstr("Somehow ended up back in entrypoint, that's probably not good - "
             "halting.  ..\n");
#endif

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}
