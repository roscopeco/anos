/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 * 
 * Copyright (c) 2024 Ross Bamford
 * 
 * NOTE: The layout of calls defined herein is currently only 
 * for test/development. They are poorly designed and **won't**
 * be sticking around in this layout. Some of the calls here
 * are only for debugging and will also be removed!
 * 
 * TODO these **badly** need their return values sorting out, in some
 * error cases callers have no way to know what they're going to get
 * (even in terms of signedness!)
 */

#include <stdint.h>

#include "capabilities/cookies.h"
#include "capabilities/map.h"
#include "debugprint.h"
#include "ipc/channel.h"
#include "ipc/named.h"
#include "kprintf.h"
#include "pmm/pagealloc.h"
#include "process.h"
#include "process/address_space.h"
#include "process/memory.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/state.h"
#include "std/string.h"
#include "structs/region_tree.h"
#include "syscalls.h"

#include "platform.h"
#include "printhex.h"
#include "task.h"
#include "throttle.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_ACPI
#define acpi_debugf(...) kprintf(__VA_ARGS__)
#ifdef VERY_NOISY_ACPI
#define acpi_vdebugf(...) kprintf(__VA_ARGS__)
#else
#define acpi_vdebugf(...)
#endif
#else
#define acpi_debugf(...)
#define acpi_vdebugf(...)
#endif

#ifdef ARCH_X86_64
#include "platform/acpi/acpitables.h"

// ACPI utility functions for firmware table handover
static inline bool has_sig(const char *expect, ACPI_SDTHeader *sdt) {
    for (int i = 0; i < 4; i++) {
        if (expect[i] != sdt->signature[i]) {
            return false;
        }
    }
    return true;
}

static inline uint32_t RSDT_ENTRY_COUNT(ACPI_RSDT *sdt) {
    return ((sdt->header.length - sizeof(ACPI_SDTHeader)) / 4);
}

static inline uint32_t XSDT_ENTRY_COUNT(ACPI_RSDT *sdt) {
    return ((sdt->header.length - sizeof(ACPI_SDTHeader)) / 8);
}
#endif

#ifdef DEBUG_PROCESS_SYSCALLS
#include "printhex.h"
#endif

#ifdef CONSERVATIVE_BUILD
#include "panic.h"
#endif

#define MIN_PROCESS_STACK_HEADROOM ((1024))
#define STACK_VALUE_SIZE ((sizeof(uintptr_t)))

extern CapabilityMap global_capability_map;

typedef void (*ThreadFunc)(void);

extern MemoryRegion *physical_region;
extern uintptr_t kernel_zero_page;

#define SYSCALL_ARGS                                                           \
    SyscallArg arg0, SyscallArg arg1, SyscallArg arg2, SyscallArg arg3,        \
            SyscallArg arg4
#define SYSCALL_NAME(name) handle_##name
#define SYSCALL_HANDLER(name)                                                  \
    static SyscallResult SYSCALL_NAME(name)(SYSCALL_ARGS)

SYSCALL_HANDLER(debugprint) {
    char *message = (char *)arg0;

    if (IS_USER_ADDRESS(message)) {
        kprintf("%s", message);
    }

    return SYSCALL_OK;
}

SYSCALL_HANDLER(debugchar) {
    const char chr = (char)arg0;

    debugchar(chr);

    return SYSCALL_OK;
}

SYSCALL_HANDLER(create_thread) {
    const ThreadFunc func = (ThreadFunc)arg0;
    const uintptr_t user_stack = (uintptr_t)arg1;

    Task *task = task_create_user(task_current()->owner, user_stack, 0,
                                  (uintptr_t)func, TASK_CLASS_NORMAL);

#ifdef SYSCALL_SCHED_ONLY_THIS_CPU
    sched_lock_this_cpu();
    sched_unblock(task);
    sched_unlock_this_cpu();
#else
    PerCPUState *target_cpu = sched_find_target_cpu();
    const uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return task->sched->tid;
}

SYSCALL_HANDLER(memstats) {
    AnosMemInfo *mem_info = (AnosMemInfo *)arg0;

    if (IS_USER_ADDRESS(mem_info)) {
        mem_info->physical_total = physical_region->size;
        mem_info->physical_avail = physical_region->free;
    }

    return SYSCALL_OK;
}

SYSCALL_HANDLER(sleep) {
    const uint64_t nanos = (uint64_t)arg0;

    const uint64_t lock_flags = sched_lock_this_cpu();
    sleep_task(task_current(), nanos);
    sched_unlock_this_cpu(lock_flags);

    return SYSCALL_OK;
}

SYSCALL_HANDLER(create_process) {
    ProcessCreateParams *process_create_params = (ProcessCreateParams *)arg0;

    // Validate process create is in userspace
    if (!IS_USER_ADDRESS(process_create_params) ||
        !(IS_USER_ADDRESS(process_create_params + 1))) {
        return SYSCALL_BADARGS;
    }

    // Validate stack arguments
    if (process_create_params->stack_base >= VM_KERNEL_SPACE_START ||
        (process_create_params->stack_base +
         process_create_params->stack_size) >= VM_KERNEL_SPACE_START) {
        return SYSCALL_BADARGS;
    }

    // Ensure number of requested stack values is valid
    if (process_create_params->stack_value_count > MAX_STACK_VALUE_COUNT) {
        return SYSCALL_BADARGS;
    }

    // Ensure enough space on stack for the requested values + minimum headroom
    if ((process_create_params->stack_value_count * STACK_VALUE_SIZE) >
        (process_create_params->stack_size - MIN_PROCESS_STACK_HEADROOM)) {
        return (int64_t)SYSCALL_BADARGS;
    }

    // Validate regions arguments
    if (process_create_params->region_count > MAX_PROCESS_REGIONS) {
        return SYSCALL_BADARGS;
    }

    AddressSpaceRegion ad_regions[process_create_params->region_count];

    for (int i = 0; i < process_create_params->region_count; i++) {
        ProcessMemoryRegion *src_ptr = &process_create_params->regions[i];
        AddressSpaceRegion *dst_ptr = &ad_regions[i];

        if (((uintptr_t)src_ptr) > VM_KERNEL_SPACE_START) {
            return SYSCALL_BADARGS;
        }

        if (((uintptr_t)src_ptr) + sizeof(AddressSpaceRegion) >
            VM_KERNEL_SPACE_START) {
            return SYSCALL_BADARGS;
        }

        // Region start and length must be page aligned
        if (src_ptr->start & 0xfff) {
            return SYSCALL_BADARGS;
        }

        if (src_ptr->len_bytes & 0xfff) {
            return SYSCALL_BADARGS;
        }

        dst_ptr->start = src_ptr->start;
        dst_ptr->len_bytes = src_ptr->len_bytes;
    }

    uintptr_t new_pml4 = address_space_create(
            process_create_params->stack_base,
            process_create_params->stack_size,
            process_create_params->region_count, ad_regions,
            process_create_params->stack_value_count,
            process_create_params->stack_values);

    if (!new_pml4) {
        debugstr("Failed to create address space\n");
        return SYSCALL_FAILURE;
    }

    Process *new_process = process_create(new_pml4);

    if (!new_process) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new process\n");
        return SYSCALL_FAILURE;
    }

    Task *new_task =
            task_create_user(new_process,
                             process_create_params->stack_base +
                                     process_create_params->stack_size -
                                     (process_create_params->stack_value_count *
                                      sizeof(uintptr_t)),
                             0, (uintptr_t)process_create_params->entry_point,
                             TASK_CLASS_NORMAL);

    if (!new_task) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new task\n");
        process_destroy(new_process);
        return SYSCALL_FAILURE;
    }

#ifdef DEBUG_PROCESS_SYSCALLS
    debugstr("Created new process ");
    printhex8(new_process->pid, debugchar);
    debugstr(" @ ");
    printhex64((uintptr_t)new_process, debugchar);
    debugstr(" with PML4 @ ");
    printhex64(new_pml4, debugchar);
    debugstr("\n");
#endif

#ifdef SYSCALL_SCHED_ONLY_THIS_CPU
    sched_lock_this_cpu();
    sched_unblock(task);
    sched_unlock_this_cpu();
#else
    PerCPUState *target_cpu = sched_find_target_cpu();
    const uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(new_task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return new_process->pid;
}

static inline uintptr_t page_align(const uintptr_t addr) {
    return addr & 0xfff ? (addr + VM_PAGE_SIZE) & ~(0xfff) : addr;
}

#ifdef MAP_VIRT_SYSCALL_STATIC
static inline void undo_partial_map(const uintptr_t virtual_base,
                                    const uintptr_t virtual_last,
                                    const uintptr_t new_page) {
    if ((new_page & 0xff) == 0) {
        // we allocated a page, so must be failing because there's already a page
        // mapped here... We need to free the page we allocated.
        process_page_free(task_current()->owner, new_page);
    }

    // Either way, we're failing, so need to undo what we've already done...
    for (uintptr_t unmap_addr = virtual_base; unmap_addr < virtual_last;
         unmap_addr += VM_PAGE_SIZE) {
        uintptr_t page = vmm_virt_to_phys_page(unmap_addr);

#ifdef CONSERVATIVE_BUILD
        if (!page) {
            panic("unmapped page found during map_virtual syscall failure "
                  "loop");
        }
#endif
        vmm_unmap_page(unmap_addr);
        // TODO likely some opportunity to make this more
        //      efficient, it's spinning through the owned
        //      page list to free each individual page...
        process_page_free(task_current()->owner, page);
    }
}
#endif

SYSCALL_HANDLER(map_virtual) {
    size_t size = (size_t)arg0;
    uintptr_t virtual_base = (uintptr_t)arg1;
    const uint64_t flags = (uint64_t)arg2;

    if (size == 0) {
        // we don't map empty regions...
        return 0;
    }

    virtual_base = page_align(virtual_base);
    size = page_align(size);

    if (!IS_USER_ADDRESS(virtual_base)) {
        // nice try...
        return 0;
    }

    if (!IS_USER_ADDRESS(virtual_base + size)) {
        // also nope...
        return 0;
    }

    // Let's try to map it in, just using small pages for now...
    const uintptr_t virtual_end = virtual_base + size;
    for (uintptr_t addr = virtual_base; addr < virtual_end;
         addr += VM_PAGE_SIZE) {

#ifdef MAP_VIRT_SYSCALL_STATIC
        const uintptr_t new_page =
                process_page_alloc(task_current()->owner, physical_region);

        if (new_page & 0xff || vmm_virt_to_phys_page(addr)) {
            undo_partial_map(virtual_base, addr, new_page);
            return 0;
        }
#endif

        uint64_t vmm_flags = PG_PRESENT | PG_USER;
        if (flags & ANOS_MAP_VIRTUAL_FLAG_READ) {
            vmm_flags |= PG_READ;
        }

        if (flags & ANOS_MAP_VIRTUAL_FLAG_WRITE) {
#ifdef MAP_VIRT_SYSCALL_STATIC
            vmm_flags |= PG_WRITE;
#else
            // TODO need to figure out if already mapped and writeable,
            //      what to do about COW and other flags...
            vmm_flags |= PG_COPY_ON_WRITE;
#endif
        }

        if (flags & ANOS_MAP_VIRTUAL_FLAG_EXEC) {
            vmm_flags |= PG_EXEC;
        }

#ifdef MAP_VIRT_SYSCALL_STATIC
        if (!vmm_map_page(addr, new_page, vmm_flags)) {
            undo_partial_map(virtual_base, addr, new_page);
#else
        if (!vmm_map_page(addr, kernel_zero_page, vmm_flags)) {
            // TODO also need to figure out here if page was previously
            //      mapped. so don't unmap pages that were fine before if
            //      we fail here...

            for (uintptr_t unmap_addr = virtual_base; unmap_addr < addr;
                 unmap_addr += VM_PAGE_SIZE) {
                vmm_unmap_page(addr);
            }
#endif
            return 0;
        }
    }

#ifdef MAP_VIRT_SYSCALL_STATIC
    memclr((void *)virtual_base, size);
#endif

    return virtual_base;
}

SYSCALL_HANDLER(unmap_virtual) {
    size_t size = (size_t)arg0;
    uintptr_t virtual_base = (uintptr_t)arg1;

    virtual_base = page_align(virtual_base);
    size = page_align(size);

    if (!IS_USER_ADDRESS(virtual_base)) {
        return SYSCALL_BADARGS;
    }

    if (!IS_USER_ADDRESS(virtual_base + size)) {
        return SYSCALL_BADARGS;
    }

    while (size > 0) {
        vmm_unmap_page(virtual_base);

        virtual_base += VM_PAGE_SIZE;
        size -= VM_PAGE_SIZE;
    }

    return SYSCALL_OK;
}

// TODO we need a nicer scheme for return values in all of the below...
// they're kind of all over the place at the moment...
//

SYSCALL_HANDLER(send_message) {
    const uint64_t channel_cookie = (uint64_t)arg0;
    const uint64_t tag = (uint64_t)arg1;
    const size_t size = (size_t)arg2;
    void *buffer = (void *)arg3;

    if (IS_USER_ADDRESS(buffer)) {
        return ipc_channel_send(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SYSCALL_HANDLER(recv_message) {
    const uint64_t channel_cookie = (uint64_t)arg0;
    uint64_t *tag = (uint64_t *)arg1;
    size_t *size = (size_t *)arg2;
    void *buffer = (void *)arg3;

    if (IS_USER_ADDRESS(tag) && IS_USER_ADDRESS(size) &&
        IS_USER_ADDRESS(buffer)) {
        return ipc_channel_recv(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SYSCALL_HANDLER(reply_message) {
    const uint64_t message_cookie = (uint64_t)arg0;
    const uint64_t reply = (uint64_t)arg1;

    return ipc_channel_reply(message_cookie, reply);
}

SYSCALL_HANDLER(create_channel) { return ipc_channel_create(); }

SYSCALL_HANDLER(destroy_channel) {
    const uint64_t cookie = (uint64_t)arg0;

    ipc_channel_destroy(cookie);
    return SYSCALL_OK;
}

SYSCALL_HANDLER(register_named_channel) {
    const uint64_t cookie = (uint64_t)arg0;
    char *name = (char *)arg1;

    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_register(cookie, name)) {
        return SYSCALL_OK;
    } else {
        return SYSCALL_BAD_NUMBER;
    }
}

SYSCALL_HANDLER(deregister_named_channel) {
    char *name = (char *)arg0;

    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_deregister(name) != 0) {
        return SYSCALL_OK;
    }

    return SYSCALL_BAD_NAME;
}

SYSCALL_HANDLER(find_named_channel) {
    char *name = (char *)arg0;

    if (!IS_USER_ADDRESS(name)) {
        return 0;
    }

    return named_channel_find(name);
}

void thread_exitpoint(void);

SYSCALL_HANDLER(kill_current_task) {
    const Task *current = task_current();

    if (current) {
        sched_lock_this_cpu();

        current->sched->status_flags |= TASK_SCHED_FLAG_KILLED;

        sched_schedule();

        __builtin_unreachable();
    } else {
        return 0;
    }
}

/*
 * NOTE:
 * This syscall layer rejects overlapping regions, but *allows* adjacent ones.
 * If a region is adjacent to an existing region with the same metadata, we
 * coalesce them into one. This prevents AVL tree bloat from tiny heaps or
 * allocator fragmentation.
 */

static Region *region_tree_find_adjacent(Region *node, const uintptr_t start,
                                         const uintptr_t end,
                                         const uint64_t flags) {
    while (node) {
        if (end == node->start && flags == node->flags) {
            return node;
        }
        if (start == node->end && flags == node->flags) {
            return node;
        }
        if (end <= node->start) {
            node = node->left;
        } else if (start >= node->end) {
            node = node->right;
        } else {
            break;
        }
    }
    return nullptr;
}

static bool region_tree_overlaps(const Region *node, const uintptr_t start,
                                 const uintptr_t end) {
    while (node) {
        if (start < node->end && end > node->start) {
            return true;
        }
        if (end <= node->start) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return false;
}

SYSCALL_HANDLER(create_region) {
    const uintptr_t start = (uintptr_t)arg0;
    const uintptr_t end = (uintptr_t)arg1;
    const uint64_t flags = (uint64_t)arg2;

#ifdef DEBUG_REGION_SYSCALLS
    debugstr("CREATE REGION! 0x");
    printhex64(start, debugchar);
    debugstr(" - 0x");
    printhex64(end, debugchar);
    debugstr("\n");
#endif

    if ((start & 0xFFF) || (end & 0xFFF) || end <= start ||
        start >= USERSPACE_LIMIT || end > USERSPACE_LIMIT) {
        return SYSCALL_BADARGS;
    }

    const Process *proc = task_current()->owner;

    // Try coalescing with an adjacent region if one exists
    Region *adj = region_tree_find_adjacent(proc->meminfo->regions, start, end,
                                            flags);
    if (adj) {
        if (end == adj->start) {
            adj->start = start;
        } else if (start == adj->end) {
            adj->end = end;
        }
        return SYSCALL_OK;
    }

    if (region_tree_overlaps(proc->meminfo->regions, start, end)) {
        return SYSCALL_FAILURE; // overlap not allowed
    }

    Region *region = slab_alloc_block();
    if (!region) {
        return SYSCALL_FAILURE;
    }

    *region = (Region){
            .start = start,
            .end = end,
            .flags = flags,
            .left = nullptr,
            .right = nullptr,
            .height = 1,
    };

    proc->meminfo->regions = region_tree_insert(proc->meminfo->regions, region);

#ifdef DEBUG_REGION_SYSCALLS
    debugstr("CREATE REGION OK!\n");
#endif

    return SYSCALL_OK;
}

SYSCALL_HANDLER(destroy_region) {
    const uintptr_t start = (uintptr_t)arg0;

    if ((start & 0xFFF) || start >= USERSPACE_LIMIT) {
        return SYSCALL_BADARGS;
    }

    const Process *proc = task_current()->owner;
    proc->meminfo->regions = region_tree_remove(proc->meminfo->regions, start);
    return SYSCALL_OK;
}

SYSCALL_HANDLER(map_firmware_tables) {
#ifdef ARCH_X86_64
    static bool acpi_tables_handed_over = false;

    ACPI_RSDP *user_rsdp = (ACPI_RSDP *)arg0;

    // Check if tables have already been handed over
    if (acpi_tables_handed_over) {
        acpi_debugf("--> ACPI already handed over\n");
        return SYSCALL_FAILURE;
    }

    // Validate user address
    if (!IS_USER_ADDRESS(user_rsdp)) {
        acpi_debugf("--> ACPI not at user address\n");
        return SYSCALL_BADARGS;
    }

    // Get the RSDP and root table pointers from platform initialization
    ACPI_RSDP *kernel_rsdp = platform_get_root_firmware_table();
    ACPI_RSDT *root_table = platform_get_acpi_root_table();
    if (!kernel_rsdp || !root_table) {
        acpi_debugf("--> ACPI roots are NULL: 0x%016lx : 0x%016lx\n",
                    (uintptr_t)kernel_rsdp, (uintptr_t)root_table);
        return SYSCALL_FAILURE;
    }

    // Convert kernel virtual addresses back to physical addresses in the tables
    if (has_sig("XSDT", &root_table->header)) {
        acpi_debugf("--> ACPI root is XSDT with %d entries\n",
                    XSDT_ENTRY_COUNT(root_table));
        const uint32_t entries = XSDT_ENTRY_COUNT(root_table);
        uint64_t *entry = ((uint64_t *)(root_table + 1));

        for (int i = 0; i < entries; i++) {
            acpi_vdebugf("----> Entry %d = 0x%016lx", i, *entry);
            if (*entry) {
                const ACPI_SDTHeader *header = ((ACPI_SDTHeader *)*entry);
                acpi_vdebugf(" [%c%c%c%c]\n", header->signature[0],
                             header->signature[1], header->signature[2],
                             header->signature[3]);
            } else {
                acpi_vdebugf(" [????]\n");
            }

            // On real hardware, entries are sometimes 0 for some reason...
            if (*entry) {
                // Convert kernel virtual address back to physical
                const uintptr_t phys_addr = vmm_virt_to_phys(*entry);
                if (phys_addr == 0) {
                    acpi_debugf("--> Remapping XSDT failed: 0x%016lx\n",
                                (uintptr_t)*entry);
                    return SYSCALL_FAILURE;
                }
                acpi_vdebugf("--> Remapped to 0x%016lx\n", phys_addr);
                *entry = phys_addr;
            } else {
                acpi_debugf("--> WARN: NULL entry %d in XSDT\n", i);
            }
            entry++;
        }
    } else if (has_sig("RSDT", &root_table->header)) {
        acpi_debugf("--> ACPI root is RSDT with %d entries\n",
                    RSDT_ENTRY_COUNT(root_table));
        const uint32_t entries = RSDT_ENTRY_COUNT(root_table);
        uint32_t *entry = ((uint32_t *)(root_table + 1));

        for (int i = 0; i < entries; i++) {
            acpi_vdebugf("----> Entry %d = 0x%08x", i, *entry);
            if (*entry) {
                const ACPI_SDTHeader *header =
                        ((ACPI_SDTHeader *)*(uintptr_t *)entry);
                acpi_vdebugf(" [%c%c%c%c]\n", header->signature[0],
                             header->signature[1], header->signature[2],
                             header->signature[3]);
            } else {
                acpi_vdebugf(" [????]\n");
            }

            // On real hardware, entries are sometimes 0 for some reason...
            if (*entry) {
                // Convert kernel virtual address back to physical
                const uintptr_t phys_addr =
                        vmm_virt_to_phys(*entry | 0xFFFFFFFF00000000);
                if (phys_addr == 0) {
                    acpi_debugf("--> Remapping RSDT failed: 0x%016lx\n",
                                (uintptr_t)*entry);
                    return SYSCALL_FAILURE;
                }
                *entry = (uint32_t)phys_addr;
            } else {
                acpi_debugf("--> WARN: NULL entry %d in RSDT\n", i);
            }
            entry++;
        }
    } else {
        acpi_debugf("--> Unknown root entry signature\n");
        return SYSCALL_FAILURE;
    }

    // Copy the RSDP to userspace
    // Note: We need to copy the full RSDP structure size
    const size_t rsdp_size = (kernel_rsdp->revision == 0) ? ACPI_R0_RSDP_SIZE
                                                          : kernel_rsdp->length;

    // Simple byte-by-byte copy to userspace
    const uint8_t *src = (uint8_t *)kernel_rsdp;
    uint8_t *dst = (uint8_t *)user_rsdp;
    for (size_t i = 0; i < rsdp_size; i++) {
        dst[i] = src[i];
    }

    // Unmap all ACPI tables from kernel space
    constexpr uintptr_t acpi_virt_start = ACPI_TABLES_VADDR_BASE;
    constexpr uintptr_t acpi_virt_end = ACPI_TABLES_VADDR_LIMIT;

    for (uintptr_t virt_addr = acpi_virt_start; virt_addr < acpi_virt_end;
         virt_addr += VM_PAGE_SIZE) {
        // Check if this virtual page is mapped and unmap it
        if (vmm_virt_to_phys_page(virt_addr) != 0) {
            vmm_unmap_page(virt_addr);
        }
    }

    // Mark tables as handed over
    acpi_tables_handed_over = true;

    acpi_debugf("--> ACPI handover complete\n");

    return SYSCALL_OK;
#else
    // On non-x86_64 architectures, firmware tables are not yet supported
    return SYSCALL_NOT_IMPL;
#endif
}

SYSCALL_HANDLER(map_physical) {
    const uintptr_t phys_addr = (uintptr_t)arg0;
    const uintptr_t user_vaddr = (uintptr_t)arg1;
    const size_t size = (size_t)arg2;
    const uint64_t flags = (uint64_t)arg3;

    // Validate user address
    if (!IS_USER_ADDRESS(user_vaddr)) {
        return SYSCALL_BADARGS;
    }

    // Ensure addresses are page-aligned
    if ((user_vaddr & 0xFFF) || (phys_addr & 0xFFF) || (size & 0xFFF)) {
        return SYSCALL_BADARGS;
    }

    // Ensure the entire region fits in userspace
    if (!IS_USER_ADDRESS(user_vaddr + size - 1)) {
        return SYSCALL_BADARGS;
    }

    // Map each page from physical to user virtual
    for (uintptr_t offset = 0; offset < size; offset += VM_PAGE_SIZE) {
        const uintptr_t target_phys_addr = phys_addr + offset;
        const uintptr_t target_user_vaddr = user_vaddr + offset;

        // Build page flags from the provided permission flags
        uint64_t page_flags = PG_PRESENT | PG_USER;

        // Default to read-only if no flags specified, otherwise use provided flags
        if (flags == 0) {
            page_flags |= PG_READ;
        } else {
            if (flags & ANOS_MAP_VIRTUAL_FLAG_READ) {
                page_flags |= PG_READ;
            }
            if (flags & ANOS_MAP_VIRTUAL_FLAG_WRITE) {
                page_flags |= PG_WRITE;
            }
            // Note: PG_EXEC flag would be needed for ANOS_MAP_VIRTUAL_FLAG_EXEC
        }

        // Map the physical page into userspace with specified permissions
        if (!vmm_map_page(target_user_vaddr, target_phys_addr, page_flags)) {
            // If mapping fails, unmap what we've already done
            for (uintptr_t unmap_offset = 0; unmap_offset < offset;
                 unmap_offset += VM_PAGE_SIZE) {
                const uintptr_t unmap_user_vaddr = user_vaddr + unmap_offset;
                vmm_unmap_page(unmap_user_vaddr);
            }
            return SYSCALL_FAILURE;
        }
    }

    return SYSCALL_OK;
}

SYSCALL_HANDLER(alloc_physical_pages) {
    const size_t size = (size_t)arg0;

    // Ensure size is page-aligned
    if (size & 0xFFF) {
        return SYSCALL_BADARGS;
    }

    // Calculate number of pages needed
    const size_t num_pages = size / VM_PAGE_SIZE;
    if (num_pages == 0) {
        return SYSCALL_BADARGS;
    }

    // Allocate contiguous physical pages
    const uintptr_t phys_addr = page_alloc_m(physical_region, num_pages);
    if (phys_addr == 0) {
        return SYSCALL_FAILURE; // Out of memory
    }

    // Return the physical address
    return (SyscallResult)phys_addr;
}

static uint64_t init_syscall_capability(CapabilityMap *map,
                                        const SyscallId syscall_id,
                                        const SyscallHandler handler) {
    if (!map) {
        return 0;
    }

    SyscallCapability *capability = slab_alloc_block();

    if (!capability) {
        return 0;
    }

    const uint64_t cookie = capability_cookie_generate();

    if (!cookie) {
        slab_free(capability);
        return 0;
    }

    capability->this.type = CAPABILITY_TYPE_SYSCALL;
    capability->this.subtype = 1;
    capability->syscall_id = syscall_id;
    capability->flags = 0;
    capability->handler = handler;

    if (!capability_map_insert(map, cookie, capability)) {
        slab_free(capability);
        return 0;
    }

    return cookie;
}

#define STACKED_LONGS_PER_CAPABILITY ((2))

#ifdef DEBUG_SYSCALL_CAPS
#define debug_syscall_cap_assignment kprintf
#else
#define debug_syscall_cap_assignment(...)
#endif

#define stack_syscall_capability_cookie(id, handler)                           \
    do {                                                                       \
        uint64_t cookie =                                                      \
                init_syscall_capability(&global_capability_map, id, handler);  \
        if (!cookie) {                                                         \
            return nullptr;                                                    \
        }                                                                      \
        debug_syscall_cap_assignment("COOKIE %d = 0x%016lx\n", id, cookie);    \
        *--current_stack = cookie;                                             \
        *--current_stack = id;                                                 \
    } while (0)

uint64_t *syscall_init_capabilities(uint64_t *stack) {
    uint64_t *current_stack = stack;

    // Stack all syscall capability cookies...
    stack_syscall_capability_cookie(SYSCALL_ID_DEBUG_PRINT,
                                    SYSCALL_NAME(debugprint));
    stack_syscall_capability_cookie(SYSCALL_ID_DEBUG_CHAR,
                                    SYSCALL_NAME(debugchar));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_THREAD,
                                    SYSCALL_NAME(create_thread));
    stack_syscall_capability_cookie(SYSCALL_ID_MEMSTATS,
                                    SYSCALL_NAME(memstats));
    stack_syscall_capability_cookie(SYSCALL_ID_SLEEP, SYSCALL_NAME(sleep));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_PROCESS,
                                    SYSCALL_NAME(create_process));
    stack_syscall_capability_cookie(SYSCALL_ID_MAP_VIRTUAL,
                                    SYSCALL_NAME(map_virtual));
    stack_syscall_capability_cookie(SYSCALL_ID_SEND_MESSAGE,
                                    SYSCALL_NAME(send_message));
    stack_syscall_capability_cookie(SYSCALL_ID_RECV_MESSAGE,
                                    SYSCALL_NAME(recv_message));
    stack_syscall_capability_cookie(SYSCALL_ID_REPLY_MESSAGE,
                                    SYSCALL_NAME(reply_message));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_CHANNEL,
                                    SYSCALL_NAME(create_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_DESTROY_CHANNEL,
                                    SYSCALL_NAME(destroy_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_REGISTER_NAMED_CHANNEL,
                                    SYSCALL_NAME(register_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_DEREGISTER_NAMED_CHANNEL,
                                    SYSCALL_NAME(deregister_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_FIND_NAMED_CHANNEL,
                                    SYSCALL_NAME(find_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_KILL_CURRENT_TASK,
                                    SYSCALL_NAME(kill_current_task));
    stack_syscall_capability_cookie(SYSCALL_ID_UNMAP_VIRTUAL,
                                    SYSCALL_NAME(unmap_virtual));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_REGION,
                                    SYSCALL_NAME(create_region));
    stack_syscall_capability_cookie(SYSCALL_ID_DESTROY_REGION,
                                    SYSCALL_NAME(destroy_region));
    stack_syscall_capability_cookie(SYSCALL_ID_MAP_FIRMWARE_TABLES,
                                    SYSCALL_NAME(map_firmware_tables));
    stack_syscall_capability_cookie(SYSCALL_ID_MAP_PHYSICAL,
                                    SYSCALL_NAME(map_physical));
    stack_syscall_capability_cookie(SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
                                    SYSCALL_NAME(alloc_physical_pages));

    // Stack dummy argc/argv for now
    // TODO this needs refactoring, syscall init shouldn't be responsible
    //      for this
    *--current_stack = 0;
    *--current_stack = 0;

    // Stack a pointer to the first cookie (at the top of the stack)
    --current_stack;
    *current_stack = (uint64_t)(current_stack + 3);

    // Stack the number of cookies
    *--current_stack = (SYSCALL_ID_END - 1);

    return current_stack;
}

SyscallResult handle_syscall_69(const SyscallArg arg0, const SyscallArg arg1,
                                const SyscallArg arg2, const SyscallArg arg3,
                                const SyscallArg arg4,
                                const SyscallArg capability_cookie) {

    Process *proc = task_current()->owner;

    const SyscallCapability *cap = capability_map_lookup(
            &global_capability_map, (uint64_t)capability_cookie);

    if (cap && cap->handler) {
#ifdef ENABLE_SYSCALL_THROTTLE_RESET
        throttle_reset(proc);
#endif
        return cap->handler(arg0, arg1, arg2, arg3, arg4);
    }

    throttle_abuse(proc);
    return SYSCALL_INCAPABLE;
}
