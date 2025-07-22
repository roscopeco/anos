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
typedef int64_t SyscallResult;
#define SYSCALL_RESULT_OK 0
#define SYSCALL_RESULT_ERROR -1

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

// Mock syscall implementations
SyscallResult anos_map_physical_syscall(uint64_t physical_addr,
                                        void *virtual_addr, size_t size,
                                        uint32_t flags) {
    mock_state.last_physical_addr = physical_addr;
    mock_state.last_virtual_addr = (uint64_t)virtual_addr;
    mock_state.last_size = size;
    mock_state.last_flags = flags;

    if (mock_state.map_physical_should_fail) {
        return SYSCALL_RESULT_ERROR;
    }

    // In a real OS, this would set up page tables to map physical_addr to virtual_addr
    // For testing, we just track the request - the actual memory access will fail
    // but that's expected behavior for this mock environment

    return SYSCALL_RESULT_OK;
}

SyscallResult anos_map_virtual_syscall(void *virtual_addr, size_t size,
                                       uint32_t flags) {
    mock_state.last_virtual_addr = (uint64_t)virtual_addr;
    mock_state.last_size = size;
    mock_state.last_flags = flags;

    if (mock_state.map_virtual_should_fail) {
        return SYSCALL_RESULT_ERROR;
    }

    // Return the next available virtual address
    uint64_t allocated_addr = mock_state.next_virtual_addr;
    mock_state.next_virtual_addr += size;
    return allocated_addr;
}

SyscallResult anos_alloc_physical_pages_syscall(size_t size) {
    mock_state.last_size = size;

    if (mock_state.alloc_physical_should_fail) {
        return 0; // Return 0 for failure to match AHCI driver expectations
    }

    // Return the next available physical page
    uint64_t allocated_addr = mock_state.next_physical_page;
    mock_state.next_physical_page += size;
    return allocated_addr;
}

SyscallResult anos_unmap_virtual_syscall(uint64_t virtual_addr, size_t size) {
    mock_state.last_virtual_addr = virtual_addr;
    mock_state.last_size = size;
    return SYSCALL_RESULT_OK;
}

// Basic channel syscalls for completeness
SyscallResult anos_send_message_syscall(uint64_t channel_id,
                                        const void *message, size_t size) {
    return SYSCALL_RESULT_OK;
}

SyscallResult anos_recv_message_syscall(uint64_t channel_id, void *buffer,
                                        size_t buffer_size,
                                        size_t *actual_size) {
    if (actual_size)
        *actual_size = 0;
    return SYSCALL_RESULT_OK;
}

SyscallResult anos_create_channel_syscall(void) {
    return 123; // Mock channel ID
}

SyscallResult anos_task_sleep_current_syscall(uint32_t ms) {
    return SYSCALL_RESULT_OK;
}

SyscallResult anos_kprint_syscall(const char *message) {
    printf("[MOCK KPRINT] %s", message);
    return SYSCALL_RESULT_OK;
}

SyscallResult anos_kputchar_syscall(char c) {
    printf("%c", c);
    return SYSCALL_RESULT_OK;
}