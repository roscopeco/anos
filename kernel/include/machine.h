/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#ifndef __ANOS_KERNEL_MACHINE_H
#define __ANOS_KERNEL_MACHINE_H

#include <stdint.h>
#include <stdnoreturn.h>

typedef enum {
  MEM_MAP_ENTRY_INVALID = 0,
  MEM_MAP_ENTRY_AVAILABLE = 1,
  MEM_MAP_ENTRY_RESERVED = 2,
  MEM_MAP_ENTRY_ACPI = 3,
  MEM_MAP_ENTRY_ACPI_NVS = 4,
  MEM_MAP_ENTRY_UNUSABLE = 5,
  MEM_MAP_ENTRY_DISABLED = 6,
  MEM_MAP_ENTRY_PERSISTENT = 7,
  MEM_MAP_ENTRY_UNKNOWN = 8,
} E820h_MemMapEntry_Type;

/*
 * E820h BIOS memory map entry.
 */
typedef struct {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t attrs;
} __attribute__((packed)) E820h_MemMapEntry;

/*
 * E820h BIOS memory map.
 */
typedef struct {
  uint16_t num_entries;
  E820h_MemMapEntry entries[];
} __attribute__((packed)) E820h_MemMap;

noreturn void halt_and_catch_fire();

#endif //__ANOS_KERNEL_MACHINE_H