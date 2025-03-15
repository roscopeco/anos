/*
 * stage3 - Named channels
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_IPC_NAMED_H
#define __ANOS_KERNEL_IPC_NAMED_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Initialize the subsystem (just allocates a hashtable currently)
 */
void named_channel_init(void);

/*
 * Register a name for an existing channel.
 *
 * Maximum length for name is 255 characters.
 *
 * Returns true if registered, false otherwise.
 */
bool named_channel_register(uint64_t cookie, char *name);

/*
 * Find an existing channel by name.
 *
 * Maximum length for name is 255 characters.
 *
 * Returns cookie if registered, zero otherwise.
 */
uint64_t named_channel_find(char *name);

/*
 * Deregister an existing named channel.
 *
 * Returns the channel cookie that was registered if successful,
 * or NULL if no previous registration existed.
 */
uint64_t named_channel_deregister(char *name);

#endif //__ANOS_KERNEL_IPC_NAMED_H
