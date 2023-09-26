/*
 * stage3 - Debug printing for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#define PHYSICAL(x, y)      (( (x << 1) + (y * 160) ))

static char *vram;
static uint8_t logical_x = 0;
static uint8_t logical_y = 0;
static uint8_t attr = 0x07;

void debugterm_init(char *vram_addr) {
    vram = vram_addr;
}

static inline uint16_t scroll() {
    for (int i = 160; i < 4000; i++) {
        vram[i - 160] = vram[i];
    }
    for (int i = 3840; i < 4000; i+=2) {
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

void debugattr(uint8_t new_attr) {
    attr = new_attr;
}
