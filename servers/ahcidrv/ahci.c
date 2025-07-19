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

static void *allocate_aligned_memory(size_t size, size_t alignment) {
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    void *addr = (void *)(AHCI_MEMORY_BASE + memory_offset);
    memory_offset += aligned_size;

    SyscallResult result = anos_map_physical(0, addr, aligned_size);
    if (result != SYSCALL_OK) {
        return nullptr;
    }

    memset(addr, 0, aligned_size);
    return addr;
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

bool ahci_controller_init(AHCIController *ctrl, uint64_t pci_base) {
    if (!ctrl) {
        return false;
    }

    memset(ctrl, 0, sizeof(AHCIController));
    ctrl->pci_base = pci_base;

    ctrl->mapped_size = 0x1000;

#ifdef DEBUG_AHCI_INIT
    printf("Mapping AHCI registers at 0x%016lx\n", pci_base);
#endif

    SyscallResult result = anos_map_physical(pci_base, (void *)0xB000000000ULL,
                                             ctrl->mapped_size);
    if (result != SYSCALL_OK) {
        printf("Failed to map AHCI registers! Error: %d\n", result);
        return false;
    }

    ctrl->mapped_regs = (void *)0xB000000000ULL;
    ctrl->regs = (AHCIRegs *)ctrl->mapped_regs;

#ifdef DEBUG_AHCI_INIT
    printf("AHCI registers mapped successfully\n");
    printf("  CAP: 0x%08x\n", ctrl->regs->host.cap);
    printf("  GHC: 0x%08x\n", ctrl->regs->host.ghc);
    printf("  IS: 0x%08x\n", ctrl->regs->host.is);
    printf("  PI: 0x%08x\n", ctrl->regs->host.pi);
    printf("  VS: 0x%08x\n", ctrl->regs->host.vs);
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

    uint32_t ssts = port_regs->ssts;
    if ((ssts & AHCI_PORT_SSTS_DET_MASK) != AHCI_PORT_SSTS_DET_PRESENT) {
#ifdef DEBUG_AHCI_INIT
        printf("Port %u: No device detected (SSTS=0x%08x)\n", port_num, ssts);
#endif
        return false;
    }

#ifdef DEBUG_AHCI_INIT
    printf("Port %u: Device detected (SSTS=0x%08x)\n", port_num, ssts);
#endif

    ahci_port_stop(port_regs);

    port->cmd_list = allocate_aligned_memory(CMD_LIST_SIZE, 1024);
    if (!port->cmd_list) {
        printf("Failed to allocate command list for port %u\n", port_num);
        return false;
    }

    port->fis_base = allocate_aligned_memory(FIS_SIZE, 256);
    if (!port->fis_base) {
        printf("Failed to allocate FIS base for port %u\n", port_num);
        return false;
    }

    port->cmd_tables = allocate_aligned_memory(CMD_TABLE_SIZE * 32, 128);
    if (!port->cmd_tables) {
        printf("Failed to allocate command tables for port %u\n", port_num);
        return false;
    }

    uint64_t cmd_list_phys = (uint64_t)port->cmd_list;
    uint64_t fis_base_phys = (uint64_t)port->fis_base;

    port_regs->clb = (uint32_t)(cmd_list_phys & 0xFFFFFFFF);
    port_regs->clbu = (uint32_t)(cmd_list_phys >> 32);
    port_regs->fb = (uint32_t)(fis_base_phys & 0xFFFFFFFF);
    port_regs->fbu = (uint32_t)(fis_base_phys >> 32);

    AHCICmdHeader *cmd_headers = (AHCICmdHeader *)port->cmd_list;
    for (int i = 0; i < 32; i++) {
        uint64_t cmd_table_phys =
                (uint64_t)port->cmd_tables + (i * CMD_TABLE_SIZE);
        cmd_headers[i].ctba = (uint32_t)(cmd_table_phys & 0xFFFFFFFF);
        cmd_headers[i].ctbau = (uint32_t)(cmd_table_phys >> 32);
    }

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

static bool ahci_wait_for_completion(AHCIPort *port, uint8_t slot) {
    AHCIPortRegs *port_regs = &port->controller->regs->ports[port->port_num];

    int timeout = 10000;
    while (timeout > 0) {
        if ((port_regs->ci & (1 << slot)) == 0) {
            return true;
        }

        if (port_regs->is & (1 << 30)) {
            printf("Port %u: Task file error\n", port->port_num);
            return false;
        }

        anos_task_sleep_current(100);
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

    void *identify_buffer = allocate_aligned_memory(512, 512);
    if (!identify_buffer) {
        printf("Failed to allocate identify buffer\n");
        return false;
    }

    memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4;
    cmd_header->prdtl = 1;

    FISRegH2D *fis = (FISRegH2D *)cmd_table->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->flags = 0x80;
    fis->command = ATA_CMD_IDENTIFY;
    fis->device = 0;

    cmd_table->prdt[0].dba = (uint32_t)((uint64_t)identify_buffer & 0xFFFFFFFF);
    cmd_table->prdt[0].dbau = (uint32_t)((uint64_t)identify_buffer >> 32);
    cmd_table->prdt[0].dbc = 511;
    cmd_table->prdt[0].i = 1;

    port_regs->ci = 1;

    if (!ahci_wait_for_completion(port, 0)) {
        printf("Port %u: IDENTIFY command failed\n", port->port_num);
        return false;
    }

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

bool ahci_port_read(AHCIPort *port, uint64_t lba, uint16_t count,
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

bool ahci_port_write(AHCIPort *port, uint64_t lba, uint16_t count,
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