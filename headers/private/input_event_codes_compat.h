/*
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Input event code compatibility header for cross-platform support
 * Provides Linux input event codes on macOS for keyboard/mouse handling
 */

#ifndef _INPUT_EVENT_CODES_COMPAT_H_
#define _INPUT_EVENT_CODES_COMPAT_H_

#if defined(__linux__)
// On Linux, use the system header
#include <linux/input-event-codes.h>

#elif defined(__APPLE__) || defined(_WIN32)
// On macOS and Windows, define the Linux input event codes we need
// These values match the Linux kernel definitions

// Mouse buttons
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112

// Keyboard modifiers
#define KEY_LEFTSHIFT   42
#define KEY_RIGHTSHIFT  54
#define KEY_LEFTCTRL    29
#define KEY_RIGHTCTRL   97
#define KEY_LEFTALT     56
#define KEY_RIGHTALT    100
#define KEY_MENU        139
#define KEY_CAPSLOCK    58
#define KEY_SCROLLLOCK  70
#define KEY_NUMLOCK     69

// Arrow keys
#define KEY_UP      103
#define KEY_DOWN    108
#define KEY_LEFT    105
#define KEY_RIGHT   106

// Navigation keys
#define KEY_HOME        102
#define KEY_END         107
#define KEY_PAGEUP      104
#define KEY_PAGEDOWN    109
#define KEY_INSERT      110
#define KEY_DELETE      111

// Function keys
#define KEY_F1      59
#define KEY_F2      60
#define KEY_F3      61
#define KEY_F4      62
#define KEY_F5      63
#define KEY_F6      64
#define KEY_F7      65
#define KEY_F8      66
#define KEY_F9      67
#define KEY_F10     68
#define KEY_F11     87
#define KEY_F12     88

// Special keys
#define KEY_ESC         1
#define KEY_TAB         15
#define KEY_ENTER       28
#define KEY_BACKSPACE   14
#define KEY_SPACE       57

// Editing keys
#define KEY_COPY        133
#define KEY_CUT         137
#define KEY_PASTE       135
#define KEY_UNDO        131

// Note: On macOS/Windows, backends will need to translate native keyCodes
// to these Linux-style codes for compatibility with the existing event handling

#else
#error "Unsupported platform for input event codes"
#endif

#endif /* _INPUT_EVENT_CODES_COMPAT_H_ */
