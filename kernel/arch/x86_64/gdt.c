/*
 * stage3 - GDT manipulation
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "gdt.h"
#include "kdrivers/cpu.h"

void init_gdt_entry(GDTEntry *entry, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t flags_limit_h) {
    entry->limit_low = (limit & 0xFFFF);
    entry->base_low = (base & 0xFFFF);
    entry->base_middle = (base >> 16) & 0xFF;
    entry->access = access;
    entry->flags_limit_h = (limit >> 16) & 0x0F;
    entry->flags_limit_h |= (flags_limit_h & 0xF0);
    entry->base_high = (base >> 24) & 0xFF;
}

GDTEntry *get_gdt_entry(GDTR *gdtr, int index) {
    int num_entries = (gdtr->limit + 1) / sizeof(GDTEntry);

    if (index < 0 || index >= num_entries) {
        // TODO handle (warn about?) invalid index
        return 0;
    }

    GDTEntry *gdt = (GDTEntry *)(gdtr->base);
    return &gdt[index];
}

inline void *gdt_entry_to_tss(GDTSystemEntry *tss_entry) {
    return (void *)((0ULL | ((uint64_t)tss_entry->base_upper << 32) |
                     ((uint64_t)tss_entry->base_high << 24) |
                     ((uint64_t)tss_entry->base_middle << 16) |
                     (uint64_t)tss_entry->base_low));
}

void *gdt_per_cpu_tss(uint8_t cpu_id) {
    if (cpu_id > MAX_CPU_COUNT) {
        return (void *)0;
    } else {
        GDTR gdtr;
        cpu_store_gdtr(&gdtr);
        GDTEntry *tss_entry =
                get_gdt_entry(&gdtr, 5 + (cpu_id * CPU_TSS_ENTRY_SIZE_MULT));

        return gdt_entry_to_tss((GDTSystemEntry *)tss_entry);
    }
}
