/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "syscalls.h"
#include "debugprint.h"
#include "printhex.h"

#include <stdint.h>

static void handle_debugprint(char *message) {
    if (((uint64_t)message & 0xf000000000000000) == 0) {
        debugstr(message);
    }
}

uint64_t handle_syscall_69(uint64_t arg0, uint64_t arg1, uint64_t arg2,
                           uint64_t arg3, uint64_t arg4, uint64_t syscall_num) {
    switch (syscall_num) {
    case 1:
        handle_debugprint((char *)arg0);
        return SYSCALL_OK;
    default:
        return SYSCALL_BAD_NUMBER;
    }
}