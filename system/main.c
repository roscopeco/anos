/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static const char *MSG = VERSION "\n";

volatile int num;

void kprint(const char *msg) {
    __asm__ volatile("mov %0,%%rdi\n\t"
                     "int $0x69\n\t"
                     :
                     : "r"(msg)
                     : "memory");
}

int subroutine(int in) { return in * 2; }

static inline void banner() {
    kprint("\n\nSYSTEM User-mode Supervisor #");
    kprint(MSG);
    kprint("\n");
}

int main(int argc, char **argv) {
    banner();

    num = 1;
    int count = 0;

    while (1) {
        num = subroutine(num);

        if (count++ % 100000000 == 0) {
            kprint(".");
        }
    }
}