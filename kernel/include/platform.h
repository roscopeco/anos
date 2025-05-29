/*
 * stage3 - Platform entrypoint interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Once the arch-specific entrypoint has initialized the basic machine,
 * it will call through to here for platform-specific (e.g. ACPI, devicetree)
 * initialization before passing control to the kernel entrypoint proper.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PLATFORM_H
#define __ANOS_KERNEL_PLATFORM_H

#include <stdint.h>

/*
 * This is the platform-specific initialization routine. The passed
 * platform_data value is handled specifically by each different
 * platform (for example, on ACPI it's a pointer to the RSDP).
 *
 * This is called once basic architecture initialisation is completed,
 * and the memory management subsystems are up (so it can use slab / FBA
 * etc).
 */
bool platform_init(uintptr_t platform_data);

bool platform_await_init_complete(void);

bool platform_task_init(void);

#endif