/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdnoreturn.h>
#include <stdbool.h>

noreturn void halt_and_catch_fire() {
    while (true) {
        __asm__ volatile (
            "hlt\n\t"
        );
    }
}
