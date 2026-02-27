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

// Forward declarations for Cairo types to avoid including cairo.h
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

#ifdef __cplusplus
extern "C" {
#endif

// Opaque types
typedef void* cosmoe_display_t;
typedef void* cosmoe_window_t;
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

typedef void (*cosmoe_button_handler_t)(cosmoe_widget_t widget, void* input,
					uint32_t time, uint32_t button,
					uint32_t state, int32_t x, int32_t y,
					void *data);

typedef int (*cosmoe_motion_handler_t)(cosmoe_widget_t widget, void* input,
				       uint32_t time, int32_t x, int32_t y,
				       void *data);

typedef void (*cosmoe_axis_handler_t)(cosmoe_widget_t widget, void* input,
				     uint32_t time, uint32_t axis,
				     double value, void *data);

// Menu callback
typedef void (*cosmoe_window_menu_func_t)(void* user_data, void* input, int index);


// Display management
cosmoe_display_t cosmoe_display_create(int* argc, char** argv);
void cosmoe_display_destroy(cosmoe_display_t display);
void cosmoe_display_run(cosmoe_display_t display);
void cosmoe_display_exit(cosmoe_display_t display);
void cosmoe_display_trigger_redraw(cosmoe_display_t display,
				   cosmoe_window_t window,
				   cosmoe_widget_t widget);
void cosmoe_display_get_screen_dimensions(cosmoe_display_t display, cosmoe_rectangle* allocation);
void cosmoe_display_set_port(cosmoe_display_t display, int32_t sender_port_id, int32_t receiver_port_id);

// Cursor management
int32_t cosmoe_display_convert_cursor(int32_t beCursorID);

// Window management — identified by (display, token) instead of raw pointer.
// 'token' is the BWindow object token from get_object_token(bwindow).
// Passing B_NULL_TOKEN (-1) is safe; the call will be a no-op.

cairo_surface_t* cosmoe_window_get_surface(cosmoe_display_t display, int32_t token);
void cosmoe_window_get_topview_offset(cosmoe_display_t display, int32_t token,
				      int32_t* offset_h, int32_t* offset_v);

// Widget management
void cosmoe_widget_set_redraw_handler(cosmoe_widget_t widget,
				      cosmoe_redraw_handler_t handler);
void cosmoe_widget_set_button_handler(cosmoe_widget_t widget,
				      cosmoe_button_handler_t handler);
void cosmoe_widget_set_motion_handler(cosmoe_widget_t widget,
				      cosmoe_motion_handler_t handler);
void cosmoe_widget_set_axis_handler(cosmoe_widget_t widget,
				    cosmoe_axis_handler_t handler);
void cosmoe_widget_get_allocation(cosmoe_widget_t widget, struct rectangle* allocation);
void cosmoe_widget_set_allocation(cosmoe_widget_t widget,
				  int32_t x, int32_t y,
				  int32_t width, int32_t height);

// Input management
void cosmoe_input_get_position(void* input, int32_t* x, int32_t* y);

cairo_t* cosmoe_widget_cairo_create(cosmoe_widget_t widget);

// Backend control
const char* cosmoe_backend_get_current_name();

// Display scaling support
void cosmoe_window_set_buffer_scale(cosmoe_display_t display, int32_t token, int32_t scale);
int32_t cosmoe_window_get_display_scale(cosmoe_display_t display, int32_t token);

// Clipboard support
int cosmoe_display_set_clipboard_text(cosmoe_display_t display, const char* text, size_t length);
char* cosmoe_display_get_clipboard_text(cosmoe_display_t display, size_t* out_length);

#ifdef __cplusplus
}
#endif

#endif // _WINDOW_BACKEND_C_API_H_
