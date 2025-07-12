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
#include "pagefault.h"
#include "panic.h"
#include "platform.h"
#include "pmm/pagealloc.h"
#include "process/address_space.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/ipwi.h"
#include "std/string.h"
#include "structs/ref_count_map.h"
#include "system.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_ACPI
#include "printhex.h"
#endif

/* Globals */
MemoryRegion *physical_region;
uintptr_t kernel_zero_page;

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
                 PG_READ | PG_WRITE | PG_PRESENT);
    uint64_t *temp_map = (uint64_t *)PER_CPU_TEMP_PAGE_BASE;
    memclr(temp_map, VM_PAGE_SIZE);
    vmm_unmap_page(PER_CPU_TEMP_PAGE_BASE);

    return true;
}

// Common entrypoint once bootloader-specific stuff is handled
noreturn void bsp_kernel_entrypoint(const uintptr_t platform_data) {
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

    if (!platform_init(platform_data)) {
        panic("Platform init failed");
    }

    if (!ipwi_init()) {
        panic("Failed to initialise IPWI subsystem for the bootstrap "
              "processor");
    }

#ifdef DEBUG_NO_START_SYSTEM
    debugstr("All is well, DEBUG_NO_START_SYSTEM was specified, so halting for "
             "now.\n");

    halt_and_catch_fire();
#else
    platform_task_init();

    process_init();
    sleep_init();

    if (!capabilities_init()) {
        panic("Capability subsystem initialisation failed");
    }

    ipc_channel_init();
    named_channel_init();

    if (!address_space_init()) {
        panic("Address space initialisation failed");
    }

    if (!platform_await_init_complete()) {
        panic("Platform initialization did not complete");
    }

    // Now they're all initialized, we can notify other subsystems
    // that IPWI etc can be used.
    panic_notify_smp_started();
    pagefault_notify_smp_started();

    // And finally, start the system!
    prepare_system();
    start_system();
#endif
}
