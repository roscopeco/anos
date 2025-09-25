/*
 * stage3 - The page fault handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "machine.h"
#include "panic.h"
#include "pmm/pagealloc.h"
#include "smp/state.h"
#include "std/string.h"
#include "structs/ref_count_map.h"
#include "structs/region_tree.h"
#include "vmm/vmmapper.h"
#include "vmm/vmregion.h"

#ifdef DEBUG_PAGEFAULT
#include "debugprint.h"
#include "kprintf.h"
#include "printhex.h"
#ifdef VERY_NOISY_PAGEFAULT
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#define vdbgx32(arg) printhex32(arg, debugchar)
#define vdbgx16(arg) printhex16(arg, debugchar)
#define vdbgx8(arg) printhex8(arg, debugchar)
#define tdebugf kprintf
#else
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx32(...)
#define vdbgx16(...)
#define vdbgx8(...)
#define vdebugf(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#define tdbgx32(arg) printhex32(arg, debugchar)
#define tdbgx16(arg) printhex16(arg, debugchar)
#define tdbgx8(arg) printhex8(arg, debugchar)
#define tdebugf kprintf
#else
#define tdebug(...)
#define tdbgx64(...)
#define tdbgx32(...)
#define tdbgx16(...)
#define tdbgx8(...)
#define tdebugf(...)
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx32(...)
#define vdbgx16(...)
#define vdbgx8(...)
#define vdebugf(...)
#endif

extern MemoryRegion *physical_region;
extern uintptr_t kernel_zero_page;

// Handle page faults before we have SMP and tasking up...
void early_page_fault_handler(const uint64_t code, const uint64_t fault_addr, const uint64_t origin_addr) {
    panic_page_fault(origin_addr, fault_addr, code);
}

// Allocate process mem if Process is non-NULL, or just unowned mem otherwise
static uintptr_t alloc_phys_appropriately(Process *current_process) {
    uintptr_t phys;

    if (current_process) {
        phys = process_page_alloc(current_process, physical_region);
    } else {
        phys = page_alloc(physical_region);
    }

    return phys;
}

static void copy_page_safely(const uintptr_t src_virt_page, const uintptr_t dest_phys_page) {
    vdebugf("SAFE COPY PAGE\n");
    const uint64_t int_flags = save_disable_interrupts();
    const PerCPUState *state = state_get_for_this_cpu();
    const uintptr_t per_cpu_temp_page = vmm_per_cpu_temp_page_addr(state->cpu_id);

    vdebugf("    * Using TEMP PAGE for CPU %d = 0x%016lx\n", state->cpu_id, per_cpu_temp_page);

    vmm_map_page(per_cpu_temp_page, dest_phys_page, PG_PRESENT | PG_READ | PG_WRITE);

    const uint64_t *src_page = (uint64_t *)src_virt_page;
    uint64_t *dest_page = (uint64_t *)per_cpu_temp_page;

    vdebugf("    * Mapped SRC @ 0x%016lx : DEST @ 0x%016lx\n", (uintptr_t)src_page, (uintptr_t)dest_page);

    memcpy(dest_page, src_page, VM_PAGE_SIZE);

    vdebugf("    * Safe memcpy done\n");

    vmm_unmap_page(per_cpu_temp_page);
    restore_saved_interrupts(int_flags);
}

// The full handler, replaces early once tasking and system is up
void page_fault_handler(const uint64_t code, const uint64_t fault_addr, const uint64_t origin_addr) {

    tdebug("PAGEFAULT: 0x");
    tdbgx64(fault_addr);
    tdebug("\n");

    const uint64_t pte = vmm_virt_to_pt_entry(fault_addr);
    const uintptr_t fault_addr_page = fault_addr & PAGE_ALIGN_MASK;
    const uintptr_t current_phys_addr = vmm_table_entry_to_phys(pte);

    const Task *current_task = task_current();
    Process *current_process = nullptr;
    if (current_task) {
        current_process = current_task->owner;
    }

    vdebug("PF for 0x%016lx (current phys 0x%016lx)\n", fault_addr_page, current_phys_addr);

    // COW & automap only works for pages mapped present in
    // userspace - we want to fail fast in kernel space!
    //
    // TODO this should really only handle processes (and remove the
    //      conditional on whether allocs are owned or not) but there's
    //      an edge case at startup right now - see comments in
    //      system.c...
    //
    if (IS_USER_ADDRESS(fault_addr) && pte & PG_PRESENT) {
        // Check if it's a copy-on-write page
        vdebug("CHECK COW\n");
        if (pte & PG_COPY_ON_WRITE) {
            vdebug("  --> IS COW\n");
            if (code & PG_WRITE) {
                vdebug("  --> IS WRITE\n");

                // This is a write to a COW page...
                // Can we just remap the page as write?
                if (current_phys_addr != kernel_zero_page) {
                    // It's not the zero page...
                    if (refcount_map_decrement(current_phys_addr) == 0) {
                        // ... and nobody else is referencing this page, assume
                        // other referees are gone. So we can just make it
                        // writeable, no need to copy.
                        vmm_map_page(fault_addr_page, pte & PAGE_ALIGN_MASK,
                                     (vmm_table_entry_to_page_flags(pte) & ~(PG_COPY_ON_WRITE)) | PG_WRITE);

                        // That's all we need!
                        return;
                    }
                }

                // This is the zero page, or there are still references to this
                // page elsewhere, so we need to copy it...
                //
                const uintptr_t phys = alloc_phys_appropriately(current_process);

                if (phys & 0xff) {
                    // phys alloc failed - panic anyway
                    panic_page_fault(origin_addr, fault_addr, code);
                }
                vdebugf("Allocated page 0x%016lx for COW destination\n", phys);

                copy_page_safely(fault_addr_page, phys);

                vmm_map_page(fault_addr_page, phys,
                             (vmm_table_entry_to_page_flags(pte) & ~(PG_COPY_ON_WRITE)) | PG_WRITE);

                return;
            }
        }
    }

    // --- Check if we're in a memory region that needs handling...
    vdebug("CHECK REGION\n");
    if (current_process) {

        const Region *region = vm_region_find_in_process(current_process, fault_addr);

#ifdef VERY_NOISY_PAGEFAULT
        if (!region) {
            vdebug("  --> NOPE\n");
        }
#endif

        if (region && region->flags & VM_REGION_AUTOMAP) {
            vdebug("PAGE IN REGION\n");

            if (code & PG_WRITE) {
                // First access to an automap region, it's a write, so just
                // allocate a page and zero it.
                //
                const uintptr_t phys = alloc_phys_appropriately(current_process);

                if (phys & 0xff) {
                    // phys alloc failed - panic anyway
                    panic_page_fault(origin_addr, fault_addr, code);
                }

                vmm_map_page(fault_addr_page, phys, PG_USER | PG_READ | PG_WRITE | PG_PRESENT);
                uint64_t *mapped_page = (uint64_t *)fault_addr_page;

                memclr(mapped_page, VM_PAGE_SIZE);

                return;
            }

            // first access, it's a read, so just map the zero page COW...
            vmm_map_page(fault_addr_page, kernel_zero_page, PG_USER | PG_READ | PG_PRESENT | PG_COPY_ON_WRITE);
            return;
        }
    }

    tdebug("Unhandled #PF - panicking\n");
    panic_page_fault(origin_addr, fault_addr, code);
}
