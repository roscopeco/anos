/*
 * Configuration handling for SYSTEM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SYSTEM_CONFIG_H
#define __ANOS_SYSTEM_CONFIG_H

typedef enum {
    PROCESS_CONFIG_OK = 0,
    PROCESS_CONFIG_NOT_FOUND = -1,
    PROCESS_CONFIG_INVALID = -2,
    PROCESS_CONFIG_FAILURE = -3,
} ProcessConfigResult;

ProcessConfigResult process_config(const char *json);

#endif