/*
 * A test server that checks the basics work
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <anos.h>
#include <anos/printf.h>
#include <anos/syscalls.h>
#include <anos/types.h>

int main(int argc, char **argv) {
    anos_kprint("Beep Boop process is up...\n");

    while (1) {
        anos_task_sleep_current_secs(5);
        printf("<beep>\n");
        anos_task_sleep_current_secs(5);
        printf("<boop>\n");
    }
}
