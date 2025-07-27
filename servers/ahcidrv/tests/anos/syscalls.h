/*
 * Mock anos/syscalls.h for AHCI driver testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SYSCALLS_H
#define __ANOS_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

// C compatibility fixes for AHCI driver
#ifndef __cplusplus
#define nullptr NULL
#define constexpr const
#endif

// Syscall result type
typedef int64_t SyscallResult;

// Syscall results
#define SYSCALL_OK 0
#define SYSCALL_FAILURE -1

// Memory mapping flags
#define ANOS_MAP_PHYSICAL_FLAG_READ 0x1
#define ANOS_MAP_PHYSICAL_FLAG_WRITE 0x2
#define ANOS_MAP_PHYSICAL_FLAG_EXEC 0x4

#define ANOS_MAP_VIRTUAL_FLAG_READ 0x1
#define ANOS_MAP_VIRTUAL_FLAG_WRITE 0x2
#define ANOS_MAP_VIRTUAL_FLAG_EXEC 0x4

// Mock syscall function declarations (implemented in mock_syscalls.c)
SyscallResult anos_map_physical(uint64_t physical_addr, void *virtual_addr,
                                size_t size, uint32_t flags);
SyscallResult anos_map_virtual(void *virtual_addr, size_t size, uint32_t flags);
SyscallResult anos_unmap_virtual(uint64_t virtual_addr, size_t size);
SyscallResult anos_alloc_physical_pages(size_t size);

// Other syscalls used by AHCI driver
SyscallResult anos_send_message(uint64_t channel_id, const void *message,
                                size_t size);
SyscallResult anos_recv_message(uint64_t channel_id, void *buffer,
                                size_t buffer_size, size_t *actual_size);
SyscallResult anos_create_channel(void);
SyscallResult anos_task_sleep_current(uint32_t ms);
SyscallResult anos_kprint(const char *message);
SyscallResult anos_kputchar(char c);

// MSI interrupt syscalls
uint8_t anos_allocate_interrupt_vector(uint32_t bus_device_func,
                                       uint64_t *msi_address,
                                       uint32_t *msi_data);
SyscallResult anos_wait_interrupt(uint8_t vector, uint32_t *event_data);

#endif /* __ANOS_SYSCALLS_H */