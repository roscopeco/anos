/*
 * libanos _putchar support routine (for printf)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "anos/anos_syscalls.h"

#define buf_len 0x400

static char buffer[buf_len];
static int ptr;

static void flush() {
    // flush
    buffer[ptr] = 0;
    kprint(buffer);
    ptr = 0;
}

// TODO this is a bit silly, but only temporary until
// proper streams are sorted out.
//
// Obvs not thread safe...
void _putchar(char character) {
    buffer[ptr++] = character;

    if ((ptr == buf_len - 1) || (character == '\n')) {
        flush();
    }
}