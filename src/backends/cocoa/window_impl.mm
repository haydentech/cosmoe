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
#include "cocoa_internal_structs.h"
#include <input_event_codes_compat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <cairo.h>
#include <cairo-quartz.h>

#include <Cursor.h>

extern "C" void cocoa_process_backend_messages(int32_t backend_port, int32_t app_port);

#define CUSTOM_CURSOR_BASE 1000
#define MAX_CUSTOM_CURSORS 256

static NSCursor* s_custom_cursors[MAX_CUSTOM_CURSORS];

static NSWindow* create_native_window(struct window* window, bool popup);

@interface CosmoeNativeMenuTarget : NSObject
@property (nonatomic, assign) struct window* window;
@property (nonatomic, assign) int commandID;
- (void)invoke:(id)sender;
@end

static NSCursor*
cursor_from_optional_selector(SEL selector, NSCursor* fallback)
{
	if ([NSCursor respondsToSelector:selector]) {
		typedef NSCursor* (*CursorMethod)(id, SEL);
		CursorMethod method = (CursorMethod)[NSCursor methodForSelector:selector];
		if (method != NULL)
			return method([NSCursor class], selector);
	}

	return fallback;
}

static NSCursor*
invisible_cursor()
{
	static NSCursor* cursor = nil;
	if (cursor == nil) {
		NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(1.0, 1.0)];
		cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSZeroPoint];
	}

	return cursor;
}

static NSCursor*
progress_cursor()
{
	return cursor_from_optional_selector(
		NSSelectorFromString(@"busyButClickableCursor"),
		[NSCursor arrowCursor]);
}

static NSCursor*
diagonal_down_cursor()
{
	return cursor_from_optional_selector(
		NSSelectorFromString(@"resizeDiagonalDownCursor"),
		[NSCursor crosshairCursor]);
}

static NSCursor*
diagonal_up_cursor()
{
	return cursor_from_optional_selector(
		NSSelectorFromString(@"resizeDiagonalUpCursor"),
		[NSCursor crosshairCursor]);
}

static bool
running_app_is_graphical(NSRunningApplication* app, pid_t current_pid)
{
	if (app == nil)
		return false;
	if (![app isFinishedLaunching])
		return false;
	if ([app processIdentifier] <= 0 || [app processIdentifier] == current_pid)
		return false;
	return true;
}

static NSString*
running_app_common_name(NSRunningApplication* app)
{
	if (app == nil)
		return nil;

	NSString* name = [app localizedName];
	if (name != nil && [name length] > 0)
		return name;

	NSURL* bundleURL = [app bundleURL];
	if (bundleURL != nil) {
		name = [[bundleURL lastPathComponent] stringByDeletingPathExtension];
		if (name != nil && [name length] > 0)
			return name;
	}

	NSURL* executableURL = [app executableURL];
	if (executableURL != nil) {
		name = [[executableURL lastPathComponent] stringByDeletingPathExtension];
		if (name != nil && [name length] > 0)
			return name;
	}

	return nil;
}

static NSArray*
copy_visible_window_info(void)
{
	CFArrayRef windowArray = CGWindowListCopyWindowInfo(
		kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
		kCGNullWindowID);
	if (windowArray == NULL)
		return nil;

	return CFBridgingRelease(windowArray);
}

static bool
window_info_is_graphical(NSDictionary* windowInfo, pid_t current_pid)
{
	NSNumber* ownerPid = windowInfo[(NSString*)kCGWindowOwnerPID];
	NSNumber* layer = windowInfo[(NSString*)kCGWindowLayer];
	NSNumber* windowNumber = windowInfo[(NSString*)kCGWindowNumber];
	NSString* ownerName = windowInfo[(NSString*)kCGWindowOwnerName];

	if (ownerPid == nil || layer == nil || windowNumber == nil)
		return false;
	if ([ownerPid intValue] <= 0 || [ownerPid intValue] == (int)current_pid)
		return false;
	if ([layer intValue] != 0)
		return false;
	if (ownerName == nil || [ownerName length] == 0)
		return false;

	return true;
}

static NSString*
common_name_for_pid_from_windows(NSArray* windows, pid_t pid)
{
	for (NSDictionary* windowInfo in windows) {
		NSNumber* ownerPid = windowInfo[(NSString*)kCGWindowOwnerPID];
		NSString* ownerName = windowInfo[(NSString*)kCGWindowOwnerName];
		if (ownerPid == nil || [ownerPid intValue] != (int)pid)
			continue;
		if (ownerName != nil && [ownerName length] > 0)
			return ownerName;
	}

	return nil;
}

@implementation CosmoeNativeMenuTarget

- (void)invoke:(id)sender
{
	(void)sender;
	if (self.window == NULL || self.window->native_menu_func == NULL)
		return;

	self.window->native_menu_func(self.window->native_menu_user_data, NULL,
		self.commandID);
}

@end

static NSMenu* cocoa_build_native_menu(struct window* window,
	const cosmoe_native_menu_item* items, int32_t count, int32_t parentID,
	NSMutableArray* targets);

static NSMenuItem*
cocoa_application_menu_item(void)
{
	NSString* appName = [[NSProcessInfo processInfo] processName];
	NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:@""
		action:nil keyEquivalent:@""];
	NSMenu* appMenu = [[NSMenu alloc] initWithTitle:appName];
	NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
	NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:quitTitle
		action:@selector(terminate:) keyEquivalent:@"q"];
	[appMenu addItem:quitItem];
	[appItem setSubmenu:appMenu];
	return appItem;
}

static NSString*
cocoa_key_equivalent_for_shortcut(const char* shortcut,
	NSEventModifierFlags* modifierMask)
{
	if (modifierMask != NULL)
		*modifierMask = 0;

	if (shortcut == NULL || shortcut[0] == '\0')
		return @"";

	NSString* shortcutString = [NSString stringWithUTF8String:shortcut];
	if (shortcutString == nil || [shortcutString length] == 0)
		return @"";

	NSArray<NSString*>* components = [shortcutString componentsSeparatedByString:@"+"];
	if ([components count] == 0)
		return @"";

	for (NSUInteger i = 0; i + 1 < [components count]; i++) {
		NSString* modifier = [[components objectAtIndex:i]
			stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
		if ([modifier caseInsensitiveCompare:@"Shift"] == NSOrderedSame) {
			if (modifierMask != NULL)
				*modifierMask |= NSEventModifierFlagShift;
		} else if ([modifier caseInsensitiveCompare:@"Ctrl"] == NSOrderedSame
			|| [modifier caseInsensitiveCompare:@"Control"] == NSOrderedSame) {
			if (modifierMask != NULL)
				*modifierMask |= NSEventModifierFlagControl;
		} else if ([modifier caseInsensitiveCompare:@"Alt"] == NSOrderedSame
			|| [modifier caseInsensitiveCompare:@"Option"] == NSOrderedSame) {
			if (modifierMask != NULL)
				*modifierMask |= NSEventModifierFlagOption;
		} else if ([modifier caseInsensitiveCompare:@"Cmd"] == NSOrderedSame
			|| [modifier caseInsensitiveCompare:@"Command"] == NSOrderedSame) {
			if (modifierMask != NULL)
				*modifierMask |= NSEventModifierFlagCommand;
		}
	}

	NSString* key = [[components lastObject]
		stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
	if ([key length] == 0)
		return @"";

	return [key lowercaseString];
}

static NSMenu*
cocoa_build_native_menu(struct window* window,
	const cosmoe_native_menu_item* items, int32_t count, int32_t parentID,
	NSMutableArray* targets)
{
	NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
	for (int32_t i = 0; i < count; i++) {
		const cosmoe_native_menu_item* item = &items[i];
		if (item->parent_id != parentID)
			continue;

		if ((item->flags & COSMOE_NATIVE_MENU_ITEM_SEPARATOR) != 0) {
			[menu addItem:[NSMenuItem separatorItem]];
			continue;
		}

		NSString* title = item->label != NULL
			? [NSString stringWithUTF8String:item->label] : @"";
		NSEventModifierFlags modifierMask = 0;
		NSString* keyEquivalent = cocoa_key_equivalent_for_shortcut(
			item->shortcut, &modifierMask);
		NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:title
			action:nil keyEquivalent:keyEquivalent];
		[menuItem setKeyEquivalentModifierMask:modifierMask];
		[menuItem setEnabled:(item->flags & COSMOE_NATIVE_MENU_ITEM_DISABLED) == 0];
		[menuItem setState:(item->flags & COSMOE_NATIVE_MENU_ITEM_MARKED) != 0
			? NSControlStateValueOn : NSControlStateValueOff];

		if ((item->flags & COSMOE_NATIVE_MENU_ITEM_SUBMENU) != 0) {
			NSMenu* submenu = cocoa_build_native_menu(window, items, count,
				item->command_id, targets);
			[menuItem setSubmenu:submenu];
		} else {
			CosmoeNativeMenuTarget* target = [[CosmoeNativeMenuTarget alloc] init];
			target.window = window;
			target.commandID = item->command_id;
			[targets addObject:target];
			[menuItem setTarget:target];
			[menuItem setAction:@selector(invoke:)];
		}

		[menu addItem:menuItem];
	}

	return menu;
}

static NSMenu*
cocoa_build_native_menubar(struct window* window,
	const cosmoe_native_menu_item* items, int32_t count, NSMutableArray* targets)
{
	NSMenu* menubar = [[NSMenu alloc] initWithTitle:@""];
	[menubar addItem:cocoa_application_menu_item()];

	for (int32_t i = 0; i < count; i++) {
		const cosmoe_native_menu_item* item = &items[i];
		if (item->parent_id != -1)
			continue;
		if ((item->flags & COSMOE_NATIVE_MENU_ITEM_SEPARATOR) != 0)
			continue;

		NSString* title = item->label != NULL
			? [NSString stringWithUTF8String:item->label] : @"";
		NSMenuItem* rootItem = [[NSMenuItem alloc] initWithTitle:title
			action:nil keyEquivalent:@""];
		[menubar addItem:rootItem];

		NSMenu* submenu = cocoa_build_native_menu(window, items, count,
			item->command_id, targets);
		[rootItem setSubmenu:submenu];
	}

	return menubar;
}

// Translate macOS keyCode to Linux-style input event code
// macOS uses different key codes than Linux, so we need to map them
static uint32_t translate_macos_keycode(uint32_t macKeyCode) {
	// Map common macOS key codes to Linux input-event-codes
	// Reference: https://developer.apple.com/documentation/appkit/nsevent/specialkey
	switch (macKeyCode) {
		// Letters
		case 0:  return 30;  // A -> KEY_A
		case 1:  return 31;  // S -> KEY_S
		case 2:  return 32;  // D -> KEY_D
		case 3:  return 33;  // F -> KEY_F
		case 4:  return 35;  // H -> KEY_H
		case 5:  return 34;  // G -> KEY_G
		case 6:  return 44;  // Z -> KEY_Z
		case 7:  return 45;  // X -> KEY_X
		case 8:  return 46;  // C -> KEY_C
		case 9:  return 47;  // V -> KEY_V
		case 11: return 48;  // B -> KEY_B
		case 12: return 16;  // Q -> KEY_Q
		case 13: return 17;  // W -> KEY_W
		case 14: return 18;  // E -> KEY_E
		case 15: return 19;  // R -> KEY_R
		case 16: return 21;  // Y -> KEY_Y
		case 17: return 20;  // T -> KEY_T
		case 31: return 24;  // O -> KEY_O
		case 32: return 22;  // U -> KEY_U
		case 34: return 23;  // I -> KEY_I
		case 35: return 25;  // P -> KEY_P
		case 37: return 38;  // L -> KEY_L
		case 38: return 36;  // J -> KEY_J
		case 40: return 37;  // K -> KEY_K
		case 45: return 49;  // N -> KEY_N
		case 46: return 50;  // M -> KEY_M

		// Number row and punctuation
		case 18: return 2;   // 1 -> KEY_1
		case 19: return 3;   // 2 -> KEY_2
		case 20: return 4;   // 3 -> KEY_3
		case 21: return 5;   // 4 -> KEY_4
		case 22: return 7;   // 6 -> KEY_6
		case 23: return 6;   // 5 -> KEY_5
		case 24: return 13;  // = -> KEY_EQUAL
		case 25: return 10;  // 9 -> KEY_9
		case 26: return 8;   // 7 -> KEY_7
		case 27: return 12;  // - -> KEY_MINUS
		case 28: return 9;   // 8 -> KEY_8
		case 29: return 11;  // 0 -> KEY_0
		case 30: return 27;  // ] -> KEY_RIGHTBRACE
		case 33: return 26;  // [ -> KEY_LEFTBRACE
		case 39: return 40;  // ' -> KEY_APOSTROPHE
		case 41: return 39;  // ; -> KEY_SEMICOLON
		case 42: return 43;  // \ -> KEY_BACKSLASH
		case 43: return 51;  // , -> KEY_COMMA
		case 44: return 53;  // / -> KEY_SLASH
		case 47: return 52;  // . -> KEY_DOT
		case 50: return 41;  // ` -> KEY_GRAVE

		// Keypad
		case 65: return 83;  // Keypad . -> KEY_KPDOT
		case 67: return 55;  // Keypad * -> KEY_KPASTERISK
		case 69: return 78;  // Keypad + -> KEY_KPPLUS
		case 71: return 69;  // Keypad Clear (NumLock) -> KEY_NUMLOCK
		case 75: return 98;  // Keypad / -> KEY_KPSLASH
		case 76: return KEY_KPENTER;
		case 78: return 74;  // Keypad - -> KEY_KPMINUS
		case 81: return 117; // Keypad = -> KEY_KPEQUAL
		case 82: return 82;  // Keypad 0 -> KEY_KP0
		case 83: return 79;  // Keypad 1 -> KEY_KP1
		case 84: return 80;  // Keypad 2 -> KEY_KP2
		case 85: return 81;  // Keypad 3 -> KEY_KP3
		case 86: return 75;  // Keypad 4 -> KEY_KP4
		case 87: return 76;  // Keypad 5 -> KEY_KP5
		case 88: return 77;  // Keypad 6 -> KEY_KP6
		case 89: return 71;  // Keypad 7 -> KEY_KP7
		case 91: return 72;  // Keypad 8 -> KEY_KP8
		case 92: return 73;  // Keypad 9 -> KEY_KP9
		
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
		case 51:  return 14;  // Backspace (Delete) -> KEY_BACKSPACE
		case 49:  return 57;  // Space -> KEY_SPACE
		
		// For any unmapped keys, return 0 so we don't accidentally collide
		// with Linux key constants (e.g. mac keycode 14 == KEY_BACKSPACE).
		default:  return 0;
	}
}

// Custom NSView subclass for handling events
@interface CosmoeWindow : NSWindow
@end

@implementation CosmoeWindow

- (BOOL)canBecomeKeyWindow {
	return YES;
}

@end

@interface CosmoePopupWindow : NSPanel
@end

@implementation CosmoePopupWindow

- (BOOL)canBecomeKeyWindow {
	return NO;
}

- (BOOL)canBecomeMainWindow {
	return NO;
}

@end

@interface CosmoeView : NSView
@property (nonatomic, assign) struct widget* widget;
@property (nonatomic, assign) NSEventModifierFlags lastModifierFlags;
@end

@interface CosmoeWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) struct window* window;
@end

@implementation CosmoeView

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (BOOL)isOpaque {
	return YES;
}

- (BOOL)preservesContentDuringLiveResize {
	return YES;
}

- (void)_eventToBackendCoords:(NSEvent*)event x:(float*)outX y:(float*)outY {
	NSPoint viewPoint = [self convertPoint:[event locationInWindow] fromView:nil];
	NSPoint backingPoint = [self convertPointToBacking:viewPoint];
	NSRect backingBounds = [self convertRectToBacking:self.bounds];

	if (outX)
		*outX = (float)backingPoint.x;
	if (outY)
		*outY = (float)(backingBounds.size.height - backingPoint.y);
}

- (void)drawRect:(NSRect)dirtyRect {
	[super drawRect:dirtyRect];

	struct widget* widget = self.widget;
	if (widget && widget->redraw_handler) {
		// Flip the coordinate system so Y points down (top-left origin)
		// instead of up (bottom-left origin) to match Cosmoe/BeOS expectations
		NSGraphicsContext* nsContext = [NSGraphicsContext currentContext];
		CGContextRef cgContext = (CGContextRef)[nsContext CGContext];
		
		CGContextSaveGState(cgContext);
		CGContextTranslateCTM(cgContext, 0, self.bounds.size.height);
		CGContextScaleCTM(cgContext, 1.0, -1.0);
		
		widget->redraw_handler(widget, widget->user_data);
		
		CGContextRestoreGState(cgContext);
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
	
	self.widget->window->key_handler(self.widget->window, NULL, time, linuxKeyCode, unicode, 1, self.widget->window->user_data);
}

- (void)keyUp:(NSEvent*)event {
	if (!self.widget || !self.widget->window || !self.widget->window->key_handler)
		return;
	
	NSString* chars = [event characters];
	uint32_t macKeyCode = [event keyCode];
	uint32_t linuxKeyCode = translate_macos_keycode(macKeyCode);
	uint32_t unicode = [chars length] > 0 ? [chars characterAtIndex:0] : 0;
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	
	self.widget->window->key_handler(self.widget->window, NULL, time, linuxKeyCode, unicode, 0, self.widget->window->user_data);
}

- (void)flagsChanged:(NSEvent*)event {
	if (!self.widget || !self.widget->window || !self.widget->window->key_handler)
		return;

	uint32_t macKeyCode = [event keyCode];
	uint32_t linuxKeyCode = translate_macos_keycode(macKeyCode);
	if (linuxKeyCode == 0)
		return;

	NSEventModifierFlags flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
	NSEventModifierFlags relevantFlag = 0;

	switch (macKeyCode) {
		case 56: // Left Shift
		case 60: // Right Shift
			relevantFlag = NSEventModifierFlagShift;
			break;
		case 59: // Left Control
		case 62: // Right Control
			relevantFlag = NSEventModifierFlagControl;
			break;
		case 58: // Left Option
		case 61: // Right Option
			relevantFlag = NSEventModifierFlagOption;
			break;
		case 55: // Left Command
		case 54: // Right Command
			relevantFlag = NSEventModifierFlagCommand;
			break;
		case 57: // Caps Lock
			relevantFlag = NSEventModifierFlagCapsLock;
			break;
		default:
			break;
	}

	if (relevantFlag == 0)
		return;

	bool wasDown = (self.lastModifierFlags & relevantFlag) != 0;
	bool isDown = (flags & relevantFlag) != 0;
	if (wasDown == isDown)
		return;

	self.lastModifierFlags = flags;

	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	self.widget->window->key_handler(self.widget->window, NULL, time,
		linuxKeyCode, 0,
		isDown ? 1 : 0,
		self.widget->window->user_data);
}

- (void)mouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x110; // BTN_LEFT

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 1, self.widget->user_data);
}

- (void)mouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x110; // BTN_LEFT

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 0, self.widget->user_data);
}

- (void)rightMouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x111; // BTN_RIGHT

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 1, self.widget->user_data);
}

- (void)rightMouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x111; // BTN_RIGHT

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 0, self.widget->user_data);
}

- (void)otherMouseDown:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x112; // BTN_MIDDLE

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 1, self.widget->user_data);
}

- (void)otherMouseUp:(NSEvent*)event {
	if (!self.widget || !self.widget->button_handler)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	uint32_t button = 0x112; // BTN_MIDDLE

	struct input inputData = { .sx = x, .sy = y };
	self.widget->button_handler(self.widget, &inputData, time, button, 0, self.widget->user_data);
}

- (void)mouseMoved:(NSEvent*)event {
	struct widget* widget = self.widget;
	if (!widget || !widget->motion_handler || !widget->user_data)
		return;

	float x, y;
	[self _eventToBackendCoords:event x:&x y:&y];
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);

	struct input inputData = { .sx = x, .sy = y };
	int cursor = widget->motion_handler(widget, &inputData, time, x, y, widget->user_data);
	if (cursor != widget->cursor) {
		widget->cursor = cursor;
		[NSCursor pop];
		if (cursor >= CUSTOM_CURSOR_BASE) {
			int index = cursor - CUSTOM_CURSOR_BASE;
			if (index >= 0 && index < MAX_CUSTOM_CURSORS
				&& s_custom_cursors[index] != nil) {
				[s_custom_cursors[index] push];
				return;
			}
		}
/*
	B_CURSOR_ID_SYSTEM_DEFAULT					= 1,

	B_CURSOR_ID_CONTEXT_MENU					= 3,
	B_CURSOR_ID_COPY							= 4,
	B_CURSOR_ID_CREATE_LINK						= 29,
							= 5,
	B_CURSOR_ID_FOLLOW_LINK						= 6,
	B_CURSOR_ID_GRAB							= 7,
	B_CURSOR_ID_GRABBING						= 8,
	B_CURSOR_ID_HELP							= 9,
	B_CURSOR_ID_I_BEAM							= 2,
	B_CURSOR_ID_I_BEAM_HORIZONTAL				= 10,
	B_CURSOR_ID_MOVE							= 11,
	B_CURSOR_ID_NO_CURSOR						= 12,
	B_CURSOR_ID_NOT_ALLOWED						= 13,
	B_CURSOR_ID_PROGRESS						= 14,
	B_CURSOR_ID_RESIZE_NORTH					= 15,
	B_CURSOR_ID_RESIZE_EAST						= 16,
	B_CURSOR_ID_RESIZE_SOUTH					= 17,
	B_CURSOR_ID_RESIZE_WEST						= 18,
	B_CURSOR_ID_RESIZE_NORTH_EAST				= 19,
	B_CURSOR_ID_RESIZE_NORTH_WEST				= 20,
	B_CURSOR_ID_RESIZE_SOUTH_EAST				= 21,
	B_CURSOR_ID_RESIZE_SOUTH_WEST				= 22,
	B_CURSOR_ID_RESIZE_NORTH_SOUTH				= 23,
	B_CURSOR_ID_RESIZE_EAST_WEST				= 24,
	B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST	= 25,
	B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST	= 26,
	B_CURSOR_ID_ZOOM_IN							= 27,
	B_CURSOR_ID_ZOOM_OUT						= 28
	*/
		switch (cursor) {
			case B_CURSOR_ID_SYSTEM_DEFAULT:
				[[NSCursor arrowCursor] push];
				break;
			case B_CURSOR_ID_I_BEAM:
				[[NSCursor IBeamCursor] push];
				break;
			case B_CURSOR_ID_I_BEAM_HORIZONTAL:
				[[NSCursor IBeamCursorForVerticalLayout] push];
				break;
			case B_CURSOR_ID_CROSS_HAIR:
				[[NSCursor crosshairCursor] push];
				break;
			case B_CURSOR_ID_FOLLOW_LINK:
				[[NSCursor pointingHandCursor] push];
				break;
			case B_CURSOR_ID_GRABBING:
			case B_CURSOR_ID_MOVE:
				[[NSCursor closedHandCursor] push];
				break;
			case B_CURSOR_ID_GRAB:
				[[NSCursor openHandCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_EAST_WEST:
				[[NSCursor resizeLeftRightCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_NORTH_SOUTH:
				[[NSCursor resizeUpDownCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_EAST:
				[[NSCursor resizeRightCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_WEST:
				[[NSCursor resizeLeftCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_NORTH:
				[[NSCursor resizeUpCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_SOUTH:
				[[NSCursor resizeDownCursor] push];
				break;
			case B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST:
				[diagonal_down_cursor() push];
				break;
			case B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST:
				[diagonal_up_cursor() push];
				break;
			case B_CURSOR_ID_NOT_ALLOWED:
				[[NSCursor operationNotAllowedCursor] push];
				break;
			case B_CURSOR_ID_NO_CURSOR:
				[invisible_cursor() push];
				break;
			case B_CURSOR_ID_PROGRESS:
				[progress_cursor() push];
				break;
			case B_CURSOR_ID_CONTEXT_MENU:
				[[NSCursor contextualMenuCursor] push];
				break;
			case B_CURSOR_ID_COPY:
				[[NSCursor dragCopyCursor] push];
				break;
			default:
				[[NSCursor arrowCursor] push];
				break;
		}
	}
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

	void* callbackData = self.widget->user_data;
	if (self.widget->window && self.widget->window->user_data)
		callbackData = self.widget->window->user_data;
	if (callbackData == NULL)
		return;
	
	uint32_t time = (uint32_t)([event timestamp] * 1000.0);
	NSPoint viewPoint = [self convertPoint:[event locationInWindow] fromView:nil];
	float x = (float)viewPoint.x;
	float y = (float)(self.bounds.size.height - viewPoint.y);
	struct input inputData = {x, y};
	double deltaY = [event scrollingDeltaY];
	double deltaX = [event scrollingDeltaX];
	int32_t fixedDeltaY = (int32_t)(deltaY * 256.0);
	int32_t fixedDeltaX = (int32_t)(deltaX * 256.0);
	if (fixedDeltaY == 0 && deltaY != 0.0)
		fixedDeltaY = deltaY > 0.0 ? 1 : -1;
	if (fixedDeltaX == 0 && deltaX != 0.0)
		fixedDeltaX = deltaX > 0.0 ? 1 : -1;
	printf("cocoa_scrollWheel: widget=%p data=%p x=%.1f y=%.1f precise=%d rawY=%.5f rawX=%.5f fixedY=%d fixedX=%d\n",
		self.widget, callbackData, x, y,
		[event hasPreciseScrollingDeltas] ? 1 : 0,
		deltaY, deltaX, fixedDeltaY, fixedDeltaX);
	
	// Send vertical scroll
	if (fixedDeltaY != 0) {
		self.widget->axis_handler(self.widget, &inputData, time, 0, fixedDeltaY, callbackData);
	}
	
	// Send horizontal scroll
	if (fixedDeltaX != 0) {
		self.widget->axis_handler(self.widget, &inputData, time, 1, fixedDeltaX, callbackData);
	}
}

@end

static NSWindow*
create_native_window(struct window* window, bool popup)
{
	int cocoaWidth = window->width + 1;
	int cocoaHeight = window->height + 1;
	if (cocoaWidth < 1)
		cocoaWidth = 1;
	if (cocoaHeight < 1)
		cocoaHeight = 1;

	NSRect contentRect = NSMakeRect(100, 100, cocoaWidth, cocoaHeight);
	NSWindow* nswindow = nil;
	if (popup) {
		NSWindowStyleMask styleMask = NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel;
		CosmoePopupWindow* panel = [[CosmoePopupWindow alloc] initWithContentRect:contentRect
									  styleMask:styleMask
									    backing:NSBackingStoreBuffered
									      defer:NO];
		[panel setFloatingPanel:YES];
		[panel setBecomesKeyOnlyIfNeeded:YES];
		[panel setLevel:NSPopUpMenuWindowLevel];
		[panel setCollectionBehavior:NSWindowCollectionBehaviorTransient | NSWindowCollectionBehaviorIgnoresCycle];
		nswindow = panel;
	} else {
		NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
					      NSWindowStyleMaskClosable |
					      NSWindowStyleMaskMiniaturizable |
					      NSWindowStyleMaskResizable;

		nswindow = [[CosmoeWindow alloc] initWithContentRect:contentRect
						      styleMask:styleMask
							backing:NSBackingStoreBuffered
							  defer:NO];
	}

	window->nswindow = nswindow;
	[nswindow setReleasedWhenClosed:NO];
	[nswindow setAcceptsMouseMovedEvents:YES];

	CosmoeWindowDelegate* delegate = [[CosmoeWindowDelegate alloc] init];
	delegate.window = window;
	[nswindow setDelegate:delegate];

	CosmoeView* contentView = [[CosmoeView alloc] initWithFrame:contentRect];
	[contentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[nswindow setContentView:contentView];

	printf("Window created: %p\n", nswindow);
	return nswindow;
}

@implementation CosmoeWindowDelegate

- (void)windowDidMove:(NSNotification*)notification {
	if (!self.window || !self.window->move_handler)
		return;
	
	NSWindow* nswindow = [notification object];
	NSRect frame = [nswindow frame];
	NSRect contentRect = [nswindow contentRectForFrameRect:frame];
	
	// Cocoa uses bottom-left origin, Be-style coordinates are top-left origin.
	NSScreen* screen = [nswindow screen] ?: [NSScreen mainScreen];
	NSRect screenFrame = [screen frame];
	CGFloat screenTop = NSMaxY(screenFrame);
	
	self.window->x = (int32_t)contentRect.origin.x;
	self.window->y = (int32_t)(screenTop - NSMaxY(contentRect));
	
	self.window->move_handler(self.window, self.window->x, self.window->y, self.window->move_user_data);
}

- (void)windowDidResize:(NSNotification*)notification {
	if (!self.window)
		return;
	
	// Don't handle resizes during window initialization to avoid calling handlers before they're set up
	if (self.window->initializing)
		return;
	
	NSWindow* nswindow = [notification object];
	NSRect frame = [nswindow contentRectForFrameRect:[nswindow frame]];
	
	int32_t width = (int32_t)frame.size.width;
	int32_t height = (int32_t)frame.size.height;
	
	self.window->width = width;
	self.window->height = height;

	if (self.window->widget) {
		self.window->widget->allocation.width = width;
		self.window->widget->allocation.height = height;
		if (self.window->widget->nsview) {
			NSView* view = (NSView*)self.window->widget->nsview;
			[view setNeedsDisplay:YES];

			// During live resize, draw immediately to avoid showing the default
			// NSWindow background color between resize and redraw.
			if ([nswindow inLiveResize])
				[view displayIfNeeded];
		}
	}
	
	// Call the frame/window resize handler (X11 compatibility)
	if (self.window->resize_handler)
		self.window->resize_handler(NULL, width - 1, height - 1, self.window->user_data);

	if (self.window->frame && self.window->frame->resize_handler)
		self.window->frame->resize_handler(NULL, width - 1, height - 1, self.window->user_data);
	
	// Also call the widget's resize handler if it exists
	if (self.window->widget && self.window->widget->resize_handler) {
		self.window->widget->resize_handler(self.window->widget, width, height, self.window->widget->user_data);
	}
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
	if (self.window && self.window->native_menu != NULL)
		[NSApp setMainMenu:(NSMenu*)self.window->native_menu];

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
// Display management
struct display* display_create(int* argc, char** argv)
{
	struct display* display = (struct display*)calloc(1, sizeof(struct display));
	if (!display)
		return NULL;
	
	// Initialize NSApplication - ALL Cocoa/AppKit operations must be on main thread
	@autoreleasepool {
		// Marshal NSApp initialization to main thread
		if ([NSThread isMainThread]) {
			// Already on main thread
			[NSApplication sharedApplication];
			display->nsapp = [NSApp retain];
			[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
			[NSApp activateIgnoringOtherApps:YES];
			[NSApp finishLaunching];
		} else {
			// On background thread - marshal to main thread
			__block void* nsapp_ptr = NULL;
			dispatch_sync(dispatch_get_main_queue(), ^{
				[NSApplication sharedApplication];
				nsapp_ptr = [NSApp retain];
				[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
				[NSApp activateIgnoringOtherApps:YES];
				[NSApp finishLaunching];
			});
			display->nsapp = nsapp_ptr;
		}
		
		// Note: We cannot call [NSApp run] here because:
		// 1. It must be on the main thread
		// 2. It blocks forever
		// Instead, we'll process events manually in display_run()
		printf("NSApp initialized, will process events manually in display_run\n");
		
		// Get screen dimensions
		NSScreen* screen = [NSScreen mainScreen];
		NSRect screenRect = [screen frame];
		display->screen_width = (int)screenRect.size.width;
		display->screen_height = (int)screenRect.size.height;
	}
	
	display->running = false;
	display->window_list = NULL;
	display->backend_port = -1;
	display->app_port = -1;
	
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
	
	printf("display_run: Starting on background thread (main=%d)\n", [NSThread isMainThread]);
	
	// Cosmoe runs on a background thread
	// NSApp event loop runs on main thread (via NSApplicationMain in macmain.mm)
	// This function just keeps the Cosmoe thread alive while windows exist
	while (display->running) {
		@autoreleasepool {
			if (display->backend_port >= 0)
				cocoa_process_backend_messages(display->backend_port, display->app_port);
			// Sleep between polls - main thread handles NSApp events
			// UI operations are marshaled to main thread via dispatch_async
			usleep(10000); // 10ms
		}
	}
	
	printf("display_run: Exiting\n");
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

void display_trigger_redraw(struct display* display, struct window* window,
	struct widget* widget, const struct rectangle* damage)
{
	(void)display;

	if (!window && widget)
		window = widget->window;
	if (!widget && window)
		widget = window->widget;

	if (!widget || !widget->nsview)
		return;

	// The caller's damage rectangle can be stack allocated. Copy it before the
	// asynchronous main-thread dispatch below captures it.
	const bool hasDamage = damage != NULL;
	const struct rectangle damageCopy = hasDamage ? *damage : (struct rectangle){};
	
	// Marshal to main thread
	dispatch_async(dispatch_get_main_queue(), ^{
		@autoreleasepool {
			NSView* view = (NSView*)widget->nsview;
			if (hasDamage) {
				// Cosmoe damage coordinates have a top-left origin. NSView uses a
				// bottom-left origin, so mirror the rectangle vertically before
				// invalidating it. drawRect: flips its CGContext back to Cosmoe's
				// coordinate system before the backing surface is painted.
				NSRect bounds = [view bounds];
				NSRect cocoaDamage = NSMakeRect(
					NSMinX(bounds) + damageCopy.x,
					NSMaxY(bounds) - damageCopy.y - damageCopy.height,
					damageCopy.width, damageCopy.height);
				[view setNeedsDisplayInRect:cocoaDamage];
			} else {
				[view setNeedsDisplay:YES];
			}
		}
	});
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

void display_set_port(struct display* display, int32_t sender_port_id, int32_t receiver_port_id)
{
	if (!display)
		return;
	display->backend_port = sender_port_id;
	display->app_port = receiver_port_id;
	printf("Cocoa: display_set_port: backend reads from port %d, writes to port %d\n",
		sender_port_id, receiver_port_id);
}

struct window* display_find_window_by_token(struct display* display, int32_t token)
{
	if (!display || token < 0)
		return NULL;
	struct window* w = display->window_list;
	while (w) {
		if (w->token == token)
			return w;
		w = w->next;
	}
	return NULL;
}

void window_set_token(struct window* window, int32_t token)
{
	if (window)
		window->token = token;
}

// Cursor conversion (Be cursor ID to Cocoa cursor)
int32_t display_convert_cursor(int32_t be_cursor_id)
{
	// TODO: Map BeOS cursor IDs to NSCursor types
	// For now, return the ID as-is
	return be_cursor_id;
}

int32_t window_create_custom_cursor(const uint8_t* bits, size_t bitsLength,
	int32_t width, int32_t height, int32_t bytesPerRow,
	int32_t colorSpace, int32_t hotX, int32_t hotY)
{
	(void)colorSpace;

	if (bits == NULL || width <= 0 || height <= 0 || bytesPerRow <= 0)
		return -1;
	if (hotX < 0 || hotY < 0 || hotX >= width || hotY >= height)
		return -1;

	size_t minSize = (size_t)bytesPerRow * (size_t)height;
	if (bitsLength < minSize)
		return -1;

	__block int freeIndex = -1;
	__block NSCursor* cursor = nil;

	dispatch_sync(dispatch_get_main_queue(), ^{
		for (int i = 0; i < MAX_CUSTOM_CURSORS; i++) {
			if (s_custom_cursors[i] == nil) {
				freeIndex = i;
				break;
			}
		}
		if (freeIndex < 0)
			return;

		const size_t dataSize = (size_t)bytesPerRow * (size_t)height;
		unsigned char* copiedBits = (unsigned char*)malloc(dataSize);
		if (copiedBits == NULL)
			return;
		memcpy(copiedBits, bits, dataSize);

		NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:NULL
			pixelsWide:width
			pixelsHigh:height
			bitsPerSample:8
			samplesPerPixel:4
			hasAlpha:YES
			isPlanar:NO
			colorSpaceName:NSDeviceRGBColorSpace
			bitmapFormat:(NSBitmapFormatAlphaFirst | NSBitmapFormatThirtyTwoBitLittleEndian)
			bytesPerRow:bytesPerRow
			bitsPerPixel:32];
		if (rep == nil) {
			free(copiedBits);
			return;
		}

		memcpy([rep bitmapData], copiedBits, dataSize);
		free(copiedBits);

		NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
		[image addRepresentation:rep];

		cursor = [[NSCursor alloc] initWithImage:image
			hotSpot:NSMakePoint((CGFloat)hotX, (CGFloat)hotY)];
		[rep release];
		[image release];

		if (cursor != nil)
			s_custom_cursors[freeIndex] = cursor;
	});

	if (cursor == nil || freeIndex < 0)
		return -1;

	return CUSTOM_CURSOR_BASE + freeIndex;
}

int window_delete_custom_cursor(int32_t cursorID)
{
	if (cursorID < CUSTOM_CURSOR_BASE)
		return -1;

	int index = cursorID - CUSTOM_CURSOR_BASE;
	if (index < 0 || index >= MAX_CUSTOM_CURSORS)
		return -1;

	dispatch_sync(dispatch_get_main_queue(), ^{
		if (s_custom_cursors[index] != nil) {
			[s_custom_cursors[index] release];
			s_custom_cursors[index] = nil;
		}
	});

	return 0;
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

int32_t display_get_app_list(struct display* display, int32_t* team_ids, int32_t max_count)
{
	if (!display)
		return 0;
	
	@autoreleasepool {
		int32_t count = 0;
		pid_t current_pid = [[NSRunningApplication currentApplication] processIdentifier];
		NSMutableSet<NSNumber*>* seenPids = [NSMutableSet set];
		
		// First, add Cosmoe windows (positive team IDs)
		struct window* win = display->window_list;
		while (win && count < max_count) {
			if (!win->is_popup && !win->is_offscreen) {
				if (team_ids)
					team_ids[count] = win->token;  // Use window token as app identifier
				count++;
			}
			win = win->next;
		}
		
		// Then enumerate native graphical applications via visible top-level windows.
		NSArray* windows = copy_visible_window_info();
		for (NSDictionary* windowInfo in windows) {
			if (count >= max_count)
				break;

			if (!window_info_is_graphical(windowInfo, current_pid))
				continue;

			NSNumber* ownerPid = windowInfo[(NSString*)kCGWindowOwnerPID];
			if ([seenPids containsObject:ownerPid])
				continue;
			[seenPids addObject:ownerPid];
			
			if (team_ids)
				team_ids[count] = -[ownerPid intValue];
			
			count++;
		}
		
		return count;
	}
}

/* Get app info by team ID */
status_t display_get_app_info(struct display* display, int32_t team_id, struct cosmoe_backend_app_info* info)
{
	if (!display || !info)
		return B_BAD_VALUE;
	
	memset(info, 0, sizeof(*info));
	info->team_id = team_id;
	
	@autoreleasepool {
		// Check if it's a Cosmoe app (positive team ID)
		if (team_id > 0) {
			struct window* win = display->window_list;
			while (win) {
				if (win->token == team_id && !win->is_popup && !win->is_offscreen) {
					strncpy(info->signature, "application/cosmoe-window", sizeof(info->signature) - 1);
					if (win->title)
						strncpy(info->name, win->title, sizeof(info->name) - 1);
					else
						strncpy(info->name, "Cosmoe App", sizeof(info->name) - 1);
					snprintf(info->identifier, sizeof(info->identifier), "cosmoe-team-%d", team_id);
					info->flags = 0;
					return B_OK;
				}
				win = win->next;
			}
			return B_ENTRY_NOT_FOUND;
		}
		
		// External app (negative process ID)
		pid_t current_pid = [[NSRunningApplication currentApplication] processIdentifier];
		NSArray* windows = copy_visible_window_info();
		pid_t pid = (pid_t)(-team_id);
		NSString* windowOwnerName = common_name_for_pid_from_windows(windows, pid);
		NSRunningApplication* app = [NSRunningApplication
			runningApplicationWithProcessIdentifier:pid];
		
		if (pid <= 0 || windowOwnerName == nil || !running_app_is_graphical(app, current_pid))
			return B_ENTRY_NOT_FOUND;

		NSString* bundleId = app.bundleIdentifier;
		NSString* appName = windowOwnerName;
		if (appName == nil || [appName length] == 0)
			appName = running_app_common_name(app);

		if (bundleId) {
			const char* bundleIdCStr = [bundleId UTF8String];
			strncpy(info->signature, bundleIdCStr, sizeof(info->signature) - 1);
			strncpy(info->identifier, bundleIdCStr, sizeof(info->identifier) - 1);
		} else {
			snprintf(info->signature, sizeof(info->signature),
				"application/x-vnd.cosmoe-hostpid-%d", (int)pid);
			snprintf(info->identifier, sizeof(info->identifier), "pid:%d",
				(int)pid);
		}

		if (appName) {
			const char* appNameCStr = [appName UTF8String];
			strncpy(info->name, appNameCStr, sizeof(info->name) - 1);
		} else {
			snprintf(info->name, sizeof(info->name), "pid:%d", (int)pid);
		}

		info->flags = 0;
		return B_OK;
		
	}
}

/* Get list of all visible windows */
int32_t display_get_window_list(struct display* display, int32_t* window_ids, int32_t max_count)
{
	if (!display)
		return 0;
	
	int32_t count = 0;
	
	// Add Cosmoe windows
	struct window* win = display->window_list;
	while (win && count < max_count) {
		if (!win->is_popup && !win->is_offscreen) {
			if (window_ids)
				window_ids[count] = win->token;
			count++;
		}
		win = win->next;
	}
	
	// Add native macOS windows using the CoreGraphics window list.
	@autoreleasepool {
		pid_t current_pid = [[NSRunningApplication currentApplication] processIdentifier];
		NSArray* windows = copy_visible_window_info();
		if (windows == nil)
			return count;

		for (NSDictionary* windowInfo in windows) {
			if (count >= max_count)
				break;

			NSNumber* windowNumber = windowInfo[(NSString*)kCGWindowNumber];
			if (windowNumber == nil)
				continue;
			if (!window_info_is_graphical(windowInfo, current_pid))
				continue;

			if (window_ids)
				window_ids[count] = -[windowNumber intValue];
			count++;
		}
	}
	
	return count;
}

/* Get window info by window ID */
status_t display_get_window_info(struct display* display, int32_t window_id, struct cosmoe_backend_window_info* info)
{
	if (!display || !info)
		return B_BAD_VALUE;
	
	memset(info, 0, sizeof(*info));
	info->window_id = window_id;
	
	// Check if it's a Cosmoe window
	struct window* win = display->window_list;
	while (win) {
		if (win->token == window_id && !win->is_popup && !win->is_offscreen) {
			info->team_id = window_id;
			info->workspaces = 1;
			info->feel = 0;
			info->show_hide_level = 0;
			info->is_mini = 0;
			
			if (win->title)
				strncpy(info->name, win->title, sizeof(info->name) - 1);
			
			snprintf(info->identifier, sizeof(info->identifier), "cosmoe-window-%d", window_id);
			return B_OK;
		}
		win = win->next;
	}
	
	if (window_id >= 0)
		return B_ENTRY_NOT_FOUND;

	@autoreleasepool {
		CGWindowID target = (CGWindowID)(-window_id);
		pid_t current_pid = [[NSRunningApplication currentApplication] processIdentifier];
		NSArray* windows = copy_visible_window_info();
		if (windows == nil)
			return B_ENTRY_NOT_FOUND;

		for (NSDictionary* windowInfo in windows) {
			NSNumber* windowNumber = windowInfo[(NSString*)kCGWindowNumber];
			NSNumber* ownerPid = windowInfo[(NSString*)kCGWindowOwnerPID];
			NSString* name = windowInfo[(NSString*)kCGWindowName];
			NSString* ownerName = windowInfo[(NSString*)kCGWindowOwnerName];

			if (windowNumber == nil || ownerPid == nil)
				continue;
			if ((CGWindowID)[windowNumber unsignedIntValue] != target)
				continue;
			if (!window_info_is_graphical(windowInfo, current_pid))
				continue;

			info->team_id = -[ownerPid intValue];
			info->workspaces = 1;
			info->feel = 0;
			info->show_hide_level = 0;
			info->is_mini = 0;

			NSString* title = name;
			if (title == nil || [title length] == 0)
				title = ownerName;
			if (title != nil)
				strncpy(info->name, [title UTF8String], sizeof(info->name) - 1);

			if (ownerName != nil && [ownerName length] > 0)
				snprintf(info->identifier, sizeof(info->identifier), "%s:%u",
					[ownerName UTF8String], (unsigned int)target);
			else
				snprintf(info->identifier, sizeof(info->identifier), "window:%u",
					(unsigned int)target);
			return B_OK;
		}
	}

	return B_ENTRY_NOT_FOUND;
}

/* Activate (focus) a window */
status_t display_activate_window(struct display* display, int32_t window_id)
{
	if (!display)
		return B_BAD_VALUE;
	
	// Check if it's a Cosmoe window
	struct window* win = display->window_list;
	while (win) {
		if (win->token == window_id && !win->is_popup && !win->is_offscreen) {
			window_activate(win, true);
			return B_OK;
		}
		win = win->next;
	}
	
	// Cannot activate non-Cosmoe windows yet
	return B_ENTRY_NOT_FOUND;
}

/* Minimize or restore a window */
status_t display_minimize_window(struct display* display, int32_t window_id, bool minimize)
{
	if (!display)
		return B_BAD_VALUE;
	
	// Check if it's a Cosmoe window
	struct window* win = display->window_list;
	while (win) {
		if (win->token == window_id && !win->is_popup && !win->is_offscreen) {
			window_minimize(win, minimize);
			return B_OK;
		}
		win = win->next;
	}
	
	// Cannot minimize non-Cosmoe windows yet
	return B_ENTRY_NOT_FOUND;
}

/* Close a window */
status_t display_close_window(struct display* display, int32_t window_id)
{
	if (!display)
		return B_BAD_VALUE;
	
	// Check if it's a Cosmoe window
	struct window* win = display->window_list;
	while (win) {
		if (win->token == window_id && !win->is_popup && !win->is_offscreen) {
			// Send close event through the handler if it exists
			if (win->close_handler) {
				win->close_handler(win->user_data);
			}
			return B_OK;
		}
		win = win->next;
	}
	
	// Cannot close non-Cosmoe windows yet
	return B_ENTRY_NOT_FOUND;
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
	window->initializing = true;
	window->width = 640;
	window->height = 480;
	
	// Check if we're already on the main thread
	if ([NSThread isMainThread]) {
		// Already on main thread, call directly
		@autoreleasepool {
			create_native_window(window, false);
		}
	} else {
		// Marshal window creation to main thread
		dispatch_sync(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				create_native_window(window, false);
			}
		});
	}
	
	// Add to window list
	window->next = display->window_list;
	display->window_list = window;
	
	return window;
}

struct window* window_popup_create(struct display* display, struct window* parent, int32_t x, int32_t y)
{
	if (!display)
		return NULL;

	struct window* window = (struct window*)calloc(1, sizeof(struct window));
	if (!window)
		return NULL;
	
	window->display = display;
	window->is_offscreen = false;
	window->is_popup = true;
	window->initializing = true;
	window->width = 640;
	window->height = 480;
	window->x = x;
	window->y = y;
	
	if ([NSThread isMainThread]) {
		@autoreleasepool {
			NSWindow* nswindow = create_native_window(window, true);
			if (nswindow && parent && parent->nswindow)
				[(NSWindow*)parent->nswindow addChildWindow:nswindow ordered:NSWindowAbove];
			window_set_position(window, x, y);
		}
	} else {
		dispatch_sync(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				NSWindow* nswindow = create_native_window(window, true);
				if (nswindow && parent && parent->nswindow)
					[(NSWindow*)parent->nswindow addChildWindow:nswindow ordered:NSWindowAbove];
				window_set_position(window, x, y);
			}
		});
	}

	window->next = display->window_list;
	display->window_list = window;
	
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
			NSRect contentRect = [nswindow contentRectForFrameRect:frame];
			
			// Cocoa uses bottom-left origin, BeOS uses top-left origin
			NSScreen* screen = [nswindow screen] ?: [NSScreen mainScreen];
			NSRect screenFrame = [screen frame];
			CGFloat screenTop = NSMaxY(screenFrame);
			
			// Return topview/content position (excluding window decorations)
			if (x) *x = (int32_t)contentRect.origin.x;
			if (y) *y = (int32_t)(screenTop - NSMaxY(contentRect));
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
			
			void (^setFrameBlock)(void) = ^{
				NSRect frame = [nswindow frame];
				NSRect contentRect = [nswindow contentRectForFrameRect:frame];

				// Convert top-left origin (BeOS) to bottom-left origin (Cocoa)
				NSScreen* screen = [nswindow screen] ?: [NSScreen mainScreen];
				NSRect screenFrame = [screen frame];
				CGFloat screenTop = NSMaxY(screenFrame);

				frame.origin.x = x;
				frame.origin.y = screenTop - y - contentRect.size.height;
				[nswindow setFrame:frame display:YES];
			};
			
			// Must run on main thread to avoid hanging
			if ([NSThread isMainThread]) {
				setFrameBlock();
			} else {
				[nswindow retain];
				dispatch_async(dispatch_get_main_queue(), setFrameBlock);
				dispatch_async(dispatch_get_main_queue(), ^{
					@autoreleasepool {
						[nswindow release];
					}
				});
			}
		}
	}
}

void window_get_decorator_size(struct window* window, int32_t* borderWidth, int32_t* tabHeight)
{
	// macOS window decorations are handled by the OS
	// Calculate actual title bar height from a sample window
	if (borderWidth)
		*borderWidth = 0; // No visible border on macOS (or minimal, included in content insets)
	
	if (tabHeight) {
		// Get actual title bar height from the window if available
		if (window && window->nswindow) {
			@autoreleasepool {
				NSWindow* nswindow = (NSWindow*)window->nswindow;
				NSRect contentRect = [nswindow contentRectForFrameRect:[nswindow frame]];
				NSRect frameRect = [nswindow frame];
				*tabHeight = (int32_t)(frameRect.size.height - contentRect.size.height);
			}
		} else {
			// Fallback: typical macOS title bar height is 28 pixels
			*tabHeight = 28;
		}
	}
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

void window_destroy(struct window* window)
{
	if (!window)
		return;

	if (window->native_menu_targets) {
		[(NSMutableArray*)window->native_menu_targets release];
		window->native_menu_targets = NULL;
	}
	if (window->native_menu) {
		[(NSMenu*)window->native_menu release];
		window->native_menu = NULL;
	}
	
	struct windowframe* frame = window->frame;
	struct widget* widget = window->widget;

	if (widget) {
		widget->user_data = NULL;
		widget->redraw_handler = NULL;
		widget->resize_handler = NULL;
		widget->button_handler = NULL;
		widget->motion_handler = NULL;
		widget->axis_handler = NULL;
		widget->idle_handler = NULL;
		widget->window = NULL;
	}
	
	// Capture window and frame for async cleanup
	NSWindow* nswindow = (NSWindow*)window->nswindow;
	
	// Marshal UI destruction to main thread
	// Always use dispatch_async to avoid use-after-free when called from delegate callbacks
	dispatch_async(dispatch_get_main_queue(), ^{
		@autoreleasepool {
			if (nswindow) {
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
	});
	
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
	if (!window)
		return;

	const char* safeTitle = title ? title : "";
	char* copiedTitle = strdup(safeTitle);
	if (!copiedTitle)
		return;
	
	if (window->title)
		free(window->title);
	window->title = strdup(safeTitle);

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow) {
		free(copiedTitle);
		return;
	}
	
	void (^setTitleBlock)(void) = ^{
		@autoreleasepool {
			NSString* string = [NSString stringWithUTF8String:copiedTitle];
			if (!string)
				string = [NSString stringWithCString:copiedTitle encoding:NSISOLatin1StringEncoding];
			if (!string)
				string = @"";
			[nswindow setTitle:string];
			free(copiedTitle);
		}
	};

	if ([NSThread isMainThread]) {
		setTitleBlock();
	} else {
		[nswindow retain];
		dispatch_async(dispatch_get_main_queue(), setTitleBlock);
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				[nswindow release];
			}
		});
	}
}

void window_set_app_id(struct window* window, const char* app_id)
{
	// App ID is less relevant on macOS - could map to bundle identifier
	(void)window;
	(void)app_id;
}

void window_set_parent(struct window* window, struct window* parent)
{
	if (!window || !parent)
		return;
	
	@autoreleasepool {
		NSWindow* nswindow = (__bridge NSWindow*)window->nswindow;
		NSWindow* parentWindow = (__bridge NSWindow*)parent->nswindow;
		
		if (nswindow && parentWindow) {
			// Add the modal dialog as a child window of the parent
			[parentWindow addChildWindow:nswindow ordered:NSWindowAbove];
			
			// Set the window level to float above the parent
			[nswindow setLevel:NSFloatingWindowLevel];
			
			// Make it modal by preventing interaction with parent
			// Note: For true modal behavior, the application should use
			// [NSApplication runModalForWindow:] or sheet APIs
			
			NSLog(@"Cocoa: Set window %@ as modal child of window %@", nswindow, parentWindow);
		}
	}
}

void window_show(struct window* window)
{
	if (!window)
		return;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow)
		return;

	if ([NSThread isMainThread]) {
		@autoreleasepool {
			if (window->is_popup) {
				// Ensure popup uses its final requested position before first paint.
				NSRect frame = [nswindow frame];
				NSRect contentRect = [nswindow contentRectForFrameRect:frame];
				NSScreen* screen = [nswindow screen] ?: [NSScreen mainScreen];
				NSRect screenFrame = [screen frame];
				CGFloat screenTop = NSMaxY(screenFrame);
				frame.origin.x = window->x;
				frame.origin.y = screenTop - window->y - contentRect.size.height;
				[nswindow setFrame:frame display:NO animate:NO];
			}
			if (window->is_popup)
				[nswindow orderFront:nil];
			else
				[nswindow makeKeyAndOrderFront:nil];
			NSView* view = [nswindow contentView];
			if (view)
				[view setNeedsDisplay:YES];
		}
	} else {
		[nswindow retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				if (window->is_popup) {
					// Ensure popup uses its final requested position before first paint.
					NSRect frame = [nswindow frame];
					NSRect contentRect = [nswindow contentRectForFrameRect:frame];
					NSScreen* screen = [nswindow screen] ?: [NSScreen mainScreen];
					NSRect screenFrame = [screen frame];
					CGFloat screenTop = NSMaxY(screenFrame);
					frame.origin.x = window->x;
					frame.origin.y = screenTop - window->y - contentRect.size.height;
					[nswindow setFrame:frame display:NO animate:NO];
				}
				if (window->is_popup)
					[nswindow orderFront:nil];
				else
					[nswindow makeKeyAndOrderFront:nil];
				NSView* view = [nswindow contentView];
				if (view)
					[view setNeedsDisplay:YES];
				[nswindow release];
			}
		});
	}
}

status_t window_set_native_menubar(struct window* window,
	const cosmoe_native_menu_item* items, int32_t count,
	cocoa_window_menu_func_t func, void* user_data)
{
	if (window == NULL || items == NULL || count < 0)
		return B_BAD_VALUE;
	if (window->is_popup)
		return B_UNSUPPORTED;

	__block status_t result = B_OK;
	dispatch_sync(dispatch_get_main_queue(), ^{
		NSMutableArray* targets = [[NSMutableArray alloc] init];
		NSMenu* menubar = cocoa_build_native_menubar(window, items, count,
			targets);
		if (menubar == nil) {
			[targets release];
			result = B_ERROR;
			return;
		}

		if (window->native_menu_targets != NULL)
			[(NSMutableArray*)window->native_menu_targets release];
		if (window->native_menu != NULL)
			[(NSMenu*)window->native_menu release];

		window->native_menu = [menubar retain];
		window->native_menu_targets = targets;
		window->native_menu_func = func;
		window->native_menu_user_data = user_data;

		NSWindow* nswindow = (NSWindow*)window->nswindow;
		if (nswindow != nil && [nswindow isKeyWindow])
			[NSApp setMainMenu:menubar];
	});

	return result;
}

status_t window_clear_native_menubar(struct window* window)
{
	if (window == NULL)
		return B_BAD_VALUE;

	dispatch_sync(dispatch_get_main_queue(), ^{
		if (window->native_menu_targets != NULL) {
			[(NSMutableArray*)window->native_menu_targets release];
			window->native_menu_targets = NULL;
		}
		if (window->native_menu != NULL) {
			NSMenu* nativeMenu = (NSMenu*)window->native_menu;
			if ([NSApp mainMenu] == nativeMenu)
				[NSApp setMainMenu:nil];
			[nativeMenu release];
			window->native_menu = NULL;
		}
		window->native_menu_func = NULL;
		window->native_menu_user_data = NULL;
	});

	return B_OK;
}

void window_hide(struct window* window)
{
	if (!window)
		return;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow)
		return;

	if ([NSThread isMainThread]) {
		@autoreleasepool {
			[nswindow orderOut:nil];
		}
	} else {
		[nswindow retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				[nswindow orderOut:nil];
				[nswindow release];
			}
		});
	}
}

void window_minimize(struct window* window, bool minimize)
{
	if (!window)
		return;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow)
		return;

	if ([NSThread isMainThread]) {
		@autoreleasepool {
			if (minimize)
				[nswindow miniaturize:nil];
			else
				[nswindow deminiaturize:nil];
		}
	} else {
		[nswindow retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				if (minimize)
					[nswindow miniaturize:nil];
				else
					[nswindow deminiaturize:nil];
				[nswindow release];
			}
		});
	}
}

void window_activate(struct window* window, bool active)
{
	if (!window)
		return;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow)
		return;

	if ([NSThread isMainThread]) {
		@autoreleasepool {
			if (active) {
				[NSApp activateIgnoringOtherApps:YES];
				[nswindow makeKeyAndOrderFront:nil];
			} else {
				[nswindow resignKeyWindow];
			}
		}
	} else {
		[nswindow retain];
		dispatch_async(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				if (active) {
					[NSApp activateIgnoringOtherApps:YES];
					[nswindow makeKeyAndOrderFront:nil];
				} else {
					[nswindow resignKeyWindow];
				}
				[nswindow release];
			}
		});
	}
}

bool window_is_front(struct window* window)
{
	if (!window)
		return false;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	if (!nswindow)
		return false;

	if ([NSThread isMainThread]) {
		@autoreleasepool {
			return [NSApp keyWindow] == nswindow;
		}
	}

	__block BOOL isFront = NO;
	dispatch_sync(dispatch_get_main_queue(), ^{
		@autoreleasepool {
			isFront = [NSApp keyWindow] == nswindow;
		}
	});

	return isFront;
}

void window_schedule_resize(struct window* window, int width, int height)
{
	if (!window)
		return;
	
	struct windowframe* frame = window->frame;
	NSWindow* nswindow = (NSWindow*)window->nswindow;
	
	// Clear initializing flag if this is being called - window is now ready
	window->initializing = false;
	
	window->width = width;
	window->height = height;
	
	if (frame) {
		frame->width = width;
		frame->height = height;
	}

	if (window->widget) {
		window->widget->allocation.width = width;
		window->widget->allocation.height = height;
	}
	
	// Check if we're already on the main thread to avoid deadlock
	BOOL onMainThread = [NSThread isMainThread];
	
	if (onMainThread) {
		// Already on main thread, execute directly
		@autoreleasepool {
			if (nswindow) {
				int cocoaWidth = width + 1;
				int cocoaHeight = height + 1;
				if (cocoaWidth < 1)
					cocoaWidth = 1;
				if (cocoaHeight < 1)
					cocoaHeight = 1;
				// IMPORTANT: Set content size, not frame size!
				// The frame includes the title bar (~28px), but BWindow expects
				// width/height to refer to the content area only.
				NSRect oldFrame = [nswindow frame];
				NSRect newFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, cocoaWidth, cocoaHeight)];
				// Preserve the window's position
				newFrame.origin = oldFrame.origin;
				[nswindow setFrame:newFrame display:YES animate:NO];
				NSView* view = [nswindow contentView];
				if (view) {
					[view setNeedsDisplay:YES];
					[view displayIfNeeded];
				}
			}
		}
	} else {
		// Marshal to main thread and block until the resize has been applied.
		// AS_WINDOW_RESIZE expects the backend to have committed the size change
		// before replying.
		if (!nswindow)
			return;
		dispatch_sync(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				if (nswindow) {
					int cocoaWidth = width + 1;
					int cocoaHeight = height + 1;
					if (cocoaWidth < 1)
						cocoaWidth = 1;
					if (cocoaHeight < 1)
						cocoaHeight = 1;
					// IMPORTANT: Set content size, not frame size!
					// The frame includes the title bar (~28px), but BWindow expects
					// width/height to refer to the content area only.
					NSRect oldFrame = [nswindow frame];
					NSRect newFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, cocoaWidth, cocoaHeight)];
					// Preserve the window's position
					newFrame.origin = oldFrame.origin;
					[nswindow setFrame:newFrame display:YES animate:NO];
					NSView* view = [nswindow contentView];
					if (view) {
						[view setNeedsDisplay:YES];
						[view displayIfNeeded];
					}
				}
			}
		});
	}
}

void window_set_min_max_allocation(struct window* window, int min_width, int min_height,
				   int max_width, int max_height)
{
	if (!window || !window->nswindow)
		return;

	NSWindow* nswindow = (NSWindow*)window->nswindow;
	
	// Check if we're already on the main thread
	if ([NSThread isMainThread]) {
		@autoreleasepool {
			// Convert content sizes to frame sizes to account for title bar
			NSRect minFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, min_width, min_height)];
			[nswindow setMinSize:minFrame.size];
			
			if (max_width > 0 && max_height > 0) {
				NSRect maxFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, max_width, max_height)];
				[nswindow setMaxSize:maxFrame.size];
			}
		}
	} else {
		// Marshal to main thread
		[nswindow retain];
		dispatch_sync(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				// Convert content sizes to frame sizes to account for title bar
				NSRect minFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, min_width, min_height)];
				[nswindow setMinSize:minFrame.size];
				
				if (max_width > 0 && max_height > 0) {
					NSRect maxFrame = [nswindow frameRectForContentRect:NSMakeRect(0, 0, max_width, max_height)];
					[nswindow setMaxSize:maxFrame.size];
				}
				[nswindow release];
			}
		});
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

void window_set_screen_handler(struct window* window, cocoa_screen_handler_t handler)
{
	if (window)
		window->screen_handler = handler;
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
	// Create a fresh Cairo surface from the current CGContext
	// The CGContext is only valid during drawRect, so we create a new surface each time
	// IMPORTANT: Caller must destroy the returned surface with cairo_surface_destroy
	if (!window || !window->widget)
		return NULL;
	
	struct widget* widget = window->widget;
	
	// Get the CGContext from the current NSGraphicsContext
	// Note: This should be called during a draw operation when NSGraphicsContext is valid
	@autoreleasepool {
		NSGraphicsContext* nsContext = [NSGraphicsContext currentContext];
		if (!nsContext)
			return NULL;
		
		CGContextRef cgContext = (CGContextRef)[nsContext CGContext];
		if (!cgContext)
			return NULL;
		
		// Create fresh cairo surface from CGContext
		// Use logical dimensions - CGContext is already configured by Cocoa for Retina
		cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(
			cgContext,
			widget->allocation.width,
			widget->allocation.height
		);
		
		// Don't set device scale - CGContext already handles Retina
		
		return surface;
	}
}

void window_get_topview_offset(struct window* window, int32_t* offset_h, int32_t* offset_v)
{
	// macOS windows don't have the same topview concept as Wayland, so we return zero offsets
	if (offset_h) *offset_h = 0;
	if (offset_v) *offset_v = 0;
}

// Window frame management
void windowframe_set_resize_handler(struct window* window,
				    cocoa_windowframe_resize_handler_t handler)
{
	if (!window)
		return;

	window->resize_handler = handler;

	if (window->frame)
		window->frame->resize_handler = handler;
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
	
	// Connect the widget to the NSView (must run on main thread)
	if ([NSThread isMainThread]) {
		@autoreleasepool {
			if (window->nswindow) {
				NSWindow* nswindow = (NSWindow*)window->nswindow;
				CosmoeView* view = (CosmoeView*)[nswindow contentView];
				if ([view isKindOfClass:[CosmoeView class]]) {
					view.widget = widget;
					widget->nsview = view;
					[view setNeedsDisplay:YES];
				}
			}
		}
	} else {
		dispatch_sync(dispatch_get_main_queue(), ^{
			@autoreleasepool {
				if (window->nswindow) {
					NSWindow* nswindow = (NSWindow*)window->nswindow;
					CosmoeView* view = (CosmoeView*)[nswindow contentView];
					if ([view isKindOfClass:[CosmoeView class]]) {
						view.widget = widget;
						widget->nsview = view;
						[view setNeedsDisplay:YES];
					}
				}
			}
		});
	}
	
	window->widget = widget;
	
	return widget;
}

struct widget* window_add_widget(struct window* window, void* data)
{
	if (!window)
		return NULL;

	if (window->widget != NULL) {
		if (data != NULL)
			window->widget->user_data = data;
		return window->widget;
	}

	struct widget* widget = widget_create(window);
	if (widget != NULL && data != NULL)
		widget->user_data = data;

	return widget;
}

void widget_destroy(struct widget* widget)
{
	if (!widget)
		return;

	widget->user_data = NULL;
	widget->redraw_handler = NULL;
	widget->resize_handler = NULL;
	widget->button_handler = NULL;
	widget->motion_handler = NULL;
	widget->axis_handler = NULL;
	widget->idle_handler = NULL;
	widget->window = NULL;

	if (widget->nsview) {
		if ([NSThread isMainThread]) {
			CosmoeView* view = (CosmoeView*)widget->nsview;
			if ([view isKindOfClass:[CosmoeView class]])
				view.widget = NULL;
		} else {
			dispatch_sync(dispatch_get_main_queue(), ^{
				@autoreleasepool {
					CosmoeView* view = (CosmoeView*)widget->nsview;
					if ([view isKindOfClass:[CosmoeView class]])
						view.widget = NULL;
				}
			});
		}
	}
	
	// No need to destroy surface - it's not cached anymore
	free(widget);
}

void widget_set_redraw_handler(struct widget* widget, cocoa_redraw_handler_t handler)
{
	if (widget) {
		widget->redraw_handler = handler;
		if (widget->nsview)
			widget_schedule_redraw(widget);
	}
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

void widget_schedule_redraw(struct widget* widget)
{
	if (!widget || !widget->nsview)
		return;

	NSView* view = (NSView*)widget->nsview;
	[view retain];
	
	// Marshal to main thread
	dispatch_async(dispatch_get_main_queue(), ^{
		@autoreleasepool {
			[view setNeedsDisplay:YES];
			[view release];
		}
	});
}

void widget_schedule_resize(struct widget* widget, int32_t width, int32_t height)
{
	if (!widget)
		return;
	
	widget->allocation.width = width;
	widget->allocation.height = height;
	
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
	
	widget->allocation.x = x;
	widget->allocation.y = y;
	widget->allocation.width = width;
	widget->allocation.height = height;
}

// Input management
void input_get_position(struct input* input, int32_t* x, int32_t* y)
{
	if (!input) {
		if (x) *x = 0;
		if (y) *y = 0;
		return;
	}
	
	if (x) *x = (int32_t)input->sx;
	if (y) *y = (int32_t)input->sy;
}

// Additional widget functions
struct window* widget_get_window(struct widget* widget)
{
	return widget ? widget->window : NULL;
}

cairo_t* widget_cairo_create(struct widget* widget)
{
	if (!widget)
		return NULL;
	
	// Get a fresh cairo surface from the window
	cairo_surface_t* surface = (cairo_surface_t*)window_get_surface(widget->window);
	
	if (!surface)
		return NULL;
	
	// cairo_create takes a reference to the surface, so we can destroy our reference
	cairo_t* cr = cairo_create(surface);
	cairo_surface_destroy(surface);
	
	// Translate to widget's local coordinates
	cairo_translate(cr, -widget->allocation.x, -widget->allocation.y);
	
	return cr;
}

// Display scaling support (stubs for now - TODO: implement HiDPI support)
void window_set_buffer_scale(struct window* window, int32_t scale)
{
	// MacOS handles scaling automatically, so this is a no-op
	(void)window;
	(void)scale;
}

void widget_set_buffer_scale(struct widget* widget, int32_t scale)
{
	// MacOS handles scaling automatically, so this is a no-op
	(void)widget;
	(void)scale;
}

int32_t window_get_display_scale(struct window* window)
{
	if (!window || !window->nswindow)
		return 100;
	
	// Get the backing scale for regular (100%) or Retina (200%) displays.
	NSWindow* nsWindow = (NSWindow*)window->nswindow;
	CGFloat scale = [nsWindow backingScaleFactor];
	return (int32_t)(scale * 100.0f + 0.5f);
}
