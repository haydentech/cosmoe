/*
 * Copyright © 2025
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "debug_log.h"
#include <windows.h>
#include <windowsx.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>

#include "window.h"

/* Cursor ID enum values from Cursor.h - duplicated here to avoid pulling in C++ headers */
enum {
	B_CURSOR_ID_SYSTEM_DEFAULT					= 1,
	B_CURSOR_ID_I_BEAM							= 2,
	B_CURSOR_ID_CONTEXT_MENU					= 3,
	B_CURSOR_ID_COPY							= 4,
	B_CURSOR_ID_CROSS_HAIR						= 5,
	B_CURSOR_ID_FOLLOW_LINK						= 6,
	B_CURSOR_ID_GRAB							= 7,
	B_CURSOR_ID_GRABBING						= 8,
	B_CURSOR_ID_HELP							= 9,
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
	B_CURSOR_ID_ZOOM_OUT						= 28,
	B_CURSOR_ID_CREATE_LINK						= 29
};

#define MAX_WINDOWS 64
#define WINDOW_CLASS_NAME L"CosmoeWindow"

struct widget {
	struct window *window;
	void *user_data;
	struct rectangle allocation;
	cairo_surface_t *surface;
	HDC hdc;             /* Device context for this widget */
	HBITMAP bitmap;      /* Off-screen bitmap for double buffering */
	void *bitmap_data;   /* Pointer to bitmap pixel data */
	int surface_width;   /* Allocated surface size */
	int surface_height;  /* Allocated surface size */
	
	widget_redraw_handler_t redraw_handler;
	widget_resize_handler_t resize_handler;
	widget_button_handler_t button_handler;
	widget_motion_handler_t motion_handler;
	widget_idle_handler_t idle_handler;
	widget_axis_handler_t axis_handler;
	
	bool deferred_destroy;
	int cursor;
};

struct window {
	struct display *display;
	HWND hwnd;
	HDC hdc;
	struct widget *widget;
	void *user_data;
	
	char *title;
	int width, height;
	int x, y;
	int min_width, min_height;
	int max_width, max_height;
	bool mapped;
	
	/* Mouse position tracking for button events */
	int mouse_x, mouse_y;
	void (*move_handler)(struct window* window, int x, int y, void* user_data);
	void *move_user_data;
	widget_resize_handler_t resize_handler;
	window_key_handler_t key_handler;
	window_close_handler_t close_handler;
	void (*focus_handler)(struct window* window, bool focused, void* user_data);
	void *focus_user_data;
	
	volatile bool deferred_destroy;
	bool need_redraw;
	bool is_popup;  /* True for popup windows (menus, tooltips) */
	bool is_tooltip;  /* True specifically for tooltip windows */

	int32_t token;  /* BWindow object token for PortLink window identification */
};

struct display {
	HINSTANCE hinstance;
	ATOM window_class_atom;
	DWORD thread_id;  /* Thread ID of the display thread for marshaling */
	
#ifndef _WIN32
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
#endif
	
	struct window *windows[MAX_WINDOWS];
	int num_windows;
	
	bool running;
	bool exit_requested;

	int32_t backend_port;  /* Port backend reads commands from */
	int32_t app_port;      /* Port backend writes replies to */
	
	/* Idle detection for tooltips */
	struct window *last_motion_window;
	struct widget *last_motion_widget;
	int last_motion_x, last_motion_y;
	DWORD last_motion_time;
	bool idle_fired;
};

/* Helper function to find window by BWindow object token */
struct window *
display_find_window_by_token(struct display *display, int32_t token)
{
	if (!display || token < 0)
		return NULL;
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->token == token)
			return display->windows[i];
	}
	return NULL;
}

void
window_set_token(struct window *window, int32_t token)
{
	if (window)
		window->token = token;
}

/* Helper function to find window by HWND */
static struct window *
display_find_window(struct display *display, HWND hwnd)
{
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->hwnd == hwnd) {
			return display->windows[i];
		}
	}
	return NULL;
}

/* Helper function to add window to display */
static void
display_add_window(struct display *display, struct window *window)
{
	debug_log("display_add_window: ENTRY - display=%p, window=%p, num_windows=%d", 
		display, window, display->num_windows);
	if (display->num_windows < MAX_WINDOWS) {
		display->windows[display->num_windows++] = window;
		debug_log("display_add_window: Added window %p, total count now %d", window, display->num_windows);
	} else {
		debug_log("display_add_window: ERROR - MAX_WINDOWS (%d) reached!", MAX_WINDOWS);
	}
}

/* Helper function to remove window from display */
static void
display_remove_window(struct display *display, struct window *window)
{
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] == window) {
			/* Shift remaining windows down */
			for (int j = i; j < display->num_windows - 1; j++) {
				display->windows[j] = display->windows[j + 1];
			}
			display->num_windows--;
			return;
		}
	}
}

/* Convert Win32 key event data to Linux input keycode values expected by
 * the interface layer key_handler() switch(KEY_*). */
static uint32_t
win32_to_linux_keycode(WPARAM vkey, LPARAM lParam)
{
	uint32_t scan = (uint32_t)((lParam >> 16) & 0xFF);
	bool extended = (lParam & 0x01000000) != 0;

	/* Most non-extended Set 1 scan codes match Linux KEY_* values used
	 * throughout this codebase (letters, digits, modifiers, function keys). */
	if (!extended && scan != 0)
		return scan;

	/* Map common extended keys to Linux KEY_* values. */
	if (extended) {
		switch (scan) {
			case 0x1C: return 96;  /* KEY_KPENTER */
			case 0x1D: return 97;  /* KEY_RIGHTCTRL */
			case 0x35: return 98;  /* KEY_KPSLASH */
			case 0x38: return 100; /* KEY_RIGHTALT */
			case 0x47: return 102; /* KEY_HOME */
			case 0x48: return 103; /* KEY_UP */
			case 0x49: return 104; /* KEY_PAGEUP */
			case 0x4B: return 105; /* KEY_LEFT */
			case 0x4D: return 106; /* KEY_RIGHT */
			case 0x4F: return 107; /* KEY_END */
			case 0x50: return 108; /* KEY_DOWN */
			case 0x51: return 109; /* KEY_PAGEDOWN */
			case 0x52: return 110; /* KEY_INSERT */
			case 0x53: return 111; /* KEY_DELETE */
			case 0x5B: return 125; /* KEY_LEFTMETA */
			case 0x5C: return 126; /* KEY_RIGHTMETA */
			case 0x5D: return 127; /* KEY_COMPOSE / menu */
		}
	}

	/* Fallback for unusual synthetic events with no scan code. */
	switch (vkey) {
		case VK_RETURN: return 28; /* KEY_ENTER */
		case VK_ESCAPE: return 1;  /* KEY_ESC */
		case VK_BACK: return 14;   /* KEY_BACKSPACE */
		case VK_TAB: return 15;    /* KEY_TAB */
		case VK_SPACE: return 57;  /* KEY_SPACE */
		default: return 0;
	}
}

static HCURSOR
cursor_id_to_hcursor(int cursor)
{
	/* Some paths provide already-converted Win32 IDC_* resource IDs. */
	if (cursor >= 32512 && cursor <= 32650) {
		HCURSOR direct = LoadCursor(NULL, MAKEINTRESOURCE(cursor));
		if (direct != NULL)
			return direct;
	}

	/* Otherwise treat as Be/Cosmoe cursor ID. */
	switch (cursor) {
		case B_CURSOR_ID_SYSTEM_DEFAULT:
			return LoadCursor(NULL, IDC_ARROW);
		case B_CURSOR_ID_I_BEAM:
		case B_CURSOR_ID_I_BEAM_HORIZONTAL:
			return LoadCursor(NULL, IDC_IBEAM);
		case B_CURSOR_ID_CROSS_HAIR:
			return LoadCursor(NULL, IDC_CROSS);
		case B_CURSOR_ID_FOLLOW_LINK:
			return LoadCursor(NULL, IDC_HAND);
		case B_CURSOR_ID_GRABBING:
		case B_CURSOR_ID_MOVE:
			return LoadCursor(NULL, IDC_SIZEALL);
		case B_CURSOR_ID_GRAB:
			return LoadCursor(NULL, IDC_HAND);
		case B_CURSOR_ID_RESIZE_EAST_WEST:
			return LoadCursor(NULL, IDC_SIZEWE);
		case B_CURSOR_ID_RESIZE_NORTH_SOUTH:
			return LoadCursor(NULL, IDC_SIZENS);
		case B_CURSOR_ID_RESIZE_EAST:
		case B_CURSOR_ID_RESIZE_WEST:
			return LoadCursor(NULL, IDC_SIZEWE);
		case B_CURSOR_ID_RESIZE_NORTH:
		case B_CURSOR_ID_RESIZE_SOUTH:
			return LoadCursor(NULL, IDC_SIZENS);
		case B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST:
			return LoadCursor(NULL, IDC_SIZENESW);
		case B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST:
			return LoadCursor(NULL, IDC_SIZENWSE);
		case B_CURSOR_ID_NOT_ALLOWED:
			return LoadCursor(NULL, IDC_NO);
		case B_CURSOR_ID_PROGRESS:
			return LoadCursor(NULL, IDC_APPSTARTING);
		case B_CURSOR_ID_CONTEXT_MENU:
			return LoadCursor(NULL, IDC_HELP);
		case B_CURSOR_ID_NO_CURSOR:
		case B_CURSOR_ID_COPY:
		default:
			return LoadCursor(NULL, IDC_ARROW);
	}
}

/* Window procedure */
// Custom messages for marshaling window creation to display thread
#define WM_CREATE_WINDOW_MARSHAL (WM_USER + 1)
#define WM_CREATE_POPUP_MARSHAL (WM_USER + 2)
#define WM_DESTROY_WINDOW_MARSHAL (WM_USER + 3)

struct window_create_params {
	struct display* display;
	struct window* result;
	HANDLE completion_event;
};

struct popup_create_params {
	struct display* display;
	struct window* parent_window;
	int x;
	int y;
	struct window* result;
	HANDLE completion_event;
};

struct window_destroy_params {
	struct window* window;
	HANDLE completion_event;
};

/* Forward declarations */
static struct window *window_create_internal(struct display *display);
static struct window *window_popup_create_internal(struct display *display, 
                                                     struct window *parent_window, 
                                                     int x, int y);
static void window_deferred_destroy_internal(struct window *window);

static LRESULT CALLBACK
window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct window *window = (struct window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	
	switch (msg) {
		case WM_CREATE:
		{
			CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
			return 0;
		}
		
		case WM_CLOSE:
			if (window && window->close_handler) {
				window->close_handler(window->user_data);
			}
			/* Let Windows proceed with default close behavior (calls DestroyWindow) */
			return DefWindowProc(hwnd, msg, wParam, lParam);
		
		case WM_DESTROY:
			if (window) {
				window->deferred_destroy = true;
				/* NULL out hwnd so window_deferred_destroy_internal won't try to DestroyWindow again */
				window->hwnd = NULL;
				/* Clear close_handler to signal that window is being destroyed.
				* This prevents double-free if BWindow destructor calls window_deferred_destroy. */
				window->close_handler = NULL;
			}
			return 0;
		
		case WM_PAINT:
		{
			debug_log("WM_PAINT: window=%p, widget=%p", window, window ? window->widget : NULL);
			if (window && window->widget) {
				PAINTSTRUCT ps;
				HDC window_dc = BeginPaint(hwnd, &ps);
				
				/* First, trigger the redraw to update the widget surface */
				debug_log("WM_PAINT: redraw_handler=%p", window->widget->redraw_handler);
				if (window->widget->redraw_handler) {
					debug_log("WM_PAINT: Calling redraw_handler");
					window->widget->redraw_handler(window->widget, window->widget->user_data);
					debug_log("WM_PAINT: redraw_handler returned");
				}
				
				debug_log("WM_PAINT: Blitting widget bitmap to window");
				/* Blit the off-screen bitmap to the window */
				if (window->widget->hdc && window->widget->bitmap) {
					debug_log("WM_PAINT: hdc=%p, bitmap=%p, size=%dx%d",
						window->widget->hdc, window->widget->bitmap,
						window->width, window->height);
					BitBlt(window_dc, 0, 0, window->width, window->height,
					       window->widget->hdc, 0, 0, SRCCOPY);
					debug_log("WM_PAINT: BitBlt completed");
				} else {
					debug_log("WM_PAINT: No bitmap to blit (hdc=%p, bitmap=%p)",
						window->widget->hdc, window->widget->bitmap);
					/* Fill with white so we can see something */
					RECT rect = {0, 0, window->width, window->height};
					FillRect(window_dc, &rect, (HBRUSH)(COLOR_WINDOW+1));
				}
				
				debug_log("WM_PAINT: Calling EndPaint");
				EndPaint(hwnd, &ps);
				debug_log("WM_PAINT: EndPaint completed");
			}
			return 0;
		}
		
		case WM_SIZE:
		{
			if (window) {
				int width = LOWORD(lParam);
				int height = HIWORD(lParam);
				
				debug_log("WM_SIZE: %dx%d (was %dx%d), widget=%p", width, height, window->width, window->height, window->widget);

				if (width != window->width || height != window->height) {
					window->width = width;
					window->height = height;
				}

				/* Always forward WM_SIZE to the frame resize handler.
				 * Backend-side size caches may already match by the time WM_SIZE is
				 * delivered, but the app still needs B_WINDOW_RESIZED to resize fTopView. */
				if (window->resize_handler) {
					debug_log("WM_SIZE: Calling window resize_handler");
					/* Pass NULL so windowframe_resize_handler uses width/height from
					 * WM_SIZE directly, not potentially stale widget allocation. */
					window->resize_handler(NULL, width, height, window->user_data);
				}
				
				/* Always update widget allocation on WM_SIZE, even if window size hasn't changed
				 * This ensures the widget has the correct size on first paint */
				if (window->widget) {
					debug_log("WM_SIZE: Updating widget allocation to %dx%d", width, height);
					window->widget->allocation.width = width;
					window->widget->allocation.height = height;
					if (window->widget->resize_handler) {
						debug_log("WM_SIZE: Calling widget resize_handler");
						window->widget->resize_handler(window->widget, width, height,
						                                window->widget->user_data);
					}
				}
			}
			return 0;
		}
		
		case WM_MOVE:
		{
			if (window) {
				window->x = (int)(short)LOWORD(lParam);
				window->y = (int)(short)HIWORD(lParam);
				
				if (window->move_handler) {
					window->move_handler(window, window->x, window->y, window->move_user_data);
				}
			}
			return 0;
		}

		case WM_SETCURSOR:
		{
			if (window && window->widget && LOWORD(lParam) == HTCLIENT) {
				SetCursor(cursor_id_to_hcursor(window->widget->cursor));
				return TRUE;
			}
			break;
		}
		
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		{
			if (window && window->widget && window->widget->button_handler) {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				uint32_t button = (msg == WM_LBUTTONDOWN) ? 1 : (msg == WM_RBUTTONDOWN) ? 3 : 2;
				uint32_t time = GetTickCount();
				
				debug_log("WM_*BUTTONDOWN: button=%d, x=%d, y=%d, calling button_handler", button, x, y);
				fflush(stderr);
				
				window->mouse_x = x;
				window->mouse_y = y;
				
				// Pass widget as input (same pattern as X11 backend)
				window->widget->button_handler(window->widget, (struct input*)window->widget, time, button, 1,
				                                 window->widget->user_data);
				debug_log("WM_*BUTTONDOWN: button_handler returned");
				fflush(stderr);
			}
			return 0;
		}
		
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		{
			if (window && window->widget && window->widget->button_handler) {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				uint32_t button = (msg == WM_LBUTTONUP) ? 1 : (msg == WM_RBUTTONUP) ? 3 : 2;
				uint32_t time = GetTickCount();
				
				window->mouse_x = x;
				window->mouse_y = y;
				
				// Pass widget as input (same pattern as X11 backend)
				window->widget->button_handler(window->widget, (struct input*)window->widget, time, button, 0,
				                                 window->widget->user_data);
			}
			return 0;
		}
		
		case WM_MOUSEMOVE:
		{
			if (window && window->widget && window->widget->motion_handler) {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				uint32_t time = GetTickCount();
				
				window->mouse_x = x;
				window->mouse_y = y;
				
				int cursor = window->widget->motion_handler(window->widget, NULL, time, (float)x, (float)y,
				                                window->widget->user_data);

				// TODO: move cursor handling to it's own function
				if (cursor != window->widget->cursor) {
					window->widget->cursor = cursor;
					SetCursor(cursor_id_to_hcursor(cursor));
				}
			}
			return 0;
		}
		
		case WM_MOUSEWHEEL:
		{
			if (window && window->widget && window->widget->axis_handler) {
				int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				uint32_t time = GetTickCount();
				/* Convert delta to wl_fixed_t format (fixed point 24.8) */
				wl_fixed_t value = (delta / WHEEL_DELTA) * 256;
				
				/* Pass widget as input context so InputGetPosition can map wheel
				 * events to the pointer location, matching X11 behavior. */
				window->widget->axis_handler(window->widget, window->widget, time, 0, -value,
				                             window->widget->user_data);
			}
			return 0;
		}
		
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			if (window && window->key_handler) {
				uint32_t key = win32_to_linux_keycode(wParam, lParam);
				uint32_t time = GetTickCount();
#ifndef _WIN32
				enum xkb_key_direction state = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? 
				                                XKB_KEY_DOWN : XKB_KEY_UP;
#else
				uint32_t state = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? 1 : 0; // 1=down, 0=up
#endif

				uint32_t unicode = 0;
				if (state == 1) {
					/* Convert to Unicode on key-down only. Calling ToUnicode on key-up can
					 * consume keyboard layout state and drop subsequent key translations. */
					BYTE keyboard_state[256] = {0};
					WCHAR unicode_char[4] = {0};
					UINT scan_code = (UINT)((lParam >> 16) & 0xFF);
					if (lParam & 0x01000000)
						scan_code |= 0xE000;

					GetKeyboardState(keyboard_state);
					keyboard_state[wParam & 0xFF] |= 0x80;

					int result = ToUnicodeEx((UINT)wParam, scan_code, keyboard_state,
						unicode_char, 4, 0, GetKeyboardLayout(0));
					if (result > 0) {
						unicode = (uint32_t)unicode_char[0];
					} else if (result < 0) {
						/* Clear dead-key state to keep later keypress translation stable. */
						WCHAR dead_buffer[4] = {0};
						ToUnicodeEx((UINT)wParam, scan_code, keyboard_state,
							dead_buffer, 4, 0, GetKeyboardLayout(0));
					}
				}
				
				window->key_handler(window, NULL, time, key, unicode, state, window->user_data);
			}
			return 0;
		}
		
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
		{
			if (window && window->focus_handler) {
				bool focused = (msg == WM_SETFOCUS);
				window->focus_handler(window, focused, window->focus_user_data);
			}
			return 0;
		}
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

/* Register the window class */
static bool
register_window_class(struct display *display)
{
	debug_log("register_window_class: Starting");
	
	WNDCLASSEXW wc = {0};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = window_proc;
	wc.cbWndExtra = sizeof(void*);  /* Space for window pointer */
	wc.hInstance = display->hinstance;
	
	debug_log("register_window_class: Loading icons...");
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	if (wc.hIcon == NULL)
		debug_log_error("register_window_class: LoadIcon(hIcon) failed");
	
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (wc.hCursor == NULL)
		debug_log_error("register_window_class: LoadCursor failed");
	
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = WINDOW_CLASS_NAME;
	
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if (wc.hIconSm == NULL)
		debug_log_error("register_window_class: LoadIcon(hIconSm) failed");
	
	debug_log("register_window_class: Calling RegisterClassExW...");
	display->window_class_atom = RegisterClassExW(&wc);
	if (display->window_class_atom == 0) {
		debug_log_error("register_window_class: RegisterClassExW FAILED");
		return false;
	}
	
	debug_log("register_window_class: SUCCESS - atom = %d", display->window_class_atom);
	return true;
}

/* Initialize xkbcommon for keyboard handling */
static bool
init_xkb(struct display *display)
{
#ifndef _WIN32
	display->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!display->xkb_context) {
		fprintf(stderr, "Failed to create xkb context\n");
		return false;
	}
	
	/* For Win32, we'll use a simple default keymap */
	struct xkb_rule_names names = {
		.rules = NULL,
		.model = NULL,
		.layout = "us",
		.variant = NULL,
		.options = NULL
	};
	
	display->xkb_keymap = xkb_keymap_new_from_names(display->xkb_context, &names,
	                                                XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!display->xkb_keymap) {
		fprintf(stderr, "Failed to create xkb keymap\n");
		xkb_context_unref(display->xkb_context);
		return false;
	}
	
	display->xkb_state = xkb_state_new(display->xkb_keymap);
	if (!display->xkb_state) {
		fprintf(stderr, "Failed to create xkb state\n");
		xkb_keymap_unref(display->xkb_keymap);
		xkb_context_unref(display->xkb_context);
		return false;
	}
#else
	// Windows uses its own keyboard APIs
	(void)display;
#endif
	
	return true;
}

struct display *
display_create(int *argc, char **argv)
{
	debug_log_init();
	debug_log("==========================================");
	debug_log("display_create: ENTRY");
	debug_log("==========================================");
	
	// Set up FontConfig to find fonts
	// Detect if we're running under Wine or native Windows
	debug_log("display_create: Configuring FontConfig...");
	
	// Disable home directory font search (we'll specify fonts explicitly)
	FcConfigEnableHome(FcFalse);
	
	// Create a blank FontConfig configuration (don't try to load system config files)
	// This avoids the "Cannot load default config file" error
	FcConfig* config = FcConfigCreate();
	if (config) {
		char fonts_dir[512];
		
		// Detect Wine vs native Windows
		// Wine always sets HOME to a Unix-style path (e.g., /home/user)
		const char* home = getenv("HOME");
		const char* wineprefix = getenv("WINEPREFIX");
		bool is_wine = false;
		
		// If WINEPREFIX is set, we're definitely under Wine
		if (wineprefix) {
			is_wine = true;
		}
		// If HOME exists and looks like a Unix path (starts with /), we're likely under Wine
		else if (home && home[0] == '/') {
			is_wine = true;
		}
		
		if (is_wine) {
			// Running under Wine - use Wine's Windows fonts directory
			if (!home) home = "/home/billh";  // Fallback
			snprintf(fonts_dir, sizeof(fonts_dir), "%s/.wine/drive_c/windows/Fonts", home);
			debug_log("display_create: Running under Wine, using fonts from: %s", fonts_dir);
		} else {
			// Running on native Windows - use actual Windows fonts directory
			char windows_dir[MAX_PATH];
			if (GetWindowsDirectoryA(windows_dir, MAX_PATH) > 0) {
				snprintf(fonts_dir, sizeof(fonts_dir), "%s\\Fonts", windows_dir);
			} else {
				// Fallback to typical location
				strcpy(fonts_dir, "C:\\Windows\\Fonts");
			}
			debug_log("display_create: Running on native Windows, using fonts from: %s", fonts_dir);
		}
		
		FcBool added = FcConfigAppFontAddDir(config, (const FcChar8*)fonts_dir);
		debug_log("display_create: FcConfigAppFontAddDir(%s) = %d", fonts_dir, added);
		
		// Set as current config BEFORE building fonts
		FcBool set = FcConfigSetCurrent(config);
		debug_log("display_create: FcConfigSetCurrent() = %d", set);
		
		// Build the font cache with our fonts
		FcConfigBuildFonts(config);
		debug_log("display_create: FontConfig configured successfully");
		
		// Verify that we can find fonts
		FcPattern* pat = FcPatternCreate();
		FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"Noto Sans");
		FcConfigSubstitute(config, pat, FcMatchPattern);
		FcDefaultSubstitute(pat);
		
		FcResult result;
		FcPattern* match = FcFontMatch(config, pat, &result);
		if (match) {
			FcChar8* family = NULL;
			FcChar8* file = NULL;
			FcPatternGetString(match, FC_FAMILY, 0, &family);
			FcPatternGetString(match, FC_FILE, 0, &file);
			debug_log("display_create: Font match for 'Noto Sans': family='%s', file='%s'", 
				family ? (char*)family : "NULL", 
				file ? (char*)file : "NULL");
			FcPatternDestroy(match);
		} else {
			debug_log("display_create: WARNING - No font match found for 'Noto Sans'");
		}
		FcPatternDestroy(pat);
		
		// Force Pango to recreate its font map now that FontConfig is properly configured
		// This ensures Pango uses our configured fonts instead of caching the empty default
		debug_log("display_create: Forcing Pango font map reset...");
		pango_cairo_font_map_set_default(NULL);  // Clear the cached default
		PangoFontMap* new_fontmap = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
		if (new_fontmap) {
			pango_cairo_font_map_set_default(PANGO_CAIRO_FONT_MAP(new_fontmap));
			debug_log("display_create: Pango font map recreated successfully");
		} else {
			debug_log("display_create: WARNING - Failed to create new Pango font map");
		}
	} else {
		debug_log("display_create: Failed to create FontConfig!");
	}
	
	struct display *display = calloc(1, sizeof(*display));
	if (!display) {
		debug_log("display_create: calloc FAILED");
		return NULL;
	}
	debug_log("display_create: Allocated display structure at %p", display);
	
	debug_log("display_create: Getting module handle...");
	display->hinstance = GetModuleHandle(NULL);
	if (display->hinstance == NULL) {
		debug_log_error("display_create: GetModuleHandle FAILED");
		free(display);
		return NULL;
	}
	debug_log("display_create: hinstance = %p", display->hinstance);
	
	if (!register_window_class(display)) {
		debug_log("display_create: register_window_class FAILED");
		fprintf(stderr, "Failed to register window class\n");
		free(display);
		return NULL;
	}
	
	debug_log("display_create: Initializing xkb...");
	if (!init_xkb(display)) {
		debug_log("display_create: init_xkb FAILED");
		free(display);
		return NULL;
	}
	debug_log("display_create: xkb initialized successfully");
	
	display->running = false;
	display->exit_requested = false;
	display->thread_id = 0;  /* Will be set by display_run on the display thread */
	display->backend_port = -1;  /* No port until set by BApplication */
	display->app_port = -1;      /* No port until set by BApplication */
	
	debug_log("display_create: SUCCESS - returning %p", display);
	debug_log("==========================================");
	return display;
}

void
display_run(struct display *display)
{
	MSG msg;
	
	// Store the display thread ID so window_create can detect it
	display->thread_id = GetCurrentThreadId();
	debug_log("display_run: Starting on thread %lu", display->thread_id);
	
	display->running = true;
	display->exit_requested = false;
	
	// Check for windows that were created on the wrong thread and log a warning
	debug_log("display_run: Checking %d existing windows for thread ownership", display->num_windows);
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->hwnd) {
			DWORD window_thread = GetWindowThreadProcessId(display->windows[i]->hwnd, NULL);
			debug_log("display_run: Window %d hwnd=%p, window thread=%lu, display thread=%lu", 
				i, display->windows[i]->hwnd, window_thread, display->thread_id);
			
			if (window_thread != display->thread_id) {
				debug_log("display_run: WARNING - Window %d was created on wrong thread! Messages will not be received!", i);
			}
			
			// Trigger initial paint regardless
			debug_log("display_run: Invalidating window %d (hwnd=%p)", i, display->windows[i]->hwnd);
			InvalidateRect(display->windows[i]->hwnd, NULL, FALSE);
			debug_log("display_run: Invalidated window, WM_PAINT will be dispatched when messages arrive");
		}
	}
	
	debug_log("display_run: Entering main loop");
	int loop_count = 0;
	while (display->running && !display->exit_requested) {
		/* Process all pending Windows messages */
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (loop_count < 20 || msg.message == WM_PAINT) {  // Log first 20 messages and all WM_PAINT
				debug_log("display_run: Got message: msg=%d (0x%x), hwnd=%p", msg.message, msg.message, msg.hwnd);
			}
			loop_count++;
			
			if (msg.message == WM_QUIT) {
				debug_log("display_run: Received WM_QUIT");
				display->running = false;
				break;
			}
			
			// Handle window creation marshal requests
			if (msg.message == WM_CREATE_WINDOW_MARSHAL) {
				debug_log("display_run: Handling WM_CREATE_WINDOW_MARSHAL");
				struct window_create_params* params = (struct window_create_params*)msg.lParam;
				params->result = window_create_internal(params->display);
				SetEvent(params->completion_event);
				continue;
			}
			
			// Handle popup window creation marshal requests
			if (msg.message == WM_CREATE_POPUP_MARSHAL) {
				debug_log("display_run: Handling WM_CREATE_POPUP_MARSHAL");
				fflush(stdout);
				struct popup_create_params* params = (struct popup_create_params*)msg.lParam;
				debug_log("display_run: Calling window_popup_create_internal...");
				fflush(stdout);
				params->result = window_popup_create_internal(params->display, 
				                                               params->parent_window,
				                                               params->x, params->y);
				debug_log("display_run: window_popup_create_internal returned %p, signaling event", params->result);
				fflush(stdout);
				SetEvent(params->completion_event);
				debug_log("display_run: Event signaled, continuing");
				fflush(stdout);
				continue;
			}
			
			// Handle window destruction marshal requests
			if (msg.message == WM_DESTROY_WINDOW_MARSHAL) {
				debug_log("display_run: Handling WM_DESTROY_WINDOW_MARSHAL");
				struct window_destroy_params* params = (struct window_destroy_params*)msg.lParam;
				window_deferred_destroy_internal(params->window);
				SetEvent(params->completion_event);
				continue;
			}
			
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		/* Handle deferred window destroys */
		for (int i = display->num_windows - 1; i >= 0; i--) {
			struct window *window = display->windows[i];
			if (window && window->deferred_destroy) {
				if (window->widget && window->widget->deferred_destroy) {
					widget_deferred_destroy(window->widget);
				}
				// Call internal version directly - we're already on display thread
				window_deferred_destroy_internal(window);
			}
		}
		
		/* Handle pending redraws (similar to Wayland's idle_redraw) */
		for (int i = 0; i < display->num_windows; i++) {
			struct window *window = display->windows[i];
			if (window && window->need_redraw && window->hwnd) {
				debug_log("display_run: Processing need_redraw for window %p", window);
				window->need_redraw = false;
				
				/* Trigger repaint asynchronously only for mapped windows.
				 * Visibility must be controlled explicitly via window_show/window_hide. */
				if (window->mapped) {
					InvalidateRect(window->hwnd, NULL, FALSE);
					debug_log("display_run: Invalidated mapped window for redraw");
				}
			}
		}
		
		/* Poll backend message port */
		if (display->backend_port >= 0) {
			extern void windows_process_backend_messages(int32_t backend_port, int32_t app_port);
			windows_process_backend_messages(display->backend_port, display->app_port);
		}

		/* Sleep briefly to avoid hogging CPU */
		if (display->num_windows == 0) {
			debug_log("display_run: No windows, sleeping 10ms");
			Sleep(10);
		} else {
			/* Use MsgWaitForMultipleObjects for efficient waiting */
			debug_log("display_run: Calling MsgWaitForMultipleObjects");
			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
			debug_log("display_run: MsgWaitForMultipleObjects returned");
		}
	}
	
	debug_log("display_run: Exited main loop");
	debug_log("display_run: COMPLETE");
}

void
display_set_port(struct display *display, int32_t sender_port_id, int32_t receiver_port_id)
{
	if (!display)
		return;
	display->backend_port = sender_port_id;
	display->app_port = receiver_port_id;
	debug_log("Windows: display_set_port: backend reads from port %d, writes to port %d",
		sender_port_id, receiver_port_id);
}

void
window_show(struct window *window)
{
	if (!window || !window->hwnd)
		return;
	ShowWindow(window->hwnd, SW_SHOW);
	window->mapped = true;
}

void
window_hide(struct window *window)
{
	if (!window || !window->hwnd)
		return;
	ShowWindow(window->hwnd, SW_HIDE);
	window->mapped = false;
}

void
window_resize(struct window *window, int width, int height)
{
	window_schedule_resize(window, width, height);
}

void
window_minimize(struct window *window, bool minimize)
{
	if (!window || !window->hwnd)
		return;
	ShowWindow(window->hwnd, minimize ? SW_MINIMIZE : SW_RESTORE);
}

void
window_activate(struct window *window, bool active)
{
	if (!window || !window->hwnd)
		return;
	if (active)
		SetForegroundWindow(window->hwnd);
}

bool
window_is_front(struct window *window)
{
	if (!window || !window->hwnd)
		return false;

	return GetForegroundWindow() == window->hwnd;
}

void
display_exit(struct display *display)
{
	display->exit_requested = true;
	display->running = false;
}

void
display_flush(struct display *display)
{
	/* Win32 messages are handled synchronously, no explicit flush needed */
	(void)display;
}

void
display_trigger_redraw(struct display *display, struct window *window,
                       struct widget *widget)
{
	if (window && window->hwnd) {
		InvalidateRect(window->hwnd, NULL, FALSE);
	}
}

void
display_get_screen_dimensions(struct display *display, struct rectangle *allocation)
{
	if (!allocation)
		return;
	
	allocation->x = 0;
	allocation->y = 0;
	allocation->width = GetSystemMetrics(SM_CXSCREEN);
	allocation->height = GetSystemMetrics(SM_CYSCREEN);
}

// Internal function to create window - must be called on display thread
static struct window *
window_create_internal(struct display *display)
{
	debug_log("window_create_internal: ENTRY - display=%p, thread=%lu", display, GetCurrentThreadId());
	
	struct window *window = calloc(1, sizeof(*window));
	if (!window) {
		debug_log("window_create_internal: calloc FAILED");
		return NULL;
	}
	debug_log("window_create_internal: Allocated window structure at %p", window);
	
	window->display = display;
	window->width = 640;
	window->height = 480;
	window->min_width = 1;
	window->min_height = 1;
	window->max_width = 32767;
	window->max_height = 32767;
	window->mapped = false;
	window->is_popup = false;
	window->is_tooltip = false;
	
	/* Create the Win32 window */
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exStyle = WS_EX_APPWINDOW;
	
	// Convert default title to UTF-16
	const char* default_title = "AAAAAAAAAA";
	int wlen = MultiByteToWideChar(CP_UTF8, 0, default_title, -1, NULL, 0);
	wchar_t* wtitle = (wchar_t*)malloc(wlen * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, default_title, -1, wtitle, wlen);
	
	debug_log("window_create_internal: UTF-16 title wlen=%d", wlen);
	debug_log("window_create_internal: First 4 wchars: %04x %04x %04x %04x",
		wtitle[0], wtitle[1], wtitle[2], wtitle[3]);
	
	debug_log("window_create_internal: Calling CreateWindowExW...");
	debug_log("  hinstance=%p, class_atom=%d", display->hinstance, display->window_class_atom);
	
	window->hwnd = CreateWindowExW(
		exStyle,
		WINDOW_CLASS_NAME,
		wtitle,  /* Set title at creation */
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window->width, window->height,
		NULL, NULL,
		display->hinstance,
		window  /* Pass window pointer through lpParam */
	);
	
	free(wtitle);
	
	if (!window->hwnd) {
		debug_log_error("window_create_internal: CreateWindowExW FAILED");
		free(window);
		return NULL;
	}
	debug_log("window_create_internal: hwnd = %p", window->hwnd);
	
	debug_log("window_create_internal: Getting DC...");
	window->hdc = GetDC(window->hwnd);
	if (!window->hdc) {
		debug_log_error("window_create_internal: GetDC FAILED");
	} else {
		debug_log("window_create_internal: hdc = %p", window->hdc);
	}
	
	debug_log("window_create_internal: Adding to display...");
	display_add_window(display, window);
	
	// Automatically set the window icon from the executable
	window_set_icon_from_exe(window);
	
	debug_log("window_create_internal: SUCCESS - returning %p", window);
	return window;
}

struct window *
window_create(struct display *display)
{
	debug_log("window_create: ENTRY - display=%p, caller_thread=%lu, display_thread=%lu", 
		display, GetCurrentThreadId(), display ? display->thread_id : 0);
	
	// If display thread hasn't started yet, or we're already on it, create directly
	if (!display || display->thread_id == 0 || GetCurrentThreadId() == display->thread_id) {
		debug_log("window_create: Creating directly (thread_id=%lu, current=%lu)", 
			display ? display->thread_id : 0, GetCurrentThreadId());
		return window_create_internal(display);
	}
	
	// We're on a different thread - marshal to display thread
	debug_log("window_create: Marshaling to display thread");
	
	struct window_create_params params;
	params.display = display;
	params.result = NULL;
	params.completion_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	if (!params.completion_event) {
		debug_log_error("window_create: CreateEvent FAILED");
		return NULL;
	}
	
	// Post message to display thread's message queue
	if (!PostThreadMessage(display->thread_id, WM_CREATE_WINDOW_MARSHAL, 0, (LPARAM)&params)) {
		DWORD error = GetLastError();
		debug_log("window_create: PostThreadMessage FAILED, error=%lu", error);
		debug_log_error("window_create: PostThreadMessage FAILED");
		CloseHandle(params.completion_event);
		return NULL;
	}
	
	// Wait for creation to complete
	debug_log("window_create: Waiting for completion...");
	WaitForSingleObject(params.completion_event, INFINITE);
	CloseHandle(params.completion_event);
	
	debug_log("window_create: Completed, result=%p", params.result);
	return params.result;
}

// Internal function to create popup window - must be called on display thread
static struct window *
window_popup_create_internal(struct display *display, struct window *parent_window, int x, int y)
{
	debug_log("window_popup_create_internal: ENTRY - display=%p, parent=%p, x=%d, y=%d, thread=%lu", 
		display, parent_window, x, y, GetCurrentThreadId());
	
	struct window *window = calloc(1, sizeof(*window));
	if (!window) {
		debug_log("window_popup_create_internal: calloc FAILED");
		return NULL;
	}
	debug_log("window_popup_create_internal: Allocated window structure at %p", window);
	
	window->display = display;
	window->width = 100;
	window->height = 100;
	window->x = x;
	window->y = y;
	window->is_popup = true;
	
	/* Create popup window (no decorations) */
	DWORD style = WS_POPUP;
	DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
	
	HWND parent_hwnd = parent_window ? parent_window->hwnd : NULL;
	
	window->hwnd = CreateWindowExW(
		exStyle,
		WINDOW_CLASS_NAME,
		L"",
		style,
		x, y,
		window->width, window->height,
		parent_hwnd, NULL,
		display->hinstance,
		window
	);
	
	if (!window->hwnd) {
		free(window);
		return NULL;
	}
	
	window->hdc = GetDC(window->hwnd);
	
	display_add_window(display, window);
	
	debug_log("window_popup_create_internal: SUCCESS - returning %p", window);
	return window;
}

struct window *
window_popup_create(struct display *display, struct window *parent_window, int x, int y)
{
	debug_log("window_popup_create: ENTRY - display=%p, parent=%p, x=%d, y=%d, caller_thread=%lu, display_thread=%lu", 
		display, parent_window, x, y, GetCurrentThreadId(), display ? display->thread_id : 0);
	
	// If display thread hasn't started yet, or we're already on it, create directly
	if (!display || display->thread_id == 0 || GetCurrentThreadId() == display->thread_id) {
		debug_log("window_popup_create: Creating directly (thread_id=%lu, current=%lu)", 
			display ? display->thread_id : 0, GetCurrentThreadId());
		return window_popup_create_internal(display, parent_window, x, y);
	}
	
	// We're on a different thread - marshal to display thread
	debug_log("window_popup_create: Marshaling to display thread");
	
	struct popup_create_params params;
	params.display = display;
	params.parent_window = parent_window;
	params.x = x;
	params.y = y;
	params.result = NULL;
	params.completion_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	if (!params.completion_event) {
		debug_log_error("window_popup_create: CreateEvent FAILED");
		return NULL;
	}
	
	// Post message to display thread's message queue
	debug_log("window_popup_create: About to PostThreadMessage to thread %lu", display->thread_id);
	fflush(stdout);
	if (!PostThreadMessage(display->thread_id, WM_CREATE_POPUP_MARSHAL, 0, (LPARAM)&params)) {
		DWORD error = GetLastError();
		debug_log("window_popup_create: PostThreadMessage FAILED, error=%lu", error);
		debug_log_error("window_popup_create: PostThreadMessage FAILED");
		CloseHandle(params.completion_event);
		return NULL;
	}
	debug_log("window_popup_create: PostThreadMessage succeeded");
	fflush(stdout);
	
	// Wait for creation to complete
	debug_log("window_popup_create: Waiting for completion event...");
	fflush(stdout);
	DWORD waitResult = WaitForSingleObject(params.completion_event, 5000); // 5 second timeout for debugging
	if (waitResult == WAIT_TIMEOUT) {
		debug_log("window_popup_create: TIMEOUT waiting for completion! Display thread may be hung.");
		fflush(stdout);
		CloseHandle(params.completion_event);
		return NULL;
	}
	CloseHandle(params.completion_event);
	
	debug_log("window_popup_create: Completed, result=%p", params.result);
	fflush(stdout);
	return params.result;
}

void
window_get_position(struct window *window, int *x, int *y)
{
	RECT rect;
	if (GetWindowRect(window->hwnd, &rect)) {
		if (x) *x = rect.left;
		if (y) *y = rect.top;
	}
}

void
window_set_position(struct window *window, int x, int y)
{
	debug_log("window_set_position: ENTRY - window=%p, x=%d, y=%d, caller_thread=%lu, display_thread=%lu",
		window, x, y, GetCurrentThreadId(), window && window->display ? window->display->thread_id : 0);
	
	if (!window || !window->hwnd)
		return;
	
	// Always call SetWindowPos directly with SWP_ASYNCWINDOWPOS flag
	// This avoids cross-thread marshaling deadlocks while still being thread-safe
	// The ASYNCWINDOWPOS flag makes SetWindowPos return immediately without waiting
	// for the window to process the resulting WM_MOVE message
	debug_log("window_set_position: Calling SetWindowPos with ASYNCWINDOWPOS");
	SetWindowPos(window->hwnd, NULL, x, y, 0, 0, 
	            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
	window->x = x;
	window->y = y;
	
	debug_log("window_set_position: Completed (async)");
}

void
window_set_title(struct window *window, const char *title)
{
	debug_log("window_set_title: hwnd=%p, title='%s', mapped=%d", 
		window->hwnd, title ? title : "(null)", window->mapped);
	
	if (window->title)
		free(window->title);
	window->title = title ? strdup(title) : NULL;
	
	if (title && window->hwnd) {
		// Convert UTF-8 to UTF-16 and use SetWindowTextW
		int wlen = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
		if (wlen > 0) {
			wchar_t* wtitle = (wchar_t*)malloc(wlen * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, wlen);
			
			debug_log("window_set_title: Calling SetWindowTextW with UTF-16 title, wlen=%d", wlen);
			BOOL result = SetWindowTextW(window->hwnd, wtitle);
			debug_log("window_set_title: SetWindowTextW returned %d", result);
			
			free(wtitle);
		}
	}
}

void
window_set_appid(struct window *window, const char *app_id)
{
	(void)window;
	if (app_id == NULL || app_id[0] == '\0')
		return;

	int wlen = MultiByteToWideChar(CP_UTF8, 0, app_id, -1, NULL, 0);
	if (wlen <= 0)
		return;

	wchar_t* wAppId = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
	if (wAppId == NULL)
		return;

	if (MultiByteToWideChar(CP_UTF8, 0, app_id, -1, wAppId, wlen) <= 0) {
		free(wAppId);
		return;
	}

	typedef HRESULT (WINAPI *SetAppIdFunc)(PCWSTR);
	HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
	if (shell32 != NULL) {
		SetAppIdFunc setAppId = (SetAppIdFunc)GetProcAddress(shell32,
			"SetCurrentProcessExplicitAppUserModelID");
		if (setAppId != NULL)
			setAppId(wAppId);
	}

	free(wAppId);
}

void
window_set_parent(struct window *window, struct window *parent)
{
	HWND parent_hwnd = parent ? parent->hwnd : NULL;
	SetParent(window->hwnd, parent_hwnd);
}

void
window_schedule_resize(struct window *window, int width, int height)
{
	debug_log("window_schedule_resize: width=%d, height=%d, mapped=%d", 
		width, height, window->mapped);
	
	/* Adjust for window decorations */
	RECT rect = { 0, 0, width, height };
	DWORD style = GetWindowLong(window->hwnd, GWL_STYLE);
	DWORD exStyle = GetWindowLong(window->hwnd, GWL_EXSTYLE);
	AdjustWindowRectEx(&rect, style, FALSE, exStyle);

	BOOL resized = SetWindowPos(window->hwnd, NULL, 0, 0,
	            rect.right - rect.left, rect.bottom - rect.top,
	            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	if (!resized) {
		debug_log_error("window_schedule_resize: SetWindowPos failed");
	} else {
		/* Keep internal size aligned with the actual client area if already committed.
		 * WM_SIZE remains authoritative and will update these values as needed. */
		RECT clientRect;
		if (GetClientRect(window->hwnd, &clientRect)) {
			window->width = clientRect.right - clientRect.left;
			window->height = clientRect.bottom - clientRect.top;
		}
	}
	
	/* Just mark for redraw - don't call ShowWindow/InvalidateRect synchronously
	 * as they can trigger WM_PAINT before the window looper thread is running */
	window->need_redraw = true;
	
	debug_log("window_schedule_resize: Set need_redraw flag");
}

void
window_set_min_max_allocation(struct window *window,
                               int min_width, int min_height,
                               int max_width, int max_height)
{
	window->min_width = min_width;
	window->min_height = min_height;
	window->max_width = max_width;
	window->max_height = max_height;
	
	/* Win32 doesn't have direct min/max size setting,
	 * would need to handle WM_GETMINMAXINFO message */
}

void
window_set_resize_handler(struct window *window, widget_resize_handler_t handler)
{
	window->resize_handler = handler;
}

void
window_set_key_handler(struct window *window, window_key_handler_t handler)
{
	window->key_handler = handler;
}

void
window_set_close_handler(struct window *window, window_close_handler_t handler)
{
	window->close_handler = handler;
}

void
window_set_focus_handler(struct window *window,
                         void (*handler)(struct window*, bool, void*),
                         void *user_data)
{
	window->focus_handler = handler;
	window->focus_user_data = user_data;
}

void
window_set_move_handler(struct window *window,
                       void (*handler)(struct window*, int, int, void*),
                       void *user_data)
{
	window->move_handler = handler;
	window->move_user_data = user_data;
}

struct display *
window_get_display(struct window *window)
{
	return window->display;
}

static void
window_deferred_destroy_internal(struct window *window)
{
	debug_log("window_deferred_destroy_internal: ENTRY - window=%p, hwnd=%p, user_data=%p, thread=%lu", 
		window, window ? window->hwnd : NULL, window ? window->user_data : NULL, GetCurrentThreadId());
	
	if (!window)
		return;
	
	if (window->widget) {
		widget_deferred_destroy(window->widget);
	}
	
	if (window->hdc && window->hwnd) {
		ReleaseDC(window->hwnd, window->hdc);
		window->hdc = NULL;
	}
	
	if (window->hwnd) {
		debug_log("window_deferred_destroy_internal: Calling DestroyWindow on hwnd=%p", window->hwnd);
		DestroyWindow(window->hwnd);
		window->hwnd = NULL;
	}
	
	if (window->title) {
		free(window->title);
		window->title = NULL;
	}
	
	display_remove_window(window->display, window);
	free(window);
	
	debug_log("window_deferred_destroy_internal: Completed");
}

void
window_deferred_destroy(struct window *window)
{
	debug_log("window_deferred_destroy: ENTRY - window=%p, caller_thread=%lu, display_thread=%lu", 
		window, GetCurrentThreadId(), window && window->display ? window->display->thread_id : 0);
	
	if (!window)
		return;
	
	// If display thread hasn't started yet, or we're already on it, destroy directly
	if (!window->display || window->display->thread_id == 0 || 
	    GetCurrentThreadId() == window->display->thread_id) {
		debug_log("window_deferred_destroy: Destroying directly (thread_id=%lu, current=%lu)", 
			window->display ? window->display->thread_id : 0, GetCurrentThreadId());
		window_deferred_destroy_internal(window);
		return;
	}
	
	// We're on a different thread - marshal to display thread
	debug_log("window_deferred_destroy: Marshaling to display thread");
	
	struct window_destroy_params params;
	params.window = window;
	params.completion_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	if (!params.completion_event) {
		debug_log_error("window_deferred_destroy: CreateEvent FAILED");
		// Fall back to direct destruction (risky but better than leaking)
		window_deferred_destroy_internal(window);
		return;
	}
	
	// Post message to display thread's message queue
	if (!PostThreadMessage(window->display->thread_id, WM_DESTROY_WINDOW_MARSHAL, 0, (LPARAM)&params)) {
		DWORD error = GetLastError();
		debug_log("window_deferred_destroy: PostThreadMessage FAILED, error=%lu", error);
		debug_log_error("window_deferred_destroy: PostThreadMessage FAILED");
		CloseHandle(params.completion_event);
		// Fall back to direct destruction
		window_deferred_destroy_internal(window);
		return;
	}
	
	// Wait for destruction to complete
	debug_log("window_deferred_destroy: Waiting for completion...");
	WaitForSingleObject(params.completion_event, INFINITE);
	CloseHandle(params.completion_event);
	
	debug_log("window_deferred_destroy: Completed");
}

void
window_set_user_data(struct window *window, void *data)
{
	window->user_data = data;
}

void *
window_get_user_data(struct window *window)
{
	return window->user_data;
}

void
window_get_decorator_size(struct window *window, int *borderWidth, int *tabHeight)
{
	RECT window_rect, client_rect;
	GetWindowRect(window->hwnd, &window_rect);
	GetClientRect(window->hwnd, &client_rect);
	
	if (borderWidth) {
		int border = ((window_rect.right - window_rect.left) - client_rect.right) / 2;
		*borderWidth = border;
	}
	
	if (tabHeight) {
		int total_border_height = (window_rect.bottom - window_rect.top) - client_rect.bottom;
		int border = ((window_rect.right - window_rect.left) - client_rect.right) / 2;
		*tabHeight = total_border_height - border;
	}
}

struct widget *
window_add_widget(struct window *window, void *data)
{
	debug_log("window_add_widget: window=%p, data=%p", window, data);

	if (window && window->widget) {
		/* Keep add-widget idempotent: update user data and return existing widget. */
		window->widget->user_data = data;
		return window->widget;
	}
	
	struct widget *widget = calloc(1, sizeof(*widget));
	if (!widget) {
		debug_log_error("window_add_widget: calloc FAILED");
		return NULL;
	}
	
	widget->window = window;
	widget->user_data = data;
	widget->allocation.width = window->width;
	widget->allocation.height = window->height;
	widget->cursor = 1;
	
	/* Don't create bitmap/hdc here - let widget_cairo_create handle it
	 * This ensures we use a DIB section that Cairo can render to */
	
	window->widget = widget;
	
	debug_log("window_add_widget: Created widget %p, size=%dx%d", 
		widget, widget->allocation.width, widget->allocation.height);
	
	return widget;
}

void
widget_deferred_destroy(struct widget *widget)
{
	if (!widget)
		return;
	
	if (widget->surface) {
		cairo_surface_destroy(widget->surface);
	}
	
	if (widget->bitmap) {
		DeleteObject(widget->bitmap);
	}
	
	if (widget->hdc) {
		DeleteDC(widget->hdc);
	}
	
	free(widget);
}

void
widget_set_redraw_handler(struct widget *widget, widget_redraw_handler_t handler)
{
	widget->redraw_handler = handler;
}

void
widget_set_resize_handler(struct widget *widget, widget_resize_handler_t handler)
{
	widget->resize_handler = handler;
}

void
widget_set_button_handler(struct widget *widget, widget_button_handler_t handler)
{
	widget->button_handler = handler;
}

void
widget_set_motion_handler(struct widget *widget, widget_motion_handler_t handler)
{
	widget->motion_handler = handler;
}

void
widget_set_axis_handler(struct widget *widget, widget_axis_handler_t handler)
{
	widget->axis_handler = handler;
}

void
widget_set_idle_handler(struct widget *widget, widget_idle_handler_t handler)
{
	widget->idle_handler = handler;
}

void
widget_get_allocation(struct widget *widget, struct rectangle *allocation)
{
	*allocation = widget->allocation;
}

void
widget_set_user_data(struct widget *widget, void *data)
{
	if (widget)
		widget->user_data = data;
}

void
widget_set_allocation(struct widget *widget, int32_t x, int32_t y,
		      int32_t width, int32_t height)
{
	debug_log("widget_set_allocation: widget=%p, x=%d, y=%d, w=%d, h=%d",
		widget, x, y, width, height);
	widget->allocation.x = x;
	widget->allocation.y = y;
	widget->allocation.width = width;
	widget->allocation.height = height;
}

cairo_t *
widget_cairo_create(struct widget *widget)
{
	int width = widget->allocation.width;
	int height = widget->allocation.height;
	
	debug_log("widget_cairo_create: widget=%p, allocation=%dx%d, surface=%p (size %dx%d)", 
		widget, width, height, widget->surface, widget->surface_width, widget->surface_height);
	
	/* Recreate surface if size changed */
	if (!widget->surface || widget->surface_width != width || widget->surface_height != height) {
		debug_log("widget_cairo_create: Creating new surface for size %dx%d", width, height);
		if (widget->surface) {
			cairo_surface_destroy(widget->surface);
			widget->surface = NULL;
		}
		if (widget->bitmap) {
			DeleteObject(widget->bitmap);
			widget->bitmap = NULL;
		}
		if (widget->hdc) {
			DeleteDC(widget->hdc);
			widget->hdc = NULL;
		}
		
		/* Create DIB section for Cairo */
		widget->hdc = CreateCompatibleDC(NULL);
		
		BITMAPINFO bmi = {0};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height;  /* Negative for top-down */
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		
		widget->bitmap = CreateDIBSection(widget->hdc, &bmi, DIB_RGB_COLORS,
		                                   &widget->bitmap_data, NULL, 0);
		SelectObject(widget->hdc, widget->bitmap);
		
		/* Create Cairo image surface using the DIB's pixel data directly
		 * This ensures Cairo draws to the same memory that BitBlt will display */
		int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
		widget->surface = cairo_image_surface_create_for_data(
			(unsigned char*)widget->bitmap_data,
			CAIRO_FORMAT_ARGB32,
			width,
			height,
			stride);
		
		widget->surface_width = width;
		widget->surface_height = height;
		
		debug_log("widget_cairo_create: Created surface=%p, hdc=%p, bitmap=%p, data=%p, stride=%d", 
			widget->surface, widget->hdc, widget->bitmap, widget->bitmap_data, stride);
	}
	
	cairo_t* cr = cairo_create(widget->surface);
	debug_log("widget_cairo_create: Returning cairo_t=%p", cr);
	return cr;
}

struct window *
widget_get_window(struct widget *widget)
{
	return widget->window;
}

void
window_get_mouse_position(struct window *window, int32_t *x, int32_t *y)
{
	if (x) *x = window->mouse_x;
	if (y) *y = window->mouse_y;
}

void
widget_schedule_redraw(struct widget *widget)
{
	if (widget && widget->window) {
		InvalidateRect(widget->window->hwnd, NULL, FALSE);
	}
}

void *
window_get_move_handler_data(struct window *window)
{
	return (void*)window->move_handler;
}

void *
window_get_move_user_data(struct window *window)
{
	return window->move_user_data;
}

void *
window_get_focus_handler_data(struct window *window)
{
	return (void*)window->focus_handler;
}

void *
window_get_focus_user_data(struct window *window)
{
	return window->focus_user_data;
}

/* Clipboard functions */

int
display_set_clipboard_text(struct display *display, const char *text, size_t length)
{
	if (!text || !OpenClipboard(NULL))
		return -1;
	
	EmptyClipboard();
	
	/* Convert UTF-8 to UTF-16 */
	int wlen = MultiByteToWideChar(CP_UTF8, 0, text, length, NULL, 0);
	if (wlen <= 0) {
		CloseClipboard();
		return -1;
	}
	
	HGLOBAL hglob = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
	if (!hglob) {
		CloseClipboard();
		return -1;
	}
	
	wchar_t *wtext = (wchar_t*)GlobalLock(hglob);
	MultiByteToWideChar(CP_UTF8, 0, text, length, wtext, wlen);
	wtext[wlen] = 0;
	GlobalUnlock(hglob);
	
	SetClipboardData(CF_UNICODETEXT, hglob);
	CloseClipboard();
	
	return 0;
}

char *
display_get_clipboard_text(struct display *display, size_t *length)
{
	if (!OpenClipboard(NULL))
		return NULL;
	
	HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
	if (!hdata) {
		CloseClipboard();
		return NULL;
	}
	
	wchar_t *wtext = (wchar_t*)GlobalLock(hdata);
	if (!wtext) {
		CloseClipboard();
		return NULL;
	}
	
	/* Convert UTF-16 to UTF-8 */
	int len = WideCharToMultiByte(CP_UTF8, 0, wtext, -1, NULL, 0, NULL, NULL);
	if (len <= 0) {
		GlobalUnlock(hdata);
		CloseClipboard();
		return NULL;
	}
	
	char *text = malloc(len);
	if (!text) {
		GlobalUnlock(hdata);
		CloseClipboard();
		return NULL;
	}
	
	WideCharToMultiByte(CP_UTF8, 0, wtext, -1, text, len, NULL, NULL);
	if (length)
		*length = len - 1;  /* Exclude null terminator */
	
	GlobalUnlock(hdata);
	CloseClipboard();
	
	return text;
}

/* Set window icon from the current executable */
void
window_set_icon_from_exe(struct window *window)
{
	if (!window || !window->hwnd)
		return;
	
	// Get the path to the current executable
	wchar_t exePath[MAX_PATH];
	DWORD pathLen = GetModuleFileNameW(NULL, exePath, MAX_PATH);
	if (pathLen == 0 || pathLen >= MAX_PATH) {
		debug_log("window_set_icon_from_exe: Failed to get executable path");
		return;
	}
	
	debug_log("window_set_icon_from_exe: Executable path obtained");
	
	// Extract icons from the executable
	// Try to load the first icon resource from the exe
	HICON hIconLarge = NULL;
	HICON hIconSmall = NULL;
	
	// Method 1: Try ExtractIconEx first (works for icons embedded in resources)
	UINT iconCount = ExtractIconExW(exePath, 0, &hIconLarge, &hIconSmall, 1);
	
	if (iconCount == 0 || (!hIconLarge && !hIconSmall)) {
		debug_log("window_set_icon_from_exe: ExtractIconEx found no icons, trying LoadImage");
		
		// Method 2: Try LoadImage with the executable module
		HMODULE hModule = GetModuleHandleW(NULL);
		if (hModule) {
			// Try to load icon with resource ID 1 (common default)
			hIconLarge = (HICON)LoadImageW(hModule, MAKEINTRESOURCEW(1), IMAGE_ICON,
				GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
			hIconSmall = (HICON)LoadImageW(hModule, MAKEINTRESOURCEW(1), IMAGE_ICON,
				GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		}
	}
	
	// Set the icons for the window if we found any
	if (hIconLarge) {
		SendMessageW(window->hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
		debug_log("window_set_icon_from_exe: Set large icon");
	}
	
	if (hIconSmall) {
		SendMessageW(window->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
		debug_log("window_set_icon_from_exe: Set small icon");
	}
	
	if (hIconLarge || hIconSmall) {
		debug_log("window_set_icon_from_exe: Icon(s) successfully set for window");
	} else {
		debug_log("window_set_icon_from_exe: No icons found in executable");
	}
	
	// Note: We don't destroy the icons because Windows needs them while the window exists
	// They will be cleaned up when the window is destroyed
}

