/*
 * Abuse Throttling Utilities
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Provides spin-based delay mechanisms for deterring brute-force
 * or abusive access patterns (e.g. invalid capability guesses) without
 * engaging the scheduler or introducing sleep-based side effects.
 *
 * Can be used in any path where we want to punish repeat offenders
 * without impacting legitimate users.
 * 
 * Also introduces jitter to mitigate potential timing attacks.
 * 
 * Implementation is architecture-specific
 */

// clang-format Language: C

#ifndef __ANOS_THROTTLE_H
#define __ANOS_THROTTLE_H

#ifdef ARCH_X86_64
#include "x86_64/throttle.h"
#elifdef ARCH_RISCV64
#include "riscv64/throttle.h"
#else
#error Need an architecture-specific throttle implementation
#endif

#endif
