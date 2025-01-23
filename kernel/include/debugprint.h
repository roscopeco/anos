/*
 * stage3 - Debug printing for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DEBUGPRINT_H
#define __ANOS_KERNEL_DEBUGPRINT_H

#include <stdint.h>

void debugterm_init(char *vram_addr);

#if __STDC_HOSTED__ == 1 && !defined(UNIT_TEST_DEBUGPRINT)
#include <stdio.h>
#define debugstr(str) printf("%s", str)
#define debugchar(chr) printf("%c", chr)
#define debugattr(...)
#else
void debugchar(char chr);
void debugstr(char *str);
void debugstr_len(char *str, int len);
void debugattr(uint8_t new_attr);
#endif

#endif //__ANOS_KERNEL_DEBUGPRINT_H
