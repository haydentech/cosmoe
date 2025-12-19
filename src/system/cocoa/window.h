/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Cocoa window backend for macOS
 * This header defines the C interface to the Cocoa backend
 */

#ifndef _COSMOE_COCOA_WINDOW_H_
#define _COSMOE_COCOA_WINDOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "rectangle.h"

// Forward declarations - opaque types for the C interface
struct display;
struct window;
struct windowframe;
struct widget;

// Callback types matching the backend API
typedef void (*cocoa_key_handler_t)(struct window* window, void* input,
				   uint32_t time, uint32_t key, uint32_t unicode,
				   uint32_t state, void* data);

typedef void (*cocoa_close_handler_t)(void* data);

typedef void (*cocoa_redraw_handler_t)(struct widget* widget, void* data);

typedef void (*cocoa_resize_handler_t)(struct widget* widget, int32_t width,
				       int32_t height, void* data);

typedef void (*cocoa_windowframe_resize_handler_t)(struct windowframe* frame,
						   int32_t width, int32_t height, void* data);

typedef void (*cocoa_move_handler_t)(struct window* window, int32_t x, int32_t y, void* user_data);

typedef void (*cocoa_focus_handler_t)(struct window* window, bool focused, void* user_data);

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

typedef void (*cocoa_window_menu_func_t)(void* user_data, void* input, int index);

// Display management
struct display* display_create(int* argc, char** argv);
void display_destroy(struct display* display);
void display_run(struct display* display);
void display_exit(struct display* display);
void display_flush(struct display* display);
void display_trigger_redraw(struct display* display, struct window* window, struct widget* widget);
void display_get_screen_dimensions(struct display* display, struct rectangle* allocation);
void* display_get_user_data(struct display* display);
void display_set_user_data(struct display* display, void* data);

// Cursor conversion (Be cursor ID to Cocoa cursor)
int32_t display_convert_cursor(int32_t be_cursor_id);

// Clipboard management
int display_set_clipboard_text(struct display* display, const char* text, size_t length);
char* display_get_clipboard_text(struct display* display, size_t* out_length);

// Window management
struct window* window_create(struct display* display, bool offscreen);
struct window* window_popup_create(struct display* display, struct window* parent, int32_t x, int32_t y);
void window_get_position(struct window* window, int32_t* x, int32_t* y);
void window_set_position(struct window* window, int32_t x, int32_t y);
void window_get_decorator_size(struct window* window, int32_t* borderWidth, int32_t* tabHeight);
struct windowframe* windowframe_create(struct window* window, void* data);
void window_destroy(struct window* window, struct windowframe* frame);
void window_set_title(struct window* window, const char* title);
void window_set_app_id(struct window* window, const char* app_id);
void window_schedule_resize(struct window* window, struct windowframe* frame, int width, int height);
void window_set_min_max_allocation(struct window* window, int min_width, int min_height,
				   int max_width, int max_height);
void window_set_key_handler(struct window* window, cocoa_key_handler_t handler);
void window_set_close_handler(struct window* window, cocoa_close_handler_t handler);
struct display* window_get_display(struct window* window);
void window_set_user_data(struct window* window, void* data);
void* window_get_user_data(struct window* window);
void* window_get_surface(struct window* window); // Returns cairo_surface_t* (lazily created from CGContext)
void window_get_topview_offset(struct window* window, int32_t* offset_h, int32_t* offset_v);

// Window frame management
void windowframe_set_resize_handler(struct window* window, struct windowframe* frame,
				    cocoa_windowframe_resize_handler_t handler);

// Movement and focus callbacks
void window_set_move_handler(struct window* window, cocoa_move_handler_t handler, void* user_data);
void window_set_focus_handler(struct window* window, cocoa_focus_handler_t handler, void* user_data);

// Context menu
void window_show_menu(struct display* display, void* input, uint32_t time,
		     struct window* window, int32_t x, int32_t y,
		     cocoa_window_menu_func_t func, void* user_data,
		     const char** entries, int count);

// Widget management
struct widget* widget_create(struct window* window);
void widget_destroy(struct widget* widget);
void widget_set_redraw_handler(struct widget* widget, cocoa_redraw_handler_t handler);
void widget_set_resize_handler(struct widget* widget, cocoa_resize_handler_t handler);
void widget_set_button_handler(struct widget* widget, cocoa_button_handler_t handler);
void widget_set_motion_handler(struct widget* widget, cocoa_motion_handler_t handler);
void widget_set_axis_handler(struct widget* widget, cocoa_axis_handler_t handler);
void widget_set_idle_handler(struct widget* widget, cocoa_idle_handler_t handler);
void widget_set_user_data(struct widget* widget, void* data);
void* widget_get_user_data(struct widget* widget);
void widget_schedule_redraw(struct widget* widget);
void widget_schedule_resize(struct widget* widget, int32_t width, int32_t height);
void widget_get_allocation(struct widget* widget, struct rectangle* allocation);
void widget_set_allocation(struct widget* widget, int32_t x, int32_t y, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif

#endif // _COSMOE_COCOA_WINDOW_H_
