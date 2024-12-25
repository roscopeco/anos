/*
 * stage3 - The slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "slab/alloc.h"
#include <stdbool.h>
#include <stdint.h>

#define NULL (((void *)0))

bool slab_alloc_init() { return true; }

void *slab_alloc_block() { return NULL; }

void *slab_alloc_blocks() { return NULL; }

void slab_free_block(void *block) {}

void slab_free_blocks(void *first_block) {}