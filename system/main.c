/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "anos/anos_syscalls.h"
#include <stdint.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static const char *MSG = VERSION "\n";

volatile int num;

#ifdef DEBUG_INT_SYSCALLS
#define kprint kprint_int
#else
#define kprint kprint_syscall
#endif

int subroutine(int in) { return in * 2; }

static inline void banner() {
    kprint("\n\nSYSTEM User-mode Supervisor #");
    kprint(MSG);
    kprint("\n");
}

int main(int argc, char **argv) {
    banner();

    uint64_t ret;

    if ((ret = testcall_int(1, 2, 3, 4, 5)) == 42) {
        kprint("GOOD\n");
    } else {
        kprint("BAD\n");
    }

    if ((ret = testcall_syscall(1, 2, 3, 4, 5)) == 42) {
        kprint("GOOD\n");
    } else {
        kprint("BAD\n");
    }

    num = 1;
    int count = 0;

    while (1) {
        num = subroutine(num);

        if (count++ % 100000000 == 0) {
            kprint(".");
        }
    }
}