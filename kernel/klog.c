/*
 * Kernel Log Ringbuffer Implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "klog.h"
#include "fba/alloc.h"
#include "spinlock.h"
#include <stdint.h>

#define KLOG_BUFFER_SIZE (64 * 1024) // 64KB ringbuffer
#define KLOG_MAX_LINE 1024           // Maximum single log line

static KernelLogBuffer klog_buffer = {nullptr};
static bool klog_initialized = false;
static bool userspace_ready = false;

// External reference to gdebugterm functions for fallback
extern void debugchar_np(char chr);
extern void debugstr(const char *str);

bool klog_init(void) {
    if (klog_initialized) {
        return true;
    }

    constexpr uint32_t pages_needed = (KLOG_BUFFER_SIZE + 4095) / 4096;

    klog_buffer.buffer = fba_alloc_blocks(pages_needed);
    if (!klog_buffer.buffer) {
        return false;
    }

    klog_buffer.size = KLOG_BUFFER_SIZE;
    klog_buffer.head = 0;
    klog_buffer.tail = 0;
    klog_buffer.count = 0;
    klog_buffer.dropped_messages = false;
    spinlock_init(&klog_buffer.lock);

    klog_initialized = true;
    return true;
}

void klog_set_userspace_ready(bool ready) { userspace_ready = ready; }

static void klog_write_char_internal(char c) {
    if (!klog_initialized) {
        return;
    }

    // Buffer is full, drop oldest character
    if (klog_buffer.count >= klog_buffer.size) {
        klog_buffer.tail = (klog_buffer.tail + 1) % klog_buffer.size;
        klog_buffer.dropped_messages = true;
    } else {
        klog_buffer.count++;
    }

    klog_buffer.buffer[klog_buffer.head] = c;
    klog_buffer.head = (klog_buffer.head + 1) % klog_buffer.size;
}

void klog_write_char(const char c) {
    if (!klog_initialized) {
        // Fallback to direct framebuffer output
        debugchar_np(c);
        return;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);

    klog_write_char_internal(c);

    if (!userspace_ready) {
        debugchar_np(c);
    }

    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);
}

void klog_write_string(const char *str) {
    if (!str)
        return;

    if (!klog_initialized) {
        debugstr(str);
        return;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);

    while (*str) {
        klog_write_char_internal(*str);

        if (!userspace_ready) {
            debugchar_np(*str);
        }

        str++;
    }

    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);
}

size_t klog_read(char *dest, const size_t max_bytes) {

    if (!klog_initialized || !dest || max_bytes == 0) {
        return 0;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);

    size_t bytes_read = 0;
    while (bytes_read < max_bytes && klog_buffer.count > 0) {
        dest[bytes_read] = klog_buffer.buffer[klog_buffer.tail];
        klog_buffer.tail = (klog_buffer.tail + 1) % klog_buffer.size;
        klog_buffer.count--;
        bytes_read++;
    }

    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);

    return bytes_read;
}

size_t klog_available(void) {
    if (!klog_initialized) {
        return 0;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);
    const size_t available = klog_buffer.count;
    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);

    return available;
}

bool klog_has_dropped_messages(void) {

    if (!klog_initialized) {
        return false;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);
    const bool dropped = klog_buffer.dropped_messages;
    klog_buffer.dropped_messages = false;
    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);

    return dropped;
}

void klog_clear(void) {
    if (!klog_initialized) {
        return;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);
    klog_buffer.head = 0;
    klog_buffer.tail = 0;
    klog_buffer.count = 0;
    klog_buffer.dropped_messages = false;
    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);
}

KernelLogStats klog_get_stats(void) {
    KernelLogStats stats = {0};

    if (!klog_initialized) {
        return stats;
    }

    const uint64_t flags = spinlock_lock_irqsave(&klog_buffer.lock);
    stats.buffer_size = klog_buffer.size;
    stats.bytes_available = klog_buffer.count;
    stats.bytes_free = klog_buffer.size - klog_buffer.count;
    stats.head_position = klog_buffer.head;
    stats.tail_position = klog_buffer.tail;
    stats.dropped_messages = klog_buffer.dropped_messages;
    spinlock_unlock_irqrestore(&klog_buffer.lock, flags);

    return stats;
}