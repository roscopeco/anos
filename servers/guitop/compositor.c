/*
 * GUI Compositor Drawing Functions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "font.h"
#include "gui_types.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG_GUI_DRAW
#define draw_debugf(...) printf(__VA_ARGS__)
#else
#define draw_debugf(...)
#endif

// Basic drawing primitives
static void draw_pixel(const Compositor *comp, const uint32_t x,
                       const uint32_t y, const uint32_t color) {

    if (x >= comp->width || y >= comp->height) {
        return;
    }

    uint32_t *pixel = (uint32_t *)((uint8_t *)comp->framebuffer +
                                   y * comp->pitch + x * sizeof(uint32_t));

    *pixel = color;
}

static void draw_rect(const Compositor *comp, const uint32_t x,
                      const uint32_t y, const uint32_t width,
                      const uint32_t height, const uint32_t color) {

    for (uint32_t dy = 0; dy < height; dy++) {
        for (uint32_t dx = 0; dx < width; dx++) {
            draw_pixel(comp, x + dx, y + dy, color);
        }
    }
}

static void draw_border(const Compositor *comp, const uint32_t x,
                        const uint32_t y, const uint32_t width,
                        const uint32_t height, const uint32_t border_color,
                        const uint32_t thickness) {

    // Top border
    draw_rect(comp, x, y, width, thickness, border_color);

    // Bottom border
    draw_rect(comp, x, y + height - thickness, width, thickness, border_color);

    // Left border
    draw_rect(comp, x, y, thickness, height, border_color);

    // Right border
    draw_rect(comp, x + width - thickness, y, thickness, height, border_color);
}

static void clear_area(const Compositor *comp, const uint32_t x,
                       const uint32_t y, const uint32_t width,
                       const uint32_t height, const uint32_t color) {

    draw_rect(comp, x, y, width, height, color);
}

static void draw_title_bar(const Compositor *comp, Window *window) {

    constexpr uint32_t title_height = 24;
    const uint32_t title_color =
            (window->window_id == comp->focused_window)
                    ? 0x0040A0FF
                    : 0x00606060; // Blue if focused, gray if not

    draw_rect(comp, window->x, window->y - title_height, window->width,
              title_height, title_color);

    draw_border(comp, window->x, window->y - title_height, window->width,
                title_height, 0x00808080, 1);

    // TODO: Draw title text (need font rendering)
    draw_debugf("Would draw title: %s\n", window->title);
}

static void draw_window_frame(const Compositor *comp, const Window *window) {

    constexpr uint32_t border_thickness = 2;
    const uint32_t border_color = (window->window_id == comp->focused_window)
                                          ? 0x00A0A0A0
                                          : 0x00404040;

    draw_border(comp, window->x - border_thickness,
                window->y - border_thickness,
                window->width + 2 * border_thickness,
                window->height + 2 * border_thickness, border_color,
                border_thickness);
}

static void draw_char(const Compositor *comp, const uint32_t x,
                      const uint32_t y, const char c, const uint32_t color) {

    constexpr uint32_t char_width = gdebugterm_font_width;
    constexpr uint32_t char_height = gdebugterm_font_height;

    constexpr uint32_t bg_color = 0x00000000;

    draw_rect(comp, x, y, char_width, char_height, bg_color);

    const uint8_t *font_ptr = gdebugterm_font + (c * gdebugterm_font_height);

    for (int dy = 0; dy < gdebugterm_font_height; dy++) {

        const uint8_t font_byte = font_ptr[dy];

        for (int dx = 0; dx < gdebugterm_font_width; dx++) {
            if (font_byte & (0x80 >> dx)) {
                draw_pixel(comp, x + dx, y + dy, color);
            }
        }
    }
}

static void draw_terminal_content(const Compositor *comp,
                                  const Window *window) {

    if (!window->terminal) {
        draw_debugf("GUI: No terminal data for window %lu\n",
                    window->window_id);
        return;
    }

    constexpr uint32_t char_width = gdebugterm_font_width;
    constexpr uint32_t char_height = gdebugterm_font_height;
    constexpr uint32_t margin = 4;
    constexpr uint32_t text_color = 0x00FFFFFF;

    draw_debugf("GUI: Drawing terminal content, line_count=%u\n",
                window->terminal->line_count);
    constexpr uint32_t bg_color = 0x00000000;

    draw_rect(comp, window->x, window->y, window->width, window->height,
              bg_color);

    const TerminalData *terminal = window->terminal;
    const uint32_t max_lines = window->height / char_height;
    const uint32_t max_cols = (window->width - 2 * margin) / char_width;

    // scrolling...
    uint32_t start_line = 0;
    if (terminal->line_count > max_lines) {
        start_line = terminal->line_count - max_lines;
    }

    for (uint32_t i = 0;
         i < max_lines && (start_line + i) < terminal->line_count; i++) {
        const char *line = terminal->lines[start_line + i];
        const uint32_t line_y = window->y + margin + (i * char_height);

        for (uint32_t j = 0; j < max_cols && line[j] != '\0'; j++) {
            const uint32_t char_x = window->x + margin + (j * char_width);
            draw_char(comp, char_x, line_y, line[j], text_color);
        }
    }

    // Draw cursor if this is the focused terminal
    if (window->window_id == comp->focused_window &&
        terminal->type == TERMINAL_TYPE_KERNEL_LOG) {
        const uint32_t cursor_line = (terminal->line_count < max_lines)
                                             ? terminal->line_count
                                             : max_lines - 1;
        const uint32_t cursor_x = window->x + margin;
        const uint32_t cursor_y =
                window->y + margin + (cursor_line * char_height);

        draw_rect(comp, cursor_x, cursor_y, char_width, 2, text_color);
    }
}

static void draw_window_content(const Compositor *comp, const Window *window) {
    if (window->is_terminal) {
        draw_terminal_content(comp, window);
    } else if (!window->buffer) {

        // No client buffer, fill with default pattern
        const uint32_t base_color = 0x00303030 + (window->window_id * 0x101010);

        // Checkerboard pattern for demo
        for (uint32_t y = 0; y < window->height; y += 32) {
            for (uint32_t x = 0; x < window->width; x += 32) {

                const uint32_t tile_color = ((x / 32 + y / 32) % 2)
                                                    ? base_color
                                                    : (base_color + 0x202020);

                const uint32_t tile_width =
                        (x + 32 > window->width) ? window->width - x : 32;

                const uint32_t tile_height =
                        (y + 32 > window->height) ? window->height - y : 32;

                draw_rect(comp, window->x + x, window->y + y, tile_width,
                          tile_height, tile_color);
            }
        }
    } else {
        // Blit from client buffer
        // TODO: Implement proper buffer blitting with clipping
        draw_debugf("Would blit client buffer for window %lu\n",
                    window->window_id);
    }
}

void composite_screen(Compositor *comp) {
    constexpr uint32_t bg_color = 0x00202020; // Dark gray desktop

    draw_debugf("Compositing screen with %u windows\n", comp->window_count);

    clear_area(comp, 0, 0, comp->width, comp->height, bg_color);

    // Sort windows by z-order (simple bubble sort)
    for (uint32_t i = 0; i < comp->window_count; i++) {
        for (uint32_t j = i + 1; j < comp->window_count; j++) {
            if (comp->windows[i].z_order > comp->windows[j].z_order) {
                const Window temp = comp->windows[i];
                comp->windows[i] = comp->windows[j];
                comp->windows[j] = temp;
            }
        }
    }

    for (uint32_t i = 0; i < comp->window_count; i++) {
        Window *window = &comp->windows[i];
        if (!window->visible)
            continue;

        draw_title_bar(comp, window);
        draw_window_frame(comp, window);

        draw_window_content(comp, window);

        window->needs_redraw = false;
    }

    comp->damage_all = false;
}

void composite_damage_region(Compositor *comp, int32_t x, int32_t y,
                             uint32_t width, uint32_t height) {
    // For now, just do full composite
    // TODO: Implement proper damage tracking and partial redraws
    composite_screen(comp);
}

bool window_contains_point(const Window *window, const uint32_t x,
                           const uint32_t y) {

    constexpr int32_t title_height = 24;
    constexpr int32_t border = 2;

    return (x >= window->x - border &&
            x < window->x + (int32_t)window->width + border &&
            y >= window->y - title_height &&
            y < window->y + (int32_t)window->height + border);
}

Window *find_window_at_point(Compositor *comp, const uint32_t x,
                             const uint32_t y) {

    // Search from top to bottom (reverse z-order)
    for (int32_t i = comp->window_count - 1; i >= 0; i--) {
        Window *window = &comp->windows[i];
        if (window->visible && window_contains_point(window, x, y)) {
            return window;
        }
    }
    return nullptr;
}

void bring_window_to_front(Compositor *comp, const uint64_t window_id) {
    Window *target = nullptr;

    // Find the window
    for (uint32_t i = 0; i < comp->window_count; i++) {
        if (comp->windows[i].window_id == window_id) {
            target = &comp->windows[i];
            break;
        }
    }

    if (!target)
        return;

    // Find the highest z-order
    uint32_t max_z = 0;
    for (uint32_t i = 0; i < comp->window_count; i++) {
        if (comp->windows[i].z_order > max_z) {
            max_z = comp->windows[i].z_order;
        }
    }

    // Set this window to the top
    target->z_order = max_z + 1;
    comp->focused_window = window_id;
    comp->damage_all = true;

    draw_debugf("Brought window %lu to front (z=%u)\n", window_id,
                target->z_order);
}