#include <stdint.h>

#include "gdt.h"

// Function to create a GDT entry
void init_gdt_entry(GDTEntry *entry, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t granularity) {
    entry->limit_low = (limit & 0xFFFF);
    entry->base_low = (base & 0xFFFF);
    entry->base_middle = (base >> 16) & 0xFF;
    entry->access = access;
    entry->granularity = (limit >> 16) & 0x0F;
    entry->granularity |= (granularity & 0xF0);
    entry->base_high = (base >> 24) & 0xFF;
}

static GDTEntry empty_entry = {0};

// Function to get a GDT entry given a GDTR and index
GDTEntry *get_gdt_entry(GDTR *gdtr, int index) {
    int num_entries = (gdtr->limit + 1) / sizeof(GDTEntry);

    if (index < 0 || index >= num_entries) {
        // TODO handle (warn about?) invalid index
        return &empty_entry;
    }

    GDTEntry *gdt = (GDTEntry *)(gdtr->base);
    return &gdt[index];
}
