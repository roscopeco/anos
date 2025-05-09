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
 * These tokens are suitable as keys in fast kernel lookup structures, and can
 * be safely handed to userspace processes since they are completely opaque.
 * 
 * To mitigate brute-force attacks, invalid token usage triggers escalating
 * randomised spin delays — forcing bad actors to waste CPU time and making
 * large-scale probing infeasible.
 * 
 * The actual implementation is architecture-specific - you can find it in
 * the relevant arch/$(ARCH)/process directory.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_CAPABILITIES_H
#define __ANOS_KERNEL_CAPABILITIES_H

#include <stdint.h>

#include "anos_assert.h"

typedef enum {
    CAPABILITY_TYPE_INVALID = 0,
    CAPABILITY_TYPE_SYSCALL,

    /* reserved */

    CAPABILITY_TYPE_USER = 255
} __attribute__((packed)) CapabilityType;

/*
 * This should never be directly instantiated, it's designed to act
 * as the header for specific structs representing individual 
 * capability types.
 */
typedef struct {
    CapabilityType type;
    uint8_t subtype;
} Capability;

static_assert_sizeof(Capability, ==, 2);

typedef struct {
    Capability cap;
    uint8_t data[62];
} UserCapability;

static_assert_sizeof(UserCapability, ==, 64);

bool capabilities_init(void);

#endif //__ANOS_KERNEL_CAPABILITIES_H
