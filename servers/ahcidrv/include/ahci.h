/*
 * ahcidrv - AHCI driver core definitions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_AHCIDRV_AHCI_H
#define __ANOS_AHCIDRV_AHCI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AHCI_VENDOR_ID_INTEL 0x8086
#define AHCI_DEVICE_ID_ICH9 0x2922

#define AHCI_CLASS_CODE 0x01
#define AHCI_SUBCLASS 0x06
#define AHCI_PROG_IF 0x01

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_ports;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
} __attribute__((packed)) AHCIHostRegs;

typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed)) AHCIPortRegs;

typedef struct {
    AHCIHostRegs host;
    AHCIPortRegs ports[32];
} __attribute__((packed)) AHCIRegs;

typedef struct {
    uint8_t cfl : 5;
    uint8_t a : 1;
    uint8_t w : 1;
    uint8_t p : 1;
    uint8_t r : 1;
    uint8_t b : 1;
    uint8_t c : 1;
    uint8_t reserved0 : 1;
    uint8_t pmp : 4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved1[4];
} __attribute__((packed)) AHCICmdHeader;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc : 22;
    uint32_t reserved2 : 9;
    uint32_t i : 1;
} __attribute__((packed)) AHCIPRDEntry;

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    AHCIPRDEntry prdt[1];
} __attribute__((packed)) AHCICmdTable;

typedef struct {
    uint8_t fis_type;
    uint8_t flags;
    uint8_t command;
    uint8_t features;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t features_exp;
    uint8_t count;
    uint8_t count_exp;
    uint8_t reserved[2];
    uint32_t reserved2;
} __attribute__((packed)) FISRegH2D;

#define FIS_TYPE_REG_H2D 0x27
#define FIS_TYPE_REG_D2H 0x34
#define FIS_TYPE_DMA_ACT 0x39
#define FIS_TYPE_DMA_SETUP 0x41
#define FIS_TYPE_DATA 0x46
#define FIS_TYPE_BIST 0x58
#define FIS_TYPE_PIO_SETUP 0x5F
#define FIS_TYPE_DEV_BITS 0xA1

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define AHCI_GHC_AHCI_ENABLE (1 << 31)
#define AHCI_GHC_RESET (1 << 0)

#define AHCI_PORT_CMD_START (1 << 0)
#define AHCI_PORT_CMD_FRE (1 << 4)
#define AHCI_PORT_CMD_FR (1 << 14)
#define AHCI_PORT_CMD_CR (1 << 15)

#define AHCI_PORT_SSTS_DET_MASK 0xF
#define AHCI_PORT_SSTS_DET_PRESENT 0x3

// AHCI device signature values
#define AHCI_SIG_ATA 0x00000101   // SATA drive
#define AHCI_SIG_ATAPI 0xEB140101 // SATAPI drive (CD/DVD)
#define AHCI_SIG_SEMB 0xC33C0101  // Enclosure management bridge
#define AHCI_SIG_PM 0x96690101    // Port multiplier

typedef struct {
    uint64_t pci_base;
    void *mapped_regs;
    size_t mapped_size;
    AHCIRegs *regs;
    uint32_t port_count;
    uint32_t active_ports;
    bool initialized;
} AHCIController;

typedef struct {
    AHCIController *controller;
    uint8_t port_num;
    bool connected;
    bool initialized;
    uint64_t sector_count;
    uint16_t sector_size;
    void *cmd_list;
    void *fis_base;
    void *cmd_tables;
} AHCIPort;

bool ahci_controller_init(AHCIController *ctrl, uint64_t pci_base);
void ahci_controller_cleanup(AHCIController *ctrl);
bool ahci_port_init(AHCIPort *port, AHCIController *ctrl, uint8_t port_num);
bool ahci_port_identify(AHCIPort *port);
bool ahci_port_read(AHCIPort *port, uint64_t lba, uint16_t count, void *buffer);
bool ahci_port_write(AHCIPort *port, uint64_t lba, uint16_t count,
                     const void *buffer);

#endif