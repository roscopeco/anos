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

noreturn void halt_and_catch_fire(void);

void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);

void restore_interrupts(uint64_t cookie);
uint64_t disable_interrupts();

/*
 * These are used by the new thread entrypoint code.
 *
 * They just returns whatever is currently in r15 (entrypoint)
 * or r14 (user stack) which is where the new thread setup puts 
 * the right values for the new thread...
 */
uintptr_t get_new_thread_entrypoint();
uintptr_t get_new_thread_userstack();

#endif //__ANOS_KERNEL_MACHINE_H