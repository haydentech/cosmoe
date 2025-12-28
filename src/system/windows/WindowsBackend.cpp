/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Win32 backend implementation - wraps Win32 window code
 */

#include "CosmoeBackend.h"
#include <cstdlib>
#include <string.h>

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
		// Convert BeOS cursor IDs to Win32 cursor IDs
		switch (beCursorID) {
			case 1: return (int32_t)IDC_ARROW;       // B_CURSOR_SYSTEM_DEFAULT
			case 2: return (int32_t)IDC_IBEAM;       // B_CURSOR_I_BEAM
			case 3: return (int32_t)IDC_CROSS;       // B_CURSOR_CROSS_HAIR
			case 4: return (int32_t)IDC_HAND;        // B_CURSOR_FOLLOW_LINK
			case 5: return (int32_t)IDC_SIZEWE;      // B_CURSOR_MOVE
			case 6: return (int32_t)IDC_SIZENS;      // B_CURSOR_RESIZE_NORTH_SOUTH
			case 7: return (int32_t)IDC_SIZEWE;      // B_CURSOR_RESIZE_EAST_WEST
			case 8: return (int32_t)IDC_SIZENWSE;    // B_CURSOR_RESIZE_NORTH_EAST_SOUTH_WEST
			case 9: return (int32_t)IDC_SIZENESW;    // B_CURSOR_RESIZE_NORTH_WEST_SOUTH_EAST
			case 10: return (int32_t)IDC_SIZEALL;    // B_CURSOR_RESIZE
			case 11: return (int32_t)IDC_NO;         // B_CURSOR_NOT_ALLOWED
			default: return (int32_t)IDC_ARROW;
		}
	}

	virtual void DisplaySetCursor(backend_display_t display, int32_t cursorID)
	{
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
	virtual backend_window_t WindowCreate(backend_display_t display)
	{
		return (backend_window_t)window_create((struct display*)display);
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display,
				       backend_window_t parent_window,
				       backend_input_t input,
				       uint32_t time, int32_t x, int32_t y,
				       window_menu_func_t func, void* user_data,
				       const char** entries, int count)
	{
		// Win32 integration for menus not implemented yet (no-op)
		(void)display; (void)parent_window; (void)input; (void)time; (void)x; (void)y;
		(void)func; (void)user_data; (void)entries; (void)count;
		return (backend_window_t)window_popup_create((struct display*)display,
		                                               (struct window*)parent_window, x, y);
	}

	virtual void DisplayShowWindowMenu(backend_display_t display,
				backend_window_t window,
				backend_input_t input,
				uint32_t time, backend_window_t parent, int32_t x, int32_t y,
				window_menu_func_t func, void* user_data,
				const char** entries, int count)
	{
		// Win32 integration for menus not implemented yet (no-op)
		(void)display; (void)window; (void)input; (void)time; (void)x; (void)y;
		(void)func; (void)user_data; (void)entries; (void)count;
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
