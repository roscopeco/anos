/*
 * Kernel driver for SBI
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "riscv64/kdrivers/sbi.h"

void sbi_ecall_1(const uint64_t eid, const uint64_t fid, uint64_t arg0) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall" : "+r"(a0) : "r"(a6), "r"(a7) : "memory");
}

void sbi_ecall_2(const uint64_t eid, const uint64_t fid, uint64_t arg0,
                 uint64_t arg1) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

void sbi_ecall_3(const uint64_t eid, const uint64_t fid, uint64_t arg0,
                 uint64_t arg1, uint64_t arg2) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a2 __asm__("a2") = arg2;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1), "+r"(a2)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

void sbi_ecall_4(const uint64_t eid, const uint64_t fid, uint64_t arg0,
                 uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a2 __asm__("a2") = arg2;
    register uint64_t a3 __asm__("a3") = arg3;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

void sbi_ecall_5(const uint64_t eid, const uint64_t fid, uint64_t arg0,
                 uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a2 __asm__("a2") = arg2;
    register uint64_t a3 __asm__("a3") = arg3;
    register uint64_t a4 __asm__("a4") = arg4;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

void sbi_ecall_6(const uint64_t eid, const uint64_t fid, uint64_t arg0,
                 uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                 uint64_t arg5) {
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a2 __asm__("a2") = arg2;
    register uint64_t a3 __asm__("a3") = arg3;
    register uint64_t a4 __asm__("a4") = arg4;
    register uint64_t a5 __asm__("a5") = arg5;
    register uint64_t a6 __asm__("a6") = fid;
    register uint64_t a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4),
                       "+r"(a5)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

void sbi_set_timer(const uint64_t stime_value) {
    sbi_ecall_1(SBI_SET_TIMER_EID, SBI_SET_TIMER_FID, stime_value);
}