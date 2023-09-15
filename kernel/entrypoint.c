/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * We're fully 64-bit at this point 🎉
 */

#include <stdbool.h>
#include <stdnoreturn.h>

// these will end up in rodata
static char *vram = (char*)0xb8000;
static char *MSG = "Made it to the kernel!";

// this will end up in bss
static char *msgp;

noreturn void start_kernel() {
    msgp = MSG;
    int vrptr = 320;

    while (*msgp) {
        vram[vrptr++] = *msgp++;
        vram[vrptr++] = 0x3b;
    }

    while (true) {
        __asm__ volatile (
            "hlt\n\t"
        );
    }
 }
