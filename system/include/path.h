/*
 * Anos path helper routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_SYSTEM_PATH_H
#define __ANOS_SYSTEM_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool parse_file_path(char *input, size_t input_len, char **mount_point, char **path);

#endif //__ANOS_SYSTEM_PATH_H
