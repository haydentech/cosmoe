/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Win32 backend implementation - wraps Win32 window code
 */

#include "CosmoeBackend.h"
#include <cstdlib>
#include <cstdio>
#include <string.h>
#include <windows.h>

// Include Win32 window header from src/system/win32/
extern "C" {
#include "window.h"
}

// Forward declare the move shim so it can be used by the C++ backend
// class before the shim is defined below.
extern "C" void win32_move_shim(struct window* w, int x, int y, void* user_data);
extern "C" void win32_focus_shim(struct window* w, bool focused, void* user_data);

namespace BPrivate {

class WindowsBackend : public CosmoeBackend {
public:
	WindowsBackend() {}
	virtual ~WindowsBackend() {}

	// Display management
	virtual backend_display_t DisplayCreate(int* argc, char** argv)
	{
		return (backend_display_t)display_create(argc, argv);
	}

	virtual void DisplayDestroy(backend_display_t display)
	{
		// Win32 window.h doesn't have display_destroy yet - may need to add
		// For now, do nothing - Win32 will clean up on exit
		(void)display;
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
		allocation->x = 0;
		allocation->y = 0;
		if (!display) {
			allocation->width = 0;
			allocation->height = 0;
			return;
		}
		display_get_screen_dimensions((struct display*)display, allocation);
	}

	virtual void* DisplayGetUserData(backend_display_t display)
	{
		// Win32 window.h doesn't have user data APIs yet
		// May need to add these, or maintain separate mapping
		(void)display;
		return NULL;
	}

	virtual void DisplaySetUserData(backend_display_t display, void* data)
	{
		// Win32 window.h doesn't have user data APIs yet
		(void)display;
		(void)data;
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		// Convert BeOS cursor IDs to Win32 cursor resource IDs
		// Return as integer that will be used with MAKEINTRESOURCE
		switch (beCursorID) {
			case 1: return 32512;  // IDC_ARROW
			case 2: return 32513;  // IDC_IBEAM
			case 3: return 32515;  // IDC_CROSS
			case 4: return 32649;  // IDC_HAND
			case 5: return 32644;  // IDC_SIZEWE
			case 6: return 32645;  // IDC_SIZENS
			case 7: return 32644;  // IDC_SIZEWE
			case 8: return 32642;  // IDC_SIZENWSE
			case 9: return 32643;  // IDC_SIZENESW
			case 10: return 32646; // IDC_SIZEALL
			case 11: return 32648; // IDC_NO
			default: return 32512; // IDC_ARROW
		}
	}

	virtual void DisplaySetCursor(backend_display_t display, int32_t cursorID)
	{
		(void)display;
		// Win32 uses system cursors, set via SetCursor
		HCURSOR cursor = LoadCursor(NULL, MAKEINTRESOURCE(cursorID));
		if (cursor) {
			SetCursor(cursor);
		}
	}

	// Clipboard
	virtual int32_t DisplaySetClipboardText(backend_display_t display, const char* text, size_t length)
	{
		return display_set_clipboard_text((struct display*)display, text, length);
	}

	virtual char* DisplayGetClipboardText(backend_display_t display, size_t* length)
	{
		return display_get_clipboard_text((struct display*)display, length);
	}

	// Window management
	virtual backend_window_t WindowCreate(backend_display_t display, bool offscreen)
	{
		(void)offscreen; // Windows backend doesn't support offscreen windows yet
		return (backend_window_t)window_create((struct display*)display);
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display,
				       backend_window_t parent_window,
				       int32_t x, int32_t y)
	{
		return (backend_window_t)window_popup_create((struct display*)display,
		                                               (struct window*)parent_window, x, y);
	}
	virtual backend_windowframe_t WindowframeCreate(backend_window_t window, void* data)
	{
		// Windows doesn't use separate window frames - return a dummy value
		(void)window;
		(void)data;
		return (backend_windowframe_t)1; // Non-null placeholder
	}

	virtual void WindowframeSetResizeHandler(backend_window_t window, backend_windowframe_t frame,
					 windowframe_resize_handler_t handler)
	{
		// Windows doesn't use separate window frames
		(void)frame;
		window_set_resize_handler((struct window*)window, (widget_resize_handler_t)handler);
	}
	virtual void WindowDestroy(backend_window_t window, backend_windowframe_t frame)
	{
		// Win32 doesn't use separate window frames - ignore frame parameter
		(void)frame;
		window_deferred_destroy((struct window*)window);
	}

	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		// Win32 doesn't use app IDs
	}

	virtual void WindowSetParent(backend_window_t window, backend_window_t parent_window)
	{
		window_set_parent((struct window*)window, (struct window*)parent_window);
	}

	virtual void WindowScheduleResize(backend_window_t window, backend_windowframe_t frame, int width, int height)
	{
		// Win32 doesn't use separate window frames - ignore frame parameter
		(void)frame;
		window_schedule_resize((struct window*)window, width, height);
	}

	virtual void WindowSetMinMaxAllocation(backend_window_t window,
				      int min_width, int min_height,
				      int max_width, int max_height)
	{
		window_set_min_max_allocation((struct window*)window,
				      min_width, min_height,
				      max_width, max_height);
	}

	virtual void WindowSetKeyHandler(backend_window_t window,
				 key_handler_t handler)
	{
		window_set_key_handler((struct window*)window,
				      (window_key_handler_t)handler);
	}

	virtual void WindowSetCloseHandler(backend_window_t window,
				   close_handler_t handler)
	{
		window_set_close_handler((struct window*)window,
					(window_close_handler_t)handler);
	}

	virtual backend_display_t WindowGetDisplay(backend_window_t window)
	{
		return (backend_display_t)window_get_display((struct window*)window);
	}

	// For compatibility with BeOS/Haiku, this gets the position of the topview, i.e. excluding window decorations
	virtual void WindowGetPosition(backend_window_t window, int32_t* x, int32_t* y)
	{
		int wx = 0, wy = 0;
		window_get_position((struct window*)window, &wx, &wy);
		if (x) *x = wx;
		if (y) *y = wy;
	}

	// For compatibility with BeOS/Haiku, this sets the position of the topview, i.e. excluding window decorations
	virtual void WindowSetPosition(backend_window_t window, int32_t x, int32_t y)
	{
		/* Move the Win32 window to the specified absolute coordinates */
		if (!window) return;
		int dx = 0, dy = 0;
		window_get_decorator_size((struct window*)window, &dx, &dy);
		window_set_position((struct window*)window, x - dx, y - dy);
	}

	virtual void WindowGetDecoratorSize(backend_window_t window, int32_t* borderWidth, int32_t* tabHeight)
	{
		// Delegate to the C implementation in window.c which performs the
		// actual Win32 frame size calculation
		window_get_decorator_size((struct window*)window, borderWidth, tabHeight);
	}

	virtual void WindowSetMoveHandler(backend_window_t window, move_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w) return;

		window_set_move_handler(w, (void (*)(struct window*, int, int, void*))handler, user_data);
	}

	virtual void WindowSetFocusHandler(backend_window_t window, focus_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w) return;

		window_set_focus_handler(w, (void (*)(struct window*, bool, void*))handler, user_data);
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
		// Win32 doesn't expose cairo surfaces through this API yet
		(void)window;
		return NULL;
	}

	virtual void WindowGetTopviewOffset(backend_window_t window,
					    int32_t* offset_h, int32_t* offset_v)
	{
		// Win32 doesn't need topview offsets
		if (offset_h) *offset_h = 0;
		if (offset_v) *offset_v = 0;
	}

	// Widget management
	virtual backend_widget_t WindowAddWidget(backend_window_t window, void* data)
	{
		// Win32 doesn't distinguish between subsurfaces and regular widgets
		// Both just use window_add_widget
		return (backend_widget_t)window_add_widget((struct window*)window, data);
	}

	virtual void WidgetDestroy(backend_widget_t widget)
	{
		widget_deferred_destroy((struct widget*)widget);
	}

	virtual void WidgetSetRedrawHandler(backend_widget_t widget,
					   redraw_handler_t handler)
	{
		widget_set_redraw_handler((struct widget*)widget,
					 (widget_redraw_handler_t)handler);
	}

	virtual void WidgetSetResizeHandler(backend_widget_t widget,
					   resize_handler_t handler)
	{
		widget_set_resize_handler((struct widget*)widget,
					 (widget_resize_handler_t)handler);
	}

	virtual void WidgetSetButtonHandler(backend_widget_t widget,
					   button_handler_t handler)
	{
		widget_set_button_handler((struct widget*)widget,
					 (widget_button_handler_t)handler);
	}

	virtual void WidgetSetMotionHandler(backend_widget_t widget,
					   motion_handler_t handler)
	{
		widget_set_motion_handler((struct widget*)widget,
					 (widget_motion_handler_t)handler);
	}

	virtual void WidgetSetAxisHandler(backend_widget_t widget,
					 axis_handler_t handler)
	{
		widget_set_axis_handler((struct widget*)widget,
				       (widget_axis_handler_t)handler);
	}

	virtual void WidgetSetIdleHandler(backend_widget_t widget,
					 idle_handler_t handler)
	{
		widget_set_idle_handler((struct widget*)widget,
				       (widget_idle_handler_t)handler);
	}

	virtual void WidgetGetAllocation(backend_widget_t widget, struct rectangle* allocation)
	{
		widget_get_allocation((struct widget*)widget, allocation);
	}

	virtual cairo_t* WidgetCairoCreate(backend_widget_t widget)
	{
		return widget_cairo_create((struct widget*)widget);
	}

	virtual void* WidgetGetUserData(backend_widget_t widget)
	{
		return widget_get_user_data((struct widget*)widget);
	}

	virtual void WidgetSetUserData(backend_widget_t widget, void *user_data)
	{
		// Not implemented in Windows backend yet
		(void)widget;
		(void)user_data;
	}

	virtual void WidgetSetAllocation(backend_widget_t widget,
					 int32_t x, int32_t y, int32_t width, int32_t height)
	{
		// Not implemented in Windows backend yet
		(void)widget;
		(void)x; (void)y; (void)width; (void)height;
	}

	virtual void WidgetScheduleResize(backend_widget_t widget, int32_t width, int32_t height)
	{
		// Not implemented in Windows backend yet
		(void)widget;
		(void)width; (void)height;
	}

	virtual backend_window_t WidgetGetWindow(backend_widget_t widget)
	{
		return (backend_window_t)widget_get_window((struct widget*)widget);
	}

	virtual void WindowGetMousePosition(backend_window_t window, int32_t* x, int32_t* y)
	{
		window_get_mouse_position((struct window*)window, x, y);
	}

	virtual void WidgetScheduleRedraw(backend_widget_t widget)
	{
		widget_schedule_redraw((struct widget*)widget);
	}

	virtual void WindowSetResizeHandler(backend_window_t window, resize_handler_t handler)
	{
		window_set_resize_handler((struct window*)window, (widget_resize_handler_t)handler);
	}

	virtual void InputGetPosition(void* input, int32_t* x, int32_t* y)
	{
		// In Windows, input is actually a widget pointer (same as X11)
		// We get the mouse position from the widget's window
		if (input) {
			struct widget* widget = (struct widget*)input;
			struct window* window = widget_get_window(widget);
			if (window) {
				window_get_mouse_position(window, x, y);
				printf("InputGetPosition: returning x=%d, y=%d\n", x ? *x : -1, y ? *y : -1);
				return;
			}
		}
		// Fallback
		if (x) *x = 0;
		if (y) *y = 0;
	}

	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale)
	{
		// Windows doesn't use Wayland-style buffer scaling
		// DPI scaling is handled differently on Windows
		(void)window;
		(void)scale;
	}

	virtual void WidgetSetBufferScale(backend_widget_t widget, int32_t scale)
	{
		// Windows doesn't use Wayland-style buffer scaling
		(void)widget;
		(void)scale;
	}

	virtual int32_t WindowGetDisplayScale(backend_window_t window)
	{
		// For now return 1 (100% scaling)
		// Could implement using GetDpiForWindow() on Windows 10+
		(void)window;
		return 1;
	}

	virtual backend_type GetType() const
	{
		return BACKEND_WINDOWS;
	}

	virtual const char* GetName() const
	{
		return "Windows";
	}
};


// The exported creation function
extern "C" CosmoeBackend* CreateCosmoeBackend()
{
	return new WindowsBackend();
}

} // namespace BPrivate

// Shim functions for move and focus handlers
// These are needed because the C window API uses function pointers with
// a specific signature, but the C++ backend uses a different signature
extern "C" void win32_move_shim(struct window* w, int x, int y, void* user_data)
{
	// Forward to the C++ handler stored in user_data
	typedef void (*cpp_move_handler)(BPrivate::backend_window_t, int32_t, int32_t, void*);
	void* handler_ptr = window_get_move_handler_data(w);
	void* handler_user_data = window_get_move_user_data(w);
	
	if (handler_ptr) {
		cpp_move_handler handler = (cpp_move_handler)handler_ptr;
		handler((BPrivate::backend_window_t)w, x, y, handler_user_data);
	}
}

extern "C" void win32_focus_shim(struct window* w, bool focused, void* user_data)
{
	// Forward to the C++ handler stored in user_data
	typedef void (*cpp_focus_handler)(BPrivate::backend_window_t, bool, void*);
	void* handler_ptr = window_get_focus_handler_data(w);
	void* handler_user_data = window_get_focus_user_data(w);
	
	if (handler_ptr) {
		cpp_focus_handler handler = (cpp_focus_handler)handler_ptr;
		handler((BPrivate::backend_window_t)w, focused, handler_user_data);
	}
}
