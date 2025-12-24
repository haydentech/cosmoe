/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * macOS Cocoa backend implementation
 * 
 * NOTE: This is a preliminary implementation that cannot be fully tested on Linux.
 * Cocoa-specific features may need refinement when built on macOS.
 */

#define COSMOE_NO_SUPPORT_TYPES 1
#define COSMOE_NO_THREAD_INFO 1
#if defined(__APPLE__)
#include <private/support/apple_compat.h>
#endif
#include <stddef.h>
#include "CosmoeBackend.h"
#include <cstdlib>
#include <string.h>

extern "C" {
#include "window.h"
}

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#endif

namespace BPrivate {

class CocoaBackend : public CosmoeBackend {
public:
	CocoaBackend() {}
	virtual ~CocoaBackend() {}

	// Display management
	virtual backend_display_t DisplayCreate(int* argc, char** argv)
	{
		return (backend_display_t)display_create(argc, argv);
	}

	virtual void DisplayDestroy(backend_display_t display)
	{
		display_destroy((struct display*)display);
	}

	virtual void DisplayRun(backend_display_t display)
	{
		display_run((struct display*)display);
	}

	virtual void DisplayExit(backend_display_t display)
	{
		display_exit((struct display*)display);
	}

	virtual void DisplayFlush(backend_display_t display)
	{
		display_flush((struct display*)display);
	}

	virtual void DisplayTriggerRedraw(backend_display_t display,
				 backend_window_t window,
				 backend_widget_t widget)
	{
		display_trigger_redraw((struct display*)display,
				      (struct window*)window,
				      (struct widget*)widget);
	}

	virtual void DisplayGetScreenDimensions(backend_display_t display, struct rectangle* allocation)
	{
		if (!allocation) return;
		if (!display) {
			allocation->x = 0;
			allocation->y = 0;
			allocation->width = 0;
			allocation->height = 0;
			return;
		}
		display_get_screen_dimensions((struct display*)display, allocation);
	}

	virtual void* DisplayGetUserData(backend_display_t display)
	{
		return display_get_user_data((struct display*)display);
	}

	virtual void DisplaySetUserData(backend_display_t display, void* data)
	{
		display_set_user_data((struct display*)display, data);
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		return display_convert_cursor(beCursorID);
	}

	// Clipboard management
	virtual int DisplaySetClipboardText(backend_display_t display, const char* text, size_t length)
	{
		return display_set_clipboard_text((struct display*)display, text, length);
	}

	virtual char* DisplayGetClipboardText(backend_display_t display, size_t* out_length)
	{
		return display_get_clipboard_text((struct display*)display, out_length);
	}

	// Window management
	virtual backend_window_t WindowCreate(backend_display_t display, bool offscreen)
	{
		return (backend_window_t)window_create((struct display*)display, offscreen);
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display, backend_window_t parent_window, int32_t x, int32_t y)
	{
		return (backend_window_t)window_popup_create((struct display*)display,
							     (struct window*)parent_window, x, y);
	}

	virtual void WindowGetPosition(backend_window_t window, int32_t* x, int32_t* y)
	{
		window_get_position((struct window*)window, x, y);
	}

	virtual void WindowSetPosition(backend_window_t window, int32_t x, int32_t y)
	{
		window_set_position((struct window*)window, x, y);
	}

	virtual void WindowGetDecoratorSize(backend_window_t window, int32_t* borderWidth, int32_t* tabHeight)
	{
		window_get_decorator_size((struct window*)window, borderWidth, tabHeight);
	}

	virtual backend_windowframe_t WindowframeCreate(backend_window_t window, void* data)
	{
		return (backend_windowframe_t)windowframe_create((struct window*)window, data);
	}

	virtual void WindowDestroy(backend_window_t window, backend_windowframe_t frame)
	{
		window_destroy((struct window*)window, (struct windowframe*)frame);
	}

	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		window_set_app_id((struct window*)window, appId);
	}

	virtual void WindowSetParent(backend_window_t window, backend_window_t parent_window)
	{
		window_set_parent((struct window*)window, (struct window*)parent_window);
	}

	virtual void WindowScheduleResize(backend_window_t window, backend_windowframe_t frame, int width, int height)
	{
		window_schedule_resize((struct window*)window, (struct windowframe*)frame, width, height);
	}

	virtual void WindowSetMinMaxAllocation(backend_window_t window,
					      int min_width, int min_height,
					      int max_width, int max_height)
	{
		window_set_min_max_allocation((struct window*)window, min_width, min_height,
					     max_width, max_height);
	}

	virtual void WindowSetKeyHandler(backend_window_t window,
					 key_handler_t handler)
	{
		window_set_key_handler((struct window*)window, (cocoa_key_handler_t)handler);
	}

	virtual void WindowSetCloseHandler(backend_window_t window,
					   close_handler_t handler)
	{
		window_set_close_handler((struct window*)window, (cocoa_close_handler_t)handler);
	}

	virtual backend_display_t WindowGetDisplay(backend_window_t window)
	{
		return (backend_display_t)window_get_display((struct window*)window);
	}

	virtual void WindowSetUserData(backend_window_t window, void* data)
	{
		window_set_user_data((struct window*)window, data);
	}

	virtual void* WindowGetUserData(backend_window_t window)
	{
		return window_get_user_data((struct window*)window);
	}

	virtual cairo_surface_t* WindowGetSurface(backend_window_t window)
	{
		// Get the cairo_quartz_surface from the window
		// The surface is created lazily when first requested during a draw operation
		return (cairo_surface_t*)window_get_surface((struct window*)window);
	}

	virtual void WindowGetTopviewOffset(backend_window_t window,
					    int32_t* offset_h, int32_t* offset_v)
	{
		window_get_topview_offset((struct window*)window, offset_h, offset_v);
	}

	// Window frame management
	virtual void WindowframeSetResizeHandler(backend_window_t window, backend_windowframe_t frame,
						 windowframe_resize_handler_t handler)
	{
		windowframe_set_resize_handler((struct window*)window, (struct windowframe*)frame,
					      (cocoa_windowframe_resize_handler_t)handler);
	}

	// Movement callback
	virtual void WindowSetMoveHandler(backend_window_t window, move_handler_t handler, void* user_data)
	{
		window_set_move_handler((struct window*)window, (cocoa_move_handler_t)handler, user_data);
	}
	
	// Focus callback
	virtual void WindowSetFocusHandler(backend_window_t window, focus_handler_t handler, void* user_data)
	{
		window_set_focus_handler((struct window*)window, (cocoa_focus_handler_t)handler, user_data);
	}

	// Show a context menu
	virtual void WindowShowMenu(backend_display_t display, void* input,
				uint32_t time, backend_window_t window, int32_t x, int32_t y,
				window_menu_func_t func, void* user_data,
				const char** entries, int count)
	{
		window_show_menu((struct display*)display, input, time,
				(struct window*)window, x, y,
				(cocoa_window_menu_func_t)func, user_data,
				entries, count);
	}

	// Widget management
	virtual backend_widget_t WidgetCreate(backend_window_t window)
	{
		return (backend_widget_t)widget_create((struct window*)window);
	}

	virtual void WidgetDestroy(backend_widget_t widget)
	{
		widget_destroy((struct widget*)widget);
	}

	virtual backend_widget_t WindowAddWidget(backend_window_t window, void* data)
	{
		// In this project widget creation is the way to add a widget to a window
		// Accepts optional user data pointer.
		struct widget* w = widget_create((struct window*)window);
		if (w) widget_set_user_data(w, data);
		return (backend_widget_t)w;
	}

	virtual backend_window_t WidgetGetWindow(backend_widget_t widget)
	{
		return (backend_window_t)widget_get_window((struct widget*)widget);
	}

	virtual void WidgetSetRedrawHandler(backend_widget_t widget,
					    redraw_handler_t handler)
	{
		widget_set_redraw_handler((struct widget*)widget,
					 (cocoa_redraw_handler_t)handler);
	}

	virtual void WidgetSetResizeHandler(backend_widget_t widget,
					    resize_handler_t handler)
	{
		widget_set_resize_handler((struct widget*)widget,
					 (cocoa_resize_handler_t)handler);
	}

	virtual void WidgetSetButtonHandler(backend_widget_t widget,
					    button_handler_t handler)
	{
		widget_set_button_handler((struct widget*)widget,
					 (cocoa_button_handler_t)handler);
	}

	virtual void WidgetSetMotionHandler(backend_widget_t widget,
					    motion_handler_t handler)
	{
		widget_set_motion_handler((struct widget*)widget,
					 (cocoa_motion_handler_t)handler);
	}

	virtual void WidgetSetAxisHandler(backend_widget_t widget,
					  axis_handler_t handler)
	{
		widget_set_axis_handler((struct widget*)widget,
				       (cocoa_axis_handler_t)handler);
	}

	virtual void WidgetSetIdleHandler(backend_widget_t widget,
					  idle_handler_t handler)
	{
		widget_set_idle_handler((struct widget*)widget,
				       (cocoa_idle_handler_t)handler);
	}

	virtual void WidgetSetUserData(backend_widget_t widget, void* data)
	{
		widget_set_user_data((struct widget*)widget, data);
	}

	virtual void* WidgetGetUserData(backend_widget_t widget)
	{
		return widget_get_user_data((struct widget*)widget);
	}

	virtual void WidgetScheduleRedraw(backend_widget_t widget)
	{
		widget_schedule_redraw((struct widget*)widget);
	}

	virtual void InputGetPosition(void* input, int32_t* x, int32_t* y)
	{
		input_get_position((struct input*)input, x, y);
	}

	virtual cairo_t* WidgetCairoCreate(backend_widget_t widget)
	{
		return widget_cairo_create((struct widget*)widget);
	}

	// Display scaling support
	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale)
	{
		window_set_buffer_scale((struct window*)window, scale);
	}

	virtual void WidgetSetBufferScale(backend_widget_t widget, int32_t scale)
	{
		widget_set_buffer_scale((struct widget*)widget, scale);
	}

	virtual int32_t WindowGetDisplayScale(backend_window_t window)
	{
		return window_get_display_scale((struct window*)window);
	}

	// Backend identification
	virtual backend_type GetType() const
	{
		return BACKEND_COCOA;
	}

	virtual const char* GetName() const
	{
		return "cocoa";
	}

	virtual void WidgetScheduleResize(backend_widget_t widget, int32_t width, int32_t height)
	{
		widget_schedule_resize((struct widget*)widget, width, height);
	}

	virtual void WidgetGetAllocation(backend_widget_t widget, struct rectangle* allocation)
	{
		widget_get_allocation((struct widget*)widget, allocation);
	}

	virtual void WidgetSetAllocation(backend_widget_t widget, int32_t x, int32_t y,
					 int32_t width, int32_t height)
	{
		widget_set_allocation((struct widget*)widget, x, y, width, height);
	}
};

} // namespace BPrivate

// Export C function to create backend instance
extern "C" BPrivate::CosmoeBackend* CreateCosmoeBackend(void)
{
	return new BPrivate::CocoaBackend();
}
