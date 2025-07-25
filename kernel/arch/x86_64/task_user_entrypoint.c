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

#define INT_FLAG_ENABLED ((0x200))

noreturn void user_thread_entrypoint(uintptr_t thread_entrypoint,
                                     uintptr_t thread_userstack) {
    // Scheduler will **always** be locked when we get here!
    sched_unlock_this_cpu(INT_FLAG_ENABLED);

    tdebug("Starting new user thread with func @ ");
    tdbgx64(thread_entrypoint);
    tdebug("\n");

    // clang-format off
    // Switch to user mode
    __asm__ volatile(
            "push $0x1B\n\t"    // Push user data segment selector (GDT entry 3)
            "push %0\n\t"       // Push user stack pointer
            "pushfq\n\t"        // Push RFLAGS
            "push $0x23\n\t"    // Push user code segment selector (GDT entry 4)
            "push %1\n\t"       // Push user code entry point
#ifndef NO_USER_GS
            "swapgs\n\t"        // Swap to user-mode GS
#endif
            "iretq\n\t"         // "Return" to user mode
            :
            : "r"(thread_userstack), "r"(thread_entrypoint)
            : "memory");
    // clang-format on

    __builtin_unreachable();
}
