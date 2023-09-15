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

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION         STRVER(VERSTR)

// these will end up in rodata
static char *vram = (char*)0xb8000;
static char *MSG = "ANOS #" VERSION;

// this will end up in bss
static char *msgp;

noreturn void halt() {
    while (true) {
        __asm__ volatile (
            "hlt\n\t"
        );
    }
}

noreturn void start_kernel() {
    msgp = MSG;
    int vrptr = 0;

    vram[vrptr++] = '[';
    vram[vrptr++] = 0x07;
    
    while (*msgp) {
        vram[vrptr++] = *msgp++;
        vram[vrptr++] = 0x0f;
    }

    vram[vrptr++] = ']';
    vram[vrptr++] = 0x07;

    halt();
 }


