/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Cocoa window implementation for macOS
 * 
 * NOTE: This is a preliminary implementation and cannot be fully tested
 * until compiled on actual macOS hardware. Some Cocoa-specific details
 * may need adjustment during testing.
 * 
 * macOS Implementation Notes:
 * - Uses NSWindow and NSView for windowing
 * - NSApplication for event loop
 * - CGContext wrapped in cairo surface for drawing
 * - NSPasteboard for clipboard
 * - No X11 or Wayland dependencies
 */

#define COSMOE_NO_SUPPORT_TYPES 1
#if defined(__APPLE__)
#include <private/support/apple_compat.h>
#endif
#include <stddef.h>
#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <cairo.h>
#include <cairo-quartz.h>

// Forward declarations
struct window;
struct widget;

// Translate macOS keyCode to Linux-style input event code
// macOS uses different key codes than Linux, so we need to map them
static uint32_t translate_macos_keycode(uint32_t macKeyCode) {
	// Map common macOS key codes to Linux input-event-codes
	// Reference: https://developer.apple.com/documentation/appkit/nsevent/specialkey
	switch (macKeyCode) {
		// Letters (macOS uses same as ASCII for a-z)
		case 0:  return 30;  // A -> KEY_A (not used in modifier logic, but included for completeness)
		case 11: return 48;  // B -> KEY_B
		// ... (letters mostly not needed for modifier detection)
		
		// Modifiers
		case 56: return 42;  // Left Shift -> KEY_LEFTSHIFT
		case 60: return 54;  // Right Shift -> KEY_RIGHTSHIFT
		case 59: return 29;  // Left Control -> KEY_LEFTCTRL
		case 62: return 97;  // Right Control -> KEY_RIGHTCTRL
		case 58: return 56;  // Left Alt/Option -> KEY_LEFTALT
		case 61: return 100; // Right Alt/Option -> KEY_RIGHTALT
		case 55: return 125; // Left Command (treat as Windows key)
		case 54: return 126; // Right Command
		case 110: return 139; // Menu/Application key -> KEY_MENU
		
		// Lock keys
		case 57: return 58;  // Caps Lock -> KEY_CAPSLOCK
		
		// Arrow keys
		case 126: return 103; // Up -> KEY_UP
		case 125: return 108; // Down -> KEY_DOWN
		case 123: return 105; // Left -> KEY_LEFT
		case 124: return 106; // Right -> KEY_RIGHT
		
		// Navigation
		case 115: return 102; // Home -> KEY_HOME
		case 119: return 107; // End -> KEY_END
		case 116: return 104; // Page Up -> KEY_PAGEUP
		case 121: return 109; // Page Down -> KEY_PAGEDOWN
		case 114: return 110; // Insert (Help on Mac) -> KEY_INSERT
		case 117: return 111; // Delete (Forward Delete) -> KEY_DELETE
		
		// Function keys
		case 122: return 59;  // F1 -> KEY_F1
		case 120: return 60;  // F2 -> KEY_F2
		case 99:  return 61;  // F3 -> KEY_F3
		case 118: return 62;  // F4 -> KEY_F4
		case 96:  return 63;  // F5 -> KEY_F5
		case 97:  return 64;  // F6 -> KEY_F6
		case 98:  return 65;  // F7 -> KEY_F7
		case 100: return 66;  // F8 -> KEY_F8
		case 101: return 67;  // F9 -> KEY_F9
		case 109: return 68;  // F10 -> KEY_F10
		case 103: return 87;  // F11 -> KEY_F11
		case 111: return 88;  // F12 -> KEY_F12
		
		// Special keys
		case 53:  return 1;   // Escape -> KEY_ESC
		case 48:  return 15;  // Tab -> KEY_TAB
		case 36:  return 28;  // Return -> KEY_ENTER
		case 76:  return 28;  // Enter (numpad) -> KEY_ENTER
		case 51:  return 14;  // Backspace (Delete) -> KEY_BACKSPACE
		case 49:  return 57;  // Space -> KEY_SPACE
		
		// For any unmapped keys, return the macOS keyCode directly
		// This allows letters/numbers to work even if not explicitly mapped
		default:  return macKeyCode;
	}
}

// Custom NSView subclass for handling events
@interface CosmoeView : NSView
@property (nonatomic, assign) struct widget* widget;
@end

@implementation CosmoeView

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
	[super drawRect:dirtyRect];
	
	if (self.widget && self.widget->redraw_handler) {
		self.widget->redraw_handler(self.widget, self.widget->user_data);
	}
}

- (void)keyDown:(NSEvent*)event {
	if (!self.widget || !self.widget->window || !self.widget->window->key_handler)
		return;
	
	NSString* chars = [event characters];
	uint32_t macKeyCode = [event keyCode];
	uint32_t linuxKeyCode = translate_macos_keycode(macKeyCode);
	uint32_t unicode = [chars length] > 0 ? [chars characterAtIndex:0] : 0;
	uint32_t time = (uint32_t)([event timestamp] * 1000.0); // Convert to milliseconds
	
	self.widget->window->key_handler(self.widget->window, NULL, time, linuxKeyCode, unicode, 1, NULL);
}

- (void)keyUp:(NSEvent*)event {
	if (!self.widget || !self.widget->window || !self.widget->window->key_handler)
		return;
	
	NSString* chars = [event characters];
	uint32_t macKeyCode = [event keyCode];
	uint32_t linuxKeyCode = translate_macos_keycode(macKeyCode);
	uint32_t unicode = [chars length] > 0 ? [chars characterAtIndex:0] : 0;
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	
	self.widget->window->key_handler(self.widget->window, NULL, time, linuxKeyCode, unicode, 0, NULL);
}

- (void)mouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 1; // Left button
	
	self.widget->button_handler(self.widget, NULL, time, button, 1, self.widget->user_data);
}

- (void)mouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 1;
	
	self.widget->button_handler(self.widget, NULL, time, button, 0, self.widget->user_data);
}

- (void)rightMouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 3; // Right button
	
	self.widget->button_handler(self.widget, NULL, time, button, 1, self.widget->user_data);
}

- (void)rightMouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 3;
	
	self.widget->button_handler(self.widget, NULL, time, button, 0, self.widget->user_data);
}

- (void)otherMouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 2; // Middle button
	
	self.widget->button_handler(self.widget, NULL, time, button, 1, self.widget->user_data);
}

- (void)otherMouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 2;
	
	self.widget->button_handler(self.widget, NULL, time, button, 0, self.widget->user_data);
}

- (void)mouseMoved:(NSEvent*)event {
	if (!self.widget || !self.widget->motion_handler)
		return;
	
	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	
	self.widget->motion_handler(self.widget, NULL, time, (float)point.x, (float)point.y, self.widget->user_data);
}

- (void)mouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
	[self mouseMoved:event];
}

- (void)scrollWheel:(NSEvent*)event {
	if (!self.widget || !self.widget->axis_handler)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	double deltaY = [event scrollingDeltaY];
	double deltaX = [event scrollingDeltaX];
	
	// Send vertical scroll
	if (deltaY != 0.0) {
		self.widget->axis_handler(self.widget, NULL, time, 0, deltaY, self.widget->user_data);
	}
	
	// Send horizontal scroll
	if (deltaX != 0.0) {
		self.widget->axis_handler(self.widget, NULL, time, 1, deltaX, self.widget->user_data);
	}
}

@end

// NSWindow delegate for handling window events
@interface CosmoeWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) struct window* window;
@end

@implementation CosmoeWindowDelegate

- (void)windowDidMove:(NSNotification*)notification {
	if (!self.window || !self.window->move_handler)
		return;
	
	NSWindow* nswindow = [notification object];
	NSRect frame = [nswindow frame];
	
	self.window->x = (int32_t)frame.origin.x;
	self.window->y = (int32_t)frame.origin.y;
	
	self.window->move_handler(self.window, self.window->x, self.window->y, self.window->move_user_data);
}

- (void)windowDidResize:(NSNotification*)notification {
	if (!self.window || !self.window->widget)
		return;
	
	NSWindow* nswindow = [notification object];
	NSRect frame = [nswindow contentRectForFrameRect:[nswindow frame]];
	
	int32_t width = (int32_t)frame.size.width;
	int32_t height = (int32_t)frame.size.height;
	
	self.window->width = width;
	self.window->height = height;
	
	if (self.window->widget->resize_handler) {
		self.window->widget->resize_handler(self.window->widget, width, height, self.window->widget->user_data);
	}
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
	if (!self.window || !self.window->focus_handler)
		return;
	
	self.window->focus_handler(self.window, true, self.window->focus_user_data);
}

- (void)windowDidResignKey:(NSNotification*)notification {
	if (!self.window || !self.window->focus_handler)
		return;
	
	self.window->focus_handler(self.window, false, self.window->focus_user_data);
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
	if (!self.window || !self.window->close_handler)
		return YES;
	
	self.window->close_handler(self.window->user_data);
	return NO; // Let the application decide whether to actually close
}

@end

// Internal structures
struct display {
	void* nsapp;           // NSApplication* 
	void* user_data;
	bool running;
	struct window* window_list;
	int screen_width;
	int screen_height;
};

struct window {
	struct display* display;
	void* nswindow;        // NSWindow*
	struct windowframe* frame;
	struct widget* widget;
	void* user_data;
	
	// Callbacks
	cocoa_key_handler_t key_handler;
	cocoa_close_handler_t close_handler;
	cocoa_move_handler_t move_handler;
	cocoa_focus_handler_t focus_handler;
	void* move_user_data;
	void* focus_user_data;
	
	char* title;
	int32_t x, y;
	int32_t width, height;
	bool is_popup;
	bool is_offscreen;
	
	struct window* next; // Linked list
};

struct windowframe {
	struct window* window;
	void* user_data;
	cocoa_windowframe_resize_handler_t resize_handler;
	int32_t width, height;
};

struct widget {
	struct window* window;
	void* nsview;          // NSView*
	void* user_data;
	
	// Callbacks
	cocoa_redraw_handler_t redraw_handler;
	cocoa_resize_handler_t resize_handler;
	cocoa_button_handler_t button_handler;
	cocoa_motion_handler_t motion_handler;
	cocoa_axis_handler_t axis_handler;
	cocoa_idle_handler_t idle_handler;
	
	struct rectangle allocation;
	cairo_surface_t* surface;
	void* cg_context;      // CGContextRef
};

// Display management
struct display* display_create(int* argc, char** argv)
{
	struct display* display = (struct display*)calloc(1, sizeof(struct display));
	if (!display)
		return NULL;
	
	// Initialize NSApplication
	@autoreleasepool {
		[NSApplication sharedApplication];
		display->nsapp = [NSApp retain];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp activateIgnoringOtherApps:YES];
		
		// Get screen dimensions
		NSScreen* screen = [NSScreen mainScreen];
		NSRect screenRect = [screen frame];
		display->screen_width = (int)screenRect.size.width;
		display->screen_height = (int)screenRect.size.height;
	}
	
	display->running = false;
	display->window_list = NULL;
	
	return display;
}

void display_destroy(struct display* display)
{
	if (!display)
		return;
	
	@autoreleasepool {
		if (display->nsapp) {
			[(NSApplication*)display->nsapp release];
		}
	}
	
	free(display);
}

void display_run(struct display* display)
{
	if (!display)
		return;
	
	display->running = true;
	
	@autoreleasepool {
		[NSApp run];
	}
}

void display_exit(struct display* display)
{
	if (!display)
		return;
	
	display->running = false;
	
	@autoreleasepool {
		[NSApp stop:nil];
		// Post a dummy event to wake up the event loop
		NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
						    location:NSMakePoint(0, 0)
					       modifierFlags:0
						   timestamp:0.0
						windowNumber:0
						     context:nil
						     subtype:0
						       data1:0
						       data2:0];
		[NSApp postEvent:event atStart:YES];
	}
}

void display_flush(struct display* display)
{
	// Cocoa handles flushing automatically
	(void)display;
}

void display_trigger_redraw(struct display* display, struct window* window, struct widget* widget)
{
	if (!widget || !widget->nsview)
		return;
	
	@autoreleasepool {
		NSView* view = (NSView*)widget->nsview;
		[view setNeedsDisplay:YES];
	}
}

void display_get_screen_dimensions(struct display* display, struct rectangle* allocation)
{
	if (!display || !allocation)
		return;
	
	allocation->x = 0;
	allocation->y = 0;
	allocation->width = display->screen_width;
	allocation->height = display->screen_height;
}

void* display_get_user_data(struct display* display)
{
	return display ? display->user_data : NULL;
}

void display_set_user_data(struct display* display, void* data)
{
	if (display)
		display->user_data = data;
}

// Cursor conversion (Be cursor ID to Cocoa cursor)
int32_t display_convert_cursor(int32_t be_cursor_id)
{
	// TODO: Map BeOS cursor IDs to NSCursor types
	// For now, return the ID as-is
	return be_cursor_id;
}

// Clipboard management
int display_set_clipboard_text(struct display* display, const char* text, size_t length)
{
	@autoreleasepool {
		NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
		[pasteboard clearContents];
		NSString* string = [[NSString alloc] initWithBytes:text
							     length:length
							   encoding:NSUTF8StringEncoding];
		[pasteboard setString:string forType:NSPasteboardTypeString];
		[string release];
		return 0;
	}
}

char* display_get_clipboard_text(struct display* display, size_t* out_length)
{
	@autoreleasepool {
		NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
		NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
		if (!string) {
			if (out_length)
				*out_length = 0;
			return NULL;
		}
		
		const char* utf8 = [string UTF8String];
		size_t len = strlen(utf8);
		char* result = (char*)malloc(len + 1);
		if (result) {
			memcpy(result, utf8, len + 1);
			if (out_length)
				*out_length = len;
		}
		return result;
	}
}

// Window management
struct window* window_create(struct display* display, bool offscreen)
{
	if (!display)
		return NULL;
	
	struct window* window = (struct window*)calloc(1, sizeof(struct window));
	if (!window)
		return NULL;
	
	window->display = display;
	window->is_offscreen = offscreen;
	window->is_popup = false;
	window->width = 640;
	window->height = 480;
	
	@autoreleasepool {
		// Create NSWindow
		NSRect contentRect = NSMakeRect(100, 100, window->width, window->height);
		NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | 
					      NSWindowStyleMaskClosable |
					      NSWindowStyleMaskMiniaturizable |
					      NSWindowStyleMaskResizable;
		
		NSWindow* nswindow = [[NSWindow alloc] initWithContentRect:contentRect
								  styleMask:styleMask
								    backing:NSBackingStoreBuffered
								      defer:NO];
		window->nswindow = nswindow;
		
		// Set up window properties
		[nswindow setReleasedWhenClosed:NO];
		[nswindow setAcceptsMouseMovedEvents:YES];
		
		// Create and set the delegate
		CosmoeWindowDelegate* delegate = [[CosmoeWindowDelegate alloc] init];
		delegate.window = window;
		[nswindow setDelegate:delegate];
		
		// Create custom content view for event handling
		CosmoeView* contentView = [[CosmoeView alloc] initWithFrame:contentRect];
		[nswindow setContentView:contentView];
		
		[nswindow makeKeyAndOrderFront:nil];
	}
	
	// Add to window list
	window->next = display->window_list;
	display->window_list = window;
	
	return window;
}

struct window* window_popup_create(struct display* display, struct window* parent, int32_t x, int32_t y)
{
	struct window* window = window_create(display, false);
	if (!window)
		return NULL;
	
	window->is_popup = true;
	window->x = x;
	window->y = y;
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			// Make it a popup-style window
			[nswindow setLevel:NSPopUpMenuWindowLevel];
			[nswindow setStyleMask:NSWindowStyleMaskBorderless];
		}
	}
	
	return window;
}

void window_get_position(struct window* window, int32_t* x, int32_t* y)
{
	if (!window)
		return;
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			NSRect frame = [nswindow frame];
			if (x) *x = (int32_t)frame.origin.x;
			if (y) *y = (int32_t)frame.origin.y;
			return;
		}
	}
	
	if (x) *x = window->x;
	if (y) *y = window->y;
}

void window_set_position(struct window* window, int32_t x, int32_t y)
{
	if (!window)
		return;
	
	window->x = x;
	window->y = y;
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			NSRect frame = [nswindow frame];
			frame.origin.x = x;
			frame.origin.y = y;
			[nswindow setFrame:frame display:YES];
		}
	}
}

void window_get_decorator_size(struct window* window, int32_t* borderWidth, int32_t* tabHeight)
{
	// macOS window decorations are handled by the OS
	// Return approximate values
	if (borderWidth)
		*borderWidth = 0; // No visible border on macOS
	if (tabHeight)
		*tabHeight = 22; // Approximate title bar height
}

struct windowframe* windowframe_create(struct window* window, void* data)
{
	if (!window)
		return NULL;
	
	struct windowframe* frame = (struct windowframe*)calloc(1, sizeof(struct windowframe));
	if (!frame)
		return NULL;
	
	frame->window = window;
	frame->user_data = data;
	frame->width = window->width;
	frame->height = window->height;
	
	window->frame = frame;
	
	return frame;
}

void window_destroy(struct window* window, struct windowframe* frame)
{
	if (!window)
		return;
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			
			// Clean up delegate
			id delegate = [nswindow delegate];
			if (delegate) {
				[nswindow setDelegate:nil];
				[delegate release];
			}
			
			// Clean up content view
			CosmoeView* view = (CosmoeView*)[nswindow contentView];
			if ([view isKindOfClass:[CosmoeView class]]) {
				view.widget = NULL;
				[view release];
			}
			
			[nswindow close];
			[nswindow release];
		}
	}
	
	if (window->title)
		free(window->title);
	
	if (frame)
		free(frame);
	
	// Remove from window list
	struct display* display = window->display;
	if (display) {
		struct window** ptr = &display->window_list;
		while (*ptr) {
			if (*ptr == window) {
				*ptr = window->next;
				break;
			}
			ptr = &(*ptr)->next;
		}
	}
	
	free(window);
}

void window_set_title(struct window* window, const char* title)
{
	if (!window || !title)
		return;
	
	if (window->title)
		free(window->title);
	window->title = strdup(title);
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			NSString* string = [NSString stringWithUTF8String:title];
			[nswindow setTitle:string];
		}
	}
}

void window_set_app_id(struct window* window, const char* app_id)
{
	// App ID is less relevant on macOS - could map to bundle identifier
	(void)window;
	(void)app_id;
}

void window_schedule_resize(struct window* window, struct windowframe* frame, int width, int height)
{
	if (!window)
		return;
	
	window->width = width;
	window->height = height;
	
	if (frame) {
		frame->width = width;
		frame->height = height;
	}
	
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			NSRect frame = [nswindow frame];
			frame.size.width = width;
			frame.size.height = height;
			[nswindow setFrame:frame display:YES animate:NO];
		}
	}
}

void window_set_min_max_allocation(struct window* window, int min_width, int min_height,
				   int max_width, int max_height)
{
	@autoreleasepool {
		if (window && window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			[nswindow setMinSize:NSMakeSize(min_width, min_height)];
			if (max_width > 0 && max_height > 0)
				[nswindow setMaxSize:NSMakeSize(max_width, max_height)];
		}
	}
}

void window_set_key_handler(struct window* window, cocoa_key_handler_t handler)
{
	if (window)
		window->key_handler = handler;
}

void window_set_close_handler(struct window* window, cocoa_close_handler_t handler)
{
	if (window)
		window->close_handler = handler;
}

struct display* window_get_display(struct window* window)
{
	return window ? window->display : NULL;
}

void window_set_user_data(struct window* window, void* data)
{
	if (window)
		window->user_data = data;
}

void* window_get_user_data(struct window* window)
{
	return window ? window->user_data : NULL;
}

void* window_get_surface(struct window* window)
{
	// Return the cached cairo surface, creating it if needed
	if (!window || !window->widget)
		return NULL;
	
	struct widget* widget = window->widget;
	
	// If we already have a surface, return it
	if (widget->surface)
		return widget->surface;
	
	// Get the CGContext from the current NSGraphicsContext
	// Note: This should be called during a draw operation when NSGraphicsContext is valid
	@autoreleasepool {
		NSGraphicsContext* nsContext = [NSGraphicsContext currentContext];
		if (!nsContext)
			return NULL;
		
		CGContextRef cgContext = (CGContextRef)[nsContext CGContext];
		if (!cgContext)
			return NULL;
		
		// Cache the CGContext
		widget->cg_context = cgContext;
		
		// Create cairo surface from CGContext
		// Use the widget's allocation dimensions
		widget->surface = cairo_quartz_surface_create_for_cg_context(
			cgContext,
			widget->allocation.width,
			widget->allocation.height
		);
		
		return widget->surface;
	}
}

void window_get_topview_offset(struct window* window, int32_t* offset_h, int32_t* offset_v)
{
	// macOS windows don't have the same topview concept
	// Return 0 offsets
	if (offset_h) *offset_h = 0;
	if (offset_v) *offset_v = 0;
}

// Window frame management
void windowframe_set_resize_handler(struct window* window, struct windowframe* frame,
				    cocoa_windowframe_resize_handler_t handler)
{
	if (frame)
		frame->resize_handler = handler;
}

void window_set_move_handler(struct window* window, cocoa_move_handler_t handler, void* user_data)
{
	if (window) {
		window->move_handler = handler;
		window->move_user_data = user_data;
	}
}

void window_set_focus_handler(struct window* window, cocoa_focus_handler_t handler, void* user_data)
{
	if (window) {
		window->focus_handler = handler;
		window->focus_user_data = user_data;
	}
}

void window_show_menu(struct display* display, void* input, uint32_t time,
		     struct window* window, int32_t x, int32_t y,
		     cocoa_window_menu_func_t func, void* user_data,
		     const char** entries, int count)
{
	// TODO: Implement NSMenu popup at location
	// This is a complex operation requiring Objective-C code
	(void)display;
	(void)input;
	(void)time;
	(void)window;
	(void)x;
	(void)y;
	(void)func;
	(void)user_data;
	(void)entries;
	(void)count;
}

// Widget management stubs
struct widget* widget_create(struct window* window)
{
	if (!window)
		return NULL;
	
	struct widget* widget = (struct widget*)calloc(1, sizeof(struct widget));
	if (!widget)
		return NULL;
	
	widget->window = window;
	widget->allocation.width = window->width;
	widget->allocation.height = window->height;
	
	// Connect the widget to the NSView
	@autoreleasepool {
		if (window->nswindow) {
			NSWindow* nswindow = (NSWindow*)window->nswindow;
			CosmoeView* view = (CosmoeView*)[nswindow contentView];
			if ([view isKindOfClass:[CosmoeView class]]) {
				view.widget = widget;
				widget->nsview = view;
			}
		}
	}
	
	window->widget = widget;
	
	return widget;
}

void widget_destroy(struct widget* widget)
{
	if (!widget)
		return;
	
	if (widget->surface)
		cairo_surface_destroy(widget->surface);
	
	free(widget);
}

void widget_set_redraw_handler(struct widget* widget, cocoa_redraw_handler_t handler)
{
	if (widget)
		widget->redraw_handler = handler;
}

void widget_set_resize_handler(struct widget* widget, cocoa_resize_handler_t handler)
{
	if (widget)
		widget->resize_handler = handler;
}

void widget_set_button_handler(struct widget* widget, cocoa_button_handler_t handler)
{
	if (widget)
		widget->button_handler = handler;
}

void widget_set_motion_handler(struct widget* widget, cocoa_motion_handler_t handler)
{
	if (widget)
		widget->motion_handler = handler;
}

void widget_set_axis_handler(struct widget* widget, cocoa_axis_handler_t handler)
{
	if (widget)
		widget->axis_handler = handler;
}

void widget_set_idle_handler(struct widget* widget, cocoa_idle_handler_t handler)
{
	if (widget)
		widget->idle_handler = handler;
}

void widget_set_user_data(struct widget* widget, void* data)
{
	if (widget)
		widget->user_data = data;
}

void* widget_get_user_data(struct widget* widget)
{
	return widget ? widget->user_data : NULL;
}

void widget_schedule_redraw(struct widget* widget)
{
	if (!widget || !widget->nsview)
		return;
	
	@autoreleasepool {
		NSView* view = (NSView*)widget->nsview;
		[view setNeedsDisplay:YES];
	}
}

void widget_schedule_resize(struct widget* widget, int32_t width, int32_t height)
{
	if (!widget)
		return;
	
	widget->allocation.width = width;
	widget->allocation.height = height;
	
	// Invalidate the cairo surface when resizing - it will be recreated with new dimensions
	if (widget->surface) {
		cairo_surface_destroy(widget->surface);
		widget->surface = NULL;
		widget->cg_context = NULL;
	}
	
	if (widget->resize_handler)
		widget->resize_handler(widget, width, height, widget->user_data);
}

void widget_get_allocation(struct widget* widget, struct rectangle* allocation)
{
	if (widget && allocation)
		*allocation = widget->allocation;
}

void widget_set_allocation(struct widget* widget, int32_t x, int32_t y, int32_t width, int32_t height)
{
	if (!widget)
		return;
	
	// Check if dimensions changed
	bool dimensions_changed = (widget->allocation.width != width || widget->allocation.height != height);
	
	widget->allocation.x = x;
	widget->allocation.y = y;
	widget->allocation.width = width;
	widget->allocation.height = height;
	
	// Invalidate the cairo surface when dimensions change
	if (dimensions_changed && widget->surface) {
		cairo_surface_destroy(widget->surface);
		widget->surface = NULL;
		widget->cg_context = NULL;
	}
}
