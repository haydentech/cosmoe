/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * C API wrapper for window backend - provides stable C interface
 * for use by BApplication, BWindow, etc.
 */

#ifndef _WINDOW_BACKEND_C_API_H_
#define _WINDOW_BACKEND_C_API_H_

#include <stdint.h>
#include <stddef.h>
#include <cairo/cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque types
typedef void* cosmoe_display_t;
typedef void* cosmoe_window_t;
typedef void* cosmoe_windowframe_t;
typedef void* cosmoe_widget_t;

// Use the common rectangle struct
#include "rectangle.h"
typedef struct rectangle cosmoe_rectangle;

// Callback types (C-compatible)
typedef void (*cosmoe_key_handler_t)(cosmoe_window_t window, void* input,
				    uint32_t time, uint32_t key,
				    uint32_t unicode, uint32_t state,
				    void *data);

typedef void (*cosmoe_close_handler_t)(void *data);

typedef void (*cosmoe_redraw_handler_t)(cosmoe_widget_t widget, void *data);

typedef void (*cosmoe_resize_handler_t)(cosmoe_widget_t widget, int32_t width,
					int32_t height, void *data);

typedef void (*cosmoe_button_handler_t)(cosmoe_widget_t widget, void* input,
					uint32_t time, uint32_t button,
					uint32_t state, int32_t x, int32_t y,
					void *data);

typedef void (*cosmoe_motion_handler_t)(cosmoe_widget_t widget, void* input,
					uint32_t time, int32_t x, int32_t y,
					void *data);

typedef void (*cosmoe_axis_handler_t)(cosmoe_widget_t widget, void* input,
				     uint32_t time, uint32_t axis,
				     double value, void *data);

typedef void (*cosmoe_idle_handler_t)(cosmoe_widget_t widget, void* input,
				      uint32_t time, int32_t x, int32_t y,
				      void *data);

// Menu callback
typedef void (*cosmoe_window_menu_func_t)(void* user_data, void* input, int index);


// Display management
cosmoe_display_t cosmoe_display_create(int* argc, char** argv);
void cosmoe_display_destroy(cosmoe_display_t display);
void cosmoe_display_run(cosmoe_display_t display);
void cosmoe_display_exit(cosmoe_display_t display);
void cosmoe_display_flush(cosmoe_display_t display);
void cosmoe_display_trigger_redraw(cosmoe_display_t display,
				   cosmoe_window_t window,
				   cosmoe_widget_t widget);
void cosmoe_display_get_screen_dimensions(cosmoe_display_t display, cosmoe_rectangle* allocation);
void* cosmoe_display_get_user_data(cosmoe_display_t display);
void cosmoe_display_set_user_data(cosmoe_display_t display, void* data);

// Cursor management
int32_t cosmoe_display_convert_cursor(int32_t beCursorID);

// Window management
cosmoe_window_t cosmoe_window_create(cosmoe_display_t display, bool offscreen);
cosmoe_window_t cosmoe_window_popup_create(cosmoe_display_t display, cosmoe_window_t parent_window, int32_t x, int32_t y);
cosmoe_windowframe_t cosmoe_windowframe_create(cosmoe_window_t display, void* data);
void cosmoe_window_destroy(cosmoe_window_t window, cosmoe_windowframe_t frame);
void cosmoe_window_set_title(cosmoe_window_t window, const char* title);
void cosmoe_window_set_appid(cosmoe_window_t window, const char* appId);
void cosmoe_window_schedule_resize(cosmoe_window_t window, cosmoe_windowframe_t, int width, int height);
void cosmoe_window_set_min_max_allocation(cosmoe_window_t window,
					  int min_width, int min_height,
					  int max_width, int max_height);
void cosmoe_window_set_key_handler(cosmoe_window_t window,
				   cosmoe_key_handler_t handler);
void cosmoe_window_set_close_handler(cosmoe_window_t window,
				    cosmoe_close_handler_t handler);
cosmoe_display_t cosmoe_window_get_display(cosmoe_window_t window);
void cosmoe_window_set_user_data(cosmoe_window_t window, void* data);
void* cosmoe_window_get_user_data(cosmoe_window_t window);
cairo_surface_t* cosmoe_window_get_surface(cosmoe_window_t window);
void cosmoe_window_get_topview_offset(cosmoe_window_t window,
				      int32_t* offset_h, int32_t* offset_v);

// Get decorator sizes for a window - returned values are left border width
// and tab/title height in pixels.
void cosmoe_window_get_decorator_size(cosmoe_window_t window, int32_t* borderWidth, int32_t* tabHeight);

// Get window position in screen coordinates. Returns 0,0 on Wayland.
void cosmoe_window_get_position(cosmoe_window_t window, int32_t* x, int32_t* y);

// Set window position in absolute screen coordinates. No-op on Wayland.
void cosmoe_window_set_position(cosmoe_window_t window, int32_t x, int32_t y);

// Display a context menu; provided function will be called with an index
void cosmoe_window_show_menu(cosmoe_display_t display, void* input,
							 uint32_t time, cosmoe_window_t window,
							 int32_t x, int32_t y,
							 cosmoe_window_menu_func_t func, void* user_data,
							 const char** entries, int count);

// Window frame management
void cosmoe_windowframe_set_resize_handler(cosmoe_window_t window, cosmoe_windowframe_t frame,
                                           cosmoe_resize_handler_t handler);

// Move handler
typedef void (*cosmoe_move_handler_t)(cosmoe_window_t window, int32_t x, int32_t y, void* user_data);
void cosmoe_window_set_move_handler(cosmoe_window_t window, cosmoe_move_handler_t handler, void* user_data);

// Focus handler
typedef void (*cosmoe_focus_handler_t)(cosmoe_window_t window, bool focused, void* user_data);
void cosmoe_window_set_focus_handler(cosmoe_window_t window, cosmoe_focus_handler_t handler, void* user_data);

// Widget management
cosmoe_widget_t cosmoe_window_add_widget(cosmoe_window_t window, void* data);
void cosmoe_widget_destroy(cosmoe_widget_t widget);
void cosmoe_widget_set_redraw_handler(cosmoe_widget_t widget,
				      cosmoe_redraw_handler_t handler);
void cosmoe_widget_set_resize_handler(cosmoe_widget_t widget,
				      cosmoe_resize_handler_t handler);
void cosmoe_widget_set_button_handler(cosmoe_widget_t widget,
				      cosmoe_button_handler_t handler);
void cosmoe_widget_set_motion_handler(cosmoe_widget_t widget,
				      cosmoe_motion_handler_t handler);
void cosmoe_widget_set_axis_handler(cosmoe_widget_t widget,
				    cosmoe_axis_handler_t handler);
void cosmoe_widget_set_idle_handler(cosmoe_widget_t widget,
				    cosmoe_idle_handler_t handler);
cosmoe_window_t cosmoe_widget_get_window(cosmoe_widget_t widget);
void cosmoe_widget_get_allocation(cosmoe_widget_t widget, struct rectangle* allocation);
void cosmoe_widget_set_user_data(cosmoe_widget_t widget, void *user_data);
void cosmoe_widget_set_allocation(cosmoe_widget_t widget,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height);
void cosmoe_widget_schedule_resize(cosmoe_widget_t widget,
				   int32_t width, int32_t height);
void cosmoe_widget_schedule_redraw(cosmoe_widget_t widget);

// Input management
void cosmoe_input_get_position(void* input, int32_t* x, int32_t* y);

cairo_t* cosmoe_widget_cairo_create(cosmoe_widget_t widget);
void* cosmoe_widget_get_user_data(cosmoe_widget_t widget);

// Backend control
void cosmoe_backend_set_preferred(const char* backend_name);
const char* cosmoe_backend_get_current_name();

// Display scaling support
void cosmoe_window_set_buffer_scale(cosmoe_window_t window, int32_t scale);
void cosmoe_widget_set_buffer_scale(cosmoe_widget_t widget, int32_t scale);
int32_t cosmoe_window_get_display_scale(cosmoe_window_t window);

// Clipboard support
int cosmoe_display_set_clipboard_text(cosmoe_display_t display, const char* text, size_t length);
char* cosmoe_display_get_clipboard_text(cosmoe_display_t display, size_t* out_length);

#ifdef __cplusplus
}
#endif

#endif // _WINDOW_BACKEND_C_API_H_
