/*
 * RISC-V Assembly Functions
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_KDRIVERS_CPU_H
#define __ANOS_KERNEL_ARCH_RISCV64_KDRIVERS_CPU_H

#include <stdint.h>

#include "vmm/vmconfig.h"

// CSR (Control and Status Register) numbers
#define CSR_SATP 0x180 // Supervisor Address Translation and Protection
#define CSR_SIE 0x104  // Supervisor Interrupt Enable
#define CSR_SIP 0x144  // Supervisor Interrupt Pending

// SATP modes
#define SATP_MODE_BARE 0  // No translation or protection
#define SATP_MODE_SV39 8  // Sv39: Page-based 39-bit virtual addressing
#define SATP_MODE_SV48 9  // Sv48: Page-based 48-bit virtual addressing
#define SATP_MODE_SV57 10 // Sv57: Page-based 57-bit virtual addressing
#define SATP_MODE_SV64 11 // Sv64: Page-based 64-bit virtual addressing

#define SATP_MODE_BARE_LEVELS 0
#define SATP_MODE_SV39_LEVELS 3
#define SATP_MODE_SV48_LEVELS 4
#define SATP_MODE_SV57_LEVELS 5
#define SATP_MODE_SV64_LEVELS 6

#define SATP_MODE_MAX_LEVELS SATP_MODE_SV64_LEVELS

// Function to read a CSR
static inline uint64_t cpu_read_csr(int csr) {
    uint64_t val;
    __asm__ volatile("csrr %0, %1" : "=r"(val) : "i"(csr));
    return val;
}

// Function to write a CSR
static inline void cpu_write_csr(int csr, uint64_t val) {
    __asm__ volatile("csrw %0, %1" ::"i"(csr), "r"(val));
}

// Function to set bits in a CSR
static inline void cpu_set_csr(int csr, uint64_t mask) {
    __asm__ volatile("csrrs x0, %0, %1" ::"i"(csr), "r"(mask));
}

// Function to clear bits in a CSR
static inline void cpu_clear_csr(int csr, uint64_t mask) {
    __asm__ volatile("csrrc x0, %0, %1" ::"i"(csr), "r"(mask));
}

// Function to read and write a CSR
static inline uint64_t cpu_swap_csr(int csr, uint64_t new_val) {
    uint64_t old_val;
    __asm__ volatile("csrrw %0, %1, %2"
                     : "=r"(old_val)
                     : "i"(csr), "r"(new_val));
    return old_val;
}

// Function to get the current SATP value
static inline uint64_t cpu_read_satp(void) { return cpu_read_csr(CSR_SATP); }

// Function to set the SATP register (page table base and mode)
static inline void cpu_set_satp(uintptr_t pt_base, uint8_t mode) {
    uint64_t satp = ((uint64_t)mode << 60) | (pt_base >> 12);
    cpu_write_csr(CSR_SATP, satp);

    // Flush the TLB
    __asm__ volatile("sfence.vma");
}

// Function to invalidate the TLB for a specific address
static inline void cpu_invalidate_tlb_addr(uintptr_t addr) {
    __asm__ volatile("sfence.vma %0" ::"r"(addr));
}

// Function to invalidate the entire TLB
static inline void cpu_invalidate_tlb_all(void) {
    __asm__ volatile("sfence.vma");
}

static inline uint8_t cpu_satp_mode(uint64_t satp) {
    return ((satp & 0xF000000000000000) >> 60);
}

static inline uintptr_t cpu_satp_to_root_table_phys(uint64_t satp) {
    return ((satp & 0xFFFFFFFFFFF) << VM_PAGE_LINEAR_SHIFT);
}

#endif // __ANOS_KERNEL_ARCH_RISCV64_KDRIVERS_CPU_H