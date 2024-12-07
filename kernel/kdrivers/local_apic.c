/*
 * stage3 - Local APIC Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "kdrivers/local_apic.h"
#include "acpitables.h"
#include "debugprint.h"
#include "kdrivers/drivers.h"
#include "machine.h"
#include "printhex.h"
#include "vmm/vmmapper.h"

void init_local_apic(BIOS_SDTHeader *madt) {
  debugstr("Init local APICs...\n");

  uint32_t *lapic_addr = ((uint32_t *)(madt + 1));
  uint32_t *flags = lapic_addr + 1;
  debugstr("LAPIC address (phys : virt) = ");
  printhex32(*lapic_addr, debugchar);
  debugstr(" : 0xFFFF800000000000 [");
  printhex32(*flags, debugchar);
  debugstr("]\n");

  vmm_map_page(STATIC_PML4, KERNEL_HARDWARE_VADDR_BASE, *lapic_addr,
               PRESENT | WRITE);

  uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);

  debugstr("LAPIC ID: ");
  printhex32(*LAPIC_ID(lapic), debugchar);
  debugstr("; Version: ");
  printhex32(*LAPIC_VERSION(lapic), debugchar);
  debugstr("\n");

  // Set spurious interrupt and enable
  *(lapic + REG_LAPIC_SPURIOUS_O) = 0x1FF;

  // Set up timer
  *LAPIC_DIVIDE(lapic) = 0x03;            // /16 mode
  *LAPIC_INITIAL_COUNT(lapic) = 20000000; // 20000000 init count
  *LAPIC_LVT_TIMER(lapic) = 0x20020;
}

void local_apic_eoe() {
  uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);
  *(lapic + 44) = 0;
}
