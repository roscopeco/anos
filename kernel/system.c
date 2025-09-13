/*
 * stage3 - User-mode supervisor start-up
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include "anos_assert.h"
#include "fba/alloc.h"
#include "panic.h"
#include "pmm/pagealloc.h"
#include "sched.h"
#include "syscalls.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support constexpr yet - Jan 2025
#ifndef constexpr
#define constexpr const
#endif
#endif

#define SYSTEM_BSS_PAGE_COUNT 64
#define SYSTEM_KERNEL_STACK_PAGE_COUNT 4
#define SYSTEM_USER_STACK_PAGE_COUNT 4

#define SYSTEM_USER_STACK_BYTES ((SYSTEM_USER_STACK_PAGE_COUNT * VM_PAGE_SIZE))
#define SYSTEM_KERNEL_STACK_BYTES                                              \
    ((SYSTEM_KERNEL_STACK_PAGE_COUNT * VM_PAGE_SIZE))

#ifndef IDLE_STACKS_PER_BLOCK
#define IDLE_STACKS_PER_BLOCK ((4))
#endif

#define IDLE_STACK_BLOCKS (((MAX_CPU_COUNT / IDLE_STACKS_PER_BLOCK)))

#if IDLE_STACK_BLOCKS < 1
#undef IDLE_STACK_BLOCKS
#define IDLE_STACK_BLOCKS 1
#endif

static_assert(IDLE_STACK_BLOCKS > 0,
              "Must allocate at least one block for idle stacks");

#define IDLE_STACK_CPU_OFFSET                                                  \
    (((KERNEL_FBA_BLOCK_SIZE / IDLE_STACKS_PER_BLOCK)))

extern MemoryRegion *physical_region;

extern void *_system_bin_start;
extern size_t _system_bin_size;

// we set this in limine_entrypoint, appropriately for the arch...
extern uintptr_t _system_bin_start_phys;

static void *idle_sstack_page;
static void *idle_ustack_page;

extern volatile bool ap_startup_wait;

void user_thread_entrypoint(void);
void kernel_thread_entrypoint(void);

static void init_idle_stacks(void) {
    // Set up pages for idle stacks ('user' and 'kernel' (really run, and interrupt...))
    //      (MAX_CPU_COUNT stacks of FBA_BLOCK_SIZE / 4 bytes each)
    // currently this means 1KiB stack per CPU, which should be
    // plenty (too much, even) for idle...
    //
    // TODO these could probably come straight from the PMM, no need to waste FBA space on them?
    idle_ustack_page = fba_alloc_blocks(IDLE_STACK_BLOCKS);
    idle_sstack_page = fba_alloc_blocks(IDLE_STACK_BLOCKS);

    if (!(idle_ustack_page && idle_sstack_page)) {
        panic("Failed to allocate idle stacks");
    }
}

void prepare_system(void) { init_idle_stacks(); }

static inline uintptr_t idle_stack_top(uint8_t cpu_id) {
    return IDLE_STACK_CPU_OFFSET * cpu_id + IDLE_STACK_CPU_OFFSET;
}

noreturn void start_system_ap(uint8_t cpu_id) {
#ifdef CONSERVATIVE_BUILD
    // invariant checks...
    if (cpu_id >= MAX_CPU_COUNT) {
        panic("start_system_ap cpu_id beyond MAX_CPU_COUNT");
    }
    if (!idle_sstack_page) {
        panic("start_system_ap called before start_system");
    }
#endif

    // create a process and task for idle
    sched_init_idle((uintptr_t)idle_ustack_page + idle_stack_top(cpu_id),
                    (uintptr_t)idle_sstack_page + idle_stack_top(cpu_id),
                    (uintptr_t)kernel_thread_entrypoint);

    // We can just get away with disabling here, no need to save/restore flags
    // because we know we're currently the only thread on this CPU...
    disable_interrupts();

    // Kick off scheduling...
    sched_schedule();

    // ... which will never return to here.
    __builtin_unreachable();
}

noreturn void start_system(void) {
    const uint64_t system_len_bytes = _system_bin_size;
    const uint64_t system_len_pages =
            (system_len_bytes + VM_PAGE_SIZE - 1) >> VM_PAGE_LINEAR_SHIFT;

    // Map pages for the user code
    for (int i = 0; i < system_len_pages; i++) {
        constexpr uint64_t system_start_virt = 0x1000000;

        vmm_map_page(system_start_virt + (i << 12),
                     _system_bin_start_phys + (i << 12),
                     PG_PRESENT | PG_READ | PG_EXEC | PG_USER);
    }

    extern uintptr_t kernel_zero_page;
    // TODO the way this is set up currently, there's no way to know how much
    //      BSS/Data we need... We'll just map a few pages for now...

    // Set up pages for the user bss / data - we're not allocating
    // here, we're just mapping the zeropage COW...
    constexpr uint64_t user_bss = 0x0000000040000000;
    for (int i = 0; i < SYSTEM_BSS_PAGE_COUNT; i++) {
        vmm_map_page(user_bss + (i * VM_PAGE_SIZE), kernel_zero_page,
                     PG_PRESENT | PG_READ | PG_USER | PG_COPY_ON_WRITE);
    }

    // ... and a few pages below that for the user stack, again, not
    // allocating, just zeropage / COW.
    uint64_t user_stack = user_bss;
    for (int i = 0; i < SYSTEM_USER_STACK_PAGE_COUNT; i++) {
        user_stack -= VM_PAGE_SIZE;
        vmm_map_page(user_stack, kernel_zero_page,
                     PG_PRESENT | PG_READ | PG_USER | PG_COPY_ON_WRITE);
    }

    // grant all syscall capabilities to SYSTEM...
    //
    // TODO this is why the #PF handler has to support "no current process",
    //      because we're not running SYSTEM yet and haven't set up
    //      current_task(). If not for this, we wouldn't need that path...
    //
    uint64_t *user_starting_sp = syscall_init_capabilities((void *)user_bss);

    // ... the FBA can give us a kernel stack... We'll allocate this, it's
    // small, and saves us weirdness when we pagefault during a interrupt/trap
    // mode switch...
    void *kernel_stack =
            fba_alloc_blocks(SYSTEM_KERNEL_STACK_PAGE_COUNT); // 16KiB

    // create a process and task for system
    if (!sched_init((uintptr_t)user_starting_sp,
                    (uintptr_t)kernel_stack + SYSTEM_KERNEL_STACK_BYTES,
                    (uintptr_t)0x0000000001000000,
                    (uintptr_t)user_thread_entrypoint, TASK_CLASS_NORMAL)) {
        panic("Scheduler initialisation failed");
    }

    // TODO check allocations...

    if (!sched_init_idle((uintptr_t)idle_ustack_page + idle_stack_top(0),
                         (uintptr_t)idle_sstack_page + idle_stack_top(0),
                         (uintptr_t)kernel_thread_entrypoint)) {
        panic("Scheduler idle thread initialisation failed");
    }

    // We can just get away with disabling here, no need to save/restore flags
    // because we know we're currently the only thread...
    disable_interrupts();

    // Just one more thing.... release the APs now the main scheduler
    // structs and system process are set up.
    ap_startup_wait = false;

    // Kick off scheduling...
    sched_schedule();

    // ... which will never return to here.
    __builtin_unreachable();
}
