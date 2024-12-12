/*
 * stage3 - GDT manipulation and setup routines
 * anos - An Operating System
 * 
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_GDT_H
#define __ANOS_KERNEL_GDT_H

#include <stdint.h>

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
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) GDTEntry;

// Execute `lgdt` to load a variable with the GDTR
static inline void load_gdtr(GDTR *gdtr) {
    __asm__ __volatile__ ("lgdt (%0)" : : "r" (gdtr));
}

// Execute `sgdt` to load GDTR from a variable
static inline void store_gdtr(GDTR *gdtr) {
    __asm__ __volatile__ ("sgdt (%0)" : : "r" (gdtr));
}

// Function to get a GDT entry given a GDTR and index
GDTEntry *get_gdt_entry(GDTR *gdtr, int index);

// Update values in a GDT entry. Caller should disable interrupts!
void init_gdt_entry(GDTEntry *entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity);

#endif//__ANOS_KERNEL_GDT_H
