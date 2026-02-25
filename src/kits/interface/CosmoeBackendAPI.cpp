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
#include <ctype.h>

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
cosmoe_display_set_user_data(cosmoe_display_t display, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplaySetUserData((backend_display_t)display, data);
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


// Window management — all functions take (display, token) instead of raw window pointer.
// The backend looks up struct window* from the token internally.

// Helper: look up the backend window pointer for a token (used internally)
static BPrivate::backend_window_t
WindowFromToken(cosmoe_display_t display, int32_t token)
{
	CosmoeBackend* backend = GetBackend();
	if (!backend || token < 0)
		return NULL;
	BPrivate::backend_window_t result = backend->WindowLookupByToken((BPrivate::backend_display_t)display, token);
	return result;
}

void
cosmoe_window_create(cosmoe_display_t display, int32_t token, bool offscreen, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowCreate((BPrivate::backend_display_t)display, token, offscreen, data);
}

void
cosmoe_window_popup_create(cosmoe_display_t display, int32_t token, int32_t parent_token, int32_t x, int32_t y, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowPopupCreate((BPrivate::backend_display_t)display, token, parent_token, x, y, data);
}


void
cosmoe_window_destroy(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowDestroy(win);
}


void
cosmoe_window_show(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	printf("cosmoe_window_show: token=%d display=%p win=%p backend=%p\n",
		(int)token, display, win, backend);
	if (backend != NULL && win != NULL)
		backend->WindowShow(win);
	else
		printf("cosmoe_window_show: FAILED to show token=%d (win=%s backend=%s)\n",
			(int)token, win?"ok":"NULL", backend?"ok":"NULL");
}


void
cosmoe_window_hide(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowHide(win);
}


void cosmoe_window_set_appid(cosmoe_display_t display, int32_t token, const char* appId)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend && win) {
		// Normalize app signature for icon lookup: take everything after last dash,
		// convert to lowercase, and convert underscores back to dashes.
		char normalizedAppId[256];
		const char* lastDash = strrchr(appId, '-');
		const char* nameToUse = lastDash ? (lastDash + 1) : appId;

		size_t i = 0;
		while (nameToUse[i] && i < sizeof(normalizedAppId) - 1) {
			char c = nameToUse[i];
			normalizedAppId[i] = (c == '_') ? '-' : tolower(c);
			i++;
		}
		normalizedAppId[i] = '\0';
		backend->WindowSetAppId(win, normalizedAppId);
	}
}

void cosmoe_window_set_parent(cosmoe_display_t display, int32_t token, int32_t parent_token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	BPrivate::backend_window_t parent = WindowFromToken(display, parent_token);
	CosmoeBackend* backend = GetBackend();
	if (backend && win)
		backend->WindowSetParent(win, parent);
}

void cosmoe_window_schedule_resize(cosmoe_display_t display, int32_t token, int width, int height)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend && win)
		backend->WindowScheduleResize(win, width, height);
}


void
cosmoe_window_set_key_handler(cosmoe_display_t display, int32_t token,
			      cosmoe_key_handler_t handler)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL) {
		backend->WindowSetKeyHandler(win, (BPrivate::key_handler_t)handler);
	}
}


void
cosmoe_window_set_close_handler(cosmoe_display_t display, int32_t token,
				cosmoe_close_handler_t handler)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL) {
		backend->WindowSetCloseHandler(win, (BPrivate::close_handler_t)handler);
	}
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
cosmoe_window_get_decorator_size(cosmoe_display_t display, int32_t token, int32_t* borderWidth, int32_t* tabHeight)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowGetDecoratorSize(win, borderWidth, tabHeight);
}

void
cosmoe_window_get_position(cosmoe_display_t display, int32_t token, int32_t* x, int32_t* y)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowGetPosition(win, x, y);
}

void
cosmoe_window_set_position(cosmoe_display_t display, int32_t token, int32_t x, int32_t y)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL) {
		backend->WindowSetPosition(win, x, y);
	}
}


void
cosmoe_windowframe_set_resize_handler(cosmoe_display_t display, int32_t token,
					    cosmoe_resize_handler_t handler)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL) {
		backend->WindowframeSetResizeHandler(win,
			(BPrivate::windowframe_resize_handler_t)handler);
	}
}

void
cosmoe_window_set_move_handler(cosmoe_display_t display, int32_t token, cosmoe_move_handler_t handler, void* user_data)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowSetMoveHandler(win, (BPrivate::move_handler_t)handler, user_data);
}

void
cosmoe_window_set_focus_handler(cosmoe_display_t display, int32_t token, cosmoe_focus_handler_t handler, void* user_data)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		backend->WindowSetFocusHandler(win, (BPrivate::focus_handler_t)handler, user_data);
}


// Widget management
cosmoe_widget_t
cosmoe_window_add_widget(cosmoe_display_t display, int32_t token, void* data)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL || win == NULL)
		return NULL;
	return (cosmoe_widget_t)backend->WindowAddWidget(win, data);
}

void
cosmoe_widget_destroy(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WidgetDestroy((BPrivate::backend_widget_t)widget);
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
cosmoe_widget_set_resize_handler(cosmoe_widget_t widget,
				 cosmoe_resize_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetResizeHandler((BPrivate::backend_widget_t)widget, (BPrivate::resize_handler_t)handler);
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
cosmoe_widget_set_idle_handler(cosmoe_widget_t widget,
			       cosmoe_idle_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetIdleHandler((BPrivate::backend_widget_t)widget, (BPrivate::idle_handler_t)handler);
	}
}


cosmoe_window_t
cosmoe_widget_get_window(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		return (cosmoe_window_t)backend->WidgetGetWindow((backend_widget_t)widget);
	}
	return NULL;
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
cosmoe_widget_set_user_data(cosmoe_widget_t widget, void *user_data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetUserData((backend_widget_t)widget, user_data);
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


void
cosmoe_widget_schedule_resize(cosmoe_widget_t widget,
			       int32_t width, int32_t height)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetScheduleResize((backend_widget_t)widget, width, height);
	}
}


void
cosmoe_widget_schedule_redraw(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WidgetScheduleRedraw((backend_widget_t)widget);
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
void
cosmoe_backend_set_preferred(const char* backend_name)
{
	if (backend_name == NULL)
		return;

	backend_type type = BACKEND_AUTO;
	if (strcasecmp(backend_name, "wayland") == 0)
		type = BACKEND_WAYLAND;
	else if (strcasecmp(backend_name, "x11") == 0)
		type = BACKEND_X11;

	CosmoeBackendFactory* factory = CosmoeBackendFactory::Instance();
	factory->SetPreferredBackend(type);
}


const char*
cosmoe_backend_get_current_name()
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return "none";
	return backend->GetName();
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


void
cosmoe_widget_set_buffer_scale(cosmoe_widget_t widget, int32_t scale)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WidgetSetBufferScale((BPrivate::backend_widget_t)widget, scale);
}


int32_t
cosmoe_window_get_display_scale(cosmoe_display_t display, int32_t token)
{
	BPrivate::backend_window_t win = WindowFromToken(display, token);
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL && win != NULL)
		return backend->WindowGetDisplayScale(win);
	return 1;
}
