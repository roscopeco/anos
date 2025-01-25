/*
 * libanos sleep syscall wrapper (for convenience)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "anos/anos_syscalls.h"

#define NANOS_PER_SEC ((1000000000))

void anos_task_sleep_current_secs(uint64_t secs) {
    anos_task_sleep_current(secs * NANOS_PER_SEC);
}