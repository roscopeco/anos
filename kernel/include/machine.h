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

typedef enum {
    LIMINE_MEMMAP_USABLE = 0,
    LIMINE_MEMMAP_RESERVED = 1,
    LIMINE_MEMMAP_ACPI_RECLAIMABLE = 2,
    LIMINE_MEMMAP_ACPI_NVS = 3,
    LIMINE_MEMMAP_BAD_MEMORY = 4,
    LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE = 5,
    LIMINE_MEMMAP_EXECUTABLE_AND_MODULES = 6,
    LIMINE_MEMMAP_FRAMEBUFFER = 7,
} Limine_MemMapEntry_Type;

/*
 * Limine Bootloader memory map entry.
 */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint64_t type;
} __attribute__((packed)) Limine_MemMapEntry;

/*
 * Limine Bootloader memory map.
 */
typedef struct {
    uint64_t revision;
    uint64_t entry_count;
    Limine_MemMapEntry **entries;
} __attribute__((packed)) Limine_MemMap;

noreturn void halt_and_catch_fire(void);

void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);

void disable_interrupts();
void enable_interrupts();
uint64_t save_disable_interrupts();
void restore_saved_interrupts(uint64_t flags);

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