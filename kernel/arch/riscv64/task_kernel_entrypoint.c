/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * Now, pay attention, because (like the equivalent user entrypoint) 
 * this is slightly weird...
 *
 * When a new kernel thread is created, the address of this is pushed to the
 * stack as the place `task_switch` should return to. It does the bare
 * minimum needed to get the thread running in a way that makes sense 
 * for all the surrounding code.
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
#define INT_FLAG_ENABLED ((0x8000000200046022))
#else
#error Need a platform-specific INT_FLAGS_ENABLED in task_kernel_entrypoint.c
#endif

noreturn void kernel_thread_entrypoint(uintptr_t thread_entrypoint,
                                       uintptr_t thread_stack) {
    // Scheduler will **always** be locked when we get here!
    sched_unlock_this_cpu(INT_FLAG_ENABLED);

    tdebug("Starting new kernel thread with func @ ");
    tdbgx64(thread_entrypoint);
    tdebug("\n");

    __asm__ volatile(
            "mv sp, %0\n\t" // Set the stack pointer
            "mv ra, zero\n\t" // Clear return address: if entrypoint returns, we trap
            "jr %1\n\t"       // Jump to thread entrypoint
            :
            : "r"(thread_stack), "r"(thread_entrypoint)
            : "memory");

    halt_and_catch_fire();
}
