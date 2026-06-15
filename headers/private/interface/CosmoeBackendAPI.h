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
#include <SupportDefs.h>

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

typedef struct cosmoe_backend_app_info {
	int32_t team_id;
	uint32_t flags;
	char signature[256];
	char name[256];
	char identifier[64];
} cosmoe_backend_app_info;

typedef struct cosmoe_backend_window_info {
	int32_t window_id;
	int32_t team_id;
	uint32_t workspaces;
	uint32_t feel;
	int32_t show_hide_level;
	uint8_t is_mini;
	char name[256];
	char identifier[256];
} cosmoe_backend_window_info;

enum cosmoe_private_window_panel_placement {
	COSMOE_PANEL_PLACEMENT_TOP = 0,
	COSMOE_PANEL_PLACEMENT_BOTTOM,
	COSMOE_PANEL_PLACEMENT_LEFT,
	COSMOE_PANEL_PLACEMENT_RIGHT,
	COSMOE_PANEL_PLACEMENT_LEFT_TOP,
	COSMOE_PANEL_PLACEMENT_RIGHT_TOP,
	COSMOE_PANEL_PLACEMENT_LEFT_BOTTOM,
	COSMOE_PANEL_PLACEMENT_RIGHT_BOTTOM,
};

enum {
	COSMOE_PRIVATE_WINDOW_PANEL_FLAG = 0x01000000,
	COSMOE_PRIVATE_WINDOW_PANEL_PLACEMENT_SHIFT = 25,
	COSMOE_PRIVATE_WINDOW_PANEL_PLACEMENT_MASK = 0x0e000000,
};

enum {
	COSMOE_APP_WATCH_LAUNCHED = 1,
	COSMOE_APP_WATCH_QUIT = 2,
};

typedef void (*cosmoe_app_watcher_t)(cosmoe_display_t display, int32_t event,
	int32_t team_id, void* user_data);

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

enum {
	COSMOE_NATIVE_MENU_ITEM_DISABLED = 0x00000001,
	COSMOE_NATIVE_MENU_ITEM_MARKED = 0x00000002,
	COSMOE_NATIVE_MENU_ITEM_SEPARATOR = 0x00000004,
	COSMOE_NATIVE_MENU_ITEM_SUBMENU = 0x00000008,
};

typedef struct cosmoe_native_menu_item {
	int32_t command_id;
	int32_t parent_id;
	uint32_t flags;
	const char* label;
	const char* shortcut;
} cosmoe_native_menu_item;


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
int32_t cosmoe_display_create_custom_cursor(const uint8_t* bits,
	size_t bitsLength, int32_t width, int32_t height,
	int32_t bytesPerRow, int32_t colorSpace,
	int32_t hotX, int32_t hotY);
status_t cosmoe_display_delete_custom_cursor(int32_t backendCursorID);

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
bool cosmoe_backend_supports_native_menus();

// Display scaling support
void cosmoe_window_set_buffer_scale(cosmoe_display_t display, int32_t token, int32_t scale);
// Returns display scale in percent (100, 200, 300, etc.).
int32_t cosmoe_window_get_display_scale(cosmoe_display_t display, int32_t token);

// Native menu support
status_t cosmoe_window_set_native_menubar(cosmoe_display_t display,
	int32_t token, const cosmoe_native_menu_item* items, int32_t count,
	cosmoe_window_menu_func_t func, void* user_data);
status_t cosmoe_window_clear_native_menubar(cosmoe_display_t display,
	int32_t token);

// Clipboard support
int cosmoe_display_set_clipboard_text(cosmoe_display_t display, const char* text, size_t length);
char* cosmoe_display_get_clipboard_text(cosmoe_display_t display, size_t* out_length);

// Running application list support
int32_t cosmoe_display_get_app_list(cosmoe_display_t display, int32_t* team_ids,
	int32_t max_count);
status_t cosmoe_display_set_app_watcher(cosmoe_display_t display,
	cosmoe_app_watcher_t watcher, void* user_data);
status_t cosmoe_display_clear_app_watcher(cosmoe_display_t display);
status_t cosmoe_display_get_app_info(cosmoe_display_t display, int32_t team_id,
	cosmoe_backend_app_info* info);

int32_t cosmoe_display_get_window_list(cosmoe_display_t display,
	int32_t* window_ids, int32_t max_count);
status_t cosmoe_display_get_window_info(cosmoe_display_t display,
	int32_t window_id, cosmoe_backend_window_info* info);
status_t cosmoe_display_activate_window(cosmoe_display_t display,
	int32_t window_id);
status_t cosmoe_display_minimize_window(cosmoe_display_t display,
	int32_t window_id, bool minimize);
status_t cosmoe_display_close_window(cosmoe_display_t display,
	int32_t window_id);

#ifdef __cplusplus
}
#endif

#endif // _WINDOW_BACKEND_C_API_H_
