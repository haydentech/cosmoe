/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Graphics backend abstraction interface for supporting multiple
 * windowing systems (Wayland, X11, etc.)
 */

#ifndef _COSMOE_BACKEND_H_
#define _COSMOE_BACKEND_H_

#include <stdint.h>
#include <cairo.h>
#include <stddef.h> /* for size_t */
#include <SupportDefs.h>  // for status_t
#include "rectangle.h"

// Forward declarations
class BRect;

namespace BPrivate {
	class CosmoeBackend;
	class CosmoeBackendFactory;
}

namespace BPrivate {

// Opaque handle types - each backend will cast these to their own types
typedef void* backend_display_t;
typedef void* backend_window_t;
typedef void* backend_windowframe_t;
typedef void* backend_widget_t;

// Backend types
enum backend_type {
	BACKEND_AUTO = 0,     // Auto-detect based on environment
	BACKEND_WAYLAND,
	BACKEND_X11,
	BACKEND_COCOA,        // macOS Cocoa backend
	BACKEND_WINDOWS       // Windows backend
};

// Callback function types (unified across backends)
typedef void (*key_handler_t)(backend_window_t window, void* input,
			     uint32_t time, uint32_t key, uint32_t unicode,
			     uint32_t state, void *data);

typedef void (*close_handler_t)(void *data);

typedef void (*redraw_handler_t)(backend_widget_t widget, void *data);

typedef void (*resize_handler_t)(backend_widget_t widget, int32_t width,
				int32_t height, void *data);

// Windowframe-specific resize handler (frames are distinct from widgets)
typedef void (*windowframe_resize_handler_t)(backend_windowframe_t frame, int32_t width,
                                             int32_t height, void *data);
typedef void (*move_handler_t)(backend_window_t window, int32_t x, int32_t y, void* user_data);
typedef void (*focus_handler_t)(backend_window_t window, bool focused, void* user_data);

typedef void (*button_handler_t)(backend_widget_t widget, void* input,
				uint32_t time, uint32_t button,
				uint32_t state, void *data);

typedef int (*motion_handler_t)(backend_widget_t widget, void* input,
				uint32_t time, float x, float y,
				void *data);

typedef void (*axis_handler_t)(backend_widget_t widget, void* input,
			      uint32_t time, uint32_t axis,
			      double value, void *data);

typedef void (*idle_handler_t)(backend_widget_t widget, void* input,
			      uint32_t time, int32_t x, int32_t y,
			      void *data);

// Menu callback typed in terms of user_data and input pointer; index is selected
typedef void (*window_menu_func_t)(void* user_data, void* input, int index);


/**
 * CosmoeBackend - Abstract interface for windowing system backends
 * 
 * This class defines the interface that all backend implementations
 * (Wayland, X11, etc.) must implement. Each backend is loaded as a
 * shared library plugin at runtime.
 */
class CosmoeBackend {
public:
	virtual ~CosmoeBackend() {}

	// Display management
	virtual backend_display_t DisplayCreate(int* argc, char** argv) = 0;
	virtual void DisplayDestroy(backend_display_t display) = 0;
	virtual void DisplayRun(backend_display_t display) = 0;
	virtual void DisplayExit(backend_display_t display) = 0;
	virtual void DisplayTriggerRedraw(backend_display_t display,
					 backend_window_t window,
					 backend_widget_t widget) = 0;
	// Get main screen/surface size for the display
	virtual void DisplayGetScreenDimensions(backend_display_t display, struct rectangle* allocation) = 0;
	virtual void* DisplayGetUserData(backend_display_t display) = 0;
	virtual void DisplaySetUserData(backend_display_t display, void* data) = 0;
	virtual void DisplaySetPort(backend_display_t display, int32_t sender_port_id, int32_t receiver_port_id) = 0;

	// Cursor management
	virtual int32_t DisplayConvertCursor(int32_t beCursorID) = 0;

	// Clipboard management
	virtual int DisplaySetClipboardText(backend_display_t display, const char* text, size_t length) = 0;
	virtual char* DisplayGetClipboardText(backend_display_t display, size_t* out_length) = 0;

	// Window management
	virtual backend_window_t WindowLookupByToken(backend_display_t display, int32_t token) = 0;
	virtual backend_window_t WindowCreate(backend_display_t display, int32_t token, bool offscreen, void* data) = 0;
	virtual backend_window_t WindowPopupCreate(backend_display_t display, int32_t token, int32_t parent_token, int32_t x, int32_t y, int32_t width, int32_t height, void* data) = 0;
	virtual void WindowGetPosition(backend_window_t window, int32_t* x, int32_t* y) = 0;
	// Set window position in absolute screen coordinates (may be a no-op on some backends)
	virtual void WindowSetPosition(backend_window_t window, int32_t x, int32_t y) = 0;
    // Get decorator sizes: border width (left side) and tab/title bar height
    virtual void WindowGetDecoratorSize(backend_window_t window, int32_t* borderWidth, int32_t* tabHeight) = 0;
	virtual void WindowDestroy(backend_window_t window) = 0;
	virtual void WindowShow(backend_window_t window) = 0;
	virtual void WindowHide(backend_window_t window) = 0;
	virtual void WindowSetAppId(backend_window_t window, const char* appId) = 0;
	virtual void WindowSetParent(backend_window_t window, backend_window_t parent_window) = 0;
	virtual void WindowScheduleResize(backend_window_t window, int width, int height) = 0;
	virtual void WindowSetMinMaxAllocation(backend_window_t window,
					      int min_width, int min_height,
					      int max_width, int max_height) = 0;
	
	// PortLink message handling virtual methods
	virtual void WindowSetTitle(backend_window_t window, const char* title) = 0;
	virtual void WindowResize(backend_window_t window, float width, float height) = 0;
	virtual void WindowMinimize(backend_window_t window, bool minimize) = 0;
	virtual void WindowActivate(backend_window_t window, bool active) = 0;
	virtual void WindowSetSizeLimits(backend_window_t window,
	                                  float minWidth, float maxWidth,
	                                  float minHeight, float maxHeight,
	                                  BRect* outFrame,
	                                  float* outMinWidth, float* outMaxWidth,
	                                  float* outMinHeight, float* outMaxHeight) = 0;

	
	virtual void WindowSetKeyHandler(backend_window_t window,
					 key_handler_t handler) = 0;
	virtual void WindowSetCloseHandler(backend_window_t window,
					   close_handler_t handler) = 0;
	virtual backend_display_t WindowGetDisplay(backend_window_t window) = 0;
	virtual void* WindowGetUserData(backend_window_t window) = 0;
	virtual cairo_surface_t* WindowGetSurface(backend_window_t window) = 0;
	virtual void WindowGetTopviewOffset(backend_window_t window,
					    int32_t* offset_h, int32_t* offset_v) = 0;

	// Window frame management
	virtual void WindowframeSetResizeHandler(backend_window_t window,
						 windowframe_resize_handler_t handler) = 0;

	// Movement callback
	virtual void WindowSetMoveHandler(backend_window_t window, move_handler_t handler, void* user_data) = 0;
	
	// Focus callback
	virtual void WindowSetFocusHandler(backend_window_t window, focus_handler_t handler, void* user_data) = 0;

	// Widget management
	virtual backend_widget_t WindowAddWidget(backend_window_t window, void* data) = 0;
	virtual void WidgetDestroy(backend_widget_t widget) = 0;
	virtual void WidgetSetRedrawHandler(backend_widget_t widget,
					   redraw_handler_t handler) = 0;
	virtual void WidgetSetResizeHandler(backend_widget_t widget,
					   resize_handler_t handler) = 0;
	virtual void WidgetSetButtonHandler(backend_widget_t widget,
					   button_handler_t handler) = 0;
	virtual void WidgetSetMotionHandler(backend_widget_t widget,
					   motion_handler_t handler) = 0;
	virtual void WidgetSetAxisHandler(backend_widget_t widget,
				 axis_handler_t handler) = 0;
	virtual void WidgetSetIdleHandler(backend_widget_t widget,
				 idle_handler_t handler) = 0;
	virtual backend_window_t WidgetGetWindow(backend_widget_t widget) = 0;
	virtual void WidgetGetAllocation(backend_widget_t widget, struct rectangle* allocation) = 0;
	virtual void WidgetSetUserData(backend_widget_t widget, void *user_data) = 0;
	virtual void WidgetSetAllocation(backend_widget_t widget,
					int32_t x, int32_t y,
					int32_t width, int32_t height) = 0;
	virtual void WidgetScheduleResize(backend_widget_t widget,
					 int32_t width, int32_t height) = 0;
	virtual void WidgetScheduleRedraw(backend_widget_t widget) = 0;

	// Input management
	virtual void InputGetPosition(void* input, int32_t* x, int32_t* y) = 0;

	virtual cairo_t* WidgetCairoCreate(backend_widget_t widget) = 0;

	// Display scaling support
	virtual void WindowSetBufferScale(backend_window_t window, int32_t scale) = 0;
	virtual void WidgetSetBufferScale(backend_widget_t widget, int32_t scale) = 0;
	// Get the display scale factor for this window (1, 2, 3, etc.)
	virtual int32_t WindowGetDisplayScale(backend_window_t window) = 0;

	// Backend identification
	virtual backend_type GetType() const = 0;
	virtual const char* GetName() const = 0;
	
	// Shared message processing (non-virtual helper defined in libbe)
	void ProcessBackendMessages(int32_t backend_port, int32_t app_port);
};


/**
 * CosmoeBackendFactory - Creates and manages backend instances
 * 
 * This factory class handles:
 * - Auto-detection of available windowing systems
 * - Loading backend plugins (shared libraries)
 * - Singleton backend instance management
 */
class CosmoeBackendFactory {
public:
	// Get the singleton instance
	static CosmoeBackendFactory* Instance();

	// Create/get backend (auto-detect or explicit type)
	CosmoeBackend* GetBackend(backend_type type = BACKEND_AUTO);

	// Explicitly set which backend to use
	void SetPreferredBackend(backend_type type);

	// Check if a specific backend is available
	bool IsBackendAvailable(backend_type type);

	// Release the current backend
	void ReleaseBackend();

private:
	CosmoeBackendFactory();
	~CosmoeBackendFactory();

	// Auto-detect best available backend
	backend_type DetectBackend();

	// Load backend from shared library
	CosmoeBackend* LoadBackend(backend_type type);

	static CosmoeBackendFactory* sInstance;
	CosmoeBackend* fCurrentBackend;
	backend_type fPreferredType;
	void* fBackendLibHandle;  // Library handle (dlopen/LoadLibrary)
};

} // namespace BPrivate

#endif // _WINDOW_BACKEND_H_
