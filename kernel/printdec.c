/*
 * stage3 - Decimal printer for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "printdec.h"
#include <stdint.h>

static char *llmin = "9223372036854775808";

static void print_llong_min(PrintDecCharHandler printfunc) {
    char *c = llmin;

    while (*c) {
        printfunc(*c++);
    }
}

void printdec(int64_t num, PrintDecCharHandler printfunc) {
    // Handle negative numbers
    if (num < 0) {
        printfunc('-');
        // Handle LLONG_MIN (-9223372036854775808) specially
        if (num == -9223372036854775807LL - 1) {
            print_llong_min(printfunc);
            return;
        }
        num = -num;
    }

    // Handle case when n is 0
    if (num == 0) {
        printfunc('0');
        return;
    }

    // Buffer to store digits in reverse
    char buf[20]; // Max 20 digits for 64-bit int
    char *p = buf;

    // Extract digits in reverse order
    while (num > 0) {
        *p++ = num % 10 + '0';
        num /= 10;
    }

    // Print digits in correct order
    while (p > buf) {
        printfunc(*--p);
    }
}