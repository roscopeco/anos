/*
 * Kernel driver for SBI
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_KDRIVERS_SBI_H
#define __ANOS_KERNEL_ARCH_RISCV64_KDRIVERS_SBI_H

#include <stdint.h>

#define SBI_SET_TIMER_EID (0x54494D45)
#define SBI_SET_TIMER_FID (0)

#define SBI_SUCCESS (0)                // Completed successfully
#define SBI_ERR_FAILED (-1)            // Failed
#define SBI_ERR_NOT_SUPPORTED (-2)     // Not supported
#define SBI_ERR_INVALID_PARAM (-3)     // Invalid parameter(s)
#define SBI_ERR_DENIED (-4)            // Denied or not allowed
#define SBI_ERR_INVALID_ADDRESS (-5)   // Invalid address(s)
#define SBI_ERR_ALREADY_AVAILABLE (-6) // Already available
#define SBI_ERR_ALREADY_STARTED (-7)   // Already started
#define SBI_ERR_ALREADY_STOPPED (-8)   // Already stopped
#define SBI_ERR_NO_SHMEM (-9)          // Shared memory not available
#define SBI_ERR_INVALID_STATE (-10)    // Invalid state
#define SBI_ERR_BAD_RANGE (-11)        // Bad (or invalid) range
#define SBI_ERR_TIMEOUT (-12)          // Failed due to timeout
#define SBI_ERR_IO (-13)               // Input/Output error
#define SBI_ERR_DENIED_LOCKED (-14) // Denied or not allowed due to lock status

struct sbiret {
    int64_t error;
    union {
        int64_t value;
        uint64_t uvalue;
    };
};

void sbi_ecall_1(uint64_t eid, uint64_t fid, uint64_t arg0);
void sbi_ecall_2(uint64_t eid, uint64_t fid, uint64_t arg0, uint64_t arg1);
void sbi_ecall_3(uint64_t eid, uint64_t fid, uint64_t arg0, uint64_t arg1,
                 uint64_t arg2);
void sbi_ecall_4(uint64_t eid, uint64_t fid, uint64_t arg0, uint64_t arg1,
                 uint64_t arg2, uint64_t arg3);
void sbi_ecall_5(uint64_t eid, uint64_t fid, uint64_t arg0, uint64_t arg1,
                 uint64_t arg2, uint64_t arg3, uint64_t arg4);
void sbi_ecall_6(uint64_t eid, uint64_t fid, uint64_t arg0, uint64_t arg1,
                 uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

void sbi_set_timer(const uint64_t stime_value);

#endif