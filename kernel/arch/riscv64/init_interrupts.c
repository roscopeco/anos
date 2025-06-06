/*
 * stage3 - Install trap / interrupt handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "panic.h"

#include "riscv64/interrupts.h"

extern void supervisor_trap_dispatcher(void);

void unknown_trap_handler(uintptr_t vector, uintptr_t origin) {
    panic_exception_no_code(vector, origin);
}

void install_interrupts(void) {
    set_supervisor_trap_vector(supervisor_trap_dispatcher);
}
