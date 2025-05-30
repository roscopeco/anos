/*
 * stage3 - Debug printing for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "banner.h"

#ifdef SERIAL_TERMINAL
#include "x86_64/kdrivers/serial.h"

// *********************************
// Serial
//

static SerialPort port;

bool debugterm_reinit(char *vram_addr, int unused1, int unused2) {
    if (serial_init(SERIAL_PORT_COM1)) {
        port = SERIAL_PORT_COM1;
    } else {
        port = SERIAL_PORT_DUMMY;
    }
    return true;
}

bool debugterm_init(char *vram_addr, int unused1, int unused2) {
    if (debugterm_reinit(vram_addr, unused1, unused2)) {
        banner();
        return true;
    }

    return false;
}

void debugchar(char chr) {
    if (chr) {
        serial_sendchar(port, chr);
    }
}

void debugchar_np(char chr) {
    if (chr) {
        serial_sendchar(port, chr);
    }
}

void debugstr(char *str) {
    while (*str) {
        debugchar(*str++);
    }
}

void debugstr_len(char *str, int len) {
    for (int i = 0; i < len; i++) {
        debugchar(*str++);
    }
}

void debugattr(uint8_t new_attr) { /* noop */ }
#else

// *********************************
// VGA Terminal
//

#define PHYSICAL(x, y) (((x << 1) + (y * 160)))

static char *vram;
static uint8_t logical_x = 0;
static uint8_t logical_y = 0;
static uint8_t attr = 0x07;

bool debugterm_reinit(char *vram_addr, int unused1, int unused2) {
    vram = vram_addr;
    return true;
}

bool debugterm_init(char *vram_addr, int unused1, int unused2) {
    if (debugterm_reinit(vram_addr, unused1, unused2)) {
        banner();
        return true;
    }

    return false;
}

static inline uint16_t scroll() {
    for (int i = 160; i < 4000; i++) {
        vram[i - 160] = vram[i];
    }
    for (int i = 3840; i < 4000; i += 2) {
        vram[i] = ' ';
        vram[i + 1] = 0x07;
    }
    logical_x = 0;
    logical_y = 24;
    return PHYSICAL(logical_x, logical_y);
}

void debugchar(char chr) {
    uint16_t phys = PHYSICAL(logical_x, logical_y);

    if (phys >= 4000 || logical_y > 24) {
        phys = scroll();
    }

    switch (chr) {
    case 10:
        // TODO scrolling etc!
        logical_y += 1;
        logical_x = 0;
        break;
    default:
        vram[phys] = chr;
        vram[phys + 1] = attr;
        logical_x += 1;
        if (logical_x > 79) {
            logical_y += 1;
            logical_x = 0;
        }
        break;
    }
}

void debugchar_np(char chr) { debugchar(chr); }

void debugstr(char *str) {
    while (*str) {
        debugchar(*str++);
    }
}

void debugstr_len(char *str, int len) {
    for (int i = 0; i < len; i++) {
        debugchar(*str++);
    }
}

void debugattr(uint8_t new_attr) { attr = new_attr; }

#endif