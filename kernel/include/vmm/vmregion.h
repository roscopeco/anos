/*
 * stage3 - Virtual memory regions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: Cpp

#ifndef __ANOS_KERNEL_VM_REGION_H
#define __ANOS_KERNEL_VM_REGION_H

#include "process.h"
#include "structs/region_tree.h"
#include "task.h"

// Region flags
#define VM_REGION_AUTOMAP ((0x01))

static inline Region *vm_region_find_in_process(const Process *process,
                                                const uintptr_t vaddr) {
    return region_tree_lookup(process->meminfo->regions, vaddr);
}

static inline Region *vm_region_find_in_current_process(const uintptr_t vaddr) {
    return region_tree_lookup(task_current()->owner->meminfo->regions, vaddr);
}

#endif //__ANOS_KERNEL_VM_REGION_H