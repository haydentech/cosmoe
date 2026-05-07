/*
 * Copyright 2025-2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * X11 backend implementation - wraps X11 window code
 */

#include "CosmoeBackend.h"
#include <Rect.h>
#include <cstdlib>
#include <string.h>

// Include X11 window header from src/system/X11/
extern "C" {
#include "window.h"
#include <X11/Xresource.h>
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
}

// Forward declare the move shim so it can be used by the C++ backend
// class before the shim is defined below.
extern "C" void x11_move_shim(struct window* w, int x, int y, void* user_data);
extern "C" void x11_focus_shim(struct window* w, bool focused, void* user_data);

namespace BPrivate {

class X11Backend : public CosmoeBackend {
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

	virtual void DisplaySetPort(backend_display_t display, int32_t sender_port_id, int32_t receiver_port_id)
	{
		display_set_port((struct display*)display, sender_port_id, receiver_port_id);
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		// TODO: Implement X11 cursor mapping
		// For now, just return the Be cursor ID
		return beCursorID;
	}

	virtual int32_t DisplayCreateCustomCursor(const uint8_t* bits,
		size_t bitsLength, int32_t width, int32_t height,
		int32_t bytesPerRow, int32_t colorSpace,
		int32_t hotX, int32_t hotY)
	{
		return window_create_custom_cursor(bits, bitsLength, width, height,
			bytesPerRow, colorSpace, hotX, hotY);
	}

	virtual status_t DisplayDeleteCustomCursor(int32_t backendCursorID)
	{
		return window_delete_custom_cursor(backendCursorID) == 0 ? B_OK : B_ERROR;
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

	virtual int32_t DisplayGetAppList(backend_display_t display, int32_t* teamIDs,
		int32_t maxCount)
	{
		return display_get_app_list((struct display*)display, teamIDs,
			maxCount);
	}

	virtual status_t DisplaySetAppWatcher(backend_display_t display,
		app_watcher_t watcher, void* userData)
	{
		return display_set_app_watcher((struct display*)display,
			(display_app_watcher_t)watcher, userData);
	}

	virtual status_t DisplayClearAppWatcher(backend_display_t display)
	{
		return display_clear_app_watcher((struct display*)display);
	}

	// Window management
	virtual backend_window_t WindowLookupByToken(backend_display_t display, int32_t token)
	{
		return (backend_window_t)display_find_window_by_token((struct display*)display, token);
	}

	virtual backend_window_t WindowCreate(backend_display_t display,
		int32_t token, uint32_t look, uint32_t flags, bool offscreen,
		void* data)
	{
		// Create X11 window (offscreen parameter currently ignored)
		struct window* win = window_create((struct display*)display, look,
			flags);
		if (win) {
			window_set_token(win, token);
			window_set_user_data(win, data);
		}
		return (backend_window_t)win;
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t token, int32_t parent_token, int32_t x, int32_t y, int32_t width, int32_t height, void* data)
	{
		(void)width;
		(void)height;
		struct window* parent = display_find_window_by_token((struct display*)display, parent_token);
		// Create a borderless override-redirect popup suitable for menus
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
		// X11 doesn't have separate window frames - set resize handler on window itself
		
		if (window) {
			// In X11, the window itself needs a resize handler, as there is no windowframe widget
			window_set_resize_handler((struct window*)window, (widget_resize_handler_t)handler);
		}
	}

	virtual void WindowDestroy(backend_window_t window)
	{
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

	virtual void WindowVerifySize(backend_window_t window, struct rectangle& frame)
	{
		(void)window;
		(void)frame;
	}

	// PortLink message handling implementations
	virtual void WindowSetTitle(backend_window_t window, const char* title)
	{
		window_set_title((struct window*)window, title);
	}

	virtual void WindowSetLook(backend_window_t window, uint32_t look)
	{
		window_set_look((struct window*)window, look);
	}

	virtual void WindowSetFeel(backend_window_t window, uint32_t feel)
	{
		window_set_feel((struct window*)window, feel);
	}

	virtual void WindowResize(backend_window_t window, float width, float height,
					  float* outWidth, float* outHeight)
	{
		int32_t scheduledWidth = (int32_t)width;
		int32_t scheduledHeight = (int32_t)height;
		window_schedule_resize((struct window*)window, scheduledWidth, scheduledHeight);
		if (outWidth)
			*outWidth = (float)scheduledWidth;
		if (outHeight)
			*outHeight = (float)scheduledHeight;
	}

	virtual void WindowMinimize(backend_window_t window, bool minimize)
	{
		window_minimize((struct window*)window, minimize);
	}

	virtual void WindowActivate(backend_window_t window, bool active)
	{
		window_activate((struct window*)window, active);
	}

	virtual bool WindowIsFront(backend_window_t window)
	{
		return window_is_front((struct window*)window);
	}

	virtual void WindowSetSizeLimits(backend_window_t window,
	                                  float minWidth, float maxWidth,
	                                  float minHeight, float maxHeight,
	                                  BRect* outFrame,
	                                  float* outMinWidth, float* outMaxWidth,
	                                  float* outMinHeight, float* outMaxHeight)
	{
		// Set the size limits
		window_set_min_max_allocation((struct window*)window,
			(int)minWidth, (int)minHeight,
			(int)maxWidth, (int)maxHeight);

		// Return the enforced limits (X11 doesn't modify them)
		if (outMinWidth) *outMinWidth = minWidth;
		if (outMaxWidth) *outMaxWidth = maxWidth;
		if (outMinHeight) *outMinHeight = minHeight;
		if (outMaxHeight) *outMaxHeight = maxHeight;

		// FIXME: Return actual window frame - for now just return empty
		if (outFrame)
			*outFrame = *outFrame; // already set by caller; backend may adjust in future
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

		window_set_move_handler(w, (void (*)(struct window*, int, int, void*))handler, user_data);
	}

	virtual void WindowSetFocusHandler(backend_window_t window, focus_handler_t handler, void* user_data)
	{
		struct window* w = (struct window*)window;
		if (!w) return;

		window_set_focus_handler(w, (void (*)(struct window*, bool, void*))handler, user_data);
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

	virtual void WidgetSetIdleHandler(backend_widget_t widget, idle_handler_t handler)
	{
		widget_set_idle_handler((struct widget*)widget, (widget_idle_handler_t)handler);
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

	virtual void WidgetSetUserData(backend_widget_t widget, void *user_data)
	{
		widget_set_user_data((struct widget*)widget, user_data);
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

	// Display scaling support
	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale)
	{
		// X11 doesn't have native buffer scale protocol like Wayland
		// Scaling is handled via Cairo device scale in the application
		(void)window;
		(void)scale;
	}

	virtual void WidgetSetBufferScale(backend_widget_t widget, int32_t scale)
	{
		// X11 doesn't have native buffer scale protocol like Wayland
		// Scaling is handled via Cairo device scale in the application
		(void)widget;
		(void)scale;
	}

	virtual int32_t WindowGetDisplayScale(backend_window_t window)
	{
	struct window* win = (struct window*)window;
	if (!win)
		return 1;
	
	Display* display = window_get_xdisplay(win);
	if (!display)
		return 1;
	
	int32_t scale = 1;		// Method 1: Check Xft.dpi resource (set by desktop environment)
		char* resource = XResourceManagerString(display);
		if (resource) {
			XrmDatabase db = XrmGetStringDatabase(resource);
			if (db) {
				char* type = NULL;
				XrmValue value;
				if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value)) {
					if (type && strcmp(type, "String") == 0) {
						int dpi = atoi((char*)value.addr);
						if (dpi > 0) {
							// 96 DPI = 1x, 192 DPI = 2x, 288 DPI = 3x
							scale = (dpi + 48) / 96;
							if (scale >= 1 && scale <= 4) {
								XrmDestroyDatabase(db);
								return scale;
							}
						}
					}
				}
				XrmDestroyDatabase(db);
			}
		}
		
#ifdef HAVE_XRANDR
		// Method 2: Calculate DPI from monitor physical dimensions via XRandR
		Window x_window = window_get_xwindow(win);
		if (x_window != None) {
			// Get screen resources
			XRRScreenResources* resources = XRRGetScreenResourcesCurrent(
				display, DefaultRootWindow(display));
			
			if (resources) {
				// Get window position to determine which monitor it's on
				XWindowAttributes attrs;
				if (XGetWindowAttributes(display, x_window, &attrs)) {
					int window_x = attrs.x + attrs.width / 2;   // Center of window
					int window_y = attrs.y + attrs.height / 2;
					
					// Check each output to find the one containing the window
					for (int i = 0; i < resources->noutput; i++) {
						XRROutputInfo* output_info = XRRGetOutputInfo(
							display, resources, resources->outputs[i]);
						
						if (!output_info || output_info->connection != RR_Connected
							|| output_info->crtc == None) {
							if (output_info)
								XRRFreeOutputInfo(output_info);
							continue;
						}
						
						XRRCrtcInfo* crtc = XRRGetCrtcInfo(
							display, resources, output_info->crtc);
						
						if (crtc) {
							// Check if window center is on this CRTC
							if (window_x >= crtc->x 
								&& window_x < crtc->x + (int)crtc->width
								&& window_y >= crtc->y 
								&& window_y < crtc->y + (int)crtc->height) {
								
								// Calculate DPI from physical dimensions
								if (output_info->mm_width > 0 && crtc->width > 0) {
									double dpi = (crtc->width * 25.4) 
										/ output_info->mm_width;
									int calculated_scale = (int)((dpi + 48.0) / 96.0);
									
									if (calculated_scale >= 1 && calculated_scale <= 4) {
										scale = calculated_scale;
									}
								}
								
								XRRFreeCrtcInfo(crtc);
								XRRFreeOutputInfo(output_info);
								break;
							}
							
							XRRFreeCrtcInfo(crtc);
						}
						
						XRRFreeOutputInfo(output_info);
					}
				}
				
				XRRFreeScreenResources(resources);
			}
		}
		
		if (scale > 1)
			return scale;
#endif
		
		// Method 3: Check GDK_SCALE environment variable (GNOME/GTK)
		const char* gdk_scale = getenv("GDK_SCALE");
		if (gdk_scale) {
			int env_scale = atoi(gdk_scale);
			if (env_scale >= 1 && env_scale <= 4)
				return env_scale;
		}
		
		// Method 4: Check QT_SCALE_FACTOR
		const char* qt_scale = getenv("QT_SCALE_FACTOR");
		if (qt_scale) {
			float qt_scale_f = atof(qt_scale);
			if (qt_scale_f >= 1.0) {
				int env_scale = (int)(qt_scale_f + 0.5);
				if (env_scale >= 1 && env_scale <= 4)
					return env_scale;
			}
		}
		
		return 1;
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
	/* C wrapper for processing backend messages - called from display_run in window.c */
	void x11_process_backend_messages(int32_t backend_port, int32_t app_port)
	{
		// Get the X11 backend instance from the factory
		BPrivate::CosmoeBackendFactory* factory = BPrivate::CosmoeBackendFactory::Instance();
		if (!factory) return;
		
		BPrivate::CosmoeBackend* backend = factory->GetBackend();
		if (!backend) return;
		
		// Call the shared message processor
		backend->ProcessBackendMessages(backend_port, app_port);
	}
	
	/* Factory function called by CosmoeBackendFactory for dynamic loading */
	BPrivate::CosmoeBackend* CreateCosmoeBackend()
	{
		return new BPrivate::X11Backend();
	}
}
