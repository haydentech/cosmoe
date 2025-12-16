/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Wayland backend implementation - wraps Wayland window code
 */

#include "WindowBackend.h"
#include <cstdlib>

// Include Wayland window header
extern "C" {
#include "../../libs/wayland/window.h"
// Forward declare C functions from window.c
void widget_set_buffer_scale(struct widget *widget, int32_t scale);
void window_set_focus_handler(struct window *window,
			      void (*handler)(struct window*, bool, void*),
			      void *user_data);
void *window_get_focus_user_data(struct window *window);
}

// Forward declare move shim so it can be used within this file's C++ class
extern "C" void wayland_move_shim(struct window* w, int x, int y, void* user_data);
extern "C" void wayland_focus_shim(struct window* w, bool focused, void* user_data);

// Cosmoe, like Haiku and BeOS, considers the dimensions of the window as being
// the dimensions of the window content area only, i.e. excluding window decorations.
// These values are not constant across different Wayland window themes, and we
// should figure out a way to determine them dynamically.  The current values
// merely reflect the default Weston theme.

// The SLOP values represent the extra space taken up by window decorations.
// We need to add these values when sizing the actual Wayland surface.
#define WAYLAND_WINDOW_H_SLOP 76
#define WAYLAND_WINDOW_V_SLOP 97

// These OFFSET values represent the offset of the BWindow's topview within the 
// enclosing Wayland window surface.
#define WAYLAND_TOPVIEW_H_OFFSET 38
#define WAYLAND_TOPVIEW_V_OFFSET 59

namespace BPrivate {

class WaylandBackend : public WindowBackend {
public:
	WaylandBackend() {}
	virtual ~WaylandBackend() {}

	// Display management
	virtual backend_display_t DisplayCreate(int* argc, char** argv)
	{
		return (backend_display_t)display_create(argc, (const char**)argv);
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
		// Wayland doesn't need explicit flush - frame callbacks handle updates
		(void)display;
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
		if (!allocation)
			return;
		allocation->x = 0;
		allocation->y = 0;
		allocation->width = 0;
		allocation->height = 0;

		if (!display)
			return;
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
		// Map Be API cursor IDs to Wayland cursor indices
		// Based on the cursors array in window.c:
		// 0=bottom_left, 1=bottom_right, 2=bottom, 3=grabbing,
		// 4=left_ptr, 5=left, 6=right, 7=top_left, 8=top_right,
		// 9=top, 10=xterm, 11=hand1, 12=watch, 13=move,
		// 14=copy, 15=forbidden, 16=context-menu, 17=crosshair,
		// 18=vertical-text, 19=zoom-in, 20=zoom-out,
		// 21=col-resize, 22=row-resize
		
		switch (beCursorID) {
			case 1:  // B_CURSOR_ID_SYSTEM_DEFAULT
				return 4;  // left_ptr
			case 2:  // B_CURSOR_ID_I_BEAM
				return 10; // xterm
			case 3:  // B_CURSOR_ID_CONTEXT_MENU
				return 16; // context-menu
			case 4:  // B_CURSOR_ID_COPY
				return 14; // copy
			case 5:  // B_CURSOR_ID_CROSS_HAIR
				return 17; // crosshair
			case 6:  // B_CURSOR_ID_FOLLOW_LINK
				return 11; // hand1
			case 7:  // B_CURSOR_ID_GRAB
				return 11; // hand1
			case 8:  // B_CURSOR_ID_GRABBING
				return 3;  // grabbing
			case 9:  // B_CURSOR_ID_HELP
				return 4;  // left_ptr (no help cursor)
			case 10: // B_CURSOR_ID_I_BEAM_HORIZONTAL
				return 18; // vertical-text
			case 11: // B_CURSOR_ID_MOVE
				return 13; // move
			case 12: // B_CURSOR_ID_NO_CURSOR
				return -1; // no cursor
			case 13: // B_CURSOR_ID_NOT_ALLOWED
				return 15; // forbidden
			case 14: // B_CURSOR_ID_PROGRESS
				return 12; // watch
			case 15: // B_CURSOR_ID_RESIZE_NORTH
				return 9;  // top
			case 16: // B_CURSOR_ID_RESIZE_EAST
				return 6;  // right
			case 17: // B_CURSOR_ID_RESIZE_SOUTH
				return 2;  // bottom
			case 18: // B_CURSOR_ID_RESIZE_WEST
				return 5;  // left
			case 19: // B_CURSOR_ID_RESIZE_NORTH_EAST
				return 8;  // top_right
			case 20: // B_CURSOR_ID_RESIZE_NORTH_WEST
				return 7;  // top_left
			case 21: // B_CURSOR_ID_RESIZE_SOUTH_EAST
				return 1;  // bottom_right
			case 22: // B_CURSOR_ID_RESIZE_SOUTH_WEST
				return 0;  // bottom_left
			case 23: // B_CURSOR_ID_RESIZE_NORTH_SOUTH
				return 22; // row-resize
			case 24: // B_CURSOR_ID_RESIZE_EAST_WEST
				return 21; // col-resize
			case 25: // B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST
				return 8;  // top_right (diagonal)
			case 26: // B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST
				return 7;  // top_left (diagonal)
			case 27: // B_CURSOR_ID_ZOOM_IN
				return 19; // zoom-in
			case 28: // B_CURSOR_ID_ZOOM_OUT
				return 20; // zoom-out
			case 29: // B_CURSOR_ID_CREATE_LINK
				return 14; // copy (similar to link)
			default:
				return 4;  // left_ptr (default)
		}
	}

	// Window management
	virtual backend_window_t WindowCreate(backend_display_t display, bool offscreen)
	{
		return (backend_window_t)window_create((struct display*)display);
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t x, int32_t y)
	{
		// Use a Wayland popup created with a given position
		return (backend_window_t)window_popup_create((struct display*)display, x, y);
	}

	virtual backend_windowframe_t WindowframeCreate(backend_window_t window, void* data)
	{
		backend_windowframe_t frame = window_frame_create((struct window*)window, data);
		set_empty_input_region(frame, window_get_display((struct window*)window));
		return frame;
	}

	virtual void WindowframeSetResizeHandler(backend_window_t window, backend_windowframe_t frame,
						 windowframe_resize_handler_t handler)
	{
		// In Wayland, the window frame is just another widget.  window is unused.
		(void)window;
		
		if (frame) {
			widget_set_resize_handler((struct widget*)frame, (widget_resize_handler_t)handler);
		}
	}

	virtual void WindowDestroy(backend_window_t window, backend_windowframe_t frame)
	{
		if (frame) {
			widget_deferred_destroy((struct widget*)frame);
		}
		window_deferred_destroy((struct window*)window);
	}

	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		window_set_appid((struct window*)window, appId);
	}

	virtual void WindowScheduleResize(backend_window_t window, backend_windowframe_t frame, int width, int height)
	{
		widget_schedule_resize((struct widget*)frame, width + WAYLAND_WINDOW_H_SLOP, height + WAYLAND_WINDOW_V_SLOP);
		widget_schedule_redraw((struct widget*)frame);
	}

	virtual void WindowSetMinMaxAllocation(backend_window_t window,
					      int min_width, int min_height,
					      int max_width, int max_height)
	{
		// Add frame widget size to window content size
		window_set_min_max_allocation((struct window*)window,
				 min_width + WAYLAND_WINDOW_H_SLOP,
				 min_height + WAYLAND_WINDOW_V_SLOP,
				 max_width + WAYLAND_WINDOW_H_SLOP,
				 max_height + WAYLAND_WINDOW_V_SLOP);
	}

	virtual void WindowSetKeyHandler(backend_window_t window,
					 key_handler_t handler)
	{
		// Cast between our unified handler type and Wayland's type
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

	virtual void WindowGetPosition(backend_window_t window, int32_t* x, int32_t* y)
	{
		// Wayland does not expose absolute window positions; return 0,0 per API
		(void)window;
		if (x) *x = 0;
		if (y) *y = 0;
	}

	virtual void WindowSetPosition(backend_window_t window, int32_t x, int32_t y)
	{
		/* Wayland does not support absolute window move; ignore the request */
		(void)window;
		(void)x;
		(void)y;
	}

	virtual void WindowGetDecoratorSize(backend_window_t window, int32_t* borderWidth, int32_t* tabHeight)
	{
		/* The Wayland backend uses constants to derive decorator sizes.
		   topview offsets should be the values added by the compositor / theme.
		   These constants are our current approximation values for the
		   Wayland decorations. */
		if (borderWidth) *borderWidth = WAYLAND_TOPVIEW_H_OFFSET;
		if (tabHeight) *tabHeight = WAYLAND_TOPVIEW_V_OFFSET;
	}


	virtual void WindowSetUserData(backend_window_t window, void* data)
	{
		window_set_user_data((struct window*)window, data);
	}

	// The wayland_move_shim is defined below; forward declared above so
	// it can be referenced by this class method. It will call into the
	// registered C++ move handler.

	virtual void* WindowGetUserData(backend_window_t window)
	{
		return window_get_user_data((struct window*)window);
	}

	virtual cairo_surface_t* WindowGetSurface(backend_window_t window)
	{
		return window_get_surface((struct window*)window);
	}

	virtual void WindowGetTopviewOffset(backend_window_t window,
					    int32_t* offset_h, int32_t* offset_v)
	{
		if (offset_h) *offset_h = WAYLAND_TOPVIEW_H_OFFSET;
		if (offset_v) *offset_v = WAYLAND_TOPVIEW_V_OFFSET;
	}

	virtual void WindowSetMoveHandler(backend_window_t window, move_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w)
			return;

		window_set_move_handler(w, (void (*)(struct window*, int, int, void*))handler, user_data);
	}

	virtual void WindowSetFocusHandler(backend_window_t window, focus_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w) return;

		window_set_focus_handler(w, (void (*)(struct window*, bool, void*))handler, user_data);
	}

	virtual void WindowShowMenu(backend_display_t display, void* input,
				uint32_t time, backend_window_t window, int32_t x, int32_t y,
				window_menu_func_t func, void* user_data,
				const char** entries, int count)
	{
		struct window* w = (struct window*)window;
		struct display* d = (struct display*)display;
		window_show_menu(d, (struct input*)input, time, w, x, y,
						(menu_func_t)func, user_data, entries, count);
	}


	static void
	set_empty_input_region(backend_widget_t widget, backend_display_t display)
	{
		struct wl_compositor *compositor;
		struct wl_surface *surface;
		struct wl_region *region;

		compositor = display_get_compositor((struct display*)display);
		surface = widget_get_wl_surface((struct widget*)widget);
		region = wl_compositor_create_region(compositor);
		wl_surface_set_input_region(surface, region);
		wl_region_destroy(region);
	}

	// Widget management
	virtual backend_widget_t WindowAddWidget(backend_window_t window, void* data)
	{
		struct window* win = (struct window*)window;
		
		backend_widget_t* widget = (backend_widget_t*)window_add_subsurface(win, data, SUBSURFACE_SYNCHRONIZED);
		set_empty_input_region(widget, window_get_display((struct window*)window));
		return (backend_widget_t)widget;
	}

	virtual void WidgetDestroy(backend_widget_t widget)
	{
		widget_deferred_destroy((struct widget*)widget);
	}

	virtual void WidgetSetRedrawHandler(backend_widget_t widget,
					   redraw_handler_t handler)
	{
		widget_set_redraw_handler((struct widget*)widget, (widget_redraw_handler_t)handler);
	}

	virtual void WidgetSetResizeHandler(backend_widget_t widget,
					   resize_handler_t handler)
	{
		widget_set_resize_handler((struct widget*)widget, (widget_resize_handler_t)handler);
	}

	virtual void WidgetSetButtonHandler(backend_widget_t widget,
					   button_handler_t handler)
	{
		widget_set_button_handler((struct widget*)widget, (widget_button_handler_t)handler);
	}

	virtual void WidgetSetMotionHandler(backend_widget_t widget,
					   motion_handler_t handler)
	{
		widget_set_motion_handler((struct widget*)widget, (widget_motion_handler_t)handler);
	}

	virtual void WidgetSetAxisHandler(backend_widget_t widget,
					 axis_handler_t handler)
	{
		widget_set_axis_handler((struct widget*)widget, (widget_axis_handler_t)handler);
	}

	virtual void WidgetGetAllocation(backend_widget_t widget,
					 struct rectangle* allocation)
	{
		widget_get_allocation((struct widget*)widget, allocation);
	}
	
	virtual void WidgetScheduleResize(backend_widget_t widget,
					  int32_t width, int32_t height)
	{
		widget_schedule_resize((struct widget*)widget, width, height);
	}

	virtual void WidgetScheduleRedraw(backend_widget_t widget)
	{
		widget_schedule_redraw((struct widget*)widget);
	}

	backend_window_t WidgetGetWindow(backend_widget_t widget)
	{
		return (backend_window_t)widget_get_window((struct widget*)widget);
	}

	virtual void WidgetSetAllocation(backend_widget_t widget,
					 int32_t x, int32_t y,
					 int32_t width, int32_t height)
	{
		widget_set_allocation((struct widget*)widget, x + WAYLAND_TOPVIEW_H_OFFSET, y + WAYLAND_TOPVIEW_V_OFFSET, width, height);
	}

	// Input management
	virtual void InputGetPosition(void* input, int32_t* x, int32_t* y)
	{
		input_get_position((struct input*)input, x, y);
	}

	virtual cairo_t* WidgetCairoCreate(backend_widget_t widget)
	{
		return widget_cairo_create((struct widget*)widget);
	}

	virtual void* WidgetGetUserData(backend_widget_t widget)
	{
		return widget_get_user_data((struct widget*)widget);
	}

	// Display scaling support
	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale)
	{
		if (!window)
			return;
		window_set_buffer_scale((struct window*)window, scale);
	}

	virtual void WidgetSetBufferScale(backend_widget_t widget, int32_t scale)
	{
		if (!widget)
			return;
		widget_set_buffer_scale((struct widget*)widget, scale);
	}

	virtual int32_t WindowGetDisplayScale(backend_window_t window)
	{
		if (!window)
			return 1;
		
		// Method 1: Check GDK_SCALE environment variable (GNOME/GTK)
		const char *gdk_scale = getenv("GDK_SCALE");
		if (gdk_scale) {
			int env_scale = atoi(gdk_scale);
			if (env_scale >= 1 && env_scale <= 4)
				return env_scale;
		}
		
		// Method 2: Check QT_SCALE_FACTOR
		const char *qt_scale = getenv("QT_SCALE_FACTOR");
		if (qt_scale) {
			float qt_scale_f = atof(qt_scale);
			if (qt_scale_f >= 1.0) {
				int env_scale = (int)(qt_scale_f + 0.5);
				if (env_scale >= 1 && env_scale <= 4)
					return env_scale;
			}
		}
		
		// Method 3: Check Wayland-specific environment variable
		const char *wayland_scale = getenv("WAYLAND_DISPLAY_SCALE");
		if (wayland_scale) {
			int env_scale = atoi(wayland_scale);
			if (env_scale >= 1 && env_scale <= 4)
				return env_scale;
		}
		
		// TODO: Query the actual Wayland output scale from wl_output
		// This would require tracking which output the window is on
		
		return 1;
	}

	// Backend identification
	virtual backend_type GetType() const
	{
		return BACKEND_WAYLAND;
	}

	virtual const char* GetName() const
	{
		return "Wayland";
	}
};

} // namespace BPrivate


// Export C function for dynamic loading
extern "C" {
	/* Factory function called by WindowBackendFactory for dynamic loading */	BPrivate::WindowBackend* CreateWindowBackend()
	{
		return new BPrivate::WaylandBackend();
	}
};
