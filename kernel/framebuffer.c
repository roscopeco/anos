/*
 * Kernel Framebuffer Management Implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "framebuffer.h"

KernelFramebufferInfo kernel_framebuffer = {0};

void framebuffer_set_info(const uintptr_t phys_addr, const uintptr_t virt_addr,
                          const uint32_t width, const uint32_t height,
                          const uint32_t bpp) {

    kernel_framebuffer.physical_address = phys_addr;
    kernel_framebuffer.virtual_address = virt_addr;
    kernel_framebuffer.width = width;
    kernel_framebuffer.height = height;
    kernel_framebuffer.bpp = bpp;
    kernel_framebuffer.pitch = width * (bpp / 8);
    kernel_framebuffer.initialized = true;
}

bool framebuffer_get_info(KernelFramebufferInfo *info) {
    if (!info || !kernel_framebuffer.initialized) {
        return false;
    }

    *info = kernel_framebuffer;
    return true;
}