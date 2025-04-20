/*
 * RISC-V Supervisor Binary Interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_SBI_H
#define __ANOS_KERNEL_ARCH_RISCV64_SBI_H

#include <stdint.h>

#define SBI_EXT_BASE 0x10

#define SBI_EXT_BASE_GET_SPEC_VERSION 0x0
#define SBI_EXT_BASE_GET_IMP_ID 0x1
#define SBI_EXT_BASE_GET_IMP_VERSION 0x2
#define SBI_EXT_BASE_GET_MVENDORID 0x4

typedef struct {
    uint64_t error;
    uint64_t value;
} SbiResult;

SbiResult sbi_ecall(int ext, int fid, unsigned long arg0, unsigned long arg1,
                    unsigned long arg2, unsigned long arg3, unsigned long arg4,
                    unsigned long arg5);

static long __sbi_base_ecall(int fid) {
    SbiResult result;

    result = sbi_ecall(SBI_EXT_BASE, fid, 0, 0, 0, 0, 0, 0);

    if (!result.error) {
        return result.value;
    } else {
        return -result.error;
    }
}

static inline long sbi_get_spec_version(void) {
    return __sbi_base_ecall(SBI_EXT_BASE_GET_SPEC_VERSION);
}

static inline long sbi_get_firmware_id(void) {
    return __sbi_base_ecall(SBI_EXT_BASE_GET_IMP_ID);
}

static inline long sbi_get_firmware_version(void) {
    return __sbi_base_ecall(SBI_EXT_BASE_GET_IMP_VERSION);
}

static inline long sbi_get_mvendor_id(void) {
    return __sbi_base_ecall(SBI_EXT_BASE_GET_MVENDORID);
}

#ifdef DEBUG_SBI
void sbi_debug_info(void);
#else
#define sbi_debug_info(...)
#endif

#endif //__ANOS_KERNEL_ARCH_RISCV64_SBI_H