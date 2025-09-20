/*
 * GUI Compositor Server (guitop)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/device_types.h"
#include "gui_types.h"
#include <anos/syscalls.h>

// Forward declarations
extern void composite_screen(Compositor *comp);

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#ifdef DEBUG_GUI
#define gui_debugf(...) printf(__VA_ARGS__)
#else
#define gui_debugf(...)
#endif

static Compositor compositor = {nullptr};
// Removed unused channel variables
static uint64_t kernel_log_terminal_id = 0;

// Compositor wrapper function
static void composite_windows(void) { composite_screen(&compositor); }

static bool init_framebuffer(void) {
    // find fb...
    AnosFramebufferInfo fb_info;
    const SyscallResult fb_result = anos_get_framebuffer_phys(&fb_info);

    if (fb_result.result != SYSCALL_OK) {
        printf("Failed to get framebuffer info from kernel\n");
        return false;
    }

    compositor.width = fb_info.width;
    compositor.height = fb_info.height;
    compositor.bpp = fb_info.bpp;
    compositor.pitch = fb_info.pitch;

    // map fb into our address space...
    const size_t fb_size = compositor.height * compositor.pitch;
    const SyscallResult map_result = anos_map_physical(
            fb_info.physical_address, (void *)0x500000000, fb_size,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE);

    if (map_result.result != SYSCALL_OK) {
        printf("Failed to map real framebuffer memory\n");
        return false;
    }

    compositor.framebuffer = (void *)0x500000000;
    compositor.window_count = 0;
    compositor.focused_window = 0;

    // ... and finally clear fb.
    memset(compositor.framebuffer, 0, fb_size);

    gui_debugf(
            "Real framebuffer initialized: %ux%u@%ubpp, pitch=%u, phys=0x%lx\n",
            compositor.width, compositor.height, compositor.bpp,
            compositor.pitch, fb_info.physical_address);

    return true;
}

static uint64_t create_terminal_window(const TerminalType type,
                                       const uint32_t width,
                                       const uint32_t height, const uint32_t x,
                                       const uint32_t y, const char *title) {

    if (compositor.window_count >= MAX_WINDOWS) {
        gui_debugf("Maximum number of windows reached\n");
        return 0;
    }

    static uint64_t next_window_id = 1;
    const uint64_t window_id = next_window_id++;

    Window *window = &compositor.windows[compositor.window_count];
    window->window_id = window_id;
    window->client_channel = 0; // GUI compositor owns terminal windows
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->z_order = compositor.window_count;
    window->visible = true;
    window->needs_redraw = true;
    window->buffer = nullptr;
    window->is_terminal = true;

    // Allocate terminal data (static allocation for now)
    static TerminalData terminal_storage[MAX_WINDOWS];
    static uint32_t terminal_alloc_count = 0;

    if (terminal_alloc_count >= MAX_WINDOWS) {
        gui_debugf("No more terminal storage available\n");
        return 0;
    }

    window->terminal = &terminal_storage[terminal_alloc_count++];

    // Initialize terminal
    window->terminal->type = type;
    window->terminal->line_count = 0;
    window->terminal->current_line = 0;
    window->terminal->scroll_offset = 0;
    window->terminal->cursor_pos = 0;
    window->terminal->needs_refresh = true;
    memset(window->terminal->lines, 0, sizeof(window->terminal->lines));
    memset(window->terminal->input_buffer, 0,
           sizeof(window->terminal->input_buffer));

    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = '\0';

    compositor.window_count++;

    gui_debugf("Created terminal window %lu: %ux%u at (%d,%d) type=%u\n",
               window_id, width, height, x, y, type);

    return window_id;
}

static void terminal_add_line(TerminalData *terminal, const char *line) {
    if (!terminal || !line)
        return;

    // If we're at max lines, shift everything up
    if (terminal->line_count >= MAX_TERMINAL_LINES) {
        for (uint32_t i = 0; i < MAX_TERMINAL_LINES - 1; i++) {
            strcpy(terminal->lines[i], terminal->lines[i + 1]);
        }
        terminal->current_line = MAX_TERMINAL_LINES - 1;
    } else {
        terminal->current_line = terminal->line_count;
        terminal->line_count++;
    }

    // Copy the new line, ensuring null termination
    size_t len = strlen(line);
    if (len >= MAX_LINE_LENGTH) {
        len = MAX_LINE_LENGTH - 1;
    }
    memcpy(terminal->lines[terminal->current_line], line, len);
    terminal->lines[terminal->current_line][len] = '\0';

    gui_debugf("GUI: Terminal now has %u lines, current_line=%u\n",
               terminal->line_count, terminal->current_line);

    terminal->needs_refresh = true;
}

static void process_kernel_log_data(const char *data, const size_t size) {
    if (!kernel_log_terminal_id)
        return;

    // Find the terminal window
    Window *terminal_window = nullptr;
    for (uint32_t i = 0; i < compositor.window_count; i++) {
        if (compositor.windows[i].window_id == kernel_log_terminal_id) {
            terminal_window = &compositor.windows[i];
            break;
        }
    }

    if (!terminal_window || !terminal_window->terminal)
        return;

    // Process data character by character to build lines
    static char current_line[MAX_LINE_LENGTH] = {0};
    static uint32_t line_pos = 0;

    for (size_t i = 0; i < size; i++) {
        char c = data[i];

        if (c == '\n' || line_pos >= MAX_LINE_LENGTH - 1) {
            // End of line - add it to terminal
            current_line[line_pos] = '\0';
            terminal_add_line(terminal_window->terminal, current_line);
            line_pos = 0;
            current_line[0] = '\0';
        } else if (c >= 32 || c == '\t') { // Printable chars + tab
            current_line[line_pos++] = c;
        }
        // Skip other control characters
    }

    // If we have partial line data, keep it for next time
    current_line[line_pos] = '\0';
}

static void poll_kernel_log(void) {
    static char __attribute__((__aligned__(0x1000))) log_buffer[0x1000];

    gui_debugf("GUI: About to poll kernel log...\n");
    const SyscallResult result =
            anos_read_kernel_log(log_buffer, sizeof(log_buffer), 0);
    gui_debugf("GUI: Back from syscall\n");

    gui_debugf("GUI: poll_kernel_log result=%ld, value=%lu\n", result.result,
               result.value);

    if (result.result == SYSCALL_OK && result.value > 0) {
        gui_debugf("GUI: Got %lu bytes from kernel log\n", result.value);
        process_kernel_log_data(log_buffer, result.value);

        // Trigger redraw if we got new data
        if (kernel_log_terminal_id) {
            for (uint32_t i = 0; i < compositor.window_count; i++) {
                if (compositor.windows[i].window_id == kernel_log_terminal_id) {
                    compositor.windows[i].needs_redraw = true;
                    gui_debugf("GUI: Marked terminal window for redraw\n");
                    break;
                }
            }
            composite_windows();
        }
    }
}

static uint64_t create_window(const uint64_t client_channel,
                              const uint32_t width, const uint32_t height,
                              const int32_t x, const int32_t y) {
    if (compositor.window_count >= MAX_WINDOWS) {
        gui_debugf("Maximum number of windows reached\n");
        return 0;
    }

    static uint64_t next_window_id = 1;
    const uint64_t window_id = next_window_id++;

    Window *window = &compositor.windows[compositor.window_count];
    window->window_id = window_id;
    window->client_channel = client_channel;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->z_order = compositor.window_count; // Simple z-ordering
    window->visible = true;
    window->needs_redraw = true;
    window->buffer = nullptr; // Client will provide buffer

    compositor.window_count++;

    gui_debugf("Created window %lu: %ux%u at (%d,%d)\n", window_id, width,
               height, x, y);

    return window_id;
}

static bool destroy_window(const uint64_t window_id) {
    for (uint32_t i = 0; i < compositor.window_count; i++) {
        if (compositor.windows[i].window_id == window_id) {
            // Move last window to this slot
            compositor.windows[i] =
                    compositor.windows[compositor.window_count - 1];
            compositor.window_count--;

            gui_debugf("Destroyed window %lu\n", window_id);
            return true;
        }
    }
    return false;
}

static __attribute__((unused)) void
handle_gui_message(const uint64_t msg_cookie, uint64_t tag, void *buffer,
                   const size_t buffer_size) {
    if (buffer_size < sizeof(GuiMessageType)) {
        gui_debugf("Invalid GUI message size\n");
        anos_reply_message(msg_cookie, 0);
        return;
    }

    const GuiMessage *gui_msg = (GuiMessage *)buffer;
    uint64_t result = 0;

    switch (gui_msg->msg_type) {
    case GUI_MSG_CREATE_WINDOW: {
        const GuiCreateWindowMessage *create_msg =
                (GuiCreateWindowMessage *)buffer;

        if (buffer_size >= sizeof(GuiCreateWindowMessage)) {
            result =
                    create_window(tag, // Use sender's channel as client channel
                                  create_msg->width, create_msg->height,
                                  create_msg->x, create_msg->y);
            if (result > 0) {
                composite_windows(); // Redraw after creating window
            }
        }
        break;
    }

    case GUI_MSG_DESTROY_WINDOW: {
        const GuiDestroyWindowMessage *destroy_msg =
                (GuiDestroyWindowMessage *)buffer;

        if (buffer_size >= sizeof(GuiDestroyWindowMessage)) {
            result = destroy_window(destroy_msg->window_id) ? 1 : 0;
            if (result) {
                composite_windows(); // Redraw after destroying window
            }
        }
        break;
    }

    case GUI_MSG_MOVE_WINDOW: {
        const GuiMoveWindowMessage *move_msg = (GuiMoveWindowMessage *)buffer;

        if (buffer_size >= sizeof(GuiMoveWindowMessage)) {
            for (uint32_t i = 0; i < compositor.window_count; i++) {
                if (compositor.windows[i].window_id == move_msg->window_id) {
                    compositor.windows[i].x = move_msg->x;
                    compositor.windows[i].y = move_msg->y;
                    compositor.windows[i].needs_redraw = true;
                    composite_windows();
                    result = 1;
                    break;
                }
            }
        }
        break;
    }

    case GUI_MSG_GET_DISPLAY_INFO: {
        GuiDisplayInfoResponse *info = (GuiDisplayInfoResponse *)buffer;
        info->width = compositor.width;
        info->height = compositor.height;
        info->bpp = compositor.bpp;
        result = sizeof(GuiDisplayInfoResponse);
        break;
    }

    case GUI_MSG_CREATE_TERMINAL: {
        const GuiCreateTerminalMessage *terminal_msg =
                (GuiCreateTerminalMessage *)buffer;

        if (buffer_size >= sizeof(GuiCreateTerminalMessage)) {
            result = create_terminal_window(
                    terminal_msg->terminal_type, terminal_msg->width,
                    terminal_msg->height, terminal_msg->x, terminal_msg->y,
                    terminal_msg->title);
            if (result > 0) {
                composite_windows(); // Redraw after creating terminal
            }
        }
        break;
    }

    default:
        gui_debugf("Unknown GUI message type: %u\n", gui_msg->msg_type);
        break;
    }

    const SyscallResult reply_result = anos_reply_message(msg_cookie, result);
    if (reply_result.result != SYSCALL_OK) {
        gui_debugf("Failed to reply to GUI message\n");
    }
}

int main(const int argc, char **argv) {
    printf("\nGUI Compositor #%s [libanos #%s]\n", VERSION, libanos_version());

    if (!init_framebuffer()) {
        printf("Failed to initialize framebuffer\n");
        return 1;
    }

    // Create initial kernel log terminal window
    kernel_log_terminal_id = create_terminal_window(
            TERMINAL_TYPE_KERNEL_LOG,
            compositor.width - 40, // Full width minus margin
            compositor.height / 2, // Half screen height
            20,                    // X offset
            20,                    // Y offset
            "Kernel Log");

    if (kernel_log_terminal_id == 0) {
        printf("Warning: Failed to create kernel log terminal\n");
    }

    // Initial composition
    composite_windows();

    // Main loop: for now, just poll kernel log periodically
    while (1) {
        poll_kernel_log();
        anos_task_sleep_current_secs(1);
    }
}