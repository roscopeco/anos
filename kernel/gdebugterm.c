/*
 * stage3 - Graphical debug terminal
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO this is slow as all hell, especially when `debugchar` is called directly.
 * The whole way this early terminal works needs redoing really...
 */

#include <stdbool.h>
#include <stdint.h>

#include "gdebugterm/font.h"

#define BACKBUF_PHYSICAL(x, y) (((x << 1) + (y * line_width_bytes)))
#define FRAMEBUF_PHYSICAL(x, y) (((x) + ((y) * fb_phys_width)))

static uint32_t *fb;
static uint16_t fb_phys_width;
static uint16_t fb_phys_height;
static uint8_t fb_bytes_per_pixel;

static uint8_t backbuf[32768];

static uint16_t line_width_bytes;
static uint16_t display_max;

static uint16_t row_count;
static uint16_t col_count;

static uint16_t logical_x;
static uint16_t logical_y;

static uint8_t attr;

static uint32_t const colors[] = {
        0x00000000, // COLOR_BLACK
        0x000000aa, // COLOR_BLUE
        0x0000aa00, // COLOR_GREEN
        0x0000aaaa, // COLOR_CYAN
        0x00aa0000, // COLOR_RED
        0x00aa00aa, // COLOR_MAGENTA
        0x00aa5500, // COLOR_YELLOW
        0x00eeeeee, // COLOR_WHITE
        0x00707070, // COLOR_BRIGHT_BLACK
        0x000000ee, // COLOR_BRIGHT_BLUE
        0x0000ee00, // COLOR_BRIGHT_GREEN
        0x0000eeee, // COLOR_BRIGHT_CYAN
        0x00ee0000, // COLOR_BRIGHT_RED
        0x00ee00ee, // COLOR_BRIGHT_MAGENTA
        0x00ee7700, // COLOR_BRIGHT_YELLOW
        0x00eeeeee, // COLOR_BRIGHT_WHITE
};

bool debugterm_init(void *_fb, uint16_t phys_width, uint16_t phys_height) {
    fb = _fb;
    fb_phys_width = phys_width;
    fb_phys_height = phys_height;
    fb_bytes_per_pixel = 4; // only support 32bpp currently

    row_count = phys_height / gdebugterm_font_height;
    col_count = phys_width / gdebugterm_font_width;

    line_width_bytes = col_count * 2;
    display_max = line_width_bytes * row_count;

    attr = 7;

    return true;
}

static inline void backend_draw_pixel(int x, int y, uint32_t color) {}

static inline void repaint_char(char c, uint8_t attr, int x, int y) {

    const uint8_t *font_ptr =
            ((uint8_t *)gdebugterm_font) + (c * gdebugterm_font_height);

    for (int dy = 0; dy < gdebugterm_font_height; dy++) {
        for (int dx = 0; dx < gdebugterm_font_width; dx++) {
            if ((*font_ptr & (1 << (gdebugterm_font_width - 1 - dx))) != 0) {
                fb[FRAMEBUF_PHYSICAL(x + dx, y + dy)] = colors[attr & 0xf];
            } else {
                fb[FRAMEBUF_PHYSICAL(x + dx, y + dy)] =
                        colors[(attr & 0xf0) >> 4];
            }
        }

        font_ptr++;
    }
}

static void repaint() {
    int x = 0;
    int y = 0;

    for (int i = 0; i < display_max; i += 2) {
        char c = backbuf[i];
        uint8_t attr = backbuf[i + 1];

        y = i / 2 / col_count * gdebugterm_font_height;
        x = i / 2 % col_count * gdebugterm_font_width;

        repaint_char(c, attr, x, y);
    }
}

static inline uint16_t scroll() {
    for (int i = line_width_bytes; i < display_max; i++) {
        backbuf[i - line_width_bytes] = backbuf[i];
    }
    for (int i = display_max - line_width_bytes; i < display_max; i += 2) {
        backbuf[i] = ' ';
        backbuf[i + 1] = 0x07;
    }
    logical_x = 0;
    logical_y = row_count - 1;
    return BACKBUF_PHYSICAL(logical_x, logical_y);
}

static inline void debugchar_np(char chr) {
    uint16_t phys = BACKBUF_PHYSICAL(logical_x, logical_y);

    if (phys >= display_max || logical_y > row_count) {
        phys = scroll();
    }

    switch (chr) {
    case 10:
        // TODO scrolling etc!
        logical_y += 1;
        logical_x = 0;
        repaint();
        break;
    default:
        backbuf[phys] = chr;
        backbuf[phys + 1] = attr;
        logical_x += 1;
        if (logical_x > col_count - 1) {
            logical_y += 1;
            logical_x = 0;
        }
        break;
    }
}

void debugattr(uint8_t new_attr) { attr = new_attr; }

void debugchar(char chr) {
    debugchar_np(chr);
    repaint();
}

static inline void debugstr_np(const char *str) {
    unsigned char c;

    while ((c = *str++)) {
        debugchar_np(c);
    }
}

void debugstr(const char *str) {
    debugstr_np(str);
    // repaint();
}
