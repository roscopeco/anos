/*
 * SYSTEM process management routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SYSTEM_PROCESS_H
#define __ANOS_SYSTEM_PROCESS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t capability_id;
    uint64_t capability_cookie;
} InitCapability;

typedef struct {
    uint64_t value_count;
    uintptr_t *data;
    size_t allocated_size;
    uintptr_t *argv_buffer;  // Buffer to be freed after use
    uintptr_t *stack_buffer; // Buffer to be freed after use
} InitStackValues;

/*
 * Create a new process with the given stack size, capabilities and arguments.
 *
 * The (full) path to the executable must be argv[0]!
 *
 * Returns negative on failure.
 */
int64_t create_server_process(uint64_t stack_size, uint16_t capc,
                              const InitCapability *capv, uint16_t argc,
                              const char *argv[]);

#endif //__ANOS_SYSTEM_PROCESS_H
