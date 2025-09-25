/*
 * stage3 - TLB shootdown wrapper for VMM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This is essentially a thin wrapper over the vmm_map/unmap_page
 * API that performs TLB shootdowns.
 *
 * Shootdowns are **expensive**! Only use these routines when
 * it's actually necessary, use the lower-level functions
 * directly if at all possible.
 */

#ifndef __ANOS_KERNEL_VM_SHOOTDOWN_H
#define __ANOS_KERNEL_VM_SHOOTDOWN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "process.h"

bool vmm_shootdown_map_page_containing_in_process(const Process *process, uintptr_t virt_addr, uintptr_t phys_addr,
                                                  uintptr_t flags);

bool vmm_shootdown_map_page_containing_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, uintptr_t phys_addr,
                                               uintptr_t flags);

bool vmm_shootdown_map_pages_containing_in_process(const Process *process, uintptr_t virt_addr, uintptr_t phys_addr,
                                                   uintptr_t flags, size_t num_pages);

bool vmm_shootdown_map_pages_containing_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, uintptr_t phys_addr,
                                                uintptr_t flags, size_t num_pages);

bool vmm_shootdown_map_page_containing(uintptr_t virt_addr, uintptr_t phys_addr, uintptr_t flags);

bool vmm_shootdown_map_page_in_process(const Process *process, uintptr_t virt_addr, uintptr_t page, uintptr_t flags);

bool vmm_shootdown_map_page_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, uintptr_t page, uintptr_t flags);

bool vmm_shootdown_map_page(uintptr_t virt_addr, uintptr_t page, uintptr_t flags);

bool vmm_shootdown_map_pages_containing(uintptr_t virt_addr, uintptr_t phys_addr, uintptr_t flags, size_t num_pages);

bool vmm_shootdown_map_pages_in_process(const Process *process, uintptr_t virt_addr, uintptr_t page, uintptr_t flags,
                                        size_t num_pages);

bool vmm_shootdown_map_pages_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, uintptr_t page, uintptr_t flags,
                                     size_t num_pages);

bool vmm_shootdown_map_pages(uintptr_t virt_addr, uintptr_t page, uintptr_t flags, size_t num_pages);

uintptr_t vmm_shootdown_unmap_page_in_process(const Process *process, uintptr_t virt_addr);

uintptr_t vmm_shootdown_unmap_page_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr);

uintptr_t vmm_shootdown_unmap_page(uintptr_t virt_addr);

uintptr_t vmm_shootdown_unmap_pages_in_process(const Process *process, uintptr_t virt_addr, size_t num_pages);

uintptr_t vmm_shootdown_unmap_pages_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, size_t num_pages);

uintptr_t vmm_shootdown_unmap_pages(uintptr_t virt_addr, size_t num_pages);

#endif //__ANOS_KERNEL_VM_SHOOTDOWN_H
