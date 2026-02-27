/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Wayland backend implementation - wraps Wayland window code
 */

#include "CosmoeBackend.h"
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
struct widget *window_get_topview_widget(struct window *window);
void window_set_topview_widget(struct window *window, struct widget *widget);
void window_show(struct window *window);
void window_hide(struct window *window);
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

class WaylandBackend : public CosmoeBackend {
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

	virtual void DisplaySetPort(backend_display_t display, int32_t sender_port_id, int32_t receiver_port_id)
	{
		display_set_port((struct display*)display, sender_port_id, receiver_port_id);
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
	virtual backend_window_t WindowLookupByToken(backend_display_t display, int32_t token)
	{
		return (backend_window_t)display_find_window_by_token((struct display*)display, token);
	}

	virtual backend_window_t WindowCreate(backend_display_t display, int32_t token, bool offscreen, void* data)
	{
		struct window* win = window_create((struct display*)display);
		if (win) {
			window_set_token(win, token);
			window_set_user_data(win, data);
			if (!offscreen) {
				// Create the Wayland window frame (decoration widget) immediately so
				// the window struct owns it from creation time.
				backend_windowframe_t frame = window_frame_create(win, data);
				if (frame)
					set_empty_input_region(frame, window_get_display(win));
			}
		}
		return (backend_window_t)win;
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t token, int32_t parent_token, int32_t x, int32_t y, void* data)
	{
		struct window* parent = display_find_window_by_token((struct display*)display, parent_token);
		// Use a Wayland popup created with a given position and parent
		struct window* win = window_popup_create((struct display*)display, parent, x, y);
		if (win) {
			window_set_token(win, token);
			window_set_user_data(win, data);
		}
		return (backend_window_t)win;
	}

	virtual void WindowframeSetResizeHandler(backend_window_t window,
						 windowframe_resize_handler_t handler)
	{
		// Ignore the externally-passed frame; retrieve it from the window struct.
		struct widget* frame_child = window_get_frame_child((struct window*)window);
		if (frame_child) {
			widget_set_resize_handler(frame_child, (widget_resize_handler_t)handler);
		}
	}

	virtual void WindowDestroy(backend_window_t window)
	{
		struct widget* topviewWidget = window_get_topview_widget((struct window*)window);
		struct widget* frame_child = window_get_frame_child((struct window*)window);
		if (topviewWidget && topviewWidget != frame_child)
			widget_deferred_destroy(topviewWidget);

		if (frame_child) {
			// Clear the BWindow back-pointer before destruction so any
			// in-flight callbacks don't dereference a stale pointer.
			widget_set_user_data(frame_child, NULL);
			widget_deferred_destroy(frame_child);
		}
		window_deferred_destroy((struct window*)window);
	}
	virtual void WindowShow(backend_window_t window)
	{
		window_show((struct window*)window);
	}

	virtual void WindowHide(backend_window_t window)
	{
		window_hide((struct window*)window);
	}
	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		window_set_appid((struct window*)window, appId);
	}

	virtual void WindowSetParent(backend_window_t window, backend_window_t parent_window)
	{
		window_set_parent((struct window*)window, (struct window*)parent_window);
	}

	virtual void WindowScheduleResize(backend_window_t window, int width, int height)
	{
		// Retrieve the frame child widget from the window struct.
		struct widget* frame_child = window_get_frame_child((struct window*)window);
		if (!frame_child) {
			/* Popup windows have no frame widget. Set pending_allocation directly
			 * so idle_resize -> window_do_resize -> surface_resize creates the
			 * cairo surface at the correct size. */
			window_schedule_resize((struct window*)window, width, height);
			return;
		}
		widget_schedule_resize(frame_child, width + WAYLAND_WINDOW_H_SLOP, height + WAYLAND_WINDOW_V_SLOP);
		widget_schedule_redraw(frame_child);
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
		/* Popup/custom windows have no frame decorations */
		if (window_is_custom((struct window*)window)) {
			if (borderWidth) *borderWidth = 0;
			if (tabHeight) *tabHeight = 0;
			return;
		}
		if (borderWidth) *borderWidth = WAYLAND_TOPVIEW_H_OFFSET;
		if (tabHeight) *tabHeight = WAYLAND_TOPVIEW_V_OFFSET;
	}


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
		/* Custom/popup windows have no frame decorations — offset is zero */
		if (window_is_custom((struct window*)window)) {
			if (offset_h) *offset_h = 0;
			if (offset_v) *offset_v = 0;
			return;
		}
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
		if (!win)
			return NULL;

		struct widget* topviewWidget = window_get_topview_widget(win);
		if (topviewWidget != NULL)
			return (backend_widget_t)topviewWidget;

		/* For regular framed windows, reuse the frame child widget as topview.
		 * The Wayland frame resize path updates this child directly; using a
		 * separate subsurface breaks topview resize propagation. */
		struct widget* frameChild = window_get_frame_child(win);
		if (frameChild != NULL) {
			widget_set_user_data(frameChild, data);
			window_set_topview_widget(win, frameChild);
			return (backend_widget_t)frameChild;
		}

		/* For popup/custom windows there is no frame widget on the main surface.
		 * We must put the content widget ON the main surface (not a subsurface)
		 * so that a buffer gets attached to the main surface and the compositor
		 * actually displays the popup.  For regular windows the frame already
		 * occupies the main surface, so the content widget goes as a subsurface. */
		if (!window_has_main_widget(win)) {
			/* No frame widget yet — this is a popup/custom window.
			 * window_add_widget sets main_surface->widget directly. */
			struct widget* widget = window_add_widget(win, data);
			window_set_topview_widget(win, widget);
			return (backend_widget_t)widget;
		}

		/* Fallback: if a window has a main widget but no frame child, create a
		 * synchronized subsurface (legacy path). */
		struct widget* widget = window_add_subsurface(win, data, SUBSURFACE_SYNCHRONIZED);
		window_set_topview_widget(win, widget);
		set_empty_input_region((backend_widget_t)widget, window_get_display((struct window*)window));
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

	virtual void WidgetSetIdleHandler(backend_widget_t widget,
					 idle_handler_t handler)
	{
		widget_set_idle_handler((struct widget*)widget, (widget_idle_handler_t)handler);
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

	virtual void WidgetSetUserData(backend_widget_t widget, void *user_data)
	{
		widget_set_user_data((struct widget*)widget, user_data);
	}

	virtual void WidgetSetAllocation(backend_widget_t widget,
					 int32_t x, int32_t y,
					 int32_t width, int32_t height)
	{
		struct window* win = (struct window*)widget_get_window((struct widget*)widget);
		if (win && window_is_custom(win)) {
			/* Popup windows have no frame — no offset to add */
			widget_set_allocation((struct widget*)widget, x, y, width, height);
			return;
		}

		/* For framed windows, the topview is the frame child widget.
		 * It is already positioned in frame-local coordinates, so do not
		 * apply extra decoration offsets here. */
		if (win && (struct widget*)widget == window_get_frame_child(win)) {
			widget_set_allocation((struct widget*)widget, x, y, width, height);
			return;
		}

		{
			widget_set_allocation((struct widget*)widget, x + WAYLAND_TOPVIEW_H_OFFSET, y + WAYLAND_TOPVIEW_V_OFFSET, width, height);
		}
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

	// Display scaling support
	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale)
	{
		if (!window)
			return;
		window_set_buffer_scale((struct window*)window, scale);

		struct widget* topviewWidget = window_get_topview_widget((struct window*)window);
		if (topviewWidget != NULL)
			widget_set_buffer_scale(topviewWidget, scale);
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

	// Window operations
	virtual void WindowResize(backend_window_t window, float width, float height)
	{
		(void)window;
		(void)width;
		(void)height;
		// TODO: Implement Wayland window resize
	}

	virtual void WindowMinimize(backend_window_t window, bool minimize)
	{
		(void)window;
		(void)minimize;
		// TODO: Implement Wayland window minimize
	}

	virtual void WindowActivate(backend_window_t window, bool active)
	{
		(void)window;
		(void)active;
		// TODO: Implement Wayland window activation
	}

	virtual void WindowSetSizeLimits(backend_window_t window, float minW, float maxW, 
	                                  float minH, float maxH, BRect* frame,
	                                  float* outMinW, float* outMaxW, 
	                                  float* outMinH, float* outMaxH)
	{
		WindowSetMinMaxAllocation(window,
			(int)minW, (int)minH, (int)maxW, (int)maxH);

		// Echo the frame back unchanged (backend may adjust in future)
		// outFrame is already the current frame sent by the client
		(void)frame;

		// Wayland doesn't enforce different limits — echo back what was requested
		if (outMinW) *outMinW = minW;
		if (outMaxW) *outMaxW = maxW;
		if (outMinH) *outMinH = minH;
		if (outMaxH) *outMaxH = maxH;
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
	/* C wrapper for processing backend messages - called from display_run in window.c */
	void wayland_process_backend_messages(int32_t backend_port, int32_t app_port)
	{
		BPrivate::CosmoeBackendFactory* factory = BPrivate::CosmoeBackendFactory::Instance();
		if (!factory) return;
		BPrivate::CosmoeBackend* backend = factory->GetBackend();
		if (!backend) return;
		backend->ProcessBackendMessages(backend_port, app_port);
	}

	/* Factory function called by CosmoeBackendFactory for dynamic loading */
	BPrivate::CosmoeBackend* CreateCosmoeBackend()
	{
		return new BPrivate::WaylandBackend();
	}
};
