/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "anos.h"
#include "anos/anos_syscalls.h"
#include "anos/printf.h"
#include <stdint.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

volatile int num;

int subroutine(int in) { return in * 2; }

static uint8_t t2_stack[4096];

static inline void banner() {
    printf("\n\nSYSTEM User-mode Supervisor #%s [libanos #%s]\n", VERSION,
           libanos_version());
}

static void thread2() {
    int count = 0;

    while (1) {
        if (count++ % 100000000 == 0) {
            kputchar('B');
        }
    }
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

    int thread = anos_create_thread(thread2, ((uintptr_t)t2_stack) + 0x1000);

    if (thread == 0) {
        printf("Thread creation failed!\n");
    } else {
        printf("Thread created: 0x%02x\n", thread);
    }

    while (1) {
        num = subroutine(num);

        if (count++ % 100000000 == 0) {
            kputchar('.');
        }
    }
}