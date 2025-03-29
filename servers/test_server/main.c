/*
 * A test server that checks the basics work
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdio.h>

#include <anos/syscalls.h>
#include <anos/types.h>

__attribute__((constructor)) void testing_init(void) {
    anos_kprint("Beep Boop process is up...\n");
}

int main(int argc, char **argv) {
    while (1) {
        anos_task_sleep_current_secs(5);
        anos_kprint("<beep>\n");
        anos_task_sleep_current_secs(5);
        anos_kprint("<boop>\n");
    }
}
