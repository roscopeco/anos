/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

noreturn void halt_and_catch_fire(void) {
    __asm__ volatile("cli\n\t");

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}

void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}