/* Internal struct mirror for Cocoa Objective-C++ code
 * This file replicates the fields from the platform window/widget
 * implementations so Objective-C++ event handlers can access members
 * without depending on C-only translation units.
 * Keep this minimal and in sync with real implementations.
 */
#ifndef _COCOA_INTERNAL_STRUCTS_H
#define _COCOA_INTERNAL_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>
#include "rectangle.h"

struct display;
struct windowframe;
struct widget;

typedef void (*cocoa_key_handler_t)(struct window* window, void* input,
                   uint32_t time, uint32_t key, uint32_t unicode,
                   uint32_t state, void* data);

typedef void (*cocoa_close_handler_t)(void* data);
typedef void (*cocoa_redraw_handler_t)(struct widget* widget, void* data);
typedef void (*cocoa_resize_handler_t)(struct widget* widget, int32_t width,
                       int32_t height, void* data);
typedef void (*cocoa_windowframe_resize_handler_t)(struct windowframe* frame,
                           int32_t width, int32_t height, void* data);
typedef void (*cocoa_move_handler_t)(struct window* window, int32_t x, int32_t y, void*);
typedef void (*cocoa_focus_handler_t)(struct window* window, bool focused, void*);
typedef void (*cocoa_button_handler_t)(struct widget* widget, void* input,
                      uint32_t time, uint32_t button,
                      uint32_t state, void* data);
typedef void (*cocoa_motion_handler_t)(struct widget* widget, void* input,
                      uint32_t time, float x, float y,
                      void* data);
typedef void (*cocoa_axis_handler_t)(struct widget* widget, void* input,
                    uint32_t time, uint32_t axis,
                    double value, void* data);
typedef void (*cocoa_idle_handler_t)(struct widget* widget, void* input,
                    uint32_t time, int32_t x, int32_t y,
                    void* data);

struct display {
    void* nsapp;
    void* user_data;
    bool running;
    struct window* window_list;
    int screen_width;
    int screen_height;
};

struct window {
    struct display* display;
    void* nswindow;
    struct windowframe* frame;
    struct widget* widget;
    void* user_data;
    cocoa_key_handler_t key_handler;
    cocoa_close_handler_t close_handler;
    cocoa_move_handler_t move_handler;
    cocoa_focus_handler_t focus_handler;
    void* move_user_data;
    void* focus_user_data;
    char* title;
    int32_t x,y;
    int32_t width,height;
    bool is_popup;
    bool is_offscreen;
    struct window* next;
};

struct windowframe {
    struct window* window;
    void* user_data;
    cocoa_windowframe_resize_handler_t resize_handler;
    int32_t width,height;
};

struct widget {
    struct window* window;
    void* nsview;
    void* user_data;
    cocoa_redraw_handler_t redraw_handler;
    cocoa_resize_handler_t resize_handler;
    cocoa_button_handler_t button_handler;
    cocoa_motion_handler_t motion_handler;
    cocoa_axis_handler_t axis_handler;
    cocoa_idle_handler_t idle_handler;
    struct rectangle allocation;
    void* surface;        // cairo_surface_t*
    void* cg_context;     // CGContextRef
};

#endif /* _COCOA_INTERNAL_STRUCTS_H */
