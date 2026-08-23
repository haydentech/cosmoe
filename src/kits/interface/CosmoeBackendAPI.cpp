/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * C API wrapper implementation - bridges C code to C++ backend
 */

#include "CosmoeBackendAPI.h"
#include "CosmoeBackend.h"

#include <string.h>
#include <cstdio>

using namespace BPrivate;

// Helper to get backend instance
static CosmoeBackend* GetBackend()
{
	CosmoeBackendFactory* factory = CosmoeBackendFactory::Instance();
	return factory->GetBackend(BACKEND_AUTO);
}


// Display management
cosmoe_display_t
cosmoe_display_create(int* argc, char** argv)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return backend->DisplayCreate(argc, argv);
}


void
cosmoe_display_destroy(cosmoe_display_t display)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplayDestroy((backend_display_t)display);
}


void
cosmoe_display_run(cosmoe_display_t display)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplayRun((backend_display_t)display);
}


void cosmoe_display_exit(cosmoe_display_t display)
{
	if (GetBackend())
		GetBackend()->DisplayExit((backend_display_t)display);
}


void
cosmoe_display_trigger_redraw(cosmoe_display_t display,
			      cosmoe_window_t window,
			      cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->DisplayTriggerRedraw((backend_display_t)display,
					     (backend_window_t)window,
					     (backend_widget_t)widget);
	}
}

void
cosmoe_display_get_screen_dimensions(cosmoe_display_t display, cosmoe_rectangle* allocation)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplayGetScreenDimensions((backend_display_t)display, allocation);
}


void
cosmoe_display_set_port(cosmoe_display_t display, int32_t sender_port_id, int32_t receiver_port_id)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplaySetPort((backend_display_t)display, sender_port_id, receiver_port_id);
}


int32_t
cosmoe_display_convert_cursor(int32_t beCursorID)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return beCursorID;
	return backend->DisplayConvertCursor(beCursorID);
}


int32_t
cosmoe_display_create_custom_cursor(const uint8_t* bits,
	size_t bitsLength, int32_t width, int32_t height,
	int32_t bytesPerRow, int32_t colorSpace,
	int32_t hotX, int32_t hotY)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return B_NO_INIT;

	return backend->DisplayCreateCustomCursor(bits, bitsLength, width,
		height, bytesPerRow, colorSpace, hotX, hotY);
}


status_t
cosmoe_display_delete_custom_cursor(int32_t backendCursorID)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return B_NO_INIT;

	return backend->DisplayDeleteCustomCursor(backendCursorID);
}


int
cosmoe_display_set_clipboard_text(cosmoe_display_t display, const char* text, size_t length)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return -1;
	return backend->DisplaySetClipboardText((backend_display_t)display, text, length);
}


char*
cosmoe_display_get_clipboard_text(cosmoe_display_t display, size_t* out_length)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL) {
		if (out_length)
			*out_length = 0;
		return NULL;
	}
	return backend->DisplayGetClipboardText((backend_display_t)display, out_length);
}


int32_t
cosmoe_display_get_app_list(cosmoe_display_t display, int32_t* team_ids,
	int32_t max_count)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL || max_count <= 0 || team_ids == NULL)
		return 0;

	return backend->DisplayGetAppList((backend_display_t)display, team_ids,
		max_count);
}


status_t
cosmoe_display_set_app_watcher(cosmoe_display_t display,
	cosmoe_app_watcher_t watcher, void* user_data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL || watcher == NULL)
		return B_BAD_VALUE;

	return backend->DisplaySetAppWatcher((backend_display_t)display,
		(app_watcher_t)watcher, user_data);
}


status_t
cosmoe_display_clear_app_watcher(cosmoe_display_t display)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL)
		return B_BAD_VALUE;

	return backend->DisplayClearAppWatcher((backend_display_t)display);
}


status_t
cosmoe_display_get_app_info(cosmoe_display_t display, int32_t team_id,
	cosmoe_backend_app_info* info)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL || info == NULL)
		return B_BAD_VALUE;

	return backend->DisplayGetAppInfo((backend_display_t)display, team_id,
		info);
}


int32_t
cosmoe_display_get_window_list(cosmoe_display_t display, int32_t* window_ids,
	int32_t max_count)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL || window_ids == NULL
		|| max_count <= 0) {
		return 0;
	}

	return backend->DisplayGetWindowList((backend_display_t)display,
		window_ids, max_count);
}


status_t
cosmoe_display_get_window_info(cosmoe_display_t display, int32_t window_id,
	cosmoe_backend_window_info* info)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL || info == NULL)
		return B_BAD_VALUE;

	return backend->DisplayGetWindowInfo((backend_display_t)display,
		window_id, info);
}


status_t
cosmoe_display_activate_window(cosmoe_display_t display, int32_t window_id)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL)
		return B_BAD_VALUE;

	return backend->DisplayActivateWindow((backend_display_t)display,
		window_id);
}


status_t
cosmoe_display_minimize_window(cosmoe_display_t display, int32_t window_id,
	bool minimize)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL)
		return B_BAD_VALUE;

	return backend->DisplayMinimizeWindow((backend_display_t)display,
		window_id, minimize);
}


status_t
cosmoe_display_close_window(cosmoe_display_t display, int32_t window_id)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || display == NULL)
		return B_BAD_VALUE;

	return backend->DisplayCloseWindow((backend_display_t)display,
		window_id);
}


status_t
cosmoe_backend_get_current_keymap(char** keymap_text, size_t* keymap_length)
{
	if (keymap_text != NULL)
		*keymap_text = NULL;
	if (keymap_length != NULL)
		*keymap_length = 0;

	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return B_NO_INIT;

	return backend->GetCurrentKeymap(keymap_text, keymap_length);
}


status_t
cosmoe_backend_get_keymap_settings(char** layout, char** variant,
	char** options, char** model)
{
	if (layout != NULL)
		*layout = NULL;
	if (variant != NULL)
		*variant = NULL;
	if (options != NULL)
		*options = NULL;
	if (model != NULL)
		*model = NULL;

	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return B_NO_INIT;

	return backend->GetKeymapSettings(layout, variant, options, model);
}


status_t
cosmoe_backend_set_keymap(const char* layout, const char* variant,
	const char* options, const char* model)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return B_NO_INIT;

	return backend->SetKeymap(layout, variant, options, model);
}


// Window management — all functions take (display, token) instead of raw window pointer.
// The backend looks up struct window* from the token internally.

// Helper: look up the backend window pointer for a token (used internally)
static BPrivate::backend_window_t
WindowFromToken(cosmoe_display_t display, int32_t token)
{
	CosmoeBackend* backend = GetBackend();
	if (!backend || !display || token < 0)
		return NULL;
	BPrivate::backend_window_t result = backend->WindowLookupByToken((BPrivate::backend_display_t)display, token);
	return result;
}

cairo_surface_t*
cosmoe_window_get_surface(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		return backend->WindowGetSurface(win);
	return NULL;
}

void
cosmoe_window_get_topview_offset(cosmoe_display_t display, int32_t token,
				  int32_t* offset_h, int32_t* offset_v)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowGetTopviewOffset(win, offset_h, offset_v);
}

void
cosmoe_widget_set_redraw_handler(cosmoe_widget_t widget,
				 cosmoe_redraw_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetRedrawHandler((BPrivate::backend_widget_t)widget, (BPrivate::redraw_handler_t)handler);
	}
}


void
cosmoe_widget_set_button_handler(cosmoe_widget_t widget,
				 cosmoe_button_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetButtonHandler((BPrivate::backend_widget_t)widget, (BPrivate::button_handler_t)handler);
	}
}


void
cosmoe_widget_set_motion_handler(cosmoe_widget_t widget,
				 cosmoe_motion_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetMotionHandler((BPrivate::backend_widget_t)widget, (BPrivate::motion_handler_t)handler);
	}
}


void
cosmoe_widget_set_axis_handler(cosmoe_widget_t widget,
			       cosmoe_axis_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetAxisHandler((BPrivate::backend_widget_t)widget, (BPrivate::axis_handler_t)handler);
	}
}


void
cosmoe_widget_get_allocation(cosmoe_widget_t widget,
			      struct rectangle* allocation)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetGetAllocation((backend_widget_t)widget, allocation);
	}
}


void
cosmoe_widget_set_allocation(cosmoe_widget_t widget,
			     int32_t x, int32_t y,
			     int32_t width, int32_t height)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetAllocation((backend_widget_t)widget, x, y, width, height);
	}
}


void
cosmoe_input_get_position(void* input, int32_t* x, int32_t* y)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->InputGetPosition(input, x, y);
}

cairo_t*
cosmoe_widget_cairo_create(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return backend->WidgetCairoCreate((backend_widget_t)widget);
}


// Backend control
const char*
cosmoe_backend_get_current_name()
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return "none";
	return backend->GetName();
}


bool
cosmoe_backend_supports_native_menus()
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return false;

	backend_type type = backend->GetType();
	return type == BACKEND_COCOA || type == BACKEND_WINDOWS;
}


// Display scaling support
void
cosmoe_window_set_buffer_scale(cosmoe_display_t display, int32_t token, int32_t scale)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowSetBufferScale(win, scale);
}


int32_t
cosmoe_window_get_display_scale(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		return backend->WindowGetDisplayScale(win);
	return 100;
}


status_t
cosmoe_window_set_native_menubar(cosmoe_display_t display, int32_t token,
	const cosmoe_native_menu_item* items, int32_t count,
	cosmoe_window_menu_func_t func, void* user_data)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || win == NULL || items == NULL || count < 0)
		return B_BAD_VALUE;

	return backend->WindowSetNativeMenuBar(win, items, count,
		(window_menu_func_t)func, user_data);
}


status_t
cosmoe_window_clear_native_menubar(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || win == NULL)
		return B_BAD_VALUE;

	return backend->WindowClearNativeMenuBar(win);
}
