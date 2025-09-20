/*
 * GUI Compositor Type Definitions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_GUI_TYPES_H
#define __ANOS_GUI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_WINDOWS 64
#define MAX_WINDOW_NAME 32

// GUI message types
typedef enum {
    GUI_MSG_CREATE_WINDOW = 1,
    GUI_MSG_DESTROY_WINDOW = 2,
    GUI_MSG_MOVE_WINDOW = 3,
    GUI_MSG_RESIZE_WINDOW = 4,
    GUI_MSG_UPDATE_BUFFER = 5,
    GUI_MSG_SET_VISIBLE = 6,
    GUI_MSG_SET_FOCUS = 7,
    GUI_MSG_GET_DISPLAY_INFO = 8,
    GUI_MSG_CREATE_TERMINAL = 9
} GuiMessageType;

// Terminal window types
typedef enum {
    TERMINAL_TYPE_KERNEL_LOG = 1,
    TERMINAL_TYPE_SHELL = 2
} TerminalType;

#define MAX_TERMINAL_LINES 50
#define MAX_TERMINAL_COLS 80
#define MAX_LINE_LENGTH 256

// Terminal data structure
typedef struct {
    TerminalType type;
    char lines[MAX_TERMINAL_LINES][MAX_LINE_LENGTH];
    uint32_t line_count;
    uint32_t current_line;
    uint32_t scroll_offset;
    char input_buffer[MAX_LINE_LENGTH];
    uint32_t cursor_pos;
    bool needs_refresh;
} TerminalData;

// Window structure
typedef struct {
    uint64_t window_id;
    uint64_t client_channel;
    int32_t x, y;           // Position
    uint32_t width, height; // Dimensions
    uint32_t z_order;       // Layer depth (0 = bottom)
    bool visible;
    bool needs_redraw;
    char title[MAX_WINDOW_NAME];
    void *buffer;           // Client's window buffer (shared memory)
    size_t buffer_size;     // Size of window buffer
    bool is_terminal;       // True if this is a terminal window
    TerminalData *terminal; // Terminal data (if is_terminal == true)
} Window;

// Compositor state
typedef struct {
    void *framebuffer;      // Main display buffer
    uint32_t width, height; // Display dimensions
    uint32_t pitch;         // Bytes per row
    uint32_t bpp;           // Bits per pixel
    Window windows[MAX_WINDOWS];
    uint32_t window_count;
    uint64_t focused_window;
    bool damage_all; // Full screen redraw needed
} Compositor;

// Base GUI message
typedef struct {
    GuiMessageType msg_type;
} GuiMessage;

// Create window message
typedef struct {
    GuiMessageType msg_type;
    uint32_t width, height;
    int32_t x, y;
    char title[MAX_WINDOW_NAME];
} GuiCreateWindowMessage;

// Create window response
typedef struct {
    uint64_t window_id;
} GuiCreateWindowResponse;

// Destroy window message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
} GuiDestroyWindowMessage;

// Move window message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
    int32_t x, y;
} GuiMoveWindowMessage;

// Resize window message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
    uint32_t width, height;
} GuiResizeWindowMessage;

// Set window visibility message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
    bool visible;
} GuiSetVisibleMessage;

// Set window focus message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
} GuiSetFocusMessage;

// Create terminal message
typedef struct {
    GuiMessageType msg_type;
    TerminalType terminal_type;
    uint32_t width, height;
    int32_t x, y;
    char title[MAX_WINDOW_NAME];
} GuiCreateTerminalMessage;

// Update window buffer message
typedef struct {
    GuiMessageType msg_type;
    uint64_t window_id;
    uint32_t x, y; // Damage rectangle
    uint32_t width, height;
    // Buffer data follows
} GuiUpdateBufferMessage;

// Display info response
typedef struct {
    uint32_t width, height;
    uint32_t bpp;
    uint32_t pitch;
} GuiDisplayInfoResponse;

// Event types for input handling (future)
typedef enum {
    GUI_EVENT_MOUSE_MOVE = 1,
    GUI_EVENT_MOUSE_BUTTON = 2,
    GUI_EVENT_KEY_PRESS = 3,
    GUI_EVENT_KEY_RELEASE = 4,
    GUI_EVENT_WINDOW_CLOSE = 5
} GuiEventType;

// Mouse event
typedef struct {
    GuiEventType event_type;
    int32_t x, y;
    uint32_t buttons; // Bitmask of pressed buttons
} GuiMouseEvent;

// Keyboard event
typedef struct {
    GuiEventType event_type;
    uint32_t scancode;
    uint32_t keycode;
    uint32_t modifiers; // Shift, Ctrl, Alt, etc.
} GuiKeyboardEvent;

#endif