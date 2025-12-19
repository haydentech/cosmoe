/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Cocoa window implementation for macOS (Objective-C++)
 */

#define COSMOE_NO_SUPPORT_TYPES 1
#define COSMOE_NO_THREAD_INFO 1
#if defined(__APPLE__)
#include <private/support/apple_compat.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#endif
#include <stddef.h>
#include "window.h"
#include "cocoa_internal_structs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cairo.h>
#include <cairo-quartz.h>

// Forward declarations
struct window;
struct widget;

// Translate macOS keyCode to Linux-style input event code
static uint32_t translate_macos_keycode(uint32_t macKeyCode) {
    switch (macKeyCode) {
        case 0:  return 30;  // A
        case 11: return 48;  // B
        case 56: return 42;  // Left Shift
        case 60: return 54;  // Right Shift
        case 59: return 29;  // Left Control
        case 62: return 97;  // Right Control
        case 58: return 56;  // Left Alt/Option
        case 61: return 100; // Right Alt/Option
        case 55: return 125; // Left Command
        case 54: return 126; // Right Command
        case 110: return 139; // Menu/Application
        case 57: return 58;  // Caps Lock
        case 126: return 103; // Up
        case 125: return 108; // Down
        case 123: return 105; // Left
        case 124: return 106; // Right
        case 115: return 102; // Home
        case 119: return 107; // End
        case 116: return 104; // Page Up
        case 121: return 109; // Page Down
        case 114: return 110; // Insert
        case 117: return 111; // Delete
        case 122: return 59;  // F1
        case 120: return 60;  // F2
        case 99:  return 61;  // F3
        case 118: return 62;  // F4
        case 96:  return 63;  // F5
        case 97:  return 64;  // F6
        case 98:  return 65;  // F7
        case 100: return 66;  // F8
        case 101: return 67;  // F9
        case 109: return 68;  // F10
        case 103: return 87;  // F11
        case 111: return 88;  // F12
        case 53:  return 1;   // Escape
        case 48:  return 15;  // Tab
        case 36:  return 28;  // Return
        case 76:  return 28;  // Enter
        case 51:  return 14;  // Backspace
        case 49:  return 57;  // Space
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
    uint32_t time = (uint32_t)([event timestamp] * 1000.0);
    self.widget->window->key_handler(self.widget->window, NULL, time, linuxKeyCode, unicode, 1, NULL);
}

// The remainder of the implementation mirrors the original window.m
// implementation converted to Objective-C++ so C structs are visible

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
    return NO;
}

@end

// Internal struct definitions are provided in cocoa_internal_structs.h

// The remainder of display/window/widget functions are implemented in the
// C source (window.c/window.h in other platforms). For macOS we hook
// into those C functions directly from the Objective-C++ file above.
