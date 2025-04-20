/*
 * Test interface to mock RISC-V specific VMM implementation for tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_RISCV64_VMM_H
#define __ANOS_TESTS_TEST_RISCV64_VMM_H

#include <stdbool.h>
#include <stdint.h>

// Additional helpers for RISC-V VMM tests
bool mock_vmm_is_page_mapped(uintptr_t virt_addr);
uintptr_t mock_vmm_get_phys_for_virt(uintptr_t virt_addr);
uint16_t mock_vmm_get_flags_for_virt(uintptr_t virt_addr);
size_t mock_vmm_get_mapping_count();

// Mock RISC-V CPU functions
uint64_t cpu_read_satp();
uint64_t cpu_satp_to_root_table_phys(uint64_t satp);
void cpu_invalidate_tlb_addr(uintptr_t addr);
void cpu_invalidate_tlb_all();

#endif //__ANOS_TESTS_TEST_RISCV64_VMM_H