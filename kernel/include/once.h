/*
 * stage3 - Single-call enforcement for conservative builds
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ONCE_H
#define __ANOS_KERNEL_ONCE_H

#ifdef CONSERVATIVE_BUILD
#ifndef UNIT_TESTS
#include "panic.h"

#define kernel_guard_once()                                                                                            \
    do {                                                                                                               \
        static int __kernel_once_guard;                                                                                \
        if (__kernel_once_guard++ > 0) {                                                                               \
            panic_sloc("Multiple calls to once func", __FILE__, __LINE__);                                             \
        }                                                                                                              \
    } while (0)
#else
#define kernel_guard_once()
#endif
#else
#define kernel_guard_once()
#endif

#endif //__ANOS_KERNEL_ONCE