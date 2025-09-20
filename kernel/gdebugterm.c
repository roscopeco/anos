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

#include "banner.h"
#include "printdec.h"
#include "printhex.h"
#include "spinlock.h"
#include "std/string.h"

#if USE_BIZCAT_FONT
#include "gdebugterm/bizcat_font.h"
#else
#include "gdebugterm/font.h"
#endif

#define ZERO 48
#define EX 120

#define BACKBUF_PHYSICAL(x, y) (((x << 1) + (y * line_width_bytes)))
#define FRAMEBUF_PHYSICAL(x, y) (((x) + ((y) * fb_phys_width)))

static uint32_t volatile *fb;
static uint16_t fb_phys_width;
static uint16_t fb_phys_height;
static uint8_t fb_bytes_per_pixel;

static uint8_t volatile backbuf[32768];

static uint16_t line_width_bytes;
static uint16_t display_max;

static uint16_t row_count;
static uint16_t col_count;

static uint16_t volatile logical_x;
static uint16_t volatile logical_y;

static uint8_t attr;

static int font_area;

static SpinLock debugterm_lock;

static uint32_t const colors[] = {
        0x00000000, // COLOR_BLACK
        0x000000aa, // COLOR_BLUE
        0x0000aa00, // COLOR_GREEN
        0x0000aaaa, // COLOR_CYAN
        0x00aa0000, // COLOR_RED
        0x00aa00aa, // COLOR_MAGENTA
        0x00aa5500, // COLOR_YELLOW
        0x00bbbbbb, // COLOR_WHITE
        0x00707070, // COLOR_BRIGHT_BLACK
        0x000000ee, // COLOR_BRIGHT_BLUE
        0x0000ee00, // COLOR_BRIGHT_GREEN
        0x0000eeee, // COLOR_BRIGHT_CYAN
        0x00ee0000, // COLOR_BRIGHT_RED
        0x00ee00ee, // COLOR_BRIGHT_MAGENTA
        0x00ee7700, // COLOR_BRIGHT_YELLOW
        0x00eeeeee, // COLOR_BRIGHT_WHITE
};

static const uint8_t bit_masks[8] = {0x80, 0x40, 0x20, 0x10,
                                     0x08, 0x04, 0x02, 0x01};

bool debugterm_reinit(void volatile *_fb, const uint16_t phys_width,
                      const uint16_t phys_height) {
    fb = _fb;
    fb_phys_width = phys_width;
    fb_phys_height = phys_height;
    fb_bytes_per_pixel = 4; // only support 32bpp currently

    row_count = phys_height / gdebugterm_font_height;
    col_count = phys_width / gdebugterm_font_width;

    line_width_bytes = col_count * 2;
    display_max = line_width_bytes * row_count;

    attr = 7;

    font_area = gdebugterm_font_height * gdebugterm_font_width;

    return true;
}

bool debugterm_init(void volatile *_fb, const uint16_t phys_width,
                    const uint16_t phys_height) {
    if (debugterm_reinit(_fb, phys_width, phys_height)) {
        banner();
        return true;
    }

    return false;
}

#define WRITE_PIXEL(n)                                                         \
    *fb_ptr++ = (font_byte & bit_masks[n]) ? fg_color : bg_color

static inline void paint_char(const uint8_t c, uint8_t attr,
                              const int fb_x_base, const int fb_y_base) {

    const uint32_t fg_color = colors[attr & 0xf];
    const uint32_t bg_color = colors[attr >> 4];

    const uint8_t *font_ptr =
            ((uint8_t *)gdebugterm_font) + (c * gdebugterm_font_height);

    uint32_t volatile *fb_char_base =
            &fb[fb_x_base + (fb_y_base * fb_phys_width)];

    for (int dy = 0; dy < gdebugterm_font_height; dy++) {
        const uint8_t font_byte = *font_ptr++;
        uint32_t volatile *fb_ptr = fb_char_base;

        WRITE_PIXEL(0);
        WRITE_PIXEL(1);
        WRITE_PIXEL(2);
        WRITE_PIXEL(3);
        WRITE_PIXEL(4);
        WRITE_PIXEL(5);
        WRITE_PIXEL(6);
        WRITE_PIXEL(7);

        fb_char_base += fb_phys_width;
    }
}

/*
 * Note! This expects the framebuffer is page aligned!
 *
 * It's not the fastest we can do by a long stretch, but the way this
 * whole thing works needs redoing anyhow so I'm not going to over-egg
 * this just now...
 */
static void repaint(void) {
    const int bytes_per_row = col_count << 1; // char + attribute

    const uint8_t volatile *buf_ptr = backbuf;

    for (int row = 0; row < display_max / bytes_per_row; row++) {
        // base Y pos for this row of characters
        const int fb_y_base = row * gdebugterm_font_height;

        // process whole row...
        for (int col = 0; col < col_count; col++) {
            const int fb_x_base = col * gdebugterm_font_width;
            const char c = *buf_ptr++;
            const uint8_t attr = *buf_ptr++;

            paint_char(c, attr, fb_x_base, fb_y_base);
        }
    }
}

static inline uint16_t scroll() {
#ifdef BYTEWISE_SCROLL_DEBUGGING
    for (int i = line_width_bytes; i < display_max; i++) {
        backbuf[i - line_width_bytes] = backbuf[i];
    }
    for (int i = display_max - line_width_bytes; i < display_max; i += 2) {
        backbuf[i] = ' ';
        backbuf[i + 1] = 0x07;
    }
#else
    memmove((void *)backbuf, (void *)&backbuf[line_width_bytes],
            display_max - line_width_bytes);

    uint64_t *p64 = (uint64_t *)(backbuf + display_max - line_width_bytes);
    constexpr uint64_t fill64 = (uint64_t)(' ' | (0x08 << 8)) *
                                0x0001000100010001ULL; // Repeat pattern

    for (size_t i = 0; i < line_width_bytes / 8; ++i) {
        p64[i] = fill64;
    }
#endif

    logical_x = 0;
    logical_y = row_count - 1;
    return BACKBUF_PHYSICAL(logical_x, logical_y);
}

void debugchar_np(const char chr) {
    uint16_t phys = BACKBUF_PHYSICAL(logical_x, logical_y);

    if (phys >= display_max || logical_y > row_count) {
        phys = scroll();
        repaint();
    }

    switch (chr) {
    case 0:
        break;
    case 10:
        // TODO scrolling etc? I _think_ we handle it above but it needs checking...
        logical_y += 1;
        logical_x = 0;
        break;
    default:
        backbuf[phys] = chr;
        backbuf[phys + 1] = attr;

        paint_char(chr, attr, logical_x * gdebugterm_font_width,
                   logical_y * gdebugterm_font_height);

        logical_x += 1;
        if (logical_x > col_count - 1) {
            logical_y += 1;
            logical_x = 0;
        }

        break;
    }
}

void debugattr(const uint8_t new_attr) { attr = new_attr; }

void debugchar(const char chr) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&debugterm_lock);
    debugchar_np(chr);
    spinlock_unlock_irqrestore(&debugterm_lock, lock_flags);
}

static inline void debugstr_np(const char *str) {
    char c;

    while ((c = *str++)) {
        debugchar_np(c);
    }
}

void debugstr(const char *str) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&debugterm_lock);
    debugstr_np(str);
    spinlock_unlock_irqrestore(&debugterm_lock, lock_flags);
}

void debugstr_len(const char *str, const int len) {
    for (int i = 0; i < len; i++) {
        if (str[i] == '\0') {
            break;
        }

        debugchar_np(str[i]);
    }
}

/* printhex */

static inline void digitprint(uint8_t digit) {
    if (digit < 10) {
        digit += 48;
    } else {
        digit += 87;
    }

    debugchar_np(digit);
}

void printhex64(uint64_t num, PrintHexCharHandler __ignored) {
    debugchar_np(ZERO);
    debugchar_np(EX);

    for (int i = 0; i < 64; i += 4) {
        const char digit = (num & 0xF000000000000000) >> 60;
        num <<= 4;
        digitprint(digit);
    }

    debugchar(0);
}

void printhex32(uint64_t num, PrintHexCharHandler __ignored) {
    debugchar_np(ZERO);
    debugchar_np(EX);

    for (int i = 0; i < 32; i += 4) {
        const char digit = (num & 0xF0000000) >> 28;
        num <<= 4;
        digitprint(digit);
    }

    debugchar(0);
}

void printhex16(uint64_t num, PrintHexCharHandler __ignored) {
    debugchar_np(ZERO);
    debugchar_np(EX);

    for (int i = 0; i < 16; i += 4) {
        const char digit = (num & 0xF000) >> 12;
        num <<= 4;
        digitprint(digit);
    }

    debugchar(0);
}

void printhex8(uint64_t num, PrintHexCharHandler __ignored) {
    debugchar_np(ZERO);
    debugchar_np(EX);

    for (int i = 0; i < 8; i += 4) {
        const char digit = (num & 0xF0) >> 4;
        num <<= 4;
        digitprint(digit);
    }

    debugchar(0);
}

/* printdec */

static const char *llmin = "9223372036854775808";

static void print_llong_min(void) {
    const char *c = llmin;

    while (*c) {
        debugchar_np(*c++);
    }
}

void printdec(int64_t num, PrintDecCharHandler __ignored) {
    // Handle negative numbers
    if (num < 0) {
        debugchar_np('-');
        // Handle LLONG_MIN (-9223372036854775808) specially
        if (num == -9223372036854775807LL - 1) {
            print_llong_min();
            return;
        }
        num = -num;
    }

    // Handle case when n is 0
    if (num == 0) {
        debugchar_np('0');
        return;
    }

    // Buffer to store digits in reverse
    char buf[20]; // Max 20 digits for 64-bit int
    char *p = buf;

    // Extract digits in reverse order
    while (num > 0) {
        *p++ = num % 10 + '0';
        num /= 10;
    }

    // Print digits in correct order
    while (p > buf) {
        debugchar_np(*--p);
    }

    debugchar(0);
}