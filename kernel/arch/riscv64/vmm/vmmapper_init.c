/*
 * RISC-V virtual memory manager - direct mapping initialisation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * First time in this file? Well, buckle up, buttercup - you're in
 * for a bit of a ride...
 *
 * On RISC-V, we've always used a direct mapping, since recursive isn't
 * possible on that platform. This is used for everything, including
 * management of page tables themselves, which presents us a bit of a
 * chicken-and-egg situation when it comes to actually building the
 * direct map during init.
 *
 * A lot of the complexity in here is dealing with that - once the
 * direct map is built, it makes the rest of VMM a breeze compared
 * to those based on recursive mapping.
 *
 * I try to be as efficient as possible with the mapping, using the
 * largest tables possible for each region and breaking it into
 * naturally-aligned blocks for larger sizes where possible.
 *
 * Overhead will vary, on qemu with 8GiB RAM, it currently needs
 * just over 32KiB for the direct-mapping tables (mapping all of
 * physical RAM).
 *
 * There's more info in MemoryMap.md.
*
 * TODO most of this is actually platform agnostic, and should be
 *      merged. We only have the platform-specific ones because,
 *      for temp mappings when creating new tables, on RISC-V we
 *      use terapages, while on x86_64 (which doesn't support them)
 *      we use gigapages... So tidy this up and factor the
 *      commonalities out into cross-platform code.
 *
 */

#include <stdint.h>

#include "panic.h"
#include "pmm/pagealloc.h"
#include "riscv64/kdrivers/cpu.h"
#include "riscv64/vmm/vmconfig.h"
#include "riscv64/vmm/vmmapper.h"

#ifdef DEBUG_VMM
#include "kprintf.h"
#define debugf kprintf
#ifdef VERY_NOISY_VMM
#define vdebugf kprintf
#else
#define vdebugf(...)
#endif
#else
#define debugf(...)
#define vdebugf(...)
#endif

// Physical memory region
extern MemoryRegion *physical_region;

uint64_t vmm_direct_mapping_terapages_used;
uint64_t vmm_direct_mapping_gigapages_used;
uint64_t vmm_direct_mapping_megapages_used;
uint64_t vmm_direct_mapping_pages_used;

static inline uintptr_t per_cpu_temp_base_vaddr_for_pt_index(uint16_t index) {
    return PER_CPU_TEMP_PAGE_BASE + (index * PAGE_SIZE);
}

static inline void *per_cpu_temp_base_vptr_for_pt_index(uint16_t index) {
    return (void *)per_cpu_temp_base_vaddr_for_pt_index(index);
}

static inline void map_readwrite_and_flush_offs(uint64_t *table, uint16_t table_index, uintptr_t base_paddr,
                                                uintptr_t base_vaddr, uintptr_t base_offs) {

    vdebugf("map_readwrite_and_flush: table @ 0x%016lx[%d]: vaddr: "
            "0x%016lx+0x%016lx => paddr: 0x%016lx\n",
            (uintptr_t)table, table_index, base_vaddr, base_offs, base_paddr);

    table[table_index] = vmm_phys_and_flags_to_table_entry(base_paddr, PG_READ | PG_WRITE | PG_PRESENT);
    cpu_invalidate_tlb_addr(base_vaddr + base_offs);
    vdebugf("map_readwrite_and_flush: done, and done\n");
}

static inline void map_readwrite_and_flush(uint64_t *table, uint16_t table_index, uintptr_t paddr, uintptr_t vaddr) {
    map_readwrite_and_flush_offs(table, table_index, paddr, vaddr, 0);
}

static inline void unmap_and_flush(uint64_t *table, uint16_t table_index, uintptr_t vaddr) {
    table[table_index] = 0;
    cpu_invalidate_tlb_addr(vaddr);
}

static void vmm_init_map_terapage(uint64_t *pml4, uintptr_t base, uint8_t flags) {
    vdebugf("vmm_init_map_terapage: Mapping phys 0x%016lx with length %ld into "
            "PML4 @ 0x%016lx\n",
            base, TERA_PAGE_SIZE, (uintptr_t)pml4);

    if ((base + TERA_PAGE_SIZE) > MAX_PHYS_ADDR) {
        debugf("WARN: Refusing to map memory at 0x%016lx [%ld bytes] due to "
               "address-space overflow\n",
               base, TERA_PAGE_SIZE);
        return;
    }

    uintptr_t vaddr = vmm_phys_to_virt(base);
    vdebugf("  -> vaddr is 0x%016lx\n", vaddr);

    uint16_t pml4_index = vmm_virt_to_pml4_index(vaddr);
    uint64_t pml4e = pml4[pml4_index];

    if (pml4e & PG_PRESENT) {
        debugf("WARN [BUG]: PML4 for terapage at 0x%016lx is already mapped\n", vaddr);
    }

    pml4[pml4e] = vmm_phys_and_flags_to_table_entry(base, flags);

    vmm_direct_mapping_terapages_used++;
    vdebugf("vmm_init_map_terapage: Region mapped successfully\n");
}

static uint64_t *const ensure_direct_table_map(uint64_t *table, uint16_t table_index, uint64_t *temp_mapping_pt,
                                               uint16_t temp_mapping_pt_index) {
    vdebugf("ensure_direct_table_map: table 0x%016lx[%d] into temp mapping "
            "0x%016lx[%d]\n",
            (uintptr_t)table, table_index, (uintptr_t)temp_mapping_pt, temp_mapping_pt_index);
    const uint64_t table_entry = table[table_index];

    const uintptr_t new_table_vaddr = per_cpu_temp_base_vaddr_for_pt_index(temp_mapping_pt_index);
    uint64_t *const new_table = per_cpu_temp_base_vptr_for_pt_index(temp_mapping_pt_index);

    if ((table_entry & PG_PRESENT) == 0) {
        vdebugf("  -> adding a new child table at at index %d\n", table_index);

        const uintptr_t new_table_paddr = page_alloc(physical_region);
        if (new_table_paddr & 0xfff) {
            panic("Ran out of memory while building new table for direct "
                  "mapping");
        }

        map_readwrite_and_flush(temp_mapping_pt, temp_mapping_pt_index, new_table_paddr, new_table_vaddr);

        vdebugf("  -> New table is mapped; Clearing...");

        for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            new_table[i] = 0;
        }

        table[table_index] = vmm_phys_and_flags_to_table_entry(new_table_paddr, PG_PRESENT);
    } else {
        vdebugf("  -> table already present, mapping...\n");
        map_readwrite_and_flush(temp_mapping_pt, temp_mapping_pt_index, vmm_table_entry_to_phys(table_entry),
                                new_table_vaddr);
    }

    return new_table;
}

static void vmm_init_map_gigapage(uint64_t *pml4, uint64_t *temp_mapping_pt, uintptr_t base, uint8_t flags) {
    vdebugf("vmm_init_map_gigapage: Mapping phys 0x%016lx with length %ld into "
            "PML4 @ 0x%016lx\n",
            base, GIGA_PAGE_SIZE, (uintptr_t)pml4);

    if ((base + GIGA_PAGE_SIZE) > MAX_PHYS_ADDR) {
        debugf("WARN: Refusing to map memory at 0x%016lx [%ld bytes] due to "
               "address-space overflow\n",
               base, GIGA_PAGE_SIZE);
        return;
    }

    const uintptr_t vaddr = vmm_phys_to_virt(base);
    vdebugf("  -> vaddr is 0x%016lx\n", vaddr);

    // ## PDPT
    const uint16_t pml4_index = vmm_virt_to_pml4_index(vaddr);
    uint64_t *const pdpt = ensure_direct_table_map(pml4, pml4_index, temp_mapping_pt, 0);

    const uint16_t pdpt_index = vmm_virt_to_pdpt_index(vaddr);

    vdebugf("  -> Mapping region at PDPT index %d\n", pdpt_index);

    if (pdpt[pdpt_index] & PG_PRESENT) {
        panic("Physical memory already direct mapped; region overlap or bug; "
              "cannot continue");
    }

    pdpt[pdpt_index] = vmm_phys_and_flags_to_table_entry(base, flags);
    unmap_and_flush(temp_mapping_pt, 0, (uintptr_t)pdpt);

    vmm_direct_mapping_gigapages_used++;
    vdebugf("vmm_init_map_gigapage: Region mapped successfully\n");
}

static void vmm_init_map_megapage(uint64_t *pml4, uint64_t *temp_mapping_pt, uintptr_t base, uint8_t flags) {
    vdebugf("vmm_init_map_megapage: Mapping phys 0x%016lx with length %ld into "
            "PML4 @ 0x%016lx\n",
            base, MEGA_PAGE_SIZE, (uintptr_t)pml4);

    if ((base + MEGA_PAGE_SIZE) > MAX_PHYS_ADDR) {
        debugf("WARN: Refusing to map memory at 0x%016lx [%ld bytes] due to "
               "address-space overflow\n",
               base, MEGA_PAGE_SIZE);
        return;
    }

    const uintptr_t vaddr = vmm_phys_to_virt(base);
    vdebugf("  -> vaddr is 0x%016lx\n", vaddr);

    // ## PDPT
    const uint16_t pml4_index = vmm_virt_to_pml4_index(vaddr);
    uint64_t *const pdpt = ensure_direct_table_map(pml4, pml4_index, temp_mapping_pt, 0);

    // ## PD
    const uint16_t pdpt_index = vmm_virt_to_pdpt_index(vaddr);
    uint64_t *const pd = ensure_direct_table_map(pdpt, pdpt_index, temp_mapping_pt, 1);

    uint16_t pd_index = vmm_virt_to_pd_index(vaddr);

    vdebugf("  -> Mapping region at PD index %d\n", pd_index);

    if (pd[pd_index] & PG_PRESENT) {
        panic("Physical memory already direct mapped; region overlap or bug; "
              "cannot continue");
    }

    pd[pd_index] = vmm_phys_and_flags_to_table_entry(base, flags);
    unmap_and_flush(temp_mapping_pt, 0, (uintptr_t)pdpt);
    unmap_and_flush(temp_mapping_pt, 1, (uintptr_t)pd);

    vmm_direct_mapping_megapages_used++;
    vdebugf("vmm_init_map_megapage: Region mapped successfully\n");
}

static void vmm_init_map_page(uint64_t *pml4, uint64_t *temp_mapping_pt, uintptr_t base, uint8_t flags) {
    vdebugf("vmm_init_map_page: Mapping phys 0x%016lx with length %ld into "
            "PML4 @ 0x%016lx\n",
            base, PAGE_SIZE, (uintptr_t)pml4);

    if ((base + PAGE_SIZE) > MAX_PHYS_ADDR) {
        debugf("WARN: Refusing to map memory at 0x%016lx [%ld bytes] due to "
               "address-space overflow\n",
               base, PAGE_SIZE);
        return;
    }

    const uintptr_t vaddr = vmm_phys_to_virt(base);
    vdebugf("  -> vaddr is 0x%016lx\n", vaddr);

    // ## PDPT
    const uint16_t pml4_index = vmm_virt_to_pml4_index(vaddr);
    uint64_t *const pdpt = ensure_direct_table_map(pml4, pml4_index, temp_mapping_pt, 0);

    // ## PD
    const uint16_t pdpt_index = vmm_virt_to_pdpt_index(vaddr);
    uint64_t *const pd = ensure_direct_table_map(pdpt, pdpt_index, temp_mapping_pt, 1);

    // ## PT
    const uint16_t pd_index = vmm_virt_to_pd_index(vaddr);
    uint64_t *const pt = ensure_direct_table_map(pd, pd_index, temp_mapping_pt, 2);

    uint16_t pt_index = vmm_virt_to_pt_index(vaddr);

    vdebugf("  -> Mapping region at PT index %d\n", pt_index);

    if (pt[pt_index] & PG_PRESENT) {
        panic("Physical memory already direct mapped; region overlap or bug; "
              "cannot continue");
    }

    pt[pt_index] = vmm_phys_and_flags_to_table_entry(base, flags);

    unmap_and_flush(temp_mapping_pt, 0, (uintptr_t)pdpt);
    unmap_and_flush(temp_mapping_pt, 1, (uintptr_t)pd);
    unmap_and_flush(temp_mapping_pt, 3, (uintptr_t)pt);

    vmm_direct_mapping_pages_used++;
    vdebugf("vmm_init_map_page: Region mapped successfully\n");
}

static void vmm_init_map_region(uint64_t *pml4, uint64_t *temp_mapping_pt, Limine_MemMapEntry *entry, bool writeable) {
    vdebugf("vmm_init_map_region: Mapping phys 0x%016lx with length %ld into "
            "PML4 @ 0x%016lx\n",
            entry->base, entry->length, (uintptr_t)pml4);
    uint8_t flags = PG_PRESENT | PG_GLOBAL | PG_READ | (writeable ? PG_WRITE : 0);
    uintptr_t base = entry->base;
    uintptr_t length = entry->length;

    while (length > 0) {
        if (length >= TERA_PAGE_SIZE && base % TERA_PAGE_SIZE == 0) {
            // map one terapage
            vmm_init_map_terapage(pml4, base, flags);
            base += TERA_PAGE_SIZE;
            length -= TERA_PAGE_SIZE;
        } else if (length >= GIGA_PAGE_SIZE && base % GIGA_PAGE_SIZE == 0) {
            // map one gigapage
            vmm_init_map_gigapage(pml4, temp_mapping_pt, base, flags);
            base += GIGA_PAGE_SIZE;
            length -= GIGA_PAGE_SIZE;
        } else if (length >= MEGA_PAGE_SIZE && base % MEGA_PAGE_SIZE == 0) {
            // map one megapage
            vmm_init_map_megapage(pml4, temp_mapping_pt, base, flags);
            base += MEGA_PAGE_SIZE;
            length -= MEGA_PAGE_SIZE;
        } else if (length >= PAGE_SIZE && base % PAGE_SIZE == 0) {
            // map one page
            vmm_init_map_page(pml4, temp_mapping_pt, base, flags);
            base += PAGE_SIZE;
            length -= PAGE_SIZE;
        } else {
            debugf("vmm_init_map_region: WARN: %ld byte area < PAGE_SIZE "
                   "wasted at 0x%016lx\n",
                   length, base);
            return;
        }
    }

    vdebugf("vmm_init_map_region: phys 0x%016lx with length %ld mapped into "
            "pml4 @ 0x%016lx\n",
            entry->base, entry->length, (uintptr_t)pml4);
}

#define TERAPAGE_ALIGN_MASK (~(TERA_PAGE_SIZE - 1))

static inline uintptr_t round_down_to_terapage(uintptr_t addr) { return addr & TERAPAGE_ALIGN_MASK; }

static inline uintptr_t offset_in_terapage(uintptr_t addr) { return addr & (TERA_PAGE_SIZE - 1); }

// this next bit is already hard enough to follow, so don't let clang-format
// mess up the formatting...
//
// clang-format off

// This just ensures a full set of page-tables exist for for the given temp_map_addr.
//
// We use this for temporarily mapping new page tables we need to create during the
// build of the direct mapping, because of the aforementioned chicken-and-egg situation -
// without the direct map, the usual vmm_ functions for managing page tables don't
// work, but we do need to be able to map pages in order to build the direct map...
//
// Although this is expected to be called in early boot it doesn't make
// assumptions about the existing table layout, other than that the PML4 it's
// given is valid. This means that as the design progresses and inevitably changes
// I'm (hopefully) less likely to waste time debugging weird issues before
// remembering that this thing exists :D
//
// It's a hell of a function, but because the process itself is a bit mind-bending
// I wrote it with a focus on understandability of the actual logic we follow
// rather than optimising for performance or perceived 'cleanliness'.
//
// Worth noting also that it's only to be used before userspace is up, since it
// (ab)uses some of the userspace PML4 mappings to do its work.
//
static uint64_t* ensure_temp_page_tables(uint64_t * const pml4, const uintptr_t temp_map_addr) {
    vdebugf("ensure_temp_page_tables(0x%016lx, 0x%016lx)\n", (uintptr_t)pml4, temp_map_addr);

    uint16_t pml4e = vmm_virt_to_pml4_index(temp_map_addr);

    vdebugf("  Will try to get entry %d from the pml4e\n", pml4e);

    // Do we have a PDPT covering the region?
    if (unlikely((pml4[pml4e] & PG_PRESENT) == 0)) {
        panic("Kernel space root mapping does not exist");
    } else {
        vdebugf("    PDPT is present, checking PDPT\n");
        // Yes - map it as a terapage in low userspace, since we know we're
        // not using that yet. It'll go at the 512GiB mark.
        //
        // Because we need mappings to be naturally aligned, we have to round the
        // phys address down to the next terapage (512GiB) boundary - which will
        // almost certainly make it 0 on any system we're likely to be running on.
        //
        // We then map that in at the 512GiB mark (to avoid accidental nulls
        // creeping in somewhere) and offset that by the phys address' offset
        // from its next-lowest 512GiB boundary (as I say, likely zero) so
        // we can get at the table.
        //
        // This is a bit painful, but saves some hassle, since we can't
        // go the nicer way until this direct mapping is set up...
        //

        // Figure out the next-lowest 512GiB base and the offset of the phys address...
        const uintptr_t pdpt_phys = vmm_table_entry_to_phys(pml4[pml4e]);
        const uintptr_t pdpt_phys_base_addr = round_down_to_terapage(pdpt_phys);
        const uintptr_t pdpt_phys_addr_offs = offset_in_terapage(pdpt_phys);

        // This is the base of the virtual space we're about to map...
        const uintptr_t pdpt_temp_vaddr = TERA_PAGE_SIZE;

        // Create a pointer we'll use to get at the table once we've mapped it...
        uint64_t * const temp_mapped_pdpt = (uint64_t*)(pdpt_temp_vaddr + pdpt_phys_addr_offs);

        // Okay, good - do the mapping & flush TLB.
        map_readwrite_and_flush_offs(pml4, 1, pdpt_phys_base_addr, pdpt_temp_vaddr, pdpt_phys_addr_offs);

        // Right, now we have a PDPT mapped with a virtual pointer we can use it...
        uint16_t pdpte = vmm_virt_to_pdpt_index(temp_map_addr);

        vdebugf("  Will try to get entry %d from the pdpt\n", pdpte);
        if (unlikely((temp_mapped_pdpt[pdpte] & PG_PRESENT) == 0)) {
            vdebugf("    PD is not present, will create PD and PT\n");

            // We need to create a PD and a PT, let's do that. We'll use the
            // next 512GiB of userspace as our temporary mapping. This probably
            // isn't necessary, since the chances are both these physical pages
            // will be below 512GiB physical, but assuming that will be great
            // until some crazy system comes along with >512GiB physical RAM...
            //
            // So let's not assume :D
            //
            // (And yeah, we _could_ set the PDPT entry before remapping these
            // to zero them, but there's a chance that could lead to TLB poisoning
            // due to speculation, so we'll map and zero them first then hook it
            // up...)
            //
            uintptr_t pd_phys = page_alloc(physical_region);
            uintptr_t pt_phys = page_alloc(physical_region);

            // #### Handle the page directory...
            //
            // This is essentially the same as we did above... figure out the next-lowest
            // 512GiB base and the offset of the phys address, for the PD to begin with.
            uintptr_t temp_table_phys = pd_phys;
            uintptr_t temp_table_phys_base_addr = round_down_to_terapage(temp_table_phys);
            uintptr_t temp_table_phys_addr_offs = offset_in_terapage(temp_table_phys);

            // This is the base of the virtual space we're about to map...
            uintptr_t temp_table_vaddr = TERA_PAGE_SIZE * 2;

            // Create a pointer we'll use to get at the table once we've mapped it...
            uint64_t* temp_mapped_table = (uint64_t*)(temp_table_vaddr + temp_table_phys_addr_offs);

            // Do do the mapping & flush TLB.
            map_readwrite_and_flush_offs(pml4, 4, temp_table_phys_base_addr, temp_table_vaddr, temp_table_phys_addr_offs);

            // Zero the page, and map in the PT...
            for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                temp_mapped_table[i] = 0;
            }

            // Right, now we have a PD mapped with a virtual pointer we can use it...
            uint16_t pde = vmm_virt_to_pd_index(temp_map_addr);
            temp_mapped_table[pde] = vmm_phys_and_flags_to_table_entry(pt_phys, PG_PRESENT);

            // #### Handle the page table...
            //
            // This is the same thing again, but I'm not refactoring out the commonality
            // since it's easier to follow this way - it's tricky enough without trying
            // to be clever (so I'll let the compiler inject cleverness :D)
            temp_table_phys = pt_phys;
            temp_table_phys_base_addr = round_down_to_terapage(temp_table_phys);
            temp_table_phys_addr_offs = offset_in_terapage(temp_table_phys);

            // Do do the mapping & flush TLB.
            map_readwrite_and_flush_offs(pml4, 2, temp_table_phys_base_addr, temp_table_vaddr, temp_table_phys_addr_offs);

            // Just zero this one, that's all we need
            for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                temp_mapped_table[i] = 0;
            }

            // Finally, map the PD into the PDPT & invalidate
            temp_mapped_pdpt[pdpte] = vmm_phys_and_flags_to_table_entry(pd_phys, PG_PRESENT);
            cpu_invalidate_tlb_addr(temp_map_addr);

            // Remove the temp PDPT mapping in the PML4
            //
            // We're returning the one at pml4[2], so we don't clean that
            // up - that'll be handled in cleanup_cpu0_temp_page_tables later.
            pml4[1] = 0;
            cpu_invalidate_tlb_addr(pdpt_temp_vaddr + pdpt_phys_addr_offs);

            // and return the PT vaddr
            vdebugf("ensure_temp_page_tables(0x%016lx, 0x%016lx): success, return 0x%016lx\n", (uintptr_t)pml4, temp_map_addr, (uintptr_t)temp_mapped_table);
            return temp_mapped_table;
        } else {
            // So we maybe just need a PT, fine.
            //
            // We'll use the next 512GiB of userspace as our temporary mapping
            // for this one (i.e. mapping at the 1TiB mark).
            //
            vdebugf("    PD is present, checking PT\n");

            // First let's get hold of the PD

            // Figure out the next-lowest 512GiB base and the offset of the phys address...
            const uintptr_t pd_phys = vmm_table_entry_to_phys(temp_mapped_pdpt[pdpte]);
            const uintptr_t pd_phys_base_addr = round_down_to_terapage(pd_phys);
            const uintptr_t pd_phys_addr_offs = offset_in_terapage(pd_phys);

            // This is the base of the virtual space we're about to map...
            const uintptr_t pd_temp_vaddr = TERA_PAGE_SIZE * 2;

            // Create a pointer we'll use to get at the table once we've mapped it...
            uint64_t * const temp_mapped_pd = (uint64_t*)(pd_temp_vaddr + pd_phys_addr_offs);

            // Okay, good - do the mapping & flush TLB.
            map_readwrite_and_flush_offs(pml4, 2, pd_phys_base_addr, pd_temp_vaddr, pd_phys_addr_offs);

            // Right, now we have a PDPT mapped with a virtual pointer we can use it...
            uint16_t pde = vmm_virt_to_pd_index(temp_map_addr);

            vdebugf("  Will try to get entry %d from the pd\n", pde);

            if (likely((temp_mapped_pd[pde] & PG_PRESENT) == 0)) {
                vdebugf("    PT is not present, will create one\n");

                uintptr_t pt_phys = page_alloc(physical_region);

                // #### Handle the page table...
                //
                // This is the same thing again, but I'm not refactoring out the commonality
                // since it's easier to follow this way - it's tricky enough without trying
                // to be clever (so I'll let the compiler inject cleverness :D)
                const uintptr_t temp_table_phys = pt_phys;
                const uintptr_t temp_table_phys_base_addr = round_down_to_terapage(temp_table_phys);
                const uintptr_t temp_table_phys_addr_offs = offset_in_terapage(temp_table_phys);

                // This is the base of the virtual space we're about to map...
                uintptr_t temp_table_vaddr = TERA_PAGE_SIZE * 3;

                // Create a pointer we'll use to get at the table once we've mapped it...
                uint64_t* temp_mapped_table = (uint64_t*)(temp_table_vaddr + temp_table_phys_addr_offs);

                // Do do the mapping & flush TLB.
                map_readwrite_and_flush_offs(pml4, 3, temp_table_phys_base_addr, temp_table_vaddr, temp_table_phys_addr_offs);

                // Just zero this one, that's all we need
                for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) {
                    temp_mapped_table[i] = 0;
                }

                // Finally, map the PT into the PDPT & invalidate
                temp_mapped_pd[pde] = vmm_phys_and_flags_to_table_entry(pt_phys, PG_PRESENT);
                cpu_invalidate_tlb_addr(temp_map_addr);

                // Remove the temp PDPT and PD mappings in the PML4
                //
                // We're returning the one at pml4[3], so we don't clean that
                // up - that'll be handled in cleanup_cpu0_temp_page_tables later.
                pml4[1] = 0;
                cpu_invalidate_tlb_addr(pdpt_temp_vaddr + pdpt_phys_addr_offs);
                pml4[2] = 0;
                cpu_invalidate_tlb_addr(pd_temp_vaddr + pd_phys_addr_offs);

                // and return the PT vaddr
                vdebugf("ensure_temp_page_tables(0x%016lx, 0x%016lx): success, return 0x%016lx\n", (uintptr_t)pml4, temp_map_addr, (uintptr_t)temp_mapped_table);
                return temp_mapped_table;
            } else {
                // I see, we don't need anything. This is a potential worry (we might've initialised
                // things in the wrong order) but fine, we'll map a vaddr for the PT and just return
                // a pointer to it.

                vdebugf("    PT is present, will just map it temporarily\n");

                // Figure out the next-lowest 512GiB base and the offset of the phys address...
                const uintptr_t pt_phys = vmm_table_entry_to_phys(temp_mapped_pd[pde]);
                const uintptr_t pt_phys_base_addr = round_down_to_terapage(pt_phys);
                const uintptr_t pt_phys_addr_offs = offset_in_terapage(pt_phys);

                // This is the base of the virtual space we're about to map...
                const uintptr_t pt_temp_vaddr = TERA_PAGE_SIZE * 2;

                // Create a pointer we'll use to get at the table once we've mapped it...
                uint64_t * const temp_mapped_pt = (uint64_t*)(pt_temp_vaddr + pt_phys_addr_offs);

                // Okay, good - do the mapping & flush TLB.
                // We'll just replace the temp mapped PD at this point, we're done with it...
                map_readwrite_and_flush_offs(pml4, 2, pt_phys_base_addr, pt_temp_vaddr, pt_phys_addr_offs);

                // Remove the temp PDPT mapping in the PML4
                //
                // We're returning the one at pml4[2], so we don't clean that
                // up - that'll be handled in cleanup_cpu0_temp_page_tables later.
                pml4[1] = 0;
                cpu_invalidate_tlb_addr(pdpt_temp_vaddr + pdpt_phys_addr_offs);

                // and return the PT vaddr
                vdebugf("ensure_temp_page_tables(0x%016lx, 0x%016lx): success, return 0x%016lx\n", (uintptr_t)pml4, temp_map_addr, (uintptr_t)temp_mapped_pt);
                return temp_mapped_pt;
            }
        }
    }

    vdebugf("ensure_temp_page_tables(0x%016lx, 0x%016lx): failed, return NULL\n", (uintptr_t)pml4, temp_map_addr);
    return 0;
}

// clang-format on

static void cleanup_temp_page_tables(uint64_t *const pml4) {
    // This _could_ go about walking the tables and cleaning up exactly
    // what we ended up mapping in ensure_temp_page_tables and during
    // the direct map build itself, but since we'll do this exactly
    // once at the end of the whole direct-mapping process, let's just
    // take the easy way out and remove all the userspace mappings we
    // might've fiddled with and dump the whole TLB so we start afresh.

    pml4[1] = 0;
    pml4[2] = 0;
    pml4[3] = 0;

    // **boom**
    cpu_invalidate_tlb_all();
}

void vmm_init_direct_mapping(uint64_t *pml4, Limine_MemMap *memmap) {
    vdebugf("vmm_init_direct_mapping: init with %ld entries at pml4 0x%016lx\n", memmap->entry_count, (uintptr_t)pml4);

    uint64_t *temp_pt = ensure_temp_page_tables(pml4, vmm_per_cpu_temp_page_addr(0));

    vdebugf("ensure tables returns 0x%016lx\n", (uintptr_t)temp_pt);

    vmm_direct_mapping_terapages_used = vmm_direct_mapping_gigapages_used = vmm_direct_mapping_megapages_used =
            vmm_direct_mapping_pages_used = 0;

    for (int i = 0; i < memmap->entry_count; i++) {
        switch (memmap->entries[i]->type) {
        case LIMINE_MEMMAP_USABLE:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            vmm_init_map_region(pml4, temp_pt, memmap->entries[i], true);
            break;
        case LIMINE_MEMMAP_ACPI_NVS:
            vmm_init_map_region(pml4, temp_pt, memmap->entries[i], false);
            break;
        default:
            vdebugf("vmm_init_direct_mapping: ignored region with type %ld\n", memmap->entries[i]->type);
        }
    }

    cleanup_temp_page_tables(pml4);
}