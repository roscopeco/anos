/*
 * xHCI (USB 3.0) Controller Driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_XHCI_H
#define __ANOS_XHCI_H

#include <stdbool.h>
#include <stdint.h>

// xHCI Register Offsets
#define XHCI_CAP_CAPLENGTH 0x00
#define XHCI_CAP_HCIVERSION 0x02
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCSPARAMS2 0x08
#define XHCI_CAP_HCSPARAMS3 0x0C
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF 0x14
#define XHCI_CAP_RTSOFF 0x18
#define XHCI_CAP_HCCPARAMS2 0x1C

// Operational Registers (offset by CAPLENGTH)
#define XHCI_OP_USBCMD 0x00
#define XHCI_OP_USBSTS 0x04
#define XHCI_OP_PAGESIZE 0x08
#define XHCI_OP_DNCTRL 0x14
#define XHCI_OP_CRCR 0x18
#define XHCI_OP_DCBAAP 0x30
#define XHCI_OP_CONFIG 0x38

// USB Command Register bits
#define XHCI_CMD_RUN (1 << 0)
#define XHCI_CMD_RESET (1 << 1)
#define XHCI_CMD_INTE (1 << 2)
#define XHCI_CMD_HSEE (1 << 3)

// USB Status Register bits
#define XHCI_STS_HCH (1 << 0)
#define XHCI_STS_HSE (1 << 2)
#define XHCI_STS_EINT (1 << 3)
#define XHCI_STS_PCD (1 << 4)
#define XHCI_STS_SSS (1 << 8)
#define XHCI_STS_RSS (1 << 9)
#define XHCI_STS_SRE (1 << 10)
#define XHCI_STS_CNR (1 << 11)
#define XHCI_STS_HCE (1 << 12)

// Port Register Offsets (from OP base + 0x400)
#define XHCI_PORT_SC 0x00
#define XHCI_PORT_PMSC 0x04
#define XHCI_PORT_LI 0x08
#define XHCI_PORT_HLPMC 0x0C

// Port Status and Control bits
#define XHCI_PORTSC_CCS (1 << 0)
#define XHCI_PORTSC_PED (1 << 1)
#define XHCI_PORTSC_OCA (1 << 3)
#define XHCI_PORTSC_PR (1 << 4)
#define XHCI_PORTSC_PLS_MASK (0xF << 5)
#define XHCI_PORTSC_PP (1 << 9)
#define XHCI_PORTSC_SPEED_MASK (0xF << 10)
#define XHCI_PORTSC_LWS (1 << 16)
#define XHCI_PORTSC_CSC (1 << 17)
#define XHCI_PORTSC_PEC (1 << 18)
#define XHCI_PORTSC_WRC (1 << 19)
#define XHCI_PORTSC_OCC (1 << 20)
#define XHCI_PORTSC_PRC (1 << 21)
#define XHCI_PORTSC_PLC (1 << 22)
#define XHCI_PORTSC_CEC (1 << 23)
#define XHCI_PORTSC_CAS (1 << 24)
#define XHCI_PORTSC_WCE (1 << 25)
#define XHCI_PORTSC_WDE (1 << 26)
#define XHCI_PORTSC_WOE (1 << 27)
#define XHCI_PORTSC_DR (1 << 30)
#define XHCI_PORTSC_WPR (1U << 31)

// Port speeds
#define XHCI_SPEED_FULL 1
#define XHCI_SPEED_LOW 2
#define XHCI_SPEED_HIGH 3
#define XHCI_SPEED_SUPER 4

// Runtime Interrupter Register Offsets (from interrupter base)
#define XHCI_IMAN 0x00      // Interrupt Management
#define XHCI_IMOD 0x04      // Interrupt Moderation
#define XHCI_ERSTSZ 0x08    // Event Ring Segment Table Size
#define XHCI_RSVD 0x0C      // Reserved
#define XHCI_ERSTBA_LO 0x10 // Event Ring Segment Table Base Address (Low)
#define XHCI_ERSTBA_HI 0x14 // Event Ring Segment Table Base Address (High)
#define XHCI_ERDP_LO 0x18   // Event Ring Dequeue Pointer (Low)
#define XHCI_ERDP_HI 0x1C   // Event Ring Dequeue Pointer (High)

// xHCI Controller structure
typedef struct {
    uint64_t base_addr;           // Base address of xHCI registers
    uint64_t pci_config_base;     // PCI config space base (physical)
    uint64_t pci_config_virt;     // PCI config space base (virtual mapped)
    volatile void *cap_regs;      // Capability registers
    volatile void *op_regs;       // Operational registers
    volatile void *port_regs;     // Port registers base
    volatile void *doorbell_regs; // Doorbell registers
    volatile void *runtime_regs;  // Runtime registers

    uint8_t cap_length;       // Capability register length
    uint16_t hci_version;     // Host Controller Interface version
    uint8_t max_slots;        // Maximum device slots
    uint8_t max_interrupters; // Maximum interrupters
    uint8_t max_ports;        // Maximum root hub ports
    uint32_t page_size;       // Controller page size

    // MSI interrupt configuration
    uint8_t msi_cap_offset; // PCI MSI capability offset
    uint8_t msi_vector;     // Allocated MSI interrupt vector
    bool msi_enabled;       // MSI interrupts enabled

    bool initialized;      // Controller initialization state
    uint32_t active_ports; // Bitmask of active ports
} XHCIController;

// xHCI Port structure
typedef struct {
    XHCIController *controller;
    uint8_t port_num;
    uint8_t speed;
    bool connected;
    bool enabled;
    bool initialized;

    // Device information (if connected)
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    char manufacturer[64];
    char product[64];
    char serial_number[32];
} XHCIPort;

// Function prototypes
bool xhci_controller_init(XHCIController *controller, uint64_t base_addr,
                          uint64_t pci_config_base);
bool xhci_controller_reset(const XHCIController *controller);
bool xhci_controller_start(const XHCIController *controller);
bool xhci_controller_stop(const XHCIController *controller);

bool xhci_port_init(XHCIPort *port, XHCIController *controller,
                    uint8_t port_num);
bool xhci_port_reset(XHCIPort *port);
bool xhci_port_enable(XHCIPort *port);
bool xhci_port_scan(XHCIPort *port);

uint32_t xhci_read32(const XHCIController *controller, volatile void *base,
                     uint32_t offset);
void xhci_write32(const XHCIController *controller, volatile void *base,
                  uint32_t offset, uint32_t value);

#endif