/*
 * Mock syscalls for AHCI driver tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define syscall results and types directly to avoid header conflicts
typedef struct {
    uint64_t result;
    uint64_t value;
} SyscallResult;

#define SYSCALL_RESULT_OK ((0))
#define SYSCALL_RESULT_ERROR ((-1))

// Mock memory for hardware registers
static uint8_t mock_ahci_registers[0x8000]; // 32KB AHCI register space
static uint8_t mock_dma_memory[0x100000];   // 1MB DMA memory pool

// Mock state tracking
static struct {
    uint64_t last_physical_addr;
    uint64_t last_virtual_addr;
    size_t last_size;
    uint32_t last_flags;
    bool map_physical_should_fail;
    bool map_virtual_should_fail;
    bool alloc_physical_should_fail;
    uint64_t next_physical_page;
    uint64_t next_virtual_addr;
    size_t dma_memory_used;
} mock_state = {
        .next_physical_page = 0x100000000ULL, // Start at 4GB
        .next_virtual_addr = 0xA000000000ULL, // AHCI virtual base
        .dma_memory_used = 0,
};

// Mock reset function for tests
void mock_syscalls_reset(void) {
    memset(&mock_state, 0, sizeof(mock_state));
    mock_state.next_physical_page = 0x100000000ULL;
    mock_state.next_virtual_addr = 0xA000000000ULL;
    mock_state.dma_memory_used = 0;

    // Clear mock memory
    memset(mock_ahci_registers, 0, sizeof(mock_ahci_registers));
    memset(mock_dma_memory, 0, sizeof(mock_dma_memory));
}

// Mock failure control functions
void mock_map_physical_set_fail(bool should_fail) {
    mock_state.map_physical_should_fail = should_fail;
}

void mock_map_virtual_set_fail(bool should_fail) {
    mock_state.map_virtual_should_fail = should_fail;
}

void mock_alloc_physical_set_fail(bool should_fail) {
    mock_state.alloc_physical_should_fail = should_fail;
}

// Getters for test verification
uint64_t mock_get_last_physical_addr(void) {
    return mock_state.last_physical_addr;
}

uint64_t mock_get_last_virtual_addr(void) {
    return mock_state.last_virtual_addr;
}

size_t mock_get_last_size(void) { return mock_state.last_size; }

uint32_t mock_get_last_flags(void) { return mock_state.last_flags; }

// Check if any allocation should fail
bool mock_should_alloc_fail(void) {
    return mock_state.map_virtual_should_fail ||
           mock_state.alloc_physical_should_fail;
}

static inline SyscallResult RESULT(uint64_t type, uint64_t val) {
    const SyscallResult result = {.result = type, .value = val};
    return result;
}

// Mock syscall implementations
SyscallResult anos_map_physical(const uint64_t physical_addr,
                                void *virtual_addr, const size_t size,
                                const uint32_t flags) {
    mock_state.last_physical_addr = physical_addr;
    mock_state.last_virtual_addr = (uint64_t)virtual_addr;
    mock_state.last_size = size;
    mock_state.last_flags = flags;

    if (mock_state.map_physical_should_fail) {
        return RESULT(SYSCALL_RESULT_ERROR, 0);
    }

    // In a real OS, this would set up page tables to map physical_addr to virtual_addr
    // For testing, we just track the request - the actual memory access will fail
    // but that's expected behavior for this mock environment

    return RESULT(SYSCALL_RESULT_OK, 0);
}

SyscallResult anos_map_virtual(void *virtual_addr, const size_t size,
                               const uint32_t flags) {
    mock_state.last_virtual_addr = (uint64_t)virtual_addr;
    mock_state.last_size = size;
    mock_state.last_flags = flags;

    if (mock_state.map_virtual_should_fail) {
        return RESULT(SYSCALL_RESULT_ERROR, 0);
    }

    // Return the next available virtual address
    const uint64_t allocated_addr = mock_state.next_virtual_addr;
    mock_state.next_virtual_addr += size;

    return RESULT(SYSCALL_RESULT_OK, allocated_addr);
}

SyscallResult anos_alloc_physical_pages(const size_t size) {
    mock_state.last_size = size;

    if (mock_state.alloc_physical_should_fail) {
        return RESULT(SYSCALL_RESULT_ERROR, 0);
    }

    // Return the next available physical page
    uint64_t allocated_addr = mock_state.next_physical_page;
    mock_state.next_physical_page += size;
    return RESULT(SYSCALL_RESULT_OK, allocated_addr);
}

SyscallResult anos_unmap_virtual(uint64_t virtual_addr, size_t size) {
    mock_state.last_virtual_addr = virtual_addr;
    mock_state.last_size = size;
    return RESULT(SYSCALL_RESULT_OK, 0);
}

// Basic channel syscalls for completeness
SyscallResult anos_send_message(uint64_t channel_id, const void *message,
                                size_t size) {
    return RESULT(SYSCALL_RESULT_OK, 0);
}

SyscallResult anos_recv_message(uint64_t channel_id, void *buffer,
                                size_t buffer_size, size_t *actual_size) {
    if (actual_size)
        *actual_size = 0;
    return RESULT(SYSCALL_RESULT_OK, 0);
}

SyscallResult anos_create_channel(void) {
    return RESULT(SYSCALL_RESULT_OK, 123);
}

SyscallResult anos_task_sleep_current(uint32_t ms) {
    return RESULT(SYSCALL_RESULT_OK, 0);
}

SyscallResult anos_kprint(const char *message) {
    printf("[MOCK KPRINT] %s", message);
    return RESULT(SYSCALL_RESULT_OK, 0);
}

SyscallResult anos_kputchar(char c) {
    printf("%c", c);
    return RESULT(SYSCALL_RESULT_OK, 0);
}

// Mock MSI interrupt syscalls
uint8_t anos_allocate_interrupt_vector(uint32_t bus_device_func,
                                       uint64_t *msi_address,
                                       uint32_t *msi_data) {
    // Return mock MSI configuration
    if (msi_address)
        *msi_address = 0xFEE00000ULL; // Standard MSI address
    if (msi_data)
        *msi_data = 0x4000; // Mock MSI data
    return 0x40;            // Mock vector number
}

SyscallResult anos_wait_interrupt(uint8_t vector, uint32_t *event_data) {
    // Mock interrupt wait - always succeeds immediately for tests
    if (event_data)
        *event_data = 0x12345678; // Mock event data
    return RESULT(SYSCALL_RESULT_OK, 0);
}