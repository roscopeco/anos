/*
 * The device manager
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdio.h>

#include <anos/syscalls.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

int main(const int argc, char **argv) {
    printf("\nDEVMAN System device manager #%s [libanos #%s]\n\n", VERSION,
           libanos_version());

    while (1) {
        anos_task_sleep_current_secs(5);
    }
}