/*
 * stage3 - The slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * The slab allocator sits on top of the fixed block
 * allocator and allocates slabs carved up into
 * blocks of between 32 and 256 bytes.
 *
 * The top block in each slab is reserved for metadata,
 * and slabs form a linked list within the kernel's slab
 * space.
 */

#ifndef __ANOS_KERNEL_PMM_SLAB_ALLOC_H
#define __ANOS_KERNEL_PMM_SLAB_ALLOC_H

#endif //__ANOS_KERNEL_PMM_SLAB_ALLOC_H