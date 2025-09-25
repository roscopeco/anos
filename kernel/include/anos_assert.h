/*
 * stage3 - Assertion backfill for Anos build
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is mostly to cover building the tests with Apple clang
 * because in 2025 it's not yet supporting the things we need...
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ASSERT_H
#define __ANOS_KERNEL_ASSERT_H

#if (__STDC_VERSION__ < 202000)
#if __STDC_HOSTED__ == 1
#include <assert.h>
#define static_assert _Static_assert
#else
#error Please use a C23-capable compiler to build a kernel!
#endif
#endif

#define static_assert_sizeof(type, op, size)                                                                           \
    static_assert(sizeof(type) op size, #type " is wrongly-sized, may need C23 sized enums...")

#endif //__ANOS_KERNEL_ASSERT_H
