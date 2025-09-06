/*
 * Device registry core functions (extracted for testing)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "device_types.h"

// Device registry state
#define MAX_DEVICES 256
DeviceInfo device_registry[MAX_DEVICES];
uint32_t device_count = 0;
uint64_t next_device_id = 1;

uint64_t register_device(const DeviceInfo *info) {
    if (!info) {
        return 0; // Invalid input
    }

    if (device_count >= MAX_DEVICES) {
        return 0; // Registry full
    }

    const uint64_t device_id = next_device_id++;

    device_registry[device_count] = *info;
    device_registry[device_count].device_id = device_id;
    device_count++;

    return device_id;
}

bool unregister_device(const uint64_t device_id) {
    for (uint32_t i = 0; i < device_count; i++) {
        if (device_registry[i].device_id == device_id) {
            // Move last device to fill gap
            device_registry[i] = device_registry[device_count - 1];
            device_count--;
            return true;
        }
    }
    return false;
}

uint32_t query_devices(const DeviceQueryType query_type,
                       const DeviceType device_type, const uint64_t target_id,
                       DeviceInfo *results, uint32_t max_results) {
    uint32_t found = 0;

    for (uint32_t i = 0; i < device_count && found < max_results; i++) {
        bool match = false;

        switch (query_type) {
        case QUERY_ALL:
            match = true;
            break;
        case QUERY_BY_TYPE:
            match = (device_registry[i].device_type == device_type);
            break;
        case QUERY_BY_ID:
            match = (device_registry[i].device_id == target_id);
            break;
        case QUERY_CHILDREN:
            match = (device_registry[i].parent_id == target_id);
            break;
        }

        if (match) {
            results[found++] = device_registry[i];
        }
    }

    return found;
}