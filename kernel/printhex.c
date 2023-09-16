/*
 * stage3 - Hex printer for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>
#include "printhex.h"

#define ZERO    48
#define EX      120

static inline void preamble(PrintHexCharHandler printfunc) {
    printfunc(ZERO);
    printfunc(EX);
}

static inline void digitprint(uint8_t digit, PrintHexCharHandler printfunc) {
    if (digit < 10) {
        digit += 48;
    } else {
        digit += 87;
    }

    printfunc(digit);
}

void printhex64(uint64_t num, PrintHexCharHandler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 64; i += 4) {
        char digit = (num & 0xF000000000000000) >> 60;
        num <<= 4;
        digitprint(digit, printfunc);        
    }
}

void printhex32(uint64_t num, PrintHexCharHandler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 32; i += 4) {
        char digit = (num & 0xF0000000) >> 28;
        num <<= 4;
        digitprint(digit, printfunc);        
    }
}

void printhex16(uint64_t num, PrintHexCharHandler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 16; i += 4) {
        char digit = (num & 0xF000) >> 12;
        num <<= 4;
        digitprint(digit, printfunc);        
    }
}

void printhex8(uint64_t num, PrintHexCharHandler printfunc) {
    preamble(printfunc);

    for (int i = 0; i < 8; i += 4) {
        char digit = (num & 0xF0) >> 4;
        num <<= 4;
        digitprint(digit, printfunc);        
    }
}
