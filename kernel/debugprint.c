/*
 * stage3 - Debug printing for visual debugging
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#define PHYSICAL(x, y)      (( (x << 1) + (y * 160) ))

static char * const vram = (char * const)0xb8000;
static uint8_t logical_x = 0;
static uint8_t logical_y = 0;
static uint8_t attr = 0x07;

void debugchar(char chr) {
    uint16_t phys;

    switch (chr) {
    case 10: 
        // TODO scrolling etc!
        logical_y += 1;
        logical_x = 0;
        break;
    default:
        phys = PHYSICAL(logical_x, logical_y);
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

void debugattr(uint8_t new_attr) {
    attr = new_attr;
}
