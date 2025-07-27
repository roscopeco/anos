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

#ifdef ARCH_X86_64
#include "platform/acpi/acpitables.h"
#endif

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

void platform_cleanup_process(uint64_t pid);

#ifdef ARCH_X86_64
/*
 * Get the RSDP pointer that was saved during platform initialization.
 * This provides direct access to the firmware table root pointer.
 */
ACPI_RSDP *platform_get_root_firmware_table(void);

/*
 * Get the ACPI root table (RSDT/XSDT) that was initialized during platform init.
 * This provides access to the table that contains pointers to all other ACPI tables.
 */
ACPI_RSDT *platform_get_acpi_root_table(void);
#endif

#endif