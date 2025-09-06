/*
 * Internal DEVMAN interfaces for testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __DEVMAN_INTERNAL_H
#define __DEVMAN_INTERNAL_H

#include "device_types.h"

#define MAX_DEVICES 256

// Global state exposed for testing
extern DeviceInfo device_registry[MAX_DEVICES];
extern uint32_t device_count;
extern uint64_t next_device_id;

// Core functions exposed for testing
uint64_t register_device(const DeviceInfo *info);
bool unregister_device(const uint64_t device_id);
uint32_t query_devices(const DeviceQueryType query_type,
                       const DeviceType device_type, const uint64_t device_id,
                       DeviceInfo *results, const uint32_t max_results);
void handle_device_message(const uint64_t msg_cookie, void *buffer,
                           const size_t buffer_size);

#endif /* __DEVMAN_INTERNAL_H */