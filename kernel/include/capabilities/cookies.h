/*
 * Capability cookies
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This module implements kernel capability tokens ("cookies") — unique 64-bit
 * identifiers used to securely reference kernel-managed resources like IPC
 * channels, handles, and objects. These tokens are:
 *
 *   - Globally unique across all cores
 *   - Non-zero and unpredictable
 *   - Generated entirely within the kernel
 *
 * Internally, the implementation mixes hardware entropy (if available), time
 * (via TSC or rdcycle), and per-core monotonic counters to ensure uniqueness
 * and randomness. No user input or memory allocation is involved.
 *
 * These tokens are suitable as keys in fast kernel lookup structures, and are
 * never exposed in a way that allows reuse or forgery by userspace.
 * 
 * The actual implementation is architecture-specific - you can find it in
 * the relevant arch/$(ARCH)/process directory.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_CAPABILITIES_COOKIES_H
#define __ANOS_KERNEL_CAPABILITIES_COOKIES_H

#include <stdint.h>

uint64_t capability_cookie_generate(void);

#endif //__ANOS_KERNEL_CAPABILITIES_COOKIES_H