/*
 * stage3 - The page fault handler for RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "pagefault.h"
#include "machine.h"

static bool smp_started;

void page_fault_wrapper(const uint64_t code, const uint64_t fault_addr,
                        const uint64_t origin_addr) {
    if (likely(smp_started)) {
        page_fault_handler(code, fault_addr, origin_addr);
    } else {
        early_page_fault_handler(code, fault_addr, origin_addr);
    }
}

void pagefault_notify_smp_started(void) { smp_started = true; }