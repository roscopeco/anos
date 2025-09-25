/*
 * Capability cookies
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#define CAPABILITY_IMPL

#include "capabilities.h"
#include "capabilities/map.h"

/* global */
CapabilityMap global_capability_map;

bool capabilities_init(void) { return capability_map_init(&global_capability_map); }
