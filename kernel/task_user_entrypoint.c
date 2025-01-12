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
 * minimum needed to get the new thread out of kernel space and back
 * into user mode.
 * 
 * The `task_create_new` function sets the stack up such that the address
 * of the actual thread function is in r15 when we enter here, so we need
 * a little bit of assembly to grab that, and then we can set up an iretq
 * to get us back into user mode...
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
#else
#define vdebug(...)
#define vdbgx64(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#else
#define tdebug(...)
#define tdbgx64(...)
#define vdebug(...)
#define vdbgx64(...)
#endif

noreturn void user_thread_entrypoint(void) {
    // _hopefully_ nothing will have trounced r15 yet :D
    uintptr_t thread_entrypoint = get_new_thread_entrypoint();
    uintptr_t thread_userstack = get_new_thread_userstack();

    sched_unlock();

    tdebug("Starting new user thread with func @ ");
    tdbgx64(thread_entrypoint);
    tdebug("\n");

    // Task *current = task_current();

    // Switch to user mode
    __asm__ volatile(
            "mov %0, %%rsp\n\t" // Set stack pointer
            "push $0x1B\n\t"    // Push user data segment selector (GDT entry 3)
            "push %0\n\t"       // Push user stack pointer
            "pushf\n\t"         // Push EFLAGS
            "push $0x23\n\t"    // Push user code segment selector (GDT entry 4)
            "push %1\n\t"       // Push user code entry point
            "iretq\n\t"         // "Return" to user mode
            :
            : "r"(thread_userstack), "r"(thread_entrypoint)
            : "memory");

    __builtin_unreachable();
}
