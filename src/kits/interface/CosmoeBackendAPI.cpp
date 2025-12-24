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

void cosmoe_display_flush(cosmoe_display_t display)
{
	if (GetBackend())
		GetBackend()->DisplayFlush((backend_display_t)display);
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


void*
cosmoe_display_get_user_data(cosmoe_display_t display)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return backend->DisplayGetUserData((backend_display_t)display);
}


void
cosmoe_display_set_user_data(cosmoe_display_t display, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->DisplaySetUserData((backend_display_t)display, data);
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


// Window management
cosmoe_window_t
cosmoe_window_create(cosmoe_display_t display, bool offscreen)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return (cosmoe_window_t)backend->WindowCreate((backend_display_t)display, offscreen);
}


cosmoe_window_t
cosmoe_window_popup_create(cosmoe_display_t display, cosmoe_window_t parent_window, int32_t x, int32_t y)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		return backend->WindowPopupCreate((backend_display_t)display, (backend_window_t)parent_window, x, y);
	return NULL;
}


cosmoe_windowframe_t
cosmoe_windowframe_create(cosmoe_window_t window, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		return (cosmoe_windowframe_t)backend->WindowframeCreate((backend_window_t)window, data);

	return NULL;
}


void
cosmoe_window_destroy(cosmoe_window_t window, cosmoe_windowframe_t frame)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowDestroy((backend_window_t)window, (backend_windowframe_t)frame);
}


void
cosmoe_window_set_title(cosmoe_window_t window, const char* title)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowSetTitle((backend_window_t)window, title);
}


void cosmoe_window_set_appid(cosmoe_window_t window, const char* appId)
{
	CosmoeBackend* backend = GetBackend();
	if (backend)
		backend->WindowSetAppId((backend_window_t)window, appId);
}

void cosmoe_window_set_parent(cosmoe_window_t window, cosmoe_window_t parent_window)
{
	CosmoeBackend* backend = GetBackend();
	if (backend)
		backend->WindowSetParent((backend_window_t)window, (backend_window_t)parent_window);
}

void cosmoe_window_schedule_resize(cosmoe_window_t window, cosmoe_windowframe_t frame, int width, int height)
{
	CosmoeBackend* backend = GetBackend();
	if (backend)
		backend->WindowScheduleResize((backend_window_t)window, (backend_windowframe_t)frame, width, height);
}

void
cosmoe_window_set_min_max_allocation(cosmoe_window_t window,
				     int min_width, int min_height,
				     int max_width, int max_height)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WindowSetMinMaxAllocation((backend_window_t)window,
						  min_width, min_height,
						  max_width, max_height);
	}
}


void
cosmoe_window_set_key_handler(cosmoe_window_t window,
			      cosmoe_key_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WindowSetKeyHandler((backend_window_t)window,
					    (key_handler_t)handler);
	}
}


void
cosmoe_window_set_close_handler(cosmoe_window_t window,
				cosmoe_close_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WindowSetCloseHandler((backend_window_t)window,
					      (close_handler_t)handler);
	}
}


cosmoe_display_t
cosmoe_window_get_display(cosmoe_window_t window)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return backend->WindowGetDisplay((backend_window_t)window);
}


void
cosmoe_window_set_user_data(cosmoe_window_t window, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowSetUserData((backend_window_t)window, data);
}


void*
cosmoe_window_get_user_data(cosmoe_window_t window)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		return backend->WindowGetUserData((backend_window_t)window);
	return NULL;
}

cairo_surface_t*
cosmoe_window_get_surface(cosmoe_window_t window)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		return backend->WindowGetSurface((backend_window_t)window);
	return NULL;
}

void
cosmoe_window_get_topview_offset(cosmoe_window_t window,
				  int32_t* offset_h, int32_t* offset_v)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowGetTopviewOffset((backend_window_t)window, offset_h, offset_v);
}

void
cosmoe_window_get_decorator_size(cosmoe_window_t window, int32_t* borderWidth, int32_t* tabHeight)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowGetDecoratorSize((backend_window_t)window, borderWidth, tabHeight);
}

void
cosmoe_window_get_position(cosmoe_window_t window, int32_t* x, int32_t* y)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowGetPosition((backend_window_t)window, x, y);
}

void
cosmoe_window_set_position(cosmoe_window_t window, int32_t x, int32_t y)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WindowSetPosition((backend_window_t)window, x, y);
	}
}


void
cosmoe_window_show_menu(cosmoe_display_t display, void* input,
						uint32_t time, cosmoe_window_t window,
						int32_t x, int32_t y,
						cosmoe_window_menu_func_t func, void* user_data,
						const char** entries, int count)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WindowShowMenu((backend_display_t)display, input, time, (backend_window_t)window,
								x, y, (window_menu_func_t)func, user_data,
								entries, count);
	}
}


void
cosmoe_windowframe_set_resize_handler(cosmoe_window_t window, cosmoe_windowframe_t frame,
					    cosmoe_resize_handler_t handler)
{
    CosmoeBackend* backend = GetBackend();
    if (backend != NULL) {
        backend->WindowframeSetResizeHandler((backend_window_t)window, (backend_windowframe_t)frame,
                                            (windowframe_resize_handler_t)handler);
    }
}

void
cosmoe_window_set_move_handler(cosmoe_window_t window, cosmoe_move_handler_t handler, void* user_data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowSetMoveHandler((backend_window_t)window, (move_handler_t)handler, user_data);
}

void
cosmoe_window_set_focus_handler(cosmoe_window_t window, cosmoe_focus_handler_t handler, void* user_data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowSetFocusHandler((backend_window_t)window, (focus_handler_t)handler, user_data);
}


// Widget management
cosmoe_widget_t
cosmoe_window_add_widget(cosmoe_window_t window, void* data)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return (cosmoe_widget_t)backend->WindowAddWidget((backend_window_t)window, data);
}


void
cosmoe_widget_destroy(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WidgetDestroy((backend_widget_t)widget);
}


void
cosmoe_widget_set_redraw_handler(cosmoe_widget_t widget,
				 cosmoe_redraw_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetRedrawHandler((backend_widget_t)widget, (redraw_handler_t)handler);
	}
}


void
cosmoe_widget_set_resize_handler(cosmoe_widget_t widget,
				 cosmoe_resize_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetResizeHandler((backend_widget_t)widget, (resize_handler_t)handler);
	}
}


void
cosmoe_widget_set_button_handler(cosmoe_widget_t widget,
				 cosmoe_button_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetButtonHandler((backend_widget_t)widget, (button_handler_t)handler);
	}
}


void
cosmoe_widget_set_motion_handler(cosmoe_widget_t widget,
				 cosmoe_motion_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetMotionHandler((backend_widget_t)widget, (motion_handler_t)handler);
	}
}


void
cosmoe_widget_set_axis_handler(cosmoe_widget_t widget,
			       cosmoe_axis_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetAxisHandler((backend_widget_t)widget, (axis_handler_t)handler);
	}
}


void
cosmoe_widget_set_idle_handler(cosmoe_widget_t widget,
			       cosmoe_idle_handler_t handler)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL) {
		backend->WidgetSetIdleHandler((backend_widget_t)widget, (idle_handler_t)handler);
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


void*
cosmoe_widget_get_user_data(cosmoe_widget_t widget)
{
	CosmoeBackend* backend = GetBackend();
	if (backend == NULL)
		return NULL;
	return backend->WidgetGetUserData((backend_widget_t)widget);
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
cosmoe_window_set_buffer_scale(cosmoe_window_t window, int32_t scale)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WindowSetBufferScale((backend_window_t)window, scale);
}


void
cosmoe_widget_set_buffer_scale(cosmoe_widget_t widget, int32_t scale)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		backend->WidgetSetBufferScale((backend_widget_t)widget, scale);
}


int32_t
cosmoe_window_get_display_scale(cosmoe_window_t window)
{
	CosmoeBackend* backend = GetBackend();
	if (backend != NULL)
		return backend->WindowGetDisplayScale((backend_window_t)window);
	return 1;
}
