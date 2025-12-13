/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * X11 backend implementation - wraps X11 window code
 */

#include "WindowBackend.h"

// Include X11 window header from src/system/X11/
extern "C" {
#include "window.h"
}

// Forward declare the move shim so it can be used by the C++ backend
// class before the shim is defined below.
extern "C" void x11_move_shim(struct window* w, int x, int y, void* user_data);

namespace BPrivate {

class X11Backend : public WindowBackend {
public:
	X11Backend() {}
	virtual ~X11Backend() {}

	// Display management
	virtual backend_display_t DisplayCreate(int* argc, char** argv)
	{
		return (backend_display_t)display_create(argc, argv);
	}

	virtual void DisplayDestroy(backend_display_t display)
	{
		// X11 window.h doesn't have display_destroy yet - may need to add
		// For now, do nothing - X will clean up on exit
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
		// X11 window.h doesn't have user data APIs yet
		// May need to add these, or maintain separate mapping
		(void)display;
		return NULL;
	}

	virtual void DisplaySetUserData(backend_display_t display, void* data)
	{
		// X11 window.h doesn't have user data APIs yet
		(void)display;
		(void)data;
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		// TODO: Implement X11 cursor mapping
		// For now, just return the Be cursor ID
		return beCursorID;
	}

	// Window management
	virtual backend_window_t WindowCreate(backend_display_t display, bool offscreen)
	{
		// Create X11 window (offscreen parameter currently ignored)
		return (backend_window_t)window_create((struct display*)display);
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t x, int32_t y)
	{
		// Create a borderless override-redirect popup suitable for menus
		return (backend_window_t)window_popup_create((struct display*)display, x, y);
	}

	virtual backend_windowframe_t WindowframeCreate(backend_window_t window, void* data)
	{
		// X11 doesn't have separate window frames - return NULL
		(void)window;
		return NULL;
	}

	virtual void WindowframeSetResizeHandler(backend_window_t window, backend_windowframe_t frame,
						 windowframe_resize_handler_t handler)
	{
		// X11 doesn't have separate window frames - set resize handler on window itself
		(void)frame;
		
		if (window) {
			// In X11, the window itself needs a resize handler, as there is no windowframe widget
			window_set_resize_handler((struct window*)window, (widget_resize_handler_t)handler);
		}
	}

	virtual void WindowShowMenu(backend_display_t display, void* input,
				uint32_t time, backend_window_t window, int32_t x, int32_t y,
				window_menu_func_t func, void* user_data,
				const char** entries, int count)
	{
		// X11 integration for menus not implemented yet (no-op)
		(void)display; (void)window; (void)input; (void)time; (void)x; (void)y;
		(void)func; (void)user_data; (void)entries; (void)count;
	}

	virtual void WindowDestroy(backend_window_t window, backend_windowframe_t frame)
	{
		// X11 doesn't use separate window frames - ignore frame parameter
		(void)frame;
		window_deferred_destroy((struct window*)window);
	}

	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		// X11 doesn't use app IDs
	}

	virtual void WindowScheduleResize(backend_window_t window, backend_windowframe_t frame, int width, int height)
	{
		// X11 doesn't use separate window frames - ignore frame parameter
		(void)frame;
		window_schedule_resize((struct window*)window, width, height);
	}

	virtual void WindowSetMinMaxAllocation(backend_window_t window,
				      int min_width, int min_height,
				      int max_width, int max_height)
	{
		// TODO: Implement X11 window size limits
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
		/* Move the X11 window to the specified absolute coordinates */
		if (!window) return;
		int dx = 0, dy = 0;
		window_get_decorator_size((struct window*)window, &dx, &dy);
		window_set_position((struct window*)window, x - dx, y - dy);
	}

	virtual void WindowGetDecoratorSize(backend_window_t window, int32_t* borderWidth, int32_t* tabHeight)
	{
		// Delegate to the C implementation in window.c which performs the
		// actual X11 frame extents lookup (EWMH _NET_FRAME_EXTENTS) and other
		// fallbacks. This keeps the backend consistent with the C API layer.
		window_get_decorator_size((struct window*)window, borderWidth, tabHeight);
	}


	virtual void WindowSetMoveHandler(backend_window_t window, move_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w) return;

		/* Use the C setter to avoid accessing struct internals from C++ */
		window_set_move_handler(w, &x11_move_shim, (void*)handler, user_data);
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
		// X11 doesn't expose cairo surfaces through this API yet
		(void)window;
		return NULL;
	}

	virtual void WindowGetTopviewOffset(backend_window_t window,
					    int32_t* offset_h, int32_t* offset_v)
	{
		// X11 doesn't need topview offsets
		if (offset_h) *offset_h = 0;
		if (offset_v) *offset_v = 0;
	}

	// Widget management
	virtual backend_widget_t WindowAddWidget(backend_window_t window, void* data)
	{
		// X11 doesn't distinguish between subsurfaces and regular widgets
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
		if (widget) {
			widget_set_redraw_handler((struct widget*)widget, (widget_redraw_handler_t)handler);
		}
	}

	virtual void WidgetSetResizeHandler(backend_widget_t widget,
					   resize_handler_t handler)
	{
		if (widget) {
			widget_set_resize_handler((struct widget*)widget, (widget_resize_handler_t)handler);
		}
	}

	virtual void WidgetSetButtonHandler(backend_widget_t widget,
					   button_handler_t handler)
	{
		widget_set_button_handler((struct widget*)widget, (widget_button_handler_t)handler);
	}


	virtual void WidgetSetMotionHandler(backend_widget_t widget, motion_handler_t handler)
	{
		widget_set_motion_handler((struct widget*)widget, (widget_motion_handler_t)handler);
	}

	virtual void WidgetSetAxisHandler(backend_widget_t widget, axis_handler_t handler)
	{
		widget_set_axis_handler((struct widget*)widget, (widget_axis_handler_t)handler);
	}


	virtual void WidgetGetAllocation(backend_widget_t widget,
					 struct rectangle* allocation) {
		widget_get_allocation((struct widget*)widget, allocation);
	}


	virtual void WidgetScheduleResize(backend_widget_t widget, int32_t width, int32_t height)
	{
		// TODO: Implement in X11 window.c
		(void)widget;
		(void)width;
		(void)height;
	}

	virtual void WidgetScheduleRedraw(backend_widget_t widget)
	{
		widget_schedule_redraw((struct widget*)widget);
	}

	virtual backend_window_t WidgetGetWindow(backend_widget_t widget)
	{
		// TODO: Implement X11 widget get window
		return NULL;
	}

	virtual void WidgetGetAllocation(backend_widget_t widget,
					 int32_t* x, int32_t* y,
					 int32_t* width, int32_t* height) {
		// TODO: Implement X11 widget allocation getter
		if (x) *x = 0;
		if (y) *y = 0;
		if (width) *width = 0;
		if (height) *height = 0;
	}

	virtual void WidgetSetAllocation(backend_widget_t widget,
					 int32_t x, int32_t y,
					 int32_t width, int32_t height)
	{
		// TODO: Implement X11 widget allocation setter
	}

	// Input management
	virtual void InputGetPosition(void* input, int32_t* x, int32_t* y)
	{
		// In X11, input is actually a widget pointer
		// We get the mouse position from the widget's window
		if (input) {
			struct widget* widget = (struct widget*)input;
			struct window* window = widget_get_window(widget);
			if (window) {
				window_get_mouse_position(window, x, y);
				return;
			}
		}
		// Fallback
		if (x) *x = 0;
		if (y) *y = 0;
	}

	virtual cairo_t* WidgetCairoCreate(backend_widget_t widget)
	{
		return widget_cairo_create((struct widget*)widget);
	}

	virtual void* WidgetGetUserData(backend_widget_t widget)
	{
		return widget_get_user_data((struct widget*)widget);
	}

	// Backend identification
	virtual backend_type GetType() const
	{
		return BACKEND_X11;
	}

	virtual const char* GetName() const
	{
		return "X11";
	}
};

} // namespace BPrivate


// Export C functions for dynamic loading
extern "C" {
	/* Shim called by C window code when a move happens; calls registered C++ handler */
	void x11_move_shim(struct window* w, int x, int y, void* user_data)
	{
		if (!w)
			return;
		void* handler_data = window_get_move_handler_data(w);
		if (!handler_data)
			return;
		BPrivate::move_handler_t handler = (BPrivate::move_handler_t)handler_data;
		if (handler)
			handler((BPrivate::backend_window_t)w, x, y, window_get_move_user_data(w));
	}

	/* Factory function called by WindowBackendFactory for dynamic loading */
	BPrivate::WindowBackend* CreateWindowBackend()
	{
		return new BPrivate::X11Backend();
	}
}
