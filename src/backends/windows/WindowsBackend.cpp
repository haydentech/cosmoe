/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Win32 backend implementation - wraps Win32 window code
 */

#include "CosmoeBackend.h"
#include <Rect.h>
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

// Forward declaration for the static backend instance (defined before the class)
namespace BPrivate { class CosmoeBackend; }
static BPrivate::CosmoeBackend* s_backend_instance = nullptr;

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

	virtual void DisplaySetPort(backend_display_t display, int32_t sender_port_id, int32_t receiver_port_id)
	{
		display_set_port((struct display*)display, sender_port_id, receiver_port_id);
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		// Preserve backend custom cursor ids allocated by window_create_custom_cursor().
		if (beCursorID >= 1000)
			return beCursorID;

		// Convert Be cursor IDs to Win32 IDC_* resource IDs.
		// These numeric values correspond to predefined system cursors.
		switch (beCursorID) {
			case 1:  return 32512; // B_CURSOR_ID_SYSTEM_DEFAULT -> IDC_ARROW
			case 2:  return 32513; // B_CURSOR_ID_I_BEAM -> IDC_IBEAM
			case 3:  return 32651; // B_CURSOR_ID_CONTEXT_MENU -> IDC_HELP
			case 4:  return 32649; // B_CURSOR_ID_COPY -> IDC_HAND (best available)
			case 5:  return 32515; // B_CURSOR_ID_CROSS_HAIR -> IDC_CROSS
			case 6:  return 32649; // B_CURSOR_ID_FOLLOW_LINK -> IDC_HAND
			case 7:  return 32649; // B_CURSOR_ID_GRAB -> IDC_HAND
			case 8:  return 32646; // B_CURSOR_ID_GRABBING -> IDC_SIZEALL
			case 9:  return 32651; // B_CURSOR_ID_HELP -> IDC_HELP
			case 10: return 32513; // B_CURSOR_ID_I_BEAM_HORIZONTAL -> IDC_IBEAM
			case 11: return 32646; // B_CURSOR_ID_MOVE -> IDC_SIZEALL
			case 12: return 32512; // B_CURSOR_ID_NO_CURSOR -> fallback IDC_ARROW
			case 13: return 32648; // B_CURSOR_ID_NOT_ALLOWED -> IDC_NO
			case 14: return 32650; // B_CURSOR_ID_PROGRESS -> IDC_APPSTARTING
			case 15: return 32645; // B_CURSOR_ID_RESIZE_NORTH -> IDC_SIZENS
			case 16: return 32644; // B_CURSOR_ID_RESIZE_EAST -> IDC_SIZEWE
			case 17: return 32645; // B_CURSOR_ID_RESIZE_SOUTH -> IDC_SIZENS
			case 18: return 32644; // B_CURSOR_ID_RESIZE_WEST -> IDC_SIZEWE
			case 19: return 32643; // B_CURSOR_ID_RESIZE_NORTH_EAST -> IDC_SIZENESW
			case 20: return 32642; // B_CURSOR_ID_RESIZE_NORTH_WEST -> IDC_SIZENWSE
			case 21: return 32642; // B_CURSOR_ID_RESIZE_SOUTH_EAST -> IDC_SIZENWSE
			case 22: return 32643; // B_CURSOR_ID_RESIZE_SOUTH_WEST -> IDC_SIZENESW
			case 23: return 32645; // B_CURSOR_ID_RESIZE_NORTH_SOUTH -> IDC_SIZENS
			case 24: return 32644; // B_CURSOR_ID_RESIZE_EAST_WEST -> IDC_SIZEWE
			case 25: return 32643; // B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST -> IDC_SIZENESW
			case 26: return 32642; // B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST -> IDC_SIZENWSE
			case 27: return 32512; // B_CURSOR_ID_ZOOM_IN -> fallback IDC_ARROW
			case 28: return 32512; // B_CURSOR_ID_ZOOM_OUT -> fallback IDC_ARROW
			case 29: return 32649; // B_CURSOR_ID_CREATE_LINK -> IDC_HAND
			default: return 32512; // IDC_ARROW
		}
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

	virtual int32_t DisplayGetAppList(backend_display_t display, int32_t* teamIDs,
		int32_t maxCount)
	{
		return display_get_app_list((struct display*)display, teamIDs,
			maxCount);
	}

	virtual status_t DisplayGetAppInfo(backend_display_t display, int32_t teamID,
		::cosmoe_backend_app_info* info)
	{
		return display_get_app_info((struct display*)display, teamID, info);
	}

	virtual int32_t DisplayGetWindowList(backend_display_t display,
		int32_t* windowIDs, int32_t maxCount)
	{
		return display_get_window_list((struct display*)display, windowIDs,
			maxCount);
	}

	virtual status_t DisplayGetWindowInfo(backend_display_t display,
		int32_t windowID, ::cosmoe_backend_window_info* info)
	{
		return display_get_window_info((struct display*)display, windowID,
			info);
	}

	virtual status_t DisplayActivateWindow(backend_display_t display,
		int32_t windowID)
	{
		return display_activate_window((struct display*)display, windowID);
	}

	virtual status_t DisplayMinimizeWindow(backend_display_t display,
		int32_t windowID, bool minimize)
	{
		return display_minimize_window((struct display*)display, windowID,
			minimize);
	}

	virtual status_t DisplayCloseWindow(backend_display_t display,
		int32_t windowID)
	{
		return display_close_window((struct display*)display, windowID);
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
		(void)look;
		(void)flags;
		(void)offscreen; // Windows backend doesn't support offscreen windows yet
		struct window* win = window_create((struct display*)display);
		if (win) {
			window_set_token(win, token);
			window_set_user_data(win, data);
		}
		return (backend_window_t)win;
	}

	virtual backend_window_t WindowPopupCreate(backend_display_t display,
				       int32_t token,
				       int32_t parent_token,
				       int32_t x, int32_t y, int32_t width, int32_t height, void* data)
	{
		(void)width;
		(void)height;
		struct window* parent = display_find_window_by_token((struct display*)display, parent_token);
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
		// Windows doesn't use separate window frames
		window_set_resize_handler((struct window*)window, (widget_resize_handler_t)handler);
	}
	virtual void WindowDestroy(backend_window_t window)
	{
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

	virtual void WindowShow(backend_window_t window)
	{
		window_show((struct window*)window);
	}

	virtual void WindowHide(backend_window_t window)
	{
		window_hide((struct window*)window);
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
		window_set_min_max_allocation((struct window*)window,
			(int)minWidth, (int)minHeight,
			(int)maxWidth, (int)maxHeight);

		if (outFrame != NULL) {
			float targetWidth = outFrame->Width();
			float targetHeight = outFrame->Height();

			if (targetWidth < minWidth)
				targetWidth = minWidth;
			if (targetHeight < minHeight)
				targetHeight = minHeight;

			if (maxWidth > 0 && targetWidth > maxWidth)
				targetWidth = maxWidth;
			if (maxHeight > 0 && targetHeight > maxHeight)
				targetHeight = maxHeight;

			if (targetWidth != outFrame->Width() || targetHeight != outFrame->Height()) {
				window_schedule_resize((struct window*)window,
					(int32_t)targetWidth, (int32_t)targetHeight);
				outFrame->right = outFrame->left + targetWidth;
				outFrame->bottom = outFrame->top + targetHeight;
			}
		}

		if (outMinWidth)  *outMinWidth  = minWidth;
		if (outMaxWidth)  *outMaxWidth  = maxWidth;
		if (outMinHeight) *outMinHeight = minHeight;
		if (outMaxHeight) *outMaxHeight = maxHeight;
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

	virtual void WidgetSetUserData(backend_widget_t widget, void *user_data)
	{
		widget_set_user_data((struct widget*)widget, user_data);
	}

	virtual void WidgetSetAllocation(backend_widget_t widget,
					 int32_t x, int32_t y, int32_t width, int32_t height)
	{
		widget_set_allocation((struct widget*)widget, x, y, width, height);
	}

	virtual void WidgetScheduleResize(backend_widget_t widget, int32_t width, int32_t height)
	{
		if (!widget)
			return;
		struct rectangle allocation;
		widget_get_allocation((struct widget*)widget, &allocation);
		widget_set_allocation((struct widget*)widget, allocation.x, allocation.y, width, height);
		widget_schedule_redraw((struct widget*)widget);
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
		// For now return 100% scaling.
		// Could implement using GetDpiForWindow() on Windows 10+,
		(void)window;
		return 100;
	}

	virtual status_t WindowSetNativeMenuBar(backend_window_t window,
		const ::cosmoe_native_menu_item* items, int32_t count,
		window_menu_func_t func, void* userData)
	{
		return window_set_native_menubar((struct window*)window, items, count,
			(cosmoe_window_menu_func_t)func, userData);
	}

	virtual status_t WindowClearNativeMenuBar(backend_window_t window)
	{
		return window_clear_native_menubar((struct window*)window);
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


// Static pointer to the single backend instance, used by windows_process_backend_messages
// The exported creation function
extern "C" CosmoeBackend* CreateCosmoeBackend()
{
	s_backend_instance = new WindowsBackend();
	return s_backend_instance;
}

} // namespace BPrivate

extern "C"
void windows_process_backend_messages(int32_t backend_port, int32_t app_port)
{
	if (s_backend_instance)
		s_backend_instance->ProcessBackendMessages(backend_port, app_port);
}

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
