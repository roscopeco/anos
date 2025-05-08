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
#include "vmm/vmmapper.h"

extern MemoryRegion *physical_region;

void handle_page_fault(uint64_t code, uint64_t fault_addr,
                       uint64_t origin_addr) {

    if (code & PG_WRITE) {
        uint64_t pte = vmm_virt_to_pt_entry(fault_addr);
        if (pte & PG_COPY_ON_WRITE) {
            // This is a write to a COW page...
            uint64_t fault_addr_page = fault_addr & PAGE_ALIGN_MASK;

            if (refcount_map_decrement(pte & PAGE_ALIGN_MASK) == 0) {
                // Nobody else is referencing this page, assume other referees are gone.
                // So we can just make it writeable, no need to copy.
                vmm_map_page(fault_addr_page, pte & PAGE_ALIGN_MASK,
                             ((pte & PAGE_FLAGS_MASK) & ~(PG_COPY_ON_WRITE)) |
                                     PG_WRITE);
            } else {
                // There are still references to this page elsewhere, so
                // we need to copy it...
                uintptr_t phys = page_alloc(physical_region);

                if (phys & 0xff) {
                    // phys alloc failed - panic anyway
                    panic_page_fault(origin_addr, fault_addr, code);
                }

                // TODO potential race condition here, if we get rescheduled onto
                // a different CPU, and this CPU then goes on to do stuff that
                // needs the temp mapping, this will go wrong.
                //
                // I dislike the whole per-CPU temp mapping idea tbh, need to
                // come up with something better...

                PerCPUState *state = state_get_for_this_cpu();
                uintptr_t per_cpu_temp_page =
                        vmm_per_cpu_temp_page_addr(state->cpu_id);

                vmm_map_page(per_cpu_temp_page, phys,
                             ((pte & PAGE_FLAGS_MASK) & ~(PG_COPY_ON_WRITE)) |
                                     PG_WRITE);
                uint64_t *src_page = (uint64_t *)fault_addr_page;
                uint64_t *dest_page = (uint64_t *)per_cpu_temp_page;

                memcpy(dest_page, src_page, VM_PAGE_SIZE);

                vmm_unmap_page(per_cpu_temp_page);
                vmm_map_page(fault_addr_page, phys,
                             ((pte & PAGE_FLAGS_MASK) & ~(PG_COPY_ON_WRITE)) |
                                     PG_WRITE);
            }
            return;
        }
    }

    panic_page_fault(origin_addr, fault_addr, code);
}
