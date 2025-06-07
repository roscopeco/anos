/*
 * The device manager
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <anos/syscalls.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

int main(const int argc, char **argv) {
    anos_kprint("\nDEVMAN System device manager #");
    anos_kprint(VERSION);
    anos_kprint(" [libanos #");
    anos_kprint(libanos_version());
    anos_kprint("]\n\n");

    while (1) {
        anos_task_sleep_current_secs(5);
    }
}