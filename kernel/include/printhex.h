/*
 * stage3 - Hex printer for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PRINTHEX_H
#define __ANOS_KERNEL_PRINTHEX_H

#include <stdint.h>

typedef void(PrintHexCharHandler)(char chr);

#if __STDC_HOSTED__ == 1 && !defined(UNIT_TEST_PRINTHEX)
#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#define printhex64(num, ignore) printf("0x%016lx", num)
#define printhex32(num, ignore) printf("0x%08x", num)
#define printhex16(num, ignore) printf("0x%04x", num)
#define printhex8(num, ignore) printf("0x%02x", num)
#pragma GCC diagnostic pop
#else
void printhex64(uint64_t num, PrintHexCharHandler printfunc);
void printhex32(uint64_t num, PrintHexCharHandler printfunc);
void printhex16(uint64_t num, PrintHexCharHandler printfunc);
void printhex8(uint64_t num, PrintHexCharHandler printfunc);
#endif

#endif //__ANOS_KERNEL_PRINTHEX_H