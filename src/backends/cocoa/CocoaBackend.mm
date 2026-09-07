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
#include <Rect.h>
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

	virtual void DisplayTriggerRedraw(backend_display_t display,
				 backend_window_t window,
				 backend_widget_t widget,
				 const struct rectangle* damage)
	{
		display_trigger_redraw((struct display*)display,
				      (struct window*)window,
				      (struct widget*)widget, damage);
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

	virtual void DisplaySetPort(backend_display_t display, int32_t sender_port_id, int32_t receiver_port_id)
	{
		display_set_port((struct display*)display, sender_port_id, receiver_port_id);
	}

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID)
	{
		if (beCursorID >= 1000)
			return beCursorID;
		return display_convert_cursor(beCursorID);
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
		(void)display;
		(void)watcher;
		(void)userData;
		return B_UNSUPPORTED;
	}

	virtual status_t DisplayClearAppWatcher(backend_display_t display)
	{
		(void)display;
		return B_UNSUPPORTED;
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

	// Window management
	virtual backend_window_t WindowLookupByToken(backend_display_t display, int32_t token)
	{
		return (backend_window_t)display_find_window_by_token((struct display*)display, token);
	}

	virtual backend_window_t WindowCreate(backend_display_t display,
		int32_t token, uint32_t look, uint32_t feel, uint32_t flags, bool offscreen,
		void* data)
	{
		(void)feel;
		struct window* win = window_create((struct display*)display, look,
			flags, offscreen);
		if (win) {
			window_set_token(win, token);
			window_set_user_data((struct window*)win, data);
		}
		return (backend_window_t)win;
	}

virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t token, int32_t parent_token, int32_t x, int32_t y, int32_t width, int32_t height, void* data)
	{
		struct window* parent = display_find_window_by_token((struct display*)display, parent_token);
		struct window* win = window_popup_create((struct display*)display, parent,
			x, y, width, height);
		if (win) {
			window_set_token(win, token);
			window_set_user_data((struct window*)win, data);
		}
		return (backend_window_t)win;
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

	virtual void WindowDestroy(backend_window_t window)
	{
		window_destroy((struct window*)window);
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

	virtual void WindowSetFeel(backend_window_t window, uint32_t feel)
	{
		window_set_feel((struct window*)window, feel);
	}

	virtual void WindowSetFlags(backend_window_t window, uint32_t flags)
	{
		window_set_flags((struct window*)window, flags);
	}

	virtual void WindowSetAppId(backend_window_t window, const char* appId)
	{
		window_set_app_id((struct window*)window, appId);
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
		window_set_min_max_allocation((struct window*)window, min_width, min_height,
					     max_width, max_height);
	}

	virtual void WindowVerifySize(backend_window_t window, struct rectangle& frame)
	{
		(void)window;
		(void)frame;
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

		if (outMinWidth)  *outMinWidth = minWidth;
		if (outMaxWidth)  *outMaxWidth = maxWidth;
		if (outMinHeight) *outMinHeight = minHeight;
		if (outMaxHeight) *outMaxHeight = maxHeight;
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

	virtual void WindowSetScreenHandler(backend_window_t window,
					   screen_handler_t handler)
	{
		window_set_screen_handler((struct window*)window,
			(cocoa_screen_handler_t)handler);
	}

	virtual backend_display_t WindowGetDisplay(backend_window_t window)
	{
		return (backend_display_t)window_get_display((struct window*)window);
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
	virtual void WindowframeSetResizeHandler(backend_window_t window,
						 windowframe_resize_handler_t handler)
	{
		windowframe_set_resize_handler((struct window*)window,
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
		return (backend_widget_t)window_add_widget((struct window*)window, data);
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

		virtual status_t WindowSetNativeMenuBar(backend_window_t window,
			const ::cosmoe_native_menu_item* items, int32_t count,
			window_menu_func_t func, void* userData)
		{
			return window_set_native_menubar((struct window*)window, items, count,
				(cocoa_window_menu_func_t)func, userData);
		}

		virtual status_t WindowClearNativeMenuBar(backend_window_t window)
		{
			return window_clear_native_menubar((struct window*)window);
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

	virtual void WidgetGetAllocation(backend_widget_t widget, struct rectangle* allocation)
	{
		widget_get_allocation((struct widget*)widget, allocation);
	}

	virtual void WidgetSetAllocation(backend_widget_t widget, int32_t x, int32_t y,
					 int32_t width, int32_t height)
	{
		widget_set_allocation((struct widget*)widget, x, y, width, height);
	}

	virtual void WidgetScheduleResize(backend_widget_t widget, int32_t width, int32_t height)
	{
		widget_schedule_resize((struct widget*)widget, width, height);
	}
};

} // namespace BPrivate

// Export C function to create backend instance
extern "C" {
	BPrivate::CosmoeBackend* CreateCosmoeBackend(void)
	{
		return new BPrivate::CocoaBackend();
	}

	/* C wrapper for processing backend messages - called from display_run in window_impl.mm */
	void cocoa_process_backend_messages(int32_t backend_port, int32_t app_port)
	{
		BPrivate::CosmoeBackendFactory* factory = BPrivate::CosmoeBackendFactory::Instance();
		if (!factory) return;
		BPrivate::CosmoeBackend* backend = factory->GetBackend();
		if (!backend) return;
		backend->ProcessBackendMessages(backend_port, app_port);
	}
}
