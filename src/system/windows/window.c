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
#include <windows.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <xkbcommon/xkbcommon.h>

#include "window.h"

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
	
	bool deferred_destroy;
	bool need_redraw;
	bool is_popup;  /* True for popup windows (menus, tooltips) */
	bool is_tooltip;  /* True specifically for tooltip windows */
};

struct display {
	HINSTANCE hinstance;
	ATOM window_class_atom;
	
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
	
	struct window *windows[MAX_WINDOWS];
	int num_windows;
	
	bool running;
	bool exit_requested;
	
	/* Idle detection for tooltips */
	struct window *last_motion_window;
	struct widget *last_motion_widget;
	int last_motion_x, last_motion_y;
	DWORD last_motion_time;
	bool idle_fired;
};

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
	if (display->num_windows < MAX_WINDOWS) {
		display->windows[display->num_windows++] = window;
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

/* Convert Win32 virtual key code to xkb keycode */
static uint32_t
vkey_to_xkb_keycode(WPARAM vkey)
{
	/* This is a simplified mapping - a full implementation would need
	 * a complete mapping table similar to X11's keycode mapping */
	if (vkey >= 'A' && vkey <= 'Z')
		return vkey - 'A' + 38;  /* A-Z map to keycodes 38-63 */
	if (vkey >= '0' && vkey <= '9')
		return vkey - '0' + 19;  /* 0-9 map to keycodes 19-28 */
	
	/* Special keys */
	switch (vkey) {
		case VK_RETURN: return 36;
		case VK_ESCAPE: return 9;
		case VK_BACK: return 22;
		case VK_TAB: return 23;
		case VK_SPACE: return 65;
		case VK_SHIFT: return 50;
		case VK_CONTROL: return 37;
		case VK_MENU: return 64;  /* ALT key */
		case VK_LEFT: return 113;
		case VK_UP: return 111;
		case VK_RIGHT: return 114;
		case VK_DOWN: return 116;
		default: return vkey + 8;  /* Rough approximation */
	}
}

/* Window procedure */
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
			return 0;
		
		case WM_DESTROY:
			if (window) {
				window->deferred_destroy = true;
			}
			return 0;
		
		case WM_PAINT:
		{
			if (window && window->widget) {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				
				/* Trigger redraw if handler is set */
				if (window->widget->redraw_handler) {
					window->widget->redraw_handler(window->widget, window->widget->user_data);
				}
				
				/* Blit the off-screen bitmap to the window */
				if (window->widget->hdc && window->widget->bitmap) {
					HDC window_dc = GetDC(hwnd);
					BitBlt(window_dc, 0, 0, window->width, window->height,
					       window->widget->hdc, 0, 0, SRCCOPY);
					ReleaseDC(hwnd, window_dc);
				}
				
				EndPaint(hwnd, &ps);
			}
			return 0;
		}
		
		case WM_SIZE:
		{
			if (window) {
				int width = LOWORD(lParam);
				int height = HIWORD(lParam);
				
				if (width != window->width || height != window->height) {
					window->width = width;
					window->height = height;
					
					if (window->resize_handler) {
						window->resize_handler(window->widget, width, height, window->user_data);
					}
					
					if (window->widget && window->widget->resize_handler) {
						window->widget->allocation.width = width;
						window->widget->allocation.height = height;
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
		
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		{
			if (window && window->widget && window->widget->button_handler) {
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);
				uint32_t button = (msg == WM_LBUTTONDOWN) ? 1 : (msg == WM_RBUTTONDOWN) ? 3 : 2;
				uint32_t time = GetTickCount();
				
				window->mouse_x = x;
				window->mouse_y = y;
				
				window->widget->button_handler(window->widget, NULL, time, button, 1,
				                                 window->widget->user_data);
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
				
				window->widget->button_handler(window->widget, NULL, time, button, 0,
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
				
				window->widget->motion_handler(window->widget, NULL, time, (float)x, (float)y,
				                                window->widget->user_data);
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
				
				window->widget->axis_handler(window->widget, NULL, time, 0, value,
				                             window->widget->user_data);
			}
			return 0;
		}
		
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (window && window->key_handler) {
				uint32_t key = vkey_to_xkb_keycode(wParam);
				uint32_t time = GetTickCount();
				enum xkb_key_direction state = (msg == WM_KEYDOWN) ? 
				                                XKB_KEY_DOWN : XKB_KEY_UP;
				
				/* Try to get the unicode character */
				BYTE keyboard_state[256];
				WCHAR unicode_char[2] = {0};
				GetKeyboardState(keyboard_state);
				int result = ToUnicode((UINT)wParam, MapVirtualKey((UINT)wParam, MAPVK_VK_TO_VSC),
				                       keyboard_state, unicode_char, 2, 0);
				uint32_t unicode = (result > 0) ? unicode_char[0] : 0;
				
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
	WNDCLASSEXW wc = {0};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = window_proc;
	wc.cbWndExtra = sizeof(void*);  /* Space for window pointer */
	wc.hInstance = display->hinstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = WINDOW_CLASS_NAME;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	
	display->window_class_atom = RegisterClassExW(&wc);
	return (display->window_class_atom != 0);
}

/* Initialize xkbcommon for keyboard handling */
static bool
init_xkb(struct display *display)
{
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
	
	return true;
}

struct display *
display_create(int *argc, char **argv)
{
	struct display *display = calloc(1, sizeof(*display));
	if (!display)
		return NULL;
	
	display->hinstance = GetModuleHandle(NULL);
	
	if (!register_window_class(display)) {
		fprintf(stderr, "Failed to register window class\n");
		free(display);
		return NULL;
	}
	
	if (!init_xkb(display)) {
		free(display);
		return NULL;
	}
	
	display->running = false;
	display->exit_requested = false;
	
	return display;
}

void
display_run(struct display *display)
{
	MSG msg;
	
	display->running = true;
	display->exit_requested = false;
	
	while (display->running && !display->exit_requested) {
		/* Process all pending Windows messages */
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				display->running = false;
				break;
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
				window_deferred_destroy(window);
			}
		}
		
		/* Sleep briefly to avoid hogging CPU */
		if (display->num_windows == 0) {
			Sleep(10);
		} else {
			/* Use MsgWaitForMultipleObjects for efficient waiting */
			MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
		}
	}
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

struct window *
window_create(struct display *display)
{
	struct window *window = calloc(1, sizeof(*window));
	if (!window)
		return NULL;
	
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
	
	window->hwnd = CreateWindowExW(
		exStyle,
		WINDOW_CLASS_NAME,
		L"Cosmoe Window",
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window->width, window->height,
		NULL, NULL,
		display->hinstance,
		window  /* Pass window pointer through lpParam */
	);
	
	if (!window->hwnd) {
		free(window);
		return NULL;
	}
	
	window->hdc = GetDC(window->hwnd);
	
	display_add_window(display, window);
	
	return window;
}

struct window *
window_popup_create(struct display *display, struct window *parent_window, int x, int y)
{
	struct window *window = calloc(1, sizeof(*window));
	if (!window)
		return NULL;
	
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
	
	return window;
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
	SetWindowPos(window->hwnd, NULL, x, y, 0, 0, 
	            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	window->x = x;
	window->y = y;
}

void
window_set_title(struct window *window, const char *title)
{
	if (window->title)
		free(window->title);
	window->title = title ? strdup(title) : NULL;
	
	if (title) {
		/* Convert UTF-8 to UTF-16 */
		int len = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
		if (len > 0) {
			wchar_t *wtitle = malloc(len * sizeof(wchar_t));
			if (wtitle) {
				MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, len);
				SetWindowTextW(window->hwnd, wtitle);
				free(wtitle);
			}
		}
	}
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
	window->width = width;
	window->height = height;
	
	/* Adjust for window decorations */
	RECT rect = { 0, 0, width, height };
	DWORD style = GetWindowLong(window->hwnd, GWL_STYLE);
	DWORD exStyle = GetWindowLong(window->hwnd, GWL_EXSTYLE);
	AdjustWindowRectEx(&rect, style, FALSE, exStyle);
	
	SetWindowPos(window->hwnd, NULL, 0, 0,
	            rect.right - rect.left, rect.bottom - rect.top,
	            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	
	/* Show the window if it's not mapped yet */
	if (!window->mapped) {
		ShowWindow(window->hwnd, SW_SHOW);
		UpdateWindow(window->hwnd);
		window->mapped = true;
	}
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

void
window_deferred_destroy(struct window *window)
{
	if (!window)
		return;
	
	if (window->widget) {
		widget_deferred_destroy(window->widget);
	}
	
	if (window->hdc) {
		ReleaseDC(window->hwnd, window->hdc);
	}
	
	if (window->hwnd) {
		DestroyWindow(window->hwnd);
	}
	
	if (window->title) {
		free(window->title);
	}
	
	display_remove_window(window->display, window);
	free(window);
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
	struct widget *widget = calloc(1, sizeof(*widget));
	if (!widget)
		return NULL;
	
	widget->window = window;
	widget->user_data = data;
	widget->allocation.width = window->width;
	widget->allocation.height = window->height;
	
	window->widget = widget;
	
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

cairo_t *
widget_cairo_create(struct widget *widget)
{
	int width = widget->allocation.width;
	int height = widget->allocation.height;
	
	/* Recreate surface if size changed */
	if (!widget->surface || widget->surface_width != width || widget->surface_height != height) {
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
		
		/* Create Cairo surface from the DIB */
		widget->surface = cairo_win32_surface_create(widget->hdc);
		widget->surface_width = width;
		widget->surface_height = height;
	}
	
	return cairo_create(widget->surface);
}

void *
widget_get_user_data(struct widget *widget)
{
	return widget->user_data;
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
