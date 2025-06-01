/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * Now, pay attention, because this is slightly weird...
 *
 * When a new user thread is created, the address of this is pushed to the
 * stack as the place `task_switch` should return to. It does the bare
 * minimum needed to get the thread out of kernel space and into user mode.
 * 
 * The `task_create_new` function sets the stack up such that the address
 * of the actual thread function is in rdi when we enter here and the
 * address of the stack is in rsi, so per the SysV ABI they are the arguments
 * to this function. With them, we can set up an iretq to get us into user 
 * mode...
 */
#include <stdint.h>
#include <stdnoreturn.h>

#include "machine.h"
#include "sched.h"
#include "task.h"

#ifdef DEBUG_TASK_SWITCH
#include "debugprint.h"
#include "printhex.h"
#ifdef VERY_NOISY_TASK_SWITCH
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#define vdbgx32(arg) printhex32(arg, debugchar)
#define vdbgx16(arg) printhex16(arg, debugchar)
#define vdbgx8(arg) printhex8(arg, debugchar)
#else
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx32(...)
#define vdbgx16(...)
#define vdbgx8(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#define tdbgx32(arg) printhex32(arg, debugchar)
#define tdbgx16(arg) printhex16(arg, debugchar)
#define tdbgx8(arg) printhex8(arg, debugchar)
#else
#define tdebug(...)
#define tdbgx64(...)
#define tdbgx32(...)
#define tdbgx16(...)
#define tdbgx8(...)
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx32(...)
#define vdbgx16(...)
#define vdbgx8(...)
#endif

#ifdef __x86_64__
#define INT_FLAG_ENABLED ((0x200))
#elifdef __riscv
// TODO this totally isn't right, we shouldn't just always set this...
#define INT_FLAG_ENABLED ((0x8000000200046020))
#else
#error Need a platform-specific INT_FLAGS_ENABLED in task_user_entrypoint.c
#endif

noreturn void user_thread_entrypoint(uintptr_t thread_entrypoint,
                                     uintptr_t thread_userstack) {
    // Scheduler will **always** be locked when we get here!
    sched_unlock_this_cpu(INT_FLAG_ENABLED);

    tdebug("Starting new user thread with func @ ");
    tdbgx8(thread_entrypoint);
    tdebug("\n");

    // clang-format off
    // Set return address (PC) to user code
    __asm__ volatile("csrw sepc, %0" :: "r"(thread_entrypoint));

    // Modify sstatus:
    //  - clear SPP (bit 8) → return to user mode
    //  - set SPIE (bit 5) → enable interrupts in U-mode after sret
    uint64_t sstatus;
    __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(1UL << 8); // Clear SPP
    //sstatus |=  (1UL << 5); // Set SPIE
    __asm__ volatile("csrw sstatus, %0" :: "r"(sstatus));

    // Set user stack pointer (put into a0 for trampoline use)
    // register uintptr_t a0 asm("a0") = thread_userstack;

    // sret to user mode
    __asm__ volatile("csrw sscratch, sp\n\t"
                     "mv sp, %0\n\t"  // Move user stack into sp
                     "sret\n\t"
                     :
                     : "r"(thread_userstack)
                     : "memory");
    // clang-format on

    halt_and_catch_fire();
}
