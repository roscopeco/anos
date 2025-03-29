/*
 * Anos path helper routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>
#include <stddef.h>

// TODO get rid of this once we have newlib
static char *strnchr(const char *s, int c, size_t max_len) {
    for (size_t i = 0; i < max_len; ++i) {
        if (s[i] == (char)c) {
            return (char *)&s[i];
        }
        if (s[i] == '\0') {
            break;
        }
    }
    return NULL;
}

bool parse_file_path(char *input, size_t input_len, char **mount_point,
                     char **path) {
    if (!input || !mount_point || !path || input_len == 0) {
        return false;
    }

    // Confirm NUL terminator exists within the input buffer
    bool has_nul = false;
    for (size_t i = 0; i < input_len; ++i) {
        if (input[i] == '\0') {
            has_nul = true;
            break;
        }
    }
    if (!has_nul) {
        return false;
    }

    // Find first colon
    char *colon = strnchr(input, ':', input_len);
    if (!colon || colon == input) {
        return false;
    }

    // Check that there's something after the colon *and* it's within the valid range
    if (*(colon + 1) == '\0') {
        return false;
    }

    *colon = '\0';
    *mount_point = input;
    *path = colon + 1;
    return true;
}
