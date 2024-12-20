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
#include "debugprint.h"
#include "gdt.h"
#include "init_pagetables.h"
#include "interrupts.h"
#include "kdrivers/drivers.h"
#include "kdrivers/local_apic.h"
#include "machine.h"
#include "pci/enumerate.h"
#include "pmm/pagealloc.h"
#include "printhex.h"
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
void debug_madt(BIOS_SDTHeader *rsdt) {
    BIOS_SDTHeader *madt = find_acpi_table(rsdt, "APIC");

    if (madt == NULL) {
        debugstr("(ACPI MADT table not found)\n");
        return;
    }

    debugstr("MADT length    : ");
    printhex32(madt->length, debugchar);
    debugstr("\n");

    uint32_t *lapic_addr = ((uint32_t *)(madt + 1));
    uint32_t *flags = lapic_addr + 1;
    debugstr("LAPIC address  : ");
    printhex32(*lapic_addr, debugchar);
    debugstr("\n");
    debugstr("Flags          : ");
    printhex32(*flags, debugchar);
    debugstr("\n");

    uint16_t remain = madt->length - 0x2C;
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

static inline void install_interrupts() { idt_install(0x18); }

static inline void init_this_cpu(BIOS_SDTHeader *rsdt) {
    // Init local APIC on this CPU
    BIOS_SDTHeader *madt = find_acpi_table(rsdt, "APIC");

    if (madt == NULL) {
        debugstr("No MADT; Halting\n");
        halt_and_catch_fire();
    }

    init_local_apic(madt);
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

    store_gdtr(&gdtr);

    user_code = get_gdt_entry(&gdtr, 1);
    user_data = get_gdt_entry(&gdtr, 2);

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

MemoryRegion *physical_region;
BIOS_SDTHeader *acpi_root_table;

noreturn void start_kernel(BIOS_RSDP *rsdp, E820h_MemMap *memmap) {
    debugterm_init(VRAM_VIRT_BASE);
    banner();

    init_kernel_gdt();

    pagetables_init();
    physical_region =
            page_alloc_init(memmap, PMM_PHYS_BASE, STATIC_PMM_VREGION);
    install_interrupts();

    debugstr("We have ");
    printhex64(physical_region->size, debugchar);
    debugstr(" bytes physical memory\n");

#ifdef DEBUG_ACPI
    debugstr("RSDP at ");
    printhex64((uint64_t)rsdp, debugchar);
    debugstr(" (physical): OEM is ");
#endif

    rsdp = (BIOS_RSDP *)(((uint64_t)rsdp) | 0xFFFFFFFF80000000);

#ifdef DEBUG_ACPI
    debugstr(rsdp->oem_id);
    debugstr("\nRSDP revision is ");
    printhex8(rsdp->revision, debugchar);
    debugstr("\nRSDT at ");
    printhex32(rsdp->rsdt_address, debugchar);
    debugstr("\n");
#endif

    acpi_root_table = map_acpi_tables(rsdp);
    if (acpi_root_table == NULL) {
        debugstr("ACPI table mapping failed; halting\n");
        halt_and_catch_fire();
    }

    debug_memmap(memmap);
    debug_madt(acpi_root_table);
    init_this_cpu(acpi_root_table);
    init_kernel_drivers(acpi_root_table);

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
    // Force another page fault, to a non-handled address. Should have the WRITE
    // bit set in the error code...
    debugstr("Forcing another, this time unhandled, with write to 0x1200000\n");
    bad = (uint32_t *)0x1200000;
    *bad = 0x0BADF00D;
#endif

#ifdef DEBUG_TEST_USER_MODE_SWITCH
    debugstr("Going to infinite user-mode loop\n");

    // Set up a page for the user code
    uint64_t *pml4 = (uint64_t *)0xFFFFFFFF8009c000;
    uint64_t user_phys_page = page_alloc(physical_region);
    uint16_t flags = PRESENT | WRITE | USER;
    vmm_map_page(pml4, 0x1000000, user_phys_page, flags);

    // TODO invalidate tlb?

    // Set up a page for the user stack
    uint64_t user_stack = 0x1001000;
    uint64_t user_stack_phys = page_alloc(physical_region);
    vmm_map_page(pml4, user_stack, user_stack_phys, flags);

    // Set up a small function in user memory. Some say that it loops forever...
    uint8_t *user_func = (uint8_t *)0x1000000;

#ifdef DEBUG_TEST_USER_MODE_PRIVILEGED_INSTRUCTION_FAULT
    user_func[0] = 0xFA; // CLI (should result in a GPF)
#else
    user_func[0] = 0xEB; // JMP instruction
    user_func[1] = 0xFE; // ... to the same instruction
#endif
    // Switch to user mode
    __asm__ volatile(
            "mov %0, %%rsp\n\t" // Set stack pointer
            "push $0x13\n\t"    // Push user data segment selector (GDT entry 2)
            "push %0\n\t"       // Push user stack pointer
            "pushf\n\t"         // Push EFLAGS
            "push $0x0B\n\t"    // Push user code segment selector (GDT entry 1)
            "push %1\n\t"       // Push user code entry point
            "iretq\n\t"         // "Return" to user mode
            :
            : "r"(user_stack + 0x1000), "r"(user_func)
            : "memory");
#endif

    pci_enumerate();
    debugstr("All is well! Halting for now.\n");

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}
