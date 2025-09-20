/*
 * Simple userspace kernel terminal server
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "gdebugterm.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

constexpr uintptr_t FB_VIRT_ADDR = 0x500000000;

static void banner(const char *banner, const uint16_t term_cols) {
    const uint16_t banner_width = strlen(banner);
    const uint16_t banner_space = (term_cols - banner_width) / 2;

    debugterm_attr(0x20);

    for (uint16_t i = 0; i < term_cols + banner_space; i++) {
        debugterm_putchar(' ');
    }

    debugterm_write((void *)banner, banner_width);

    for (uint16_t i = banner_space + banner_width; i < term_cols; i++) {
        debugterm_putchar(' ');
    }

    for (uint16_t i = 0; i < term_cols; i++) {
        debugterm_putchar(' ');
    }

    debugterm_putchar('\n');

    debugterm_attr(0x07);
}

static bool init_framebuffer(void) {
    // find fb...
    AnosFramebufferInfo fb_info;
    const SyscallResult fb_result = anos_get_framebuffer_phys(&fb_info);

    if (fb_result.result != SYSCALL_OK) {
        printf("Failed to get framebuffer info from kernel\n");
        return false;
    }

    // map fb into our address space...
    const size_t fb_size = fb_info.height * fb_info.pitch;
    const SyscallResult map_result = anos_map_physical(
            fb_info.physical_address, (void *)FB_VIRT_ADDR, fb_size,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE);

    if (map_result.result != SYSCALL_OK) {
        printf("Failed to map real framebuffer memory\n");
        return false;
    }

    debugterm_init((void *)FB_VIRT_ADDR, fb_info.width, fb_info.height);
    debugterm_clear();
    banner("[ Anos Usermode Kernel Terminal #" VERSION " ]",
           debugterm_col_count());

    return true;
}

static void poll_kernel_log(void) {
    static char __attribute__((__aligned__(0x1000))) log_buffer[0x1000];

    const SyscallResult result =
            anos_read_kernel_log(log_buffer, sizeof(log_buffer), 0);

    if (result.result == SYSCALL_OK && result.value > 0) {
        debugterm_write(log_buffer, result.value);
    }
}

int main(const int argc, char **argv) {
    printf("\nKernel Terminal #%s [libanos #%s]\n", VERSION, libanos_version());

    if (!init_framebuffer()) {
        printf("Failed to initialize framebuffer\n");
        return 1;
    }

    while (1) {
        poll_kernel_log();
    }
}