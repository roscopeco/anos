/*
 * stage3 - Decimal printer for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PRINTDEC_H
#define __ANOS_KERNEL_PRINTDEC_H

#include <stdint.h>

typedef void(PrintDecCharHandler)(char chr);

void printdec(int64_t num, PrintDecCharHandler printfunc);

#endif //__ANOS_KERNEL_PRINTDEC_H
