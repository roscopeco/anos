/*
 * stage3 - GDT manipulation and setup routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_GDT_H
#define __ANOS_KERNEL_ARCH_X86_64_GDT_H

#include <stdint.h>

#include "anos_assert.h"

#define GDT_ENTRY_ACCESS_ACCESSED 0x01
#define GDT_ENTRY_ACCESS_READ_WRITE 0x02
#define GDT_ENTRY_ACCESS_DOWN_CONFORMING 0x04
#define GDT_ENTRY_ACCESS_EXECUTABLE 0x08
#define GDT_ENTRY_ACCESS_NON_SYSTEM 0x10
#define GDT_ENTRY_ACCESS_DPL_MASK 0x60
#define GDT_ENTRY_ACCESS_PRESENT 0x80

#define GDT_ENTRY_ACCESS_DPL(dpl) (((dpl & 0x03) << 5))

#define GDT_ENTRY_ACCESS_RING0 0x00
#define GDT_ENTRY_ACCESS_RING1 0x20
#define GDT_ENTRY_ACCESS_RING2 0x40
#define GDT_ENTRY_ACCESS_RING3 0x60

#define GDT_ENTRY_FLAGS_LONG_MODE 0x20
#define GDT_ENTRY_FLAGS_SIZE 0x40
#define GDT_ENTRY_FLAGS_GRANULARITY 0x80

#define GDT_ENTRY_FLAGS_64BIT ((GDT_ENTRY_FLAGS_LONG_MODE))

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GDTR;

// Structure representing a GDT entry
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags_limit_h;
    uint8_t base_high;
} __attribute__((packed)) GDTEntry;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags_limit_h;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) GDTSystemEntry;

static_assert_sizeof(GDTR, ==, 10);
static_assert_sizeof(GDTEntry, ==, 8);
static_assert_sizeof(GDTSystemEntry, ==, 16);

// Function to get a GDT entry given a GDTR and index
GDTEntry *get_gdt_entry(const GDTR *gdtr, int index);

// Update values in a GDT entry. Caller should disable interrupts!
void init_gdt_entry(GDTEntry *entry, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t flags_limit_h);

// Get a TSS pointer from a TSS GDT entry
void *gdt_entry_to_tss(const GDTSystemEntry *tss_entry);

// Get a per-CPU TSS pointer
void *gdt_per_cpu_tss(uint8_t cpu_id);

#endif //__ANOS_KERNEL_ARCH_X86_64_GDT_H
