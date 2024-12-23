/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "debugprint.h"
#include <stdint.h>

void handle_syscall_69(char *message) {
    if (((uint64_t)message & 0xf000000000000000) == 0) {
        debugstr(message);
    }
}