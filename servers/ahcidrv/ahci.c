/*
 * ahcidrv - AHCI driver implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// TODO there's a ridiculous amount of unit test mocking going on in here...

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anos/syscalls.h>

#include "ahci.h"
#include "pci.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr or constexpr yet - Jul 2025
#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr (((void *)0))
#endif
#endif
#define constexpr const
#endif

// Virtual memory layout
#define AHCI_MEMORY_BASE 0xA000000000ULL

// AHCI structure sizes (from AHCI spec)
#define AHCI_CMD_LIST_SIZE 1024
#define AHCI_FIS_SIZE 256
#define AHCI_CMD_TABLE_SIZE 128
#define AHCI_MAX_COMMAND_SLOTS 32

// ATA IDENTIFY data offsets
#define ATA_IDENTIFY_MODEL_OFFSET 27
#define ATA_IDENTIFY_MODEL_LENGTH 20
#define ATA_IDENTIFY_SECTORS_28BIT_LO 60
#define ATA_IDENTIFY_SECTORS_28BIT_HI 61
#define ATA_IDENTIFY_SECTORS_48BIT_LO 100
#define ATA_IDENTIFY_SECTORS_48BIT_HI 103

// Memory alignment requirements
#define AHCI_CMD_LIST_ALIGN 1024
#define AHCI_FIS_ALIGN 256
#define AHCI_CMD_TABLE_ALIGN 128

// Timeouts and limits
#define AHCI_COMMAND_TIMEOUT_MS 30
#define AHCI_COMMAND_SLEEP_US 100000
#define AHCI_MAX_DMA_ADDRESS 0x100000000ULL

#ifdef DEBUG_AHCI_INIT
#define debugf(...) printf(__VA_ARGS__)
#ifdef VERY_NOISY_AHCI_INIT
#define vdebugf(...) printf(__VA_ARGS__)
#else
#define vdebugf(...)
#endif
#else
#define debugf(...)
#define vdebugf(...)
#endif

#define PCI_CONFIG_BASE_ADDRESS ((0xC000000000ULL))
#define AHCI_CONFIG_BASE_ADDRESS ((0xB000000000ULL))

static uint64_t memory_offset = 0;

#ifdef UNIT_TESTS
// Mock AHCI registers for unit testing
static AHCIRegs mock_ahci_registers;
// Mock DMA memory pool for unit testing
static uint8_t mock_dma_memory[0x100000]; // 1MB pool
static size_t mock_dma_offset = 0;

// Mock command completion state
static void *mock_identify_buffer = NULL;

// Mock IDENTIFY command data (ATA IDENTIFY response)
static void setup_mock_identify_data(void *buffer) {
    uint16_t *data = (uint16_t *)buffer;
    memset(data, 0, 512);

    // Sector count (48-bit LBA at words 100-103)
    data[100] = 0x1000; // Low 16 bits
    data[101] = 0x0000; //
    data[102] = 0x0000; //
    data[103] = 0x0000; // High 16 bits (1000 sectors = ~512KB test drive)

    const char model[] = "TEST MOCK DEVICE v1.0                   ";
    for (int i = 0; i < 20; i++) {
        data[27 + i] = (model[i * 2] << 8) | model[i * 2 + 1];
    }
}

// Mock command completion - called when CI register is written
static void mock_command_completion(uint32_t port_num, uint32_t command_slot) {
    if (port_num >= 32 || command_slot >= 32)
        return;

    // If there's a pending IDENTIFY command, simulate completion
    if (mock_identify_buffer) {
        setup_mock_identify_data(mock_identify_buffer);
        mock_identify_buffer = NULL;
    }

    // Clear the command issue bit to indicate completion
    mock_ahci_registers.ports[port_num].ci &= ~(1U << command_slot);

    // Set interrupt status to indicate command completion
    mock_ahci_registers.ports[port_num].is |=
            (1U << 0); // Device to Host FIS interrupt
}

// Reset mock state for unit tests
void ahci_reset_test_state(void) {
    memset(&mock_ahci_registers, 0, sizeof(mock_ahci_registers));
    memset(mock_dma_memory, 0, sizeof(mock_dma_memory));
    mock_dma_offset = 0;
    mock_identify_buffer = NULL;

    // Set up mock AHCI controller with reasonable defaults
    // Capability register: support 4 ports (bits 0-4 = 3), 64-bit DMA
    mock_ahci_registers.host.cap =
            0x3 | (1U << 31); // 4 ports + 64-bit addressing

    // Port implemented register: port 0 is available
    mock_ahci_registers.host.pi = 0x1;

    // Set up port 0 with device present
    mock_ahci_registers.ports[0].ssts = 0x3;       // AHCI_PORT_SSTS_DET_PRESENT
    mock_ahci_registers.ports[0].sig = 0x00000101; // AHCI_SIG_ATA (SATA drive)
}
#endif

static void *allocate_aligned_memory(const size_t size, const size_t alignment,
                                     uint64_t *phys_addr_out) {
    const size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Round up to page size for physical allocation
    const size_t page_aligned_size = (aligned_size + 0xFFF) & ~0xFFF;

    // Allocate physical pages
    const SyscallResultA alloc_result =
            anos_alloc_physical_pages(page_aligned_size);
    const uint64_t phys_addr = alloc_result.value;

    if (alloc_result.result != SYSCALL_OK || phys_addr == 0) {
        return nullptr;
    }

#ifdef UNIT_TESTS
    // In unit tests, check if mock syscalls are set to fail
    extern bool mock_should_alloc_fail(void);
    if (mock_should_alloc_fail()) {
        return nullptr;
    }

    // In unit tests, allocate from mock DMA memory pool
    if (mock_dma_offset + aligned_size > sizeof(mock_dma_memory)) {
        return nullptr; // Out of mock memory
    }

    void *virt_addr = mock_dma_memory + mock_dma_offset;
    mock_dma_offset += aligned_size;

    if (phys_addr_out) {
        *phys_addr_out = phys_addr; // Use the mock physical address
    }

    memset(virt_addr, 0, aligned_size);
#else
    void *virt_addr = (void *)(AHCI_MEMORY_BASE + memory_offset);
    memory_offset += page_aligned_size;

    const SyscallResult result = anos_map_physical(
            phys_addr, virt_addr, page_aligned_size,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    if (result.result != SYSCALL_OK) {
        // TODO: Free the physical pages on error
        return nullptr;
    }

    if (phys_addr_out) {
        *phys_addr_out = phys_addr;
    }

    memset(virt_addr, 0, aligned_size);
#endif
    return virt_addr;
}

static bool try_map_firmware_dma(AHCIPort *port, const uint8_t port_num,
                                 const AHCIPortRegs *port_regs) {

    const uint64_t cmd_list_phys =
            ((uint64_t)port_regs->clbu << 32) | port_regs->clb;
    const uint64_t fis_base_phys =
            ((uint64_t)port_regs->fbu << 32) | port_regs->fb;

    vdebugf("Port %u: Firmware DMA structures:\n", port_num);
    vdebugf("  Command List: phys=0x%016lx (CLB=0x%08x CLBU=0x%08x)\n",
            cmd_list_phys, port_regs->clb, port_regs->clbu);
    vdebugf("  FIS Base: phys=0x%016lx (FB=0x%08x FBU=0x%08x)\n", fis_base_phys,
            port_regs->fb, port_regs->fbu);

    // Check if addresses look reasonable
    if (cmd_list_phys == 0 || cmd_list_phys > AHCI_MAX_DMA_ADDRESS) {
        debugf("  -> Command list address looks invalid (0x%016lx), falling "
               "back to our allocation\n",
               cmd_list_phys);

        return false;
    }

    // Try to map the firmware's command list
    void *cmd_list_virt = (void *)(AHCI_MEMORY_BASE + memory_offset);
    constexpr size_t cmd_list_map_size = (AHCI_CMD_LIST_SIZE + 0xFFF) & ~0xFFF;
    memory_offset += cmd_list_map_size;

    vdebugf("  -> Attempting to map firmware command list: phys=0x%016lx -> "
            "virt=%p (size=0x%lx)\n",
            cmd_list_phys, cmd_list_virt, cmd_list_map_size);

    SyscallResult result = anos_map_physical(
            cmd_list_phys, cmd_list_virt, cmd_list_map_size,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    if (result.result != SYSCALL_OK) {
        debugf("  -> Failed to map firmware command list (syscall error %ld), "
               "falling back to our allocation\n",
               result.result);
        memory_offset -= cmd_list_map_size;
        return false;
    }

    port->cmd_list = cmd_list_virt;

    // Map the firmware's FIS base
    void *fis_base_virt = (void *)(AHCI_MEMORY_BASE + memory_offset);
    constexpr size_t fis_map_size = (AHCI_FIS_SIZE + 0xFFF) & ~0xFFF;
    memory_offset += fis_map_size;

    result = anos_map_physical(fis_base_phys, fis_base_virt, fis_map_size,
                               ANOS_MAP_VIRTUAL_FLAG_READ |
                                       ANOS_MAP_VIRTUAL_FLAG_WRITE);

    if (result.result != SYSCALL_OK) {
        debugf("Failed to map firmware FIS base for port %u\n", port_num);
        return false;
    }

    port->fis_base = fis_base_virt;

    vdebugf("  -> Successfully using firmware DMA structures\n");
    return true;
}

static bool allocate_own_dma(AHCIPort *port, const uint8_t port_num,
                             AHCIPortRegs *port_regs) {

    vdebugf("  -> Allocating our own DMA structures\n");

    uint64_t our_cmd_list_phys;
    port->cmd_list = allocate_aligned_memory(
            AHCI_CMD_LIST_SIZE, AHCI_CMD_LIST_ALIGN, &our_cmd_list_phys);

    if (!port->cmd_list) {
        debugf("Failed to allocate command list for port %u\n", port_num);
        return false;
    }

    uint64_t our_fis_base_phys;
    port->fis_base = allocate_aligned_memory(AHCI_FIS_SIZE, AHCI_FIS_ALIGN,
                                             &our_fis_base_phys);

    if (!port->fis_base) {
        debugf("Failed to allocate FIS base for port %u\n", port_num);
        return false;
    }

    // Update port registers with our physical addresses
    port_regs->clb = (uint32_t)(our_cmd_list_phys & 0xFFFFFFFF);
    port_regs->clbu = (uint32_t)(our_cmd_list_phys >> 32);
    port_regs->fb = (uint32_t)(our_fis_base_phys & 0xFFFFFFFF);
    port_regs->fbu = (uint32_t)(our_fis_base_phys >> 32);

    return true;
}

static bool setup_command_tables(AHCIPort *port, uint8_t port_num) {
    uint64_t cmd_tables_phys;
    port->cmd_tables = allocate_aligned_memory(
            AHCI_CMD_TABLE_SIZE * AHCI_MAX_COMMAND_SLOTS, AHCI_CMD_TABLE_ALIGN,
            &cmd_tables_phys);

    if (!port->cmd_tables) {
        debugf("Failed to allocate command tables for port %u\n", port_num);
        return false;
    }

    // Set up command headers to point to our command tables
    AHCICmdHeader *cmd_headers = (AHCICmdHeader *)port->cmd_list;

    for (int i = 0; i < AHCI_MAX_COMMAND_SLOTS; i++) {
        const uint64_t cmd_table_phys =
                cmd_tables_phys + (i * AHCI_CMD_TABLE_SIZE);

        cmd_headers[i].ctba = (uint32_t)(cmd_table_phys & 0xFFFFFFFF);
        cmd_headers[i].ctbau = (uint32_t)(cmd_table_phys >> 32);
    }

#ifdef DEBUG_AHCI_INIT
    debugf("  Testing DMA buffer access...\n");
    debugf("    Command header test: CTBA=0x%08x CTBAU=0x%08x\n",
           cmd_headers[0].ctba, cmd_headers[0].ctbau);

    const AHCICmdTable *test_table = (AHCICmdTable *)port->cmd_tables;
    debugf("    Command table test: FIS[0]=0x%02x FIS[1]=0x%02x\n",
           test_table->cfis[0], test_table->cfis[1]);

    debugf("    Physical addr test: cmd_tables_phys=0x%016lx\n",
           cmd_tables_phys);
    debugf("    Slot 0 table phys: 0x%016lx\n",
           cmd_tables_phys + (0 * AHCI_CMD_TABLE_SIZE));
#endif

    return true;
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

bool ahci_controller_init(AHCIController *ctrl, uint64_t ahci_base,
                          uint64_t pci_config_base) {

    if (!ctrl || ahci_base == 0 || pci_config_base == 0) {
        return false;
    }

    memset(ctrl, 0, sizeof(AHCIController));
    ctrl->pci_base = pci_config_base;

    ctrl->mapped_size = 0x1000;

    // Map PCI configuration space
    debugf("Mapping PCI config space: phys=0x%016lx -> virt=0x%016llx "
           "(size=0x1000)\n",
           pci_config_base, PCI_CONFIG_BASE_ADDRESS);

    const SyscallResult pci_result = anos_map_physical(
            pci_config_base, (void *)PCI_CONFIG_BASE_ADDRESS, 0x1000,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    if (pci_result.result != SYSCALL_OK) {
        debugf("FAILED to map PCI config space! Error code: %ld\n",
               pci_result.result);
        return false;
    }

    // Update pci_base to point to mapped virtual address
    ctrl->pci_base = PCI_CONFIG_BASE_ADDRESS;

    debugf("Mapping AHCI registers: phys=0x%016lx -> virt=0x%016llx "
           "(size=0x%lx)\n",
           ahci_base, AHCI_CONFIG_BASE_ADDRESS, ctrl->mapped_size);

    const SyscallResult result = anos_map_physical(
            ahci_base, (void *)AHCI_CONFIG_BASE_ADDRESS, ctrl->mapped_size,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    if (result.result != SYSCALL_OK) {
        debugf("FAILED to map AHCI registers! Error code: %ld\n",
               result.result);
        vdebugf("  Attempted mapping: phys=0x%016lx -> virt=0x%016llx "
                "(size=0x%lx)\n",
                ahci_base, AHCI_CONFIG_BASE_ADDRESS, ctrl->mapped_size);

        return false;
    }

#ifdef UNIT_TESTS
    // In unit tests, use mock registers instead of real hardware addresses
    ctrl->mapped_regs = &mock_ahci_registers;
    ctrl->regs = &mock_ahci_registers;
#else
    ctrl->mapped_regs = (void *)AHCI_CONFIG_BASE_ADDRESS;
    ctrl->regs = (AHCIRegs *)ctrl->mapped_regs;
#endif

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
    vdebugf("AHCI structure sizes:\n");
    vdebugf("  sizeof(AHCIHostRegs): %zu (should be 256)\n",
            sizeof(AHCIHostRegs));
    vdebugf("  sizeof(AHCIPortRegs): %zu (should be 128)\n",
            sizeof(AHCIPortRegs));
    vdebugf("  sizeof(AHCIRegs): %zu\n", sizeof(AHCIRegs));
    vdebugf("  Port 0 offset: 0x%lx (should be 0x100)\n",
            (uintptr_t)&ctrl->regs->ports[0] - (uintptr_t)ctrl->regs);

    vdebugf("AHCI registers mapped successfully\n");
    vdebugf("  CAP: 0x%08x\n", ctrl->regs->host.cap);
    vdebugf("  GHC: 0x%08x\n", ctrl->regs->host.ghc);
    vdebugf("  IS: 0x%08x\n", ctrl->regs->host.is);
    vdebugf("  PI: 0x%08x\n", ctrl->regs->host.pi);
    vdebugf("  VS: 0x%08x\n", ctrl->regs->host.vs);
#endif

    // Verify mapping by checking if we get sane values
    if (ctrl->regs->host.cap == 0xffffffff || ctrl->regs->host.cap == 0) {
        debugf("WARNING: CAP register looks invalid (0x%08x) - mapping may be "
               "wrong!\n",
               ctrl->regs->host.cap);
    }

#ifdef VERY_NOISY_AHCI_INIT
    // Test reading from different offsets to verify mapping
    volatile uint32_t *test_ptr = (volatile uint32_t *)ctrl->mapped_regs;
    vdebugf("Raw register reads: [0]=0x%08x [1]=0x%08x [2]=0x%08x [3]=0x%08x\n",
            test_ptr[0], test_ptr[1], test_ptr[2], test_ptr[3]);
#endif
#endif

    // TODO reset controller and set it up from scratch? Or not worth the hassle?

    ctrl->port_count = (ctrl->regs->host.cap & 0x1F) + 1;
    ctrl->active_ports = ctrl->regs->host.pi;

    if (ctrl->port_count > 32) {
        ctrl->port_count = 32;
    }

    // Find MSI capability for later use
#ifdef UNIT_TESTS
    // In unit tests, mock finding MSI capability
    ctrl->msi_cap_offset = 0x50; // Mock MSI capability offset
#else
    ctrl->msi_cap_offset = pci_find_msi_capability(PCI_CONFIG_BASE_ADDRESS);
#endif

#ifdef DEBUG_AHCI_INIT
    debugf("Controller supports %u ports, active mask: 0x%08x\n",
           ctrl->port_count, ctrl->active_ports);
    if (ctrl->msi_cap_offset) {
        debugf("MSI capability found at offset 0x%02x\n", ctrl->msi_cap_offset);
    } else {
        debugf("No MSI capability found - interrupts will not work\n");
    }
#endif

    // Enable global AHCI interrupts
    ctrl->regs->host.ghc |= (1U << 1); // Set IE (Interrupt Enable) bit
    debugf("AHCI global interrupts enabled (GHC=0x%08x)\n",
           ctrl->regs->host.ghc);

    ctrl->initialized = true;
    return true;
}

void ahci_controller_cleanup(AHCIController *ctrl) {
    if (!ctrl || !ctrl->initialized) {
        return;
    }

    for (int i = 0; i < ctrl->port_count; i++) {
        if (ctrl->active_ports & (1 << i)) {
            ahci_port_stop(&ctrl->regs->ports[i]);
        }
    }

    // Clear all controller state
    ctrl->initialized = false;
    ctrl->pci_base = 0;
    ctrl->mapped_regs = NULL;
    ctrl->regs = NULL;
    ctrl->port_count = 0;
    ctrl->active_ports = 0;
}

void ahci_port_cleanup(AHCIPort *port) {
    if (!port || !port->initialized) {
        return;
    }

    // Deallocate MSI vector if we allocated one
    if (port->msi_enabled && port->msi_vector != 0) {
        // Note: MSI cleanup is automatic when the process exits,
        // but we could explicitly deallocate here if needed
        debugf("Port %u: MSI vector 0x%02x will be cleaned up automatically\n",
               port->port_num, port->msi_vector);
        port->msi_enabled = false;
        port->msi_vector = 0;
    }

    port->initialized = false;
    port->connected = false;
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
    vdebugf("Port %u register access debug:\n", port_num);
    vdebugf("  ctrl->regs = %p\n", (void *)ctrl->regs);
    vdebugf("  port_regs = %p\n", (void *)port_regs);
    vdebugf("  Expected offset from base: 0x%lx\n",
            (uintptr_t)port_regs - (uintptr_t)ctrl->regs);

    // Test raw access to port registers
    volatile uint32_t *port_raw = (volatile uint32_t *)port_regs;
    vdebugf("  Raw port reads: [0]=0x%08x [1]=0x%08x [2]=0x%08x [9]=0x%08x "
            "[10]=0x%08x\n",
            port_raw[0], port_raw[1], port_raw[2], port_raw[9], port_raw[10]);
    vdebugf("  Structured reads: CLB=0x%08x CLBU=0x%08x TFD=0x%08x SIG=0x%08x "
            "SSTS=0x%08x\n",
            port_regs->clb, port_regs->clbu, port_regs->tfd, port_regs->sig,
            port_regs->ssts);
#endif
#endif

    uint32_t ssts = port_regs->ssts;
    if ((ssts & AHCI_PORT_SSTS_DET_MASK) != AHCI_PORT_SSTS_DET_PRESENT) {
        debugf("Port %u: No device detected (SSTS=0x%08x)\n", port_num, ssts);
        return false;
    }

#ifdef DEBUG_AHCI_INIT
    // Check device signature
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

    debugf("Port %u: Device detected - %s (SSTS=0x%08x, SIG=0x%08x)\n",
           port_num, device_type, ssts, sig);
#endif

    ahci_port_stop(port_regs);

    // Try to use firmware DMA structures first, fallback to our own if needed
    if (!try_map_firmware_dma(port, port_num, port_regs)) {
        if (!allocate_own_dma(port, port_num, port_regs)) {
            return false;
        }
    }

    // Set up command tables (always our own allocation)
    if (!setup_command_tables(port, port_num)) {
        return false;
    }

    // Clear error and interrupt status, enable interrupts
    port_regs->serr = 0xFFFFFFFF;
    port_regs->is = 0xFFFFFFFF;
    port_regs->ie = 0xFFFFFFFF;

    ahci_port_start(port_regs);

    // Initialize MSI interrupt for this port
    port->msi_enabled = false;
    port->msi_vector = 0;

    // Try to allocate MSI vector for this port
    // Use PCI bus/device/function from controller's base address
    // For simplicity, we'll derive a mock bus/device/function from the port number
    const uint32_t bus_device_func =
            0x010000 | (port_num << 3); // Mock PCI address

    uint64_t msi_address;
    uint32_t msi_data;

    const SyscallResultU8 alloc_result = anos_allocate_interrupt_vector(
            bus_device_func, &msi_address, &msi_data);

    const uint8_t vector = alloc_result.value;

    if (alloc_result.result == SYSCALL_OK && vector != 0 &&
        ctrl->msi_cap_offset != 0) {
        // Configure the PCI MSI capability registers
#ifdef UNIT_TESTS
        // In unit tests, mock successful MSI configuration
        port->msi_vector = vector;
        port->msi_enabled = true;
        debugf("Port %u: MSI vector 0x%02x configured and enabled (mock)\n",
               port_num, vector);
#else
        if (pci_configure_msi(ctrl->pci_base, ctrl->msi_cap_offset, msi_address,
                              msi_data)) {
            port->msi_vector = vector;
            port->msi_enabled = true;
            debugf("Port %u: MSI vector 0x%02x configured and enabled\n",
                   port_num, vector);
        } else {
            debugf("Port %u: Failed to configure MSI hardware, using polling\n",
                   port_num);
        }
#endif
    } else {
        if (vector == 0) {
            debugf("Port %u: Failed to allocate MSI vector, using polling\n",
                   port_num);
        } else {
            debugf("Port %u: No MSI capability, using polling\n", port_num);
        }
    }

    port->connected = true;
    port->initialized = true;

    debugf("Port %u initialized successfully%s\n", port_num,
           port->msi_enabled ? " with MSI interrupts" : " with polling");

    return true;
}

static bool ahci_wait_for_completion(const AHCIPort *port, const uint8_t slot) {
    const AHCIPortRegs *port_regs =
            &port->controller->regs->ports[port->port_num];

    if (port->msi_enabled) {
        // Use MSI interrupt-driven completion
        debugf("Port %u: Waiting for MSI interrupt on vector 0x%02x\n",
               port->port_num, port->msi_vector);

        uint32_t event_data;
        const SyscallResult result =
                anos_wait_interrupt(port->msi_vector, &event_data);

        if (result.result == SYSCALL_OK) {
            debugf("Port %u: Received MSI interrupt (data=0x%08x)\n",
                   port->port_num, event_data);

            // Check if command completed
            if ((port_regs->ci & (1 << slot)) == 0) {
                return true;
            }

            debugf("Port %u: Interrupt received but command still pending\n",
                   port->port_num);
        } else {
            debugf("Port %u: MSI wait failed with result %ld\n", port->port_num,
                   result.result);
        }

        // Fall back to polling if MSI fails
        debugf("Port %u: Falling back to polling\n", port->port_num);
    }

    // Original polling-based completion
    int timeout = AHCI_COMMAND_TIMEOUT_MS * 10; // Convert ms to 100μs units
    while (timeout > 0) {
        if ((port_regs->ci & (1 << slot)) == 0) {
            return true;
        }

        if (port_regs->is & (1 << 30)) {
            debugf("Port %u: Task file error\n", port->port_num);
            return false;
        }

        anos_task_sleep_current(AHCI_COMMAND_SLEEP_US);
        timeout--;
    }

    debugf("Port %u: Command timeout on slot %u\n", port->port_num, slot);
    return false;
}

#ifdef DEBUG_AHCI_INIT
#ifdef VERY_NOISY_AHCI_INIT
static void vdebug_dump_d2h_fis(const uint8_t *fis_area) {
    vdebugf("     D2H FIS: %02x %02x %02x %02x %02x %02x %02x "
            "%02x\n",
            fis_area[0x40], fis_area[0x41], fis_area[0x42], fis_area[0x43],
            fis_area[0x44], fis_area[0x45], fis_area[0x46], fis_area[0x47]);
    vdebugf("     SDB FIS: %02x %02x %02x %02x %02x %02x %02x "
            "%02x\n",
            fis_area[0x58], fis_area[0x59], fis_area[0x5A], fis_area[0x5B],
            fis_area[0x5C], fis_area[0x5D], fis_area[0x5E], fis_area[0x5F]);
}

static void vdebug_dump_h2d_fis(const uint8_t *fis_bytes) {
    vdebugf("Raw H2D FIS: %02x %02x %02x %02x %02x %02x %02x %02x\n",
            fis_bytes[0], fis_bytes[1], fis_bytes[2], fis_bytes[3],
            fis_bytes[4], fis_bytes[5], fis_bytes[6], fis_bytes[7]);
}
#else
#define vdebug_dump_d2h_fis(...)
#define vdebug_dump_h2d_fis(...)
#endif
#else
#define vdebug_dump_d2h_fis(...)
#define vdebug_dump_h2d_fis(...)
#endif

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
        debugf("Failed to allocate identify buffer\n");

        return false;
    }

    vdebugf("IDENTIFY setup: cmd_list=%p cmd_table=%p buffer=%p "
            "(phys=0x%016lx)\n",
            port->cmd_list, port->cmd_tables, identify_buffer,
            identify_buffer_phys);
    vdebugf("Port registers: CLB=0x%08x:0x%08x FB=0x%08x:0x%08x\n",
            port_regs->clbu, port_regs->clb, port_regs->fbu, port_regs->fb);

    // Don't clear the entire header - preserve CTBA/CTBAU set during port init
    // memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    // Clear and set only the fields we need to modify
    cmd_header->cfl = sizeof(FISRegH2D) / 4; // Should be 5 (20 bytes / 4)
    cmd_header->prdtl = 1;                   // 1 PRDT entry
    cmd_header->a = 0;                       // Not ATAPI
    cmd_header->w = 0;                       // Read operation
    cmd_header->p = 0;                       // No prefetch
    cmd_header->r = 0;                       // Not reset
    cmd_header->b = 0;                       // Not BIST
    cmd_header->c = 0;                       // Not clear busy upon R_OK
    cmd_header->prdbc = 0;                   // Clear byte count
    cmd_header->pmp = 0;                     // Port multiplier port

    vdebugf("Command header: CFL=%u PRDTL=%u\n", cmd_header->cfl,
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

    vdebugf("H2D FIS setup: type=0x%02x flags=0x%02x cmd=0x%02x dev=0x%02x\n",
            fis->fis_type, fis->flags, fis->command, fis->device);

    vdebug_dump_h2d_fis((uint8_t *)fis);

    cmd_table->prdt[0].dba = (uint32_t)(identify_buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt[0].dbau = (uint32_t)(identify_buffer_phys >> 32);
    cmd_table->prdt[0].dbc = 511; // 512 bytes - 1 (0-based count)
    cmd_table->prdt[0].i = 1;

    port_regs->ci = 1;

#ifdef UNIT_TESTS
    // In unit tests, set up mock IDENTIFY completion
    mock_identify_buffer = identify_buffer;
    mock_command_completion(port->port_num, 0);
#endif

    vdebugf("Port %u post-command issue:\n", port->port_num);
    vdebugf("  CI: 0x%08x\n", port_regs->ci);

    if (!ahci_wait_for_completion(port, 0)) {
        vdebugf("Port %u: IDENTIFY command timeout\n", port->port_num);
        vdebugf("Final port status:\n");
        vdebugf("  TFD: 0x%08x\n", port_regs->tfd);
        vdebugf("  SERR: 0x%08x\n", port_regs->serr);
        vdebugf("  IS: 0x%08x\n", port_regs->is);
        vdebugf("  CI: 0x%08x\n", port_regs->ci);

        // Check if device actually completed but we missed it due to interrupt handling
        if (port_regs->is & 0x1) {
            vdebugf("  -> Device sent D2H FIS (IS bit 0 set), trying manual "
                    "completion...\n");
            // Clear the interrupt status
            port_regs->is = 0x1;
            vdebugf("  -> Cleared IS, now CI=0x%08x\n", port_regs->ci);
            // Check if CI cleared after interrupt handling
            if ((port_regs->ci & 0x1) == 0) {
                vdebugf("  -> Command actually completed! Continuing...\n");
                // Command completed successfully, continue to identify parsing below
            } else {
                vdebugf("  -> CI still set (0x%08x), command may have failed\n",
                        port_regs->ci);
                // Check for errors in the received FIS or TFD
                vdebugf("  -> TFD after IS clear: 0x%08x\n", port_regs->tfd);
                if (port_regs->tfd & 0x1) {
                    vdebugf("  -> Device reports ERROR bit set in TFD\n");
                }

                // Let's see what FIS the device actually sent us
                vdebugf("  -> Examining received FIS at %p:\n", port->fis_base);
                vdebug_dump_d2h_fis((uint8_t *)port->fis_base);

                return false;
            }
        } else {
            return false;
        }
    }

    // Command completed successfully - parse identify data

    const uint16_t *id_data = (uint16_t *)identify_buffer;

    // Extract 48-bit LBA sector count first
    port->sector_count =
            ((uint64_t)id_data[ATA_IDENTIFY_SECTORS_48BIT_HI] << 48) |
            ((uint64_t)id_data[ATA_IDENTIFY_SECTORS_48BIT_HI - 1] << 32) |
            ((uint64_t)id_data[ATA_IDENTIFY_SECTORS_48BIT_LO + 1] << 16) |
            id_data[ATA_IDENTIFY_SECTORS_48BIT_LO];

    // Fallback to 28-bit LBA if 48-bit is not available
    if (port->sector_count == 0) {
        port->sector_count =
                ((uint32_t)id_data[ATA_IDENTIFY_SECTORS_28BIT_HI] << 16) |
                id_data[ATA_IDENTIFY_SECTORS_28BIT_LO];
    }

    port->sector_size = 512;

    // Extract device model string (ATA spec: words 27-46)
    char model_string[41] = {0}; // 40 chars + null terminator

    // Copy and byte-swap the model string (ATA stores strings byte-swapped)
    for (int i = 0; i < ATA_IDENTIFY_MODEL_LENGTH; i++) {
        const uint16_t word = id_data[ATA_IDENTIFY_MODEL_OFFSET + i];
        model_string[i * 2] = (char)((word >> 8) & 0xFF); // High byte first
        model_string[i * 2 + 1] = (char)(word & 0xFF);    // Low byte second
    }

    // Trim trailing spaces
    for (int i = 39;
         i >= 0 && (model_string[i] == ' ' || model_string[i] == '\0'); i--) {
        model_string[i] = '\0';
    }

#ifdef DEBUG_AHCI_INIT
    debugf("Port %u: Device identification complete\n", port->port_num);
    debugf("  Model: '%s'\n", model_string);
    debugf("  Sectors: %lu\n", port->sector_count);
    debugf("  Sector size: %u bytes\n", port->sector_size);
    debugf("  Capacity: %lu MB\n",
           (port->sector_count * port->sector_size) / (1024 * 1024));
#else
    printf("Found %lu MiB storage device '%s' on port %u\n",
           (port->sector_count * port->sector_size) / (1024 * 1024),
           model_string, port->port_num);
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

    // Don't clear the entire header - preserve CTBA/CTBAU set during port init
    // memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4;
    cmd_header->prdtl = 1;
    cmd_header->a = 0;
    cmd_header->w = 0; // Read operation
    cmd_header->p = 0;
    cmd_header->r = 0;
    cmd_header->b = 0;
    cmd_header->c = 0;
    cmd_header->prdbc = 0;
    cmd_header->pmp = 0;

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

    // Don't clear the entire header - preserve CTBA/CTBAU set during port init
    // memset(cmd_header, 0, sizeof(AHCICmdHeader));
    memset(cmd_table, 0, sizeof(AHCICmdTable));

    cmd_header->cfl = sizeof(FISRegH2D) / 4;
    cmd_header->prdtl = 1;
    cmd_header->a = 0;
    cmd_header->w = 1; // Write operation
    cmd_header->p = 0;
    cmd_header->r = 0;
    cmd_header->b = 0;
    cmd_header->c = 0;
    cmd_header->prdbc = 0;
    cmd_header->pmp = 0;

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