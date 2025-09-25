/*
* Userspace Graphical debug terminal
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_USER_GDEBUGTERM_H
#define __ANOS_USER_GDEBUGTERM_H

#include <stdbool.h>
#include <stdint.h>

bool debugterm_init(void volatile *_fb, uint16_t phys_width, uint16_t phys_height);

void debugterm_clear(void);

void debugterm_putchar(char chr);
void debugterm_putstr(const char *str);

void debugterm_attr(uint8_t new_attr);

void debugterm_write(void *buffer, size_t size);

uint16_t debugterm_row_count(void);
uint16_t debugterm_col_count(void);

#endif