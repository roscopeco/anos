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

void printhex64(uint64_t num, PrintHexCharHandler printfunc);
void printhex32(uint64_t num, PrintHexCharHandler printfunc);
void printhex16(uint64_t num, PrintHexCharHandler printfunc);
void printhex8(uint64_t num, PrintHexCharHandler printfunc);

#endif //__ANOS_KERNEL_PRINTHEX_H