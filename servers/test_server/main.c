/*
 * A test server that checks the basics work
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "anos.h"
#include "anos/anos_syscalls.h"
#include "anos/anos_types.h"
#include "anos/printf.h"

int main(int argc, char **argv) {
    anos_kprint("Beep Boop process is up...\n");

    while (1) {
        anos_task_sleep_current_secs(10);
        printf("<beep>");
        anos_task_sleep_current_secs(10);
        printf("<boop>");
    }
}
