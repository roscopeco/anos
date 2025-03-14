/*
 * stage3 - Internal types for IPC message channel
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This shouldn't be included anywhere other than ipc/channel.c,
 * and in tests.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_IPC_CHANNEL_INTERNAL_H
#define __ANOS_KERNEL_IPC_CHANNEL_INTERNAL_H

#include <stdint.h>

#include "spinlock.h"
#include "structs/list.h"
#include "task.h"

typedef struct {
    ListNode this;
    uint64_t cookie;
    uint64_t tag;
    uint64_t arg;
    Task *waiter;
    uint64_t reply;
    bool handled;
} IpcMessage;

typedef struct {
    uint64_t cookie;
    Task *receivers;
    SpinLock *receivers_lock;
    IpcMessage *queue;
    SpinLock *queue_lock;
    uint64_t reserved[3];
} IpcChannel;

static_assert_sizeof(IpcMessage, ==, 64);
static_assert_sizeof(IpcChannel, ==, 64);

#endif //__ANOS_KERNEL_IPC_CHANNEL_INTERNAL_H
