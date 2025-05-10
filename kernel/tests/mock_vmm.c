/*
 * Mock implementation of the vmm for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "mock_recursive.h"
#include "vmm/vmmapper.h"

static uint32_t total_page_maps = 0;
static uint32_t total_page_unmaps = 0;

static uint64_t last_page_map_paddr = 0;
static uint64_t last_page_map_vaddr = 0;
static uint16_t last_page_map_flags = 0;
static uint64_t last_page_map_pml4 = 0;

static uintptr_t last_page_unmap_pml4 = 0;
static uintptr_t last_page_unmap_virt = 0;

void mock_vmm_reset() {
    total_page_maps = 0;
    total_page_unmaps = 0;
}

uint64_t mock_vmm_get_last_page_map_paddr() { return last_page_map_paddr; }

uint64_t mock_vmm_get_last_page_map_vaddr() { return last_page_map_vaddr; }

uint16_t mock_vmm_get_last_page_map_flags() { return last_page_map_flags; }

uint64_t mock_vmm_get_last_page_map_pml4() { return last_page_map_pml4; }

uint32_t mock_vmm_get_total_page_maps() { return total_page_maps; }

uint32_t mock_vmm_get_total_page_unmaps() { return total_page_unmaps; }

uintptr_t mock_vmm_get_last_page_unmap_pml4() { return last_page_unmap_pml4; }

uintptr_t mock_vmm_get_last_page_unmap_virt() { return last_page_unmap_virt; }

bool vmm_map_page_in(uint64_t *pml4, const uintptr_t virt_addr,
                     const uint64_t page, const uint16_t flags) {
    last_page_map_paddr = page;
    last_page_map_vaddr = (uint64_t)virt_addr;
    last_page_map_flags = flags;
    last_page_map_pml4 = (uint64_t)pml4;

    total_page_maps++;

    return true;
}

bool vmm_map_page(const uintptr_t virt_addr, const uint64_t page,
                  const uint16_t flags) {
    return vmm_map_page_in((uint64_t *)vmm_find_pml4(), virt_addr, page, flags);
}

uintptr_t vmm_unmap_page_in(uint64_t *pml4, const uintptr_t virt_addr) {
    last_page_unmap_pml4 = (uintptr_t)pml4;
    last_page_unmap_virt = virt_addr;

    total_page_unmaps++;

    return last_page_map_paddr;
}

bool vmm_map_page_containing(const uintptr_t virt_addr,
                             const uint64_t phys_addr, const uint16_t flags) {
    return vmm_map_page(virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

uintptr_t vmm_unmap_page(const uintptr_t virt_addr) {
    return vmm_unmap_page_in((uint64_t *)vmm_find_pml4(), virt_addr);
}