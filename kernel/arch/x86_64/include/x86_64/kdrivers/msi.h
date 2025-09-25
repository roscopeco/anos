/*
 * stage3 - MSI/MSI-X Interrupt Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_KDRIVERS_MSI_H
#define __ANOS_KERNEL_KDRIVERS_MSI_H

#include <stdbool.h>
#include <stdint.h>

#include "process.h"

#define MSI_VECTOR_BASE 0x40
#define MSI_VECTOR_TOP 0xDF
#define MSI_VECTOR_COUNT (MSI_VECTOR_TOP - MSI_VECTOR_BASE + 1)

#define MSI_QUEUE_SIZE 4
#define MSI_TIMEOUT_MS 100

typedef struct {
    uint32_t data;
    uint64_t timestamp_ms;
} MSIEvent;

typedef struct {
    uint8_t vector;
    uint32_t bus_device_func;
    uint64_t owner_pid;

    MSIEvent queue[MSI_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;

    uint64_t last_event_ms;
    uint32_t overflow_count;
    bool slow_consumer_detected;

    Task *waiting_task;
    uint64_t total_events;
} MSIDevice;

typedef struct {
    MSIDevice devices[MSI_VECTOR_COUNT];
    uint8_t allocated_vectors[MSI_VECTOR_COUNT];
    uint32_t next_vector_hint;
} MSIManager;

void msi_init(void);
uint8_t msi_allocate_vector(uint32_t bus_device_func, uint64_t owner_pid, uint64_t *msi_address, uint32_t *msi_data);
bool msi_deallocate_vector(uint8_t vector, uint64_t owner_pid);
bool msi_register_handler(uint8_t vector, Task *task);
bool msi_wait_interrupt(uint8_t vector, Task *task, uint32_t *event_data);
void msi_handle_interrupt(uint8_t vector, uint32_t data);
void msi_cleanup_process(uint64_t pid);
bool msi_is_slow_consumer(uint8_t vector);
bool msi_verify_ownership(uint8_t vector, uint64_t pid);

#endif