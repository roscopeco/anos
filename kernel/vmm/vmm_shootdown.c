/*
 * stage3 - TLB shootdown
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This is essentially a thin wrapper over the vmm_map/unmap_page
 * API that performs TLB shootdowns.
 *
 * Shootdowns are **expensive**! Only use these routines when
 * it's actually necessary, use the lower-level functions
 * directly if at all possible.
 */

#include "process.h"
#include "smp/ipwi.h"
#include "task.h"
#include "vmm/vmmapper.h"

bool vmm_shootdown_map_page_containing_in_process(const Process *process, const uintptr_t virt_addr,
                                                  const uintptr_t phys_addr, const uintptr_t flags) {
    // This is **incredibly slow** on x86_64, so do it before
    // disabling interrupts...
    //
    // TODO this actually **cannot** work on x86_64 currently, phys_to_virt
    //      only works within the current address space due to recursive paging!
    void *pml4_virt = vmm_phys_to_virt_ptr(process->pml4);

    if (!pml4_virt) {
        return false;
    }

    const uint64_t intr_flags = save_disable_interrupts();

    const uintptr_t result = vmm_map_page_containing_in(pml4_virt, virt_addr, phys_addr, flags);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = process->pid;
    payload->target_pml4 = 0;

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(intr_flags);

    return result;
}

bool vmm_shootdown_map_page_containing_in_pml4(uint64_t *pml4_virt, uintptr_t virt_addr, const uintptr_t phys_addr,
                                               const uintptr_t flags) {
    const uint64_t intr_flags = save_disable_interrupts();

    const uintptr_t result = vmm_map_page_containing_in(pml4_virt, virt_addr, phys_addr, flags);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = 0;
    payload->target_pml4 = vmm_virt_to_phys((uintptr_t)pml4_virt);

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(intr_flags);

    return result;
}

bool vmm_shootdown_map_pages_containing_in_process(const Process *process, const uintptr_t virt_addr,
                                                   const uintptr_t phys_addr, const uintptr_t flags,
                                                   const size_t num_pages) {
    // This is **incredibly slow** on x86_64, so do it before
    // disabling interrupts...
    void *pml4_virt = vmm_phys_to_virt_ptr(process->pml4);

    if (!pml4_virt) {
        return false;
    }

    const uint64_t intr_flags = save_disable_interrupts();

    const uintptr_t result = vmm_map_pages_containing_in(pml4_virt, virt_addr, phys_addr, flags, num_pages);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = process->pid;
    payload->target_pml4 = 0;

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(intr_flags);

    return result;
}

bool vmm_shootdown_map_pages_containing_in_pml4(uint64_t *pml4_virt, const uintptr_t virt_addr,
                                                const uintptr_t phys_addr, const uintptr_t flags,
                                                const size_t num_pages) {
    const uint64_t intr_flags = save_disable_interrupts();

    const uintptr_t result = vmm_map_pages_containing_in(pml4_virt, virt_addr, phys_addr, flags, num_pages);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = 0;
    payload->target_pml4 = vmm_virt_to_phys((uintptr_t)pml4_virt);

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(intr_flags);

    return result;
}

bool vmm_shootdown_map_page_containing(const uintptr_t virt_addr, const uintptr_t phys_addr, const uintptr_t flags) {
    return vmm_shootdown_map_page_containing_in_process(task_current()->owner, virt_addr, phys_addr, flags);
}

bool vmm_shootdown_map_page_in_process(const Process *process, const uintptr_t virt_addr, const uintptr_t page,
                                       const uintptr_t flags) {
    return vmm_shootdown_map_page_containing_in_process(process, virt_addr, page, flags);
}

bool vmm_shootdown_map_page_in_pml4(uint64_t *pml4_virt, const uintptr_t virt_addr, const uintptr_t page,
                                    const uintptr_t flags) {
    return vmm_shootdown_map_page_containing_in_pml4(pml4_virt, virt_addr, page, flags);
}

bool vmm_shootdown_map_page(const uintptr_t virt_addr, const uintptr_t page, const uintptr_t flags) {
    return vmm_shootdown_map_page_containing_in_process(task_current()->owner, virt_addr, page, flags);
}

uintptr_t vmm_shootdown_unmap_page_in_process(const Process *process, const uintptr_t virt_addr) {
    // This is **incredibly slow** on x86_64, so do it before
    // disabling interrupts...
    void *pml4_virt = vmm_phys_to_virt_ptr(process->pml4);

    if (!pml4_virt) {
        return false;
    }

    const uint64_t flags = save_disable_interrupts();

    const uintptr_t result = vmm_unmap_page_in(pml4_virt, virt_addr);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = process->pid;
    payload->target_pml4 = 0;

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(flags);

    return result;
}

uintptr_t vmm_shootdown_unmap_page_in_pml4(uint64_t *pml4_virt, const uintptr_t virt_addr) {
    const uint64_t flags = save_disable_interrupts();

    const uintptr_t result = vmm_unmap_page_in(pml4_virt, virt_addr);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = 0;
    payload->target_pml4 = vmm_virt_to_phys((uintptr_t)pml4_virt);

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(flags);

    return result;
}

uintptr_t vmm_shootdown_unmap_page(const uintptr_t virt_addr) {
    return vmm_shootdown_unmap_page_in_process(task_current()->owner, virt_addr);
}

bool vmm_shootdown_map_pages_containing(const uintptr_t virt_addr, const uintptr_t phys_addr, const uintptr_t flags,
                                        const size_t num_pages) {
    return vmm_shootdown_map_pages_containing_in_process(task_current()->owner, virt_addr, phys_addr, flags, num_pages);
}

bool vmm_shootdown_map_pages_in_process(const Process *process, const uintptr_t virt_addr, const uintptr_t page,
                                        const uintptr_t flags, const size_t num_pages) {
    return vmm_shootdown_map_pages_containing_in_process(process, virt_addr, page, flags, num_pages);
}

bool vmm_shootdown_map_pages_in_pml4(uint64_t *pml4_virt, const uintptr_t virt_addr, const uintptr_t page,
                                     const uintptr_t flags, const size_t num_pages) {
    return vmm_shootdown_map_pages_containing_in_pml4(pml4_virt, virt_addr, page, flags, num_pages);
}

bool vmm_shootdown_map_pages(const uintptr_t virt_addr, const uintptr_t page, const uintptr_t flags,
                             const size_t num_pages) {
    return vmm_shootdown_map_pages_containing_in_process(task_current()->owner, virt_addr, page, flags, num_pages);
}

uintptr_t vmm_shootdown_unmap_pages_in_process(const Process *process, const uintptr_t virt_addr,
                                               const size_t num_pages) {
    // This is **incredibly slow** on x86_64, so do it before
    // disabling interrupts...
    void *pml4_virt = vmm_phys_to_virt_ptr(process->pml4);

    if (!pml4_virt) {
        return false;
    }

    const uint64_t flags = save_disable_interrupts();

    const uintptr_t result = vmm_unmap_pages_in(pml4_virt, virt_addr, num_pages);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = process->pid;
    payload->target_pml4 = 0;

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(flags);

    return result;
}

uintptr_t vmm_shootdown_unmap_pages_in_pml4(uint64_t *pml4_virt, const uintptr_t virt_addr, const size_t num_pages) {
    const uint64_t flags = save_disable_interrupts();

    const uintptr_t result = vmm_unmap_pages_in(pml4_virt, virt_addr, num_pages);

    IpwiWorkItem work_item = {
            .type = IPWI_TYPE_TLB_SHOOTDOWN,
            .flags = 0,
    };

    IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&work_item.payload;
    payload->page_count = 1;
    payload->start_vaddr = virt_addr;
    payload->target_pid = 0;
    payload->target_pml4 = vmm_virt_to_phys((uintptr_t)pml4_virt);

    ipwi_enqueue_all_except_current(&work_item);
    restore_saved_interrupts(flags);

    return result;
}

uintptr_t vmm_shootdown_unmap_pages(const uintptr_t virt_addr, const size_t num_pages) {
    return vmm_shootdown_unmap_pages_in_process(task_current()->owner, virt_addr, num_pages);
}
