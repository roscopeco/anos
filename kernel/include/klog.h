/*
 * Kernel Log Ringbuffer Interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_KLOG_H
#define __ANOS_KERNEL_KLOG_H

#include <stddef.h>
#include <stdint.h>

#include "spinlock.h"

struct Task;

// Kernel log buffer structure
typedef struct {
    char *buffer;                 // Circular buffer storage
    size_t size;                  // Total buffer size
    volatile size_t head;         // Write position (next char goes here)
    volatile size_t tail;         // Read position (next char read from here)
    volatile size_t count;        // Number of unread bytes
    SpinLock lock;                // Synchronization lock
    bool dropped_messages;        // True if messages were dropped due to overflow
    struct Task *waiting_readers; // Linked list of tasks waiting for data
} KernelLogBuffer;

// Statistics structure for userspace
typedef struct {
    size_t buffer_size;     // Total buffer size
    size_t bytes_available; // Bytes available to read
    size_t bytes_free;      // Free space in buffer
    size_t head_position;   // Current write position
    size_t tail_position;   // Current read position
    bool dropped_messages;  // Whether messages were dropped
} KernelLogStats;

// Initialization and control
bool klog_init(void);
void klog_set_userspace_ready(bool ready);

// Writing to log (from kernel)
void klog_write_char(char c);
void klog_write_string(const char *str);

// Reading from log (for userspace via syscall)
size_t klog_read(char *dest, size_t max_bytes);
size_t klog_available(void);
bool klog_has_dropped_messages(void);
void klog_clear(void);
KernelLogStats klog_get_stats(void);

#endif