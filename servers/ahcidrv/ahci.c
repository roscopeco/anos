/*
 * ahcidrv - AHCI driver implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anos/syscalls.h>

#include "ahci.h"

#define AHCI_MEMORY_BASE 0xA000000000ULL
#define CMD_LIST_SIZE 1024
#define FIS_SIZE 256
#define CMD_TABLE_SIZE 128

static uint64_t memory_offset = 0;

static void *allocate_aligned_memory(const size_t size, const size_t alignment,
                                     uint64_t *phys_addr_out) {
    const size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Round up to page size for physical allocation
    const size_t page_aligned_size = (aligned_size + 0xFFF) & ~0xFFF;

    // Allocate physical pages
    const uint64_t phys_addr = anos_alloc_physical_pages(page_aligned_size);
    if (phys_addr == 0) {
        return nullptr;
    }

    // Map the physical pages to virtual memory
    void *virt_addr = (void *)(AHCI_MEMORY_BASE + memory_offset);
    memory_offset += page_aligned_size;

    SyscallResult result = anos_map_physical(
            phys_addr, virt_addr, page_aligned_size,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    if (result != SYSCALL_OK) {
        // TODO: Free the physical pages on error
        return nullptr;
    }

    // Return both virtual and physical addresses
    if (phys_addr_out) {
        *phys_addr_out = phys_addr;
    }

    memset(virt_addr, 0, aligned_size);
    return virt_addr;
}

static void ahci_port_stop(AHCIPortRegs *port) {
    port->cmd &= ~AHCI_PORT_CMD_START;
    port->cmd &= ~AHCI_PORT_CMD_FRE;

    while ((port->cmd & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_CR)) != 0) {
        anos_task_sleep_current(10000);
    }
}

static void ahci_port_start(AHCIPortRegs *port) {
    while (port->cmd & AHCI_PORT_CMD_CR) {
        anos_task_sleep_current(1000);
    }

    port->cmd |= AHCI_PORT_CMD_FRE;
    port->cmd |= AHCI_PORT_CMD_START;
}

static void ahci_reset_controller(AHCIRegs *regs) {
#ifdef DEBUG_AHCI_INIT
    printf("Resetting AHCI controller...\n");
#endif

    regs->host.ghc |= AHCI_GHC_RESET;

    int timeout = 1000;
    while ((regs->host.ghc & AHCI_GHC_RESET) && timeout > 0) {
        anos_task_sleep_current(1000);
        timeout--;
    }

    if (timeout == 0) {
        printf("Warning: AHCI controller reset timeout\n");
    }

    regs->host.ghc |= AHCI_GHC_AHCI_ENABLE;

#ifdef DEBUG_AHCI_INIT
    printf("AHCI controller reset complete\n");
#endif
}

bool ahci_controller_init(AHCIController *ctrl, const uint64_t pci_base) {
    if (!ctrl) {
        return false;
    }

    memset(ctrl, 0, sizeof(AHCIController));
    ctrl->pci_base = pci_base;

    ctrl->mapped_size = 0x1000;

#ifdef DEBUG_AHCI_INIT
    printf("Mapping AHCI registers: phys=0x%016lx -> virt=0x%016llx "
           "(size=0x%lx)\n",
           pci_base, 0xB000000000ULL, ctrl->mapped_size);
#endif

    const SyscallResult result = anos_map_physical(
            pci_base, (void *)0xB000000000ULL, ctrl->mapped_size,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    if (result != SYSCALL_OK) {
        printf("FAILED to map AHCI registers! Error code: %d\n", result);
        printf("  Attempted mapping: phys=0x%016lx -> virt=0x%016llx "
               "(size=0x%lx)\n",
               pci_base, 0xB000000000ULL, ctrl->mapped_size);
        return false;
    }

    ctrl->mapped_regs = (void *)0xB000000000ULL;
    ctrl->regs = (AHCIRegs *)ctrl->mapped_regs;

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    printf("AHCI structure sizes:\n");
    printf("  sizeof(AHCIHostRegs): %zu (should be 256)\n",
           sizeof(AHCIHostRegs));
    printf("  sizeof(AHCIPortRegs): %zu (should be 128)\n",
           sizeof(AHCIPortRegs));
    printf("  sizeof(AHCIRegs): %zu\n", sizeof(AHCIRegs));
    printf("  Port 0 offset: 0x%lx (should be 0x100)\n",
           (uintptr_t)&ctrl->regs->ports[0] - (uintptr_t)ctrl->regs);

    printf("AHCI registers mapped successfully\n");
    printf("  CAP: 0x%08x\n", ctrl->regs->host.cap);
    printf("  GHC: 0x%08x\n", ctrl->regs->host.ghc);
    printf("  IS: 0x%08x\n", ctrl->regs->host.is);
    printf("  PI: 0x%08x\n", ctrl->regs->host.pi);
    printf("  VS: 0x%08x\n", ctrl->regs->host.vs);
#endif
    // Verify mapping by checking if we get sane values
    if (ctrl->regs->host.cap == 0xffffffff || ctrl->regs->host.cap == 0) {
        printf("WARNING: CAP register looks invalid (0x%08x) - mapping may be "
               "wrong!\n",
               ctrl->regs->host.cap);
    }

#ifdef VERY_NOISY_AHCI_INIT
    // Test reading from different offsets to verify mapping
    volatile uint32_t *test_ptr = (volatile uint32_t *)ctrl->mapped_regs;
    printf("Raw register reads: [0]=0x%08x [1]=0x%08x [2]=0x%08x [3]=0x%08x\n",
           test_ptr[0], test_ptr[1], test_ptr[2], test_ptr[3]);
#endif
#endif
    ahci_reset_controller(ctrl->regs);

    ctrl->port_count = (ctrl->regs->host.cap & 0x1F) + 1;
    ctrl->active_ports = ctrl->regs->host.pi;

    if (ctrl->port_count > 32) {
        ctrl->port_count = 32;
    }

#ifdef DEBUG_AHCI_INIT
    printf("Controller supports %u ports, active mask: 0x%08x\n",
           ctrl->port_count, ctrl->active_ports);
#endif

    ctrl->initialized = true;
    return true;
}

void ahci_controller_cleanup(AHCIController *ctrl) {
    if (!ctrl || !ctrl->initialized) {
        return;
    }

    for (uint8_t i = 0; i < ctrl->port_count; i++) {
        if (ctrl->active_ports & (1 << i)) {
            ahci_port_stop(&ctrl->regs->ports[i]);
        }
    }

    ctrl->initialized = false;
}

bool ahci_port_init(AHCIPort *port, AHCIController *ctrl, uint8_t port_num) {
    if (!port || !ctrl || !ctrl->initialized || port_num >= ctrl->port_count) {
        return false;
    }

    memset(port, 0, sizeof(AHCIPort));
    port->controller = ctrl;
    port->port_num = port_num;

    AHCIPortRegs *port_regs = &ctrl->regs->ports[port_num];

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    // Debug: verify port register access
    printf("Port %u register access debug:\n", port_num);
    printf("  ctrl->regs = %p\n", (void *)ctrl->regs);
    printf("  port_regs = %p\n", (void *)port_regs);
    printf("  Expected offset from base: 0x%lx\n",
           (uintptr_t)port_regs - (uintptr_t)ctrl->regs);

    // Test raw access to port registers
    volatile uint32_t *port_raw = (volatile uint32_t *)port_regs;
    printf("  Raw port reads: [0]=0x%08x [1]=0x%08x [2]=0x%08x [9]=0x%08x "
           "[10]=0x%08x\n",
           port_raw[0], port_raw[1], port_raw[2], port_raw[9], port_raw[10]);
    printf("  Structured reads: CLB=0x%08x CLBU=0x%08x TFD=0x%08x SIG=0x%08x "
           "SSTS=0x%08x\n",
           port_regs->clb, port_regs->clbu, port_regs->tfd, port_regs->sig,
           port_regs->ssts);
#endif
#endif

    uint32_t ssts = port_regs->ssts;
    if ((ssts & AHCI_PORT_SSTS_DET_MASK) != AHCI_PORT_SSTS_DET_PRESENT) {
#ifdef DEBUG_AHCI_INIT
        printf("Port %u: No device detected (SSTS=0x%08x)\n", port_num, ssts);
#endif
        return false;
    }

    // Check device signature (QEMU reports 0xffffffff, which is a known limitation)
    uint32_t sig = port_regs->sig;
    const char *device_type = "SATA drive (assumed)";

    if (sig != 0xffffffff && sig != 0x00000000) {
        switch (sig) {
        case AHCI_SIG_ATA:
            device_type = "SATA drive";
            break;
        case AHCI_SIG_ATAPI:
            device_type = "SATAPI drive";
            break;
        case AHCI_SIG_SEMB:
            device_type = "Enclosure management";
            break;
        case AHCI_SIG_PM:
            device_type = "Port multiplier";
            break;
        default:
            device_type = "Unknown device";
            break;
        }
    } else {
        printf("Port %u: Signature register invalid (0x%08x) - assuming SATA "
               "drive\n",
               port_num, sig);
    }

#ifdef DEBUG_AHCI_INIT
    printf("Port %u: Device detected - %s (SSTS=0x%08x, SIG=0x%08x)\n",
           port_num, device_type, ssts, sig);
#endif

    ahci_port_stop(port_regs);

    uint64_t cmd_list_phys;
    port->cmd_list =
            allocate_aligned_memory(CMD_LIST_SIZE, 1024, &cmd_list_phys);
    if (!port->cmd_list) {
        printf("Failed to allocate command list for port %u\n", port_num);
        return false;
    }

    uint64_t fis_base_phys;
    port->fis_base = allocate_aligned_memory(FIS_SIZE, 256, &fis_base_phys);
    if (!port->fis_base) {
        printf("Failed to allocate FIS base for port %u\n", port_num);
        return false;
    }

    uint64_t cmd_tables_phys;
    port->cmd_tables =
            allocate_aligned_memory(CMD_TABLE_SIZE * 32, 128, &cmd_tables_phys);
    if (!port->cmd_tables) {
        printf("Failed to allocate command tables for port %u\n", port_num);
        return false;
    }

    port_regs->clb = (uint32_t)(cmd_list_phys & 0xFFFFFFFF);
    port_regs->clbu = (uint32_t)(cmd_list_phys >> 32);
    port_regs->fb = (uint32_t)(fis_base_phys & 0xFFFFFFFF);
    port_regs->fbu = (uint32_t)(fis_base_phys >> 32);

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    printf("Port %u DMA addresses:\n", port_num);
    printf("  Command List: virt=%p phys=0x%016lx -> CLB=0x%08x:0x%08x\n",
           port->cmd_list, cmd_list_phys, port_regs->clbu, port_regs->clb);
    printf("  FIS Base: virt=%p phys=0x%016lx -> FB=0x%08x:0x%08x\n",
           port->fis_base, fis_base_phys, port_regs->fbu, port_regs->fb);
    printf("  Command Tables: virt=%p phys=0x%016lx\n", port->cmd_tables,
           cmd_tables_phys);

#endif
#endif

    AHCICmdHeader *cmd_headers = (AHCICmdHeader *)port->cmd_list;

    for (int i = 0; i < 32; i++) {
        const uint64_t cmd_table_phys = cmd_tables_phys + (i * CMD_TABLE_SIZE);
        cmd_headers[i].ctba = (uint32_t)(cmd_table_phys & 0xFFFFFFFF);
        cmd_headers[i].ctbau = (uint32_t)(cmd_table_phys >> 32);
    }

#ifdef DEBUG_AHCI_INIT
    // Verify we can actually access these buffers AFTER setup
    printf("  Testing DMA buffer access...\n");
    printf("    Command header test: CTBA=0x%08x CTBAU=0x%08x\n",
           cmd_headers[0].ctba, cmd_headers[0].ctbau);

    const AHCICmdTable *test_table = (AHCICmdTable *)port->cmd_tables;
    printf("    Command table test: FIS[0]=0x%02x FIS[1]=0x%02x\n",
           test_table->cfis[0], test_table->cfis[1]);

    // Test reading the physical addresses we think we allocated
    printf("    Physical addr test: cmd_tables_phys=0x%016lx\n",
           cmd_tables_phys);
    printf("    Slot 0 table phys: 0x%016lx\n",
           cmd_tables_phys + (0 * CMD_TABLE_SIZE));
#endif

    port_regs->serr = 0xFFFFFFFF;
    port_regs->is = 0xFFFFFFFF;
    port_regs->ie = 0xFFFFFFFF;

    ahci_port_start(port_regs);

    port->connected = true;
    port->initialized = true;

#ifdef DEBUG_AHCI_INIT
    printf("Port %u initialized successfully\n", port_num);
#endif

    return true;
}

static bool ahci_wait_for_completion(const AHCIPort *port, const uint8_t slot) {
    const AHCIPortRegs *port_regs =
            &port->controller->regs->ports[port->port_num];

    int timeout = 300; // 30ms total timeout (300 * 100μs)
    while (timeout > 0) {
        if ((port_regs->ci & (1 << slot)) == 0) {
            return true;
        }

        if (port_regs->is & (1 << 30)) {
            printf("Port %u: Task file error\n", port->port_num);
            return false;
        }

        anos_task_sleep_current(100000); // 100μs sleep
        timeout--;
    }

    printf("Port %u: Command timeout on slot %u\n", port->port_num, slot);
    return false;
}

bool ahci_port_identify(AHCIPort *port) {
    if (!port || !port->initialized) {
        return false;
    }

    AHCIPortRegs *port_regs = &port->controller->regs->ports[port->port_num];
    AHCICmdHeader *cmd_header = (AHCICmdHeader *)port->cmd_list;
    AHCICmdTable *cmd_table = (AHCICmdTable *)port->cmd_tables;

    uint64_t identify_buffer_phys;
    void *identify_buffer =
            allocate_aligned_memory(512, 512, &identify_buffer_phys);
    if (!identify_buffer) {
        printf("Failed to allocate identify buffer\n");
        return false;
    }

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    printf("IDENTIFY setup: cmd_list=%p cmd_table=%p buffer=%p "
           "(phys=0x%016lx)\n",
           port->cmd_list, port->cmd_tables, identify_buffer,
           identify_buffer_phys);
    printf("Port registers: CLB=0x%08x:0x%08x FB=0x%08x:0x%08x\n",
           port_regs->clbu, port_regs->clb, port_regs->fbu, port_regs->fb);
#endif
#endif

    memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4; // Should be 5 (20 bytes / 4)
    cmd_header->prdtl = 1;                   // 1 PRDT entry
    cmd_header->a = 0;                       // Not ATAPI
    cmd_header->w = 0;                       // Read operation
    cmd_header->p = 0;                       // No prefetch
    cmd_header->r = 0;                       // Not reset
    cmd_header->b = 0;                       // Not BIST
    cmd_header->c = 0;                       // Not clear busy upon R_OK

    printf("Command header: CFL=%u PRDTL=%u\n", cmd_header->cfl,
           cmd_header->prdtl);

    FISRegH2D *fis = (FISRegH2D *)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->flags = 0x80; // C bit (Command bit) = 1, PM Port = 0
    fis->command = ATA_CMD_IDENTIFY;
    fis->device = 0xa0; // Try 0xa0 since device responds with this
    fis->lba0 = 0;
    fis->lba1 = 0;
    fis->lba2 = 0;
    fis->lba3 = 0;
    fis->lba4 = 0;
    fis->lba5 = 0;
    fis->count = 0;
    fis->count_exp = 0;
    // fis->control = 0;  // Not in our FIS structure

    printf("H2D FIS setup: type=0x%02x flags=0x%02x cmd=0x%02x dev=0x%02x\n",
           fis->fis_type, fis->flags, fis->command, fis->device);

    // Show the raw FIS bytes we're sending
    uint8_t *fis_bytes = (uint8_t *)fis;
    printf("Raw H2D FIS: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           fis_bytes[0], fis_bytes[1], fis_bytes[2], fis_bytes[3], fis_bytes[4],
           fis_bytes[5], fis_bytes[6], fis_bytes[7]);

    cmd_table->prdt[0].dba = (uint32_t)(identify_buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt[0].dbau = (uint32_t)(identify_buffer_phys >> 32);
    cmd_table->prdt[0].dbc = 511; // 512 bytes - 1 (0-based count)
    cmd_table->prdt[0].i = 1;

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    printf("PRDT[0]: DBA=0x%08x DBAU=0x%08x DBC=%u (phys=0x%016lx)\n",
           cmd_table->prdt[0].dba, cmd_table->prdt[0].dbau,
           cmd_table->prdt[0].dbc, identify_buffer_phys);
#endif
#endif

    // Debug: Check port status before issuing command
    printf("Port %u pre-command status:\n", port->port_num);
    printf("  TFD: 0x%08x\n", port_regs->tfd);
    printf("  SSTS: 0x%08x\n", port_regs->ssts);
    printf("  SERR: 0x%08x\n", port_regs->serr);
    printf("  IS: 0x%08x\n", port_regs->is);
    printf("  CI: 0x%08x\n", port_regs->ci);
    printf("  CMD: 0x%08x\n", port_regs->cmd);

    port_regs->ci = 1;

    printf("Port %u post-command issue:\n", port->port_num);
    printf("  CI: 0x%08x\n", port_regs->ci);

    if (!ahci_wait_for_completion(port, 0)) {
        printf("Port %u: IDENTIFY command timeout\n", port->port_num);
        printf("Final port status:\n");
        printf("  TFD: 0x%08x\n", port_regs->tfd);
        printf("  SERR: 0x%08x\n", port_regs->serr);
        printf("  IS: 0x%08x\n", port_regs->is);
        printf("  CI: 0x%08x\n", port_regs->ci);

        // Check if device actually completed but we missed it due to interrupt handling
        if (port_regs->is & 0x1) {
            printf("  -> Device sent D2H FIS (IS bit 0 set), trying manual "
                   "completion...\n");
            // Clear the interrupt status
            port_regs->is = 0x1;
            printf("  -> Cleared IS, now CI=0x%08x\n", port_regs->ci);
            // Check if CI cleared after interrupt handling
            if ((port_regs->ci & 0x1) == 0) {
                printf("  -> Command actually completed! Continuing...\n");
                goto command_completed;
            } else {
                printf("  -> CI still set (0x%08x), command may have failed\n",
                       port_regs->ci);
                // Check for errors in the received FIS or TFD
                printf("  -> TFD after IS clear: 0x%08x\n", port_regs->tfd);
                if (port_regs->tfd & 0x1) {
                    printf("  -> Device reports ERROR bit set in TFD\n");
                }

                // Let's see what FIS the device actually sent us
                printf("  -> Examining received FIS at %p:\n", port->fis_base);
                uint8_t *fis_area = (uint8_t *)port->fis_base;
                printf("     D2H FIS: %02x %02x %02x %02x %02x %02x %02x "
                       "%02x\n",
                       fis_area[0x40], fis_area[0x41], fis_area[0x42],
                       fis_area[0x43], fis_area[0x44], fis_area[0x45],
                       fis_area[0x46], fis_area[0x47]);
                printf("     SDB FIS: %02x %02x %02x %02x %02x %02x %02x "
                       "%02x\n",
                       fis_area[0x58], fis_area[0x59], fis_area[0x5A],
                       fis_area[0x5B], fis_area[0x5C], fis_area[0x5D],
                       fis_area[0x5E], fis_area[0x5F]);
            }
        }
        return false;
    }

command_completed:

    uint16_t *id_data = (uint16_t *)identify_buffer;

    port->sector_count = ((uint64_t)id_data[103] << 48) |
                         ((uint64_t)id_data[102] << 32) |
                         ((uint64_t)id_data[101] << 16) | id_data[100];

    if (port->sector_count == 0) {
        port->sector_count = ((uint32_t)id_data[61] << 16) | id_data[60];
    }

    port->sector_size = 512;

#ifdef DEBUG_AHCI_INIT
    printf("Port %u: Device identification complete\n", port->port_num);
    printf("  Sectors: %lu\n", port->sector_count);
    printf("  Sector size: %u bytes\n", port->sector_size);
    printf("  Capacity: %lu MB\n",
           (port->sector_count * port->sector_size) / (1024 * 1024));
#endif

    return true;
}

bool ahci_port_read(AHCIPort *port, const uint64_t lba, const uint16_t count,
                    void *buffer) {

    if (!port || !port->initialized || !buffer || count == 0) {
        return false;
    }

    AHCIPortRegs *port_regs = &port->controller->regs->ports[port->port_num];
    AHCICmdHeader *cmd_header = (AHCICmdHeader *)port->cmd_list;
    AHCICmdTable *cmd_table = (AHCICmdTable *)port->cmd_tables;

    memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4;
    cmd_header->prdtl = 1;

    FISRegH2D *fis = (FISRegH2D *)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->flags = 0x80;
    fis->command = ATA_CMD_READ_DMA_EX;
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->lba4 = (lba >> 32) & 0xFF;
    fis->lba5 = (lba >> 40) & 0xFF;
    fis->device = 0x40;
    fis->count = count & 0xFF;
    fis->count_exp = (count >> 8) & 0xFF;

    cmd_table->prdt[0].dba = (uint32_t)((uint64_t)buffer & 0xFFFFFFFF);
    cmd_table->prdt[0].dbau = (uint32_t)((uint64_t)buffer >> 32);
    cmd_table->prdt[0].dbc = (count * port->sector_size) - 1;
    cmd_table->prdt[0].i = 1;

    port_regs->ci = 1;

    return ahci_wait_for_completion(port, 0);
}

bool ahci_port_write(AHCIPort *port, const uint64_t lba, const uint16_t count,
                     const void *buffer) {
    if (!port || !port->initialized || !buffer || count == 0) {
        return false;
    }

    AHCIPortRegs *port_regs = &port->controller->regs->ports[port->port_num];
    AHCICmdHeader *cmd_header = (AHCICmdHeader *)port->cmd_list;
    AHCICmdTable *cmd_table = (AHCICmdTable *)port->cmd_tables;

    memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4;
    cmd_header->w = 1;
    cmd_header->prdtl = 1;

    FISRegH2D *fis = (FISRegH2D *)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->flags = 0x80;
    fis->command = ATA_CMD_WRITE_DMA_EX;
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->lba4 = (lba >> 32) & 0xFF;
    fis->lba5 = (lba >> 40) & 0xFF;
    fis->device = 0x40;
    fis->count = count & 0xFF;
    fis->count_exp = (count >> 8) & 0xFF;

    cmd_table->prdt[0].dba = (uint32_t)((uint64_t)buffer & 0xFFFFFFFF);
    cmd_table->prdt[0].dbau = (uint32_t)((uint64_t)buffer >> 32);
    cmd_table->prdt[0].dbc = (count * port->sector_size) - 1;
    cmd_table->prdt[0].i = 1;

    port_regs->ci = 1;

    return ahci_wait_for_completion(port, 0);
}