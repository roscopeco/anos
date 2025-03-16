/*
 * stage3 - IPC message channel
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_IPC_CHANNEL_H
#define __ANOS_KERNEL_IPC_CHANNEL_H

#include <stddef.h>
#include <stdint.h>

void ipc_channel_init(void);

uint64_t ipc_channel_create(void);

void ipc_channel_destroy(uint64_t channel);

bool ipc_channel_exists(uint64_t cookie);

uint64_t ipc_channel_recv(uint64_t cookie, uint64_t *tag, size_t *buffer_size,
                          void *buffer);
uint64_t ipc_channel_send(uint64_t cookie, uint64_t tag, size_t buffer_size,
                          void *buffer);
uint64_t ipc_channel_reply(uint64_t message_cookie, uint64_t result);

#endif //__ANOS_KERNEL_IPC_CHANNEL_H
