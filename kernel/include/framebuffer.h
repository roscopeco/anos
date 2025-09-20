/*
 * Kernel Framebuffer Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_FRAMEBUFFER_H
#define __ANOS_KERNEL_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uintptr_t physical_address; // Physical address of framebuffer
    uintptr_t virtual_address;  // Kernel virtual address
    uint32_t width;             // Width in pixels
    uint32_t height;            // Height in pixels
    uint32_t pitch;             // Bytes per row
    uint32_t bpp;               // Bits per pixel
    bool initialized;           // Whether framebuffer is set up
} KernelFramebufferInfo;

extern KernelFramebufferInfo kernel_framebuffer;

// Initialize framebuffer information
void framebuffer_set_info(uintptr_t phys_addr, uintptr_t virt_addr,
                          uint32_t width, uint32_t height, uint32_t bpp);

// Get framebuffer information (for syscalls)
bool framebuffer_get_info(KernelFramebufferInfo *info);

#endif