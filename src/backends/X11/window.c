/*
 * Copyright © 2025-2026, Bill Hayden
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
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib-xcb.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

#include <cairo.h>
#include <cairo-xlib.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <png.h>
#include <ctype.h>


#include <OS.h>  /* For port APIs */

#include "window.h"
#include "ServerProtocol.h"  /* Backend protocol message codes */

/* Implemented in X11Backend.cpp and exported with C linkage. */
extern void x11_process_backend_messages(int32_t backend_port, int32_t app_port);

//#define DEBUG
#ifdef DEBUG
#define X11_LOG(...) fprintf(stderr, __VA_ARGS__)
#define X11_FLUSH() fflush(stderr)
#else
#define X11_LOG(...) do {} while (0)
#define X11_FLUSH() do {} while (0)
#endif

struct message_header {
	int32_t size;
	uint32_t code;
	uint32_t flags;
};

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
#define CUSTOM_CURSOR_BASE 1000
#define MAX_CUSTOM_CURSORS 256

static Cursor s_custom_cursors[MAX_CUSTOM_CURSORS];
static Display* s_custom_cursor_display;

struct widget {
	struct window *window;
	void *user_data;
	struct rectangle allocation;
	cairo_surface_t *surface;
	Pixmap pixmap;       /* Off-screen buffer for double buffering */
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
	Window xwindow;
	GC gc;           /* Graphics context for copying pixmap to window */
	struct widget *widget;
	void *user_data;
	
	char *title;
	int width, height;
	int x, y;
	int min_width, min_height;
	int max_width, max_height;
	bool mapped;
	bool hidden;  /* True = intentionally hidden via window_hide() */
	
	/* Mouse position tracking for button events */
	int mouse_x, mouse_y;
	void (*move_handler)(struct window* window, int x, int y, void* user_data);
	void *move_user_data;
	widget_resize_handler_t resize_handler;		// In X11, the window itself needs a resize handler, as there is no windowframe widget
	window_key_handler_t key_handler;
	window_close_handler_t close_handler;
	void (*focus_handler)(struct window* window, bool focused, void* user_data);
	void *focus_user_data;
	
	bool deferred_destroy;
	bool need_redraw;
	bool is_popup;  /* True for override-redirect popup windows (menus, tooltips) */
	bool is_tooltip;  /* True specifically for tooltip windows (subset of is_popup) */

	int32_t token;  /* BWindow object token for PortLink window identification */
};

struct display {
	Display *xdisplay;
	xcb_connection_t *xcb_conn;
	int screen;
	Visual *visual;
	Colormap colormap;
	
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
	int32_t xkb_device_id;
	
	Atom wm_protocols;
	Atom wm_delete_window;
	Atom wm_state;
	Atom utf8_string;
	Atom net_wm_name;
	Atom net_wm_state;
	Atom net_client_list;
	Atom net_wm_pid;
	Atom net_wm_window_type;
	Atom net_wm_window_type_normal;
	Atom net_wm_state_skip_taskbar;
	Atom net_wm_state_maximized_vert;
	Atom net_wm_state_maximized_horz;
	Atom net_wm_state_modal;
	
	struct window *windows[MAX_WINDOWS];
	int num_windows;
	
	bool running;
	bool exit_requested;
	
	/* PortLink communication */
	int32_t backend_port;
	int32_t app_port;

	/* Idle detection for tooltips */
	struct window *last_motion_window;
	struct widget *last_motion_widget;
	int last_motion_x, last_motion_y;
	struct timespec last_motion_time;
	bool idle_fired;

	/* Clipboard request completion is observed only by the display loop. */
	pthread_mutex_t clipboard_state_lock;
	bool clipboard_request_pending;
	bool clipboard_response_ready;
	bool clipboard_response_success;
	Window clipboard_requestor;
	Atom clipboard_request_selection;
	Atom clipboard_request_property;

	display_app_watcher_t app_watcher;
	void *app_watcher_user_data;
	int32_t *watched_app_teams;
	int32_t watched_app_team_count;
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

/* Helper function to find window by X11 Window ID */
static struct window *
display_find_window(struct display *display, Window xwindow)
{
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->xwindow == xwindow) {
			return display->windows[i];
		}
	}
	/* Log when a button/motion event misses - show registered xwindows */
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
			for (int j = i; j < display->num_windows - 1; j++) {
				display->windows[j] = display->windows[j + 1];
			}
			display->num_windows--;
			break;
		}
	}
}

static bool
team_list_contains(const int32_t *team_ids, int32_t count, int32_t team_id)
{
	for (int32_t i = 0; i < count; i++) {
		if (team_ids[i] == team_id)
			return true;
	}

	return false;
}


static int32_t
display_collect_app_list(struct display *display, int32_t **_team_ids)
{
	if (_team_ids == NULL)
		return 0;

	*_team_ids = NULL;
	if (display == NULL || display->xdisplay == NULL)
		return 0;

	Display *xdisplay = display->xdisplay;
	Window root = RootWindow(xdisplay, display->screen);
	Atom actual_type;
	int actual_format;
	unsigned long nitems = 0;
	unsigned long bytes_after = 0;
	unsigned char *client_data = NULL;

	if (XGetWindowProperty(xdisplay, root, display->net_client_list, 0, (~0L),
			False, AnyPropertyType, &actual_type, &actual_format, &nitems,
			&bytes_after, &client_data) != Success || client_data == NULL) {
		return 0;
	}

	if (actual_format != 32) {
		XFree(client_data);
		return 0;
	}

	int32_t capacity = (int32_t)nitems;
	if (capacity < 1)
		capacity = 1;

	int32_t *team_ids = calloc(capacity, sizeof(int32_t));
	if (team_ids == NULL) {
		XFree(client_data);
		return 0;
	}

	Window *windows = (Window *)client_data;
	int32_t count = 0;

	for (unsigned long i = 0; i < nitems; i++) {
		Window win = windows[i];
		unsigned char *prop = NULL;
		unsigned long prop_items = 0;
		unsigned long prop_bytes_after = 0;
		Atom prop_type;
		int prop_format;

		if (XGetWindowProperty(xdisplay, win, display->wm_state, 0, 2, False,
				AnyPropertyType, &prop_type, &prop_format, &prop_items,
				&prop_bytes_after, &prop) != Success || prop == NULL) {
			continue;
		}
		XFree(prop);

		XWindowAttributes attrs;
		if (!XGetWindowAttributes(xdisplay, win, &attrs)
			|| attrs.override_redirect) {
			continue;
		}

		prop = NULL;
		prop_items = 0;
		if (XGetWindowProperty(xdisplay, win, display->net_wm_state, 0, (~0L),
				False, AnyPropertyType, &prop_type, &prop_format, &prop_items,
				&prop_bytes_after, &prop) == Success && prop != NULL) {
			Atom *states = (Atom *)prop;
			bool skip_taskbar = false;
			for (unsigned long s = 0; s < prop_items; s++) {
				if (states[s] == display->net_wm_state_skip_taskbar) {
					skip_taskbar = true;
					break;
				}
			}
			XFree(prop);
			if (skip_taskbar)
				continue;
		}

		prop = NULL;
		prop_items = 0;
		if (XGetWindowProperty(xdisplay, win, display->net_wm_window_type, 0,
				(~0L), False, AnyPropertyType, &prop_type, &prop_format,
				&prop_items, &prop_bytes_after, &prop) != Success || prop == NULL) {
			continue;
		}

		Atom *types = (Atom *)prop;
		bool is_normal = false;
		for (unsigned long t = 0; t < prop_items; t++) {
			if (types[t] == display->net_wm_window_type_normal) {
				is_normal = true;
				break;
			}
		}
		XFree(prop);
		if (!is_normal)
			continue;

		Window transient_for = None;
		if (XGetTransientForHint(xdisplay, win, &transient_for)
			&& transient_for != None) {
			continue;
		}

		prop = NULL;
		prop_items = 0;
		if (XGetWindowProperty(xdisplay, win, display->net_wm_pid, 0, 1, False,
				AnyPropertyType, &prop_type, &prop_format, &prop_items,
				&prop_bytes_after, &prop) != Success || prop == NULL
			|| prop_items < 1) {
			if (prop != NULL)
				XFree(prop);
			continue;
		}

		int32_t team_id = (int32_t)(*((unsigned long *)prop));
		XFree(prop);

		if (team_list_contains(team_ids, count, team_id))
			continue;

		team_ids[count++] = team_id;
	}

	XFree(client_data);
	*_team_ids = team_ids;
	return count;
}


static void
display_dispatch_app_list_changes(struct display *display)
{
	if (display == NULL || display->app_watcher == NULL)
		return;

	int32_t *team_ids = NULL;
	int32_t count = display_collect_app_list(display, &team_ids);

	for (int32_t i = 0; i < count; i++) {
		if (!team_list_contains(display->watched_app_teams,
				display->watched_app_team_count, team_ids[i])) {
			display->app_watcher(display, DISPLAY_APP_WATCH_LAUNCHED,
				team_ids[i], display->app_watcher_user_data);
		}
	}

	for (int32_t i = 0; i < display->watched_app_team_count; i++) {
		if (!team_list_contains(team_ids, count, display->watched_app_teams[i])) {
			display->app_watcher(display, DISPLAY_APP_WATCH_QUIT,
				display->watched_app_teams[i], display->app_watcher_user_data);
		}
	}

	free(display->watched_app_teams);
	display->watched_app_teams = team_ids;
	display->watched_app_team_count = count;
}

/* Display functions */

struct display *
display_create(int *argc, char **argv)
{
	struct display *display;
	
	display = calloc(1, sizeof *display);
	if (!display)
		return NULL;
	
	display->xdisplay = XOpenDisplay(NULL);
	if (!display->xdisplay) {
		fprintf(stderr, "Failed to open X display\n");
		free(display);
		return NULL;
	}

	s_custom_cursor_display = display->xdisplay;
	for (int i = 0; i < MAX_CUSTOM_CURSORS; i++)
		s_custom_cursors[i] = None;
	
	display->screen = DefaultScreen(display->xdisplay);
	display->visual = DefaultVisual(display->xdisplay, display->screen);
	display->colormap = DefaultColormap(display->xdisplay, display->screen);
	
	/* Get XCB connection for XKB */
	display->xcb_conn = XGetXCBConnection(display->xdisplay);
	if (!display->xcb_conn) {
		fprintf(stderr, "Failed to get XCB connection\n");
		XCloseDisplay(display->xdisplay);
		free(display);
		return NULL;
	}
	
	/* Initialize XKB */
	display->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!display->xkb_context) {
		fprintf(stderr, "Failed to create XKB context\n");
		XCloseDisplay(display->xdisplay);
		free(display);
		return NULL;
	}
	
	display->xkb_device_id = xkb_x11_get_core_keyboard_device_id(display->xcb_conn);
	if (display->xkb_device_id == -1) {
		fprintf(stderr, "Failed to get XKB device ID\n");
		xkb_context_unref(display->xkb_context);
		XCloseDisplay(display->xdisplay);
		free(display);
		return NULL;
	}
	
	display->xkb_keymap = xkb_x11_keymap_new_from_device(display->xkb_context,
							      display->xcb_conn,
							      display->xkb_device_id,
							      XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!display->xkb_keymap) {
		fprintf(stderr, "Failed to create XKB keymap\n");
		xkb_context_unref(display->xkb_context);
		XCloseDisplay(display->xdisplay);
		free(display);
		return NULL;
	}
	
	display->xkb_state = xkb_x11_state_new_from_device(display->xkb_keymap,
							    display->xcb_conn,
							    display->xkb_device_id);
	if (!display->xkb_state) {
		fprintf(stderr, "Failed to create XKB state\n");
		xkb_keymap_unref(display->xkb_keymap);
		xkb_context_unref(display->xkb_context);
		XCloseDisplay(display->xdisplay);
		free(display);
		return NULL;
	}
	
	/* Get X11 atoms */
	display->wm_protocols = XInternAtom(display->xdisplay, "WM_PROTOCOLS", False);
	display->wm_delete_window = XInternAtom(display->xdisplay, "WM_DELETE_WINDOW", False);
	display->wm_state = XInternAtom(display->xdisplay, "WM_STATE", False);
	display->utf8_string = XInternAtom(display->xdisplay, "UTF8_STRING", False);
	display->net_wm_name = XInternAtom(display->xdisplay, "_NET_WM_NAME", False);
	display->net_wm_state = XInternAtom(display->xdisplay, "_NET_WM_STATE", False);
	display->net_client_list = XInternAtom(display->xdisplay,
		"_NET_CLIENT_LIST", False);
	display->net_wm_pid = XInternAtom(display->xdisplay, "_NET_WM_PID", False);
	display->net_wm_window_type = XInternAtom(display->xdisplay,
		"_NET_WM_WINDOW_TYPE", False);
	display->net_wm_window_type_normal = XInternAtom(display->xdisplay,
		"_NET_WM_WINDOW_TYPE_NORMAL", False);
	display->net_wm_state_skip_taskbar = XInternAtom(display->xdisplay,
		"_NET_WM_STATE_SKIP_TASKBAR", False);
	display->net_wm_state_maximized_vert = XInternAtom(display->xdisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	display->net_wm_state_maximized_horz = XInternAtom(display->xdisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	display->net_wm_state_modal = XInternAtom(display->xdisplay, "_NET_WM_STATE_MODAL", False);

	Window root = RootWindow(display->xdisplay, display->screen);
	XWindowAttributes root_attributes;
	long event_mask = PropertyChangeMask;
	if (XGetWindowAttributes(display->xdisplay, root, &root_attributes))
		event_mask |= root_attributes.your_event_mask;
	XSelectInput(display->xdisplay, root, event_mask);

	pthread_mutex_init(&display->clipboard_state_lock, NULL);
	display->clipboard_request_pending = false;
	display->clipboard_response_ready = false;
	display->clipboard_response_success = false;
	display->clipboard_requestor = None;
	display->clipboard_request_selection = None;
	display->clipboard_request_property = None;
	
	display->running = false;
	display->exit_requested = false;
	display->num_windows = 0;
	display->backend_port = -1;  /* No port until set by BApplication */
	display->app_port = -1;  /* No port until set by BApplication */
	
	/* Initialize idle detection - set to current time so tooltip doesn't
	 * fire immediately on startup with uninitialized widget */
	clock_gettime(CLOCK_MONOTONIC, &display->last_motion_time);
	display->idle_fired = false;
	display->last_motion_widget = NULL;
	display->last_motion_window = NULL;
	
	return display;
}

static void
window_handle_configure_notify(struct window *window, XConfigureEvent *event)
{
	/*
	 * ConfigureNotify x/y can be parent-relative on reparenting WMs
	 * Translate to root coordinates so the stored frame origin
	 * remains stable during resize-only updates.
	 */
	int old_x = window->x;
	int old_y = window->y;
	int new_x = event->x;
	int new_y = event->y;
	Window child;
	if (XTranslateCoordinates(window->display->xdisplay, window->xwindow,
			RootWindow(window->display->xdisplay, window->display->screen),
			0, 0, &new_x, &new_y, &child)) {
		window->x = new_x;
		window->y = new_y;
	} else {
		window->x = event->x;
		window->y = event->y;
	}
	bool moved = (old_x != window->x) || (old_y != window->y);

	if (window->width != event->width || window->height != event->height) {
		int new_w = event->width;
		int new_h = event->height;

		/* enforce min/max */
		if (window->min_width > 0 && new_w < window->min_width) new_w = window->min_width;
		if (window->min_height > 0 && new_h < window->min_height) new_h = window->min_height;
		if (window->max_width > 0 && new_w > window->max_width) new_w = window->max_width;
		if (window->max_height > 0 && new_h > window->max_height) new_h = window->max_height;

		if (new_w != event->width || new_h != event->height) {
			/* ask X to resize back to allowed size */
			XResizeWindow(window->display->xdisplay, window->xwindow, new_w, new_h);
			XFlush(window->display->xdisplay);
			return; /* next ConfigureNotify will have the clamped size */
		}

		window->width = new_w;
		window->height = new_h;
		
		if (window->widget) {
			window->widget->allocation.width = new_w;
			window->widget->allocation.height = new_h;
			
			/* Grow surface in chunks to reduce reallocation frequency during interactive resizing */
			const int SURFACE_GROW_CHUNK = 100;  /* pixels to over-allocate */
			int needed_width = new_w;
			int needed_height = new_h;
			
			/* Only recreate surface if we need to grow it */
			if (needed_width > window->widget->surface_width || 
			    needed_height > window->widget->surface_height) {
				/* Grow only in the dimension(s) that need it, adding chunk to each */
				int new_width = needed_width > window->widget->surface_width ? 
					needed_width + SURFACE_GROW_CHUNK : window->widget->surface_width;
				int new_height = needed_height > window->widget->surface_height ? 
					needed_height + SURFACE_GROW_CHUNK : window->widget->surface_height;
				
				if (window->widget->surface) {
					cairo_surface_destroy(window->widget->surface);
				}
				if (window->widget->pixmap) {
					XFreePixmap(window->display->xdisplay, window->widget->pixmap);
				}
				
				/* Recreate pixmap at new size */
				window->widget->pixmap = XCreatePixmap(
					window->display->xdisplay,
					window->xwindow,
					new_width, new_height,
					DefaultDepth(window->display->xdisplay, window->display->screen));
				
				/* Recreate cairo surface on the new pixmap */
				window->widget->surface = cairo_xlib_surface_create(
					window->display->xdisplay,
					window->widget->pixmap,
					window->display->visual,
					new_width, new_height);
				
				/* Initialize new pixmap to background gray to avoid garbage display */
				cairo_t *init_cr = cairo_create(window->widget->surface);
				cairo_set_source_rgb(init_cr, 0.847, 0.847, 0.847);
				cairo_paint(init_cr);
				cairo_destroy(init_cr);
				cairo_surface_flush(window->widget->surface);
				
				window->widget->surface_width = new_width;
				window->widget->surface_height = new_height;
			} else {
				/* Surface is large enough, just update its drawable size */
				cairo_xlib_surface_set_size(window->widget->surface, 
					event->width, event->height);
			}
			
			/* Call resize handler if set */
			if (window->widget->resize_handler) {
				window->widget->resize_handler(window->widget,
					event->width, event->height,
					window->widget->user_data);
			}
			
			window->need_redraw = true;

			/* Maintain the 1-pixel difference between what BeOS/Haiku expects
			and what X11 expects regarding window size */
			if (window->resize_handler)
				window->resize_handler(NULL, window->width - 1, window->height - 1, window->user_data);
		}

	}

	/* Notify move handler if window position changed */
	if (moved && window->move_handler) {
		window->move_handler(window, window->x, window->y, window->move_user_data);
	}
}

void
window_get_position(struct window *window, int *x, int *y)
{
	if (!window) {
		if (x) *x = 0;
		if (y) *y = 0;
		return;
	}

	/* Translate window coordinates to root window coordinates */
	Window child;
	int root_x = 0, root_y = 0;
	if (XTranslateCoordinates(window->display->xdisplay, window->xwindow,
			RootWindow(window->display->xdisplay, window->display->screen),
			0, 0, &root_x, &root_y, &child)) {
		if (x) *x = root_x;
		if (y) *y = root_y;
	} else {
		/* Fall back to stored coordinates if translation fails */
		if (x) *x = window->x;
		if (y) *y = window->y;
	}
}

void
window_set_position(struct window *window, int x, int y)
{
	if (!window)
		return;

	Display *xdisplay = window->display->xdisplay;
	if (!xdisplay)
		return;

	Window root, parent;
	Window *children;
	unsigned int nchildren;

	XQueryTree(xdisplay, window->xwindow, &root, &parent, &children, &nchildren);
	if (children) XFree(children);
	
	/* Request the X server to move the window; the resulting ConfigureNotify
	 * will be processed by the event loop and notify any registered move
	 * handler. We update internal coords immediately to keep the stored
	 * state consistent. */
	XMoveWindow(xdisplay, window->xwindow, x, y);
	window->x = x;
	window->y = y;
}

static void
window_handle_expose(struct window *window, XExposeEvent *event)
{
	window->need_redraw = true;
}

static void
window_handle_key_press(struct window *window, XKeyEvent *event)
{
	if (!window->key_handler)
		return;
	
	/* Update XKB state so modifiers affect character output */
	xkb_state_update_key(window->display->xkb_state, event->keycode, XKB_KEY_DOWN);
	
	/* X11 keycodes have an offset of 8 compared to Linux input codes */
	xkb_keycode_t keycode = event->keycode - 8;
	
	char buf[32];
	int count = xkb_state_key_get_utf8(window->display->xkb_state, event->keycode, buf, sizeof(buf));
	uint32_t unicode = 0;
	if (count > 0) {
		buf[count] = '\0';
		/* Simple UTF-8 to unicode conversion for ASCII */
		unicode = (uint32_t)buf[0];
	}
	
	/* Pass adjusted keycode (scan code) and unicode character to match Wayland/Linux behavior */
	/* Normalize carriage return to line feed for B_ENTER */
	if (unicode == 13)
		unicode = 10;

	window->key_handler(window, NULL, event->time, keycode, unicode,
			    XKB_KEY_DOWN, window->user_data);
}

static void
window_handle_key_release(struct window *window, XKeyEvent *event)
{
	if (!window->key_handler)
		return;
	
	/* Update XKB state so modifiers are released */
	xkb_state_update_key(window->display->xkb_state, event->keycode, XKB_KEY_UP);
	
	/* X11 keycodes have an offset of 8 compared to Linux input codes */
	xkb_keycode_t keycode = event->keycode - 8;
	
	/* Pass key release event */
	window->key_handler(window, NULL, event->time, keycode, 0,
			    XKB_KEY_UP, window->user_data);
}

static void
window_handle_button_press(struct window *window, XButtonEvent *event)
{
	struct widget *widget = window->widget;
	
	X11_LOG("X11: button press - button=%d, x=%d, y=%d\n", event->button, event->x, event->y);
	
	/* Track mouse position */
	window->mouse_x = event->x;
	window->mouse_y = event->y;
	
	if (!widget || !widget->button_handler) {
		/* Handle scroll wheel events even without button handler */
		if (widget && widget->axis_handler && (event->button == 4 || event->button == 5)) {
			double value = (event->button == 4) ? -1.0 : 1.0;
			wl_fixed_t fixed_value = cosmoe_double_to_fixed(value);
			widget->axis_handler(widget, (struct input*)widget, event->time, 0, fixed_value, widget->user_data);
		}
		return;
	}
	
	/* Handle scroll wheel */
	if (widget->axis_handler && (event->button == 4 || event->button == 5)) {
		double value = (event->button == 4) ? -1.0 : 1.0;
		wl_fixed_t fixed_value = cosmoe_double_to_fixed(value);
		widget->axis_handler(widget, (struct input*)widget, event->time, 0, fixed_value, widget->user_data);
		return;
	}
	
	/* Convert X11 button codes (1,2,3) to Linux input codes (0x110,0x111,0x112) */
	uint32_t button_code;
	switch (event->button) {
		case 1: button_code = 0x110; break; /* BTN_LEFT */
		case 2: button_code = 0x112; break; /* BTN_MIDDLE */
		case 3: button_code = 0x111; break; /* BTN_RIGHT */
		default: button_code = event->button; break;
	}
	
	/* Regular button press - pass widget as input for position tracking */
	X11_LOG("X11: calling button_handler for press (converted button code %d->%d)\n", event->button, button_code);
	widget->button_handler(widget, (struct input*)widget, event->time, button_code, 1,
			       widget->user_data);
}

static void
window_handle_button_release(struct window *window, XButtonEvent *event)
{
	struct widget *widget = window->widget;
	
	X11_LOG("X11: button release - button=%d, x=%d, y=%d\n", event->button, event->x, event->y);
	
	/* Track mouse position */
	window->mouse_x = event->x;
	window->mouse_y = event->y;
	
	if (!widget || !widget->button_handler)
		return;
	
	/* Don't send release events for scroll wheel */
	if (event->button == 4 || event->button == 5)
		return;
	
	/* Convert X11 button codes (1,2,3) to Linux input codes (0x110,0x111,0x112) */
	uint32_t button_code;
	switch (event->button) {
		case 1: button_code = 0x110; break; /* BTN_LEFT */
		case 2: button_code = 0x112; break; /* BTN_MIDDLE */
		case 3: button_code = 0x111; break; /* BTN_RIGHT */
		default: button_code = event->button; break;
	}
	
	/* Pass widget as input for position tracking */
	widget->button_handler(widget, (struct input*)widget, event->time, button_code, 0,
			       widget->user_data);
}

static void
window_handle_motion_notify(struct window *window, XMotionEvent *event)
{
	struct widget *widget = window->widget;
	struct display *display = window->display;
	
	/* Track mouse position for button events */
	window->mouse_x = event->x;
	window->mouse_y = event->y;
	
	/* Tooltip windows should not process motion events at all.
	 * Even though we don't select PointerMotionMask for popups, X11 still delivers
	 * motion events to whatever window the pointer is over. Processing these events
	 * causes FindView() to fail (coords relative to tooltip, not main window) and
	 * generates spurious B_EXITED_VIEW events that immediately hide the tooltip.
	 * Menu popups DO need motion events for interaction, so only skip tooltips.
	 * Note: We also skip popups whose title hasn't been set yet, as tooltips may
	 * receive motion events before window_set_title is called. */
	if (window->is_tooltip) {
		return;
	}
	
	/* Reset idle detection only for non-popup windows.
	 * Popup windows (like menus/tooltips) shouldn't reset the idle timer. */
	if (!window->is_popup) {
		display->last_motion_window = window;
		display->last_motion_widget = widget;
		display->last_motion_x = event->x;
		display->last_motion_y = event->y;
		clock_gettime(CLOCK_MONOTONIC, &display->last_motion_time);
		display->idle_fired = false;
	}
	
	if (!widget || !widget->motion_handler)
		return;
	
	/* Pass widget as input for position tracking */
	int cursor = widget->motion_handler(widget, (struct input*)widget, event->time, (float)event->x, (float)event->y,
			       widget->user_data);

	/* Update cursor if changed */
	if (cursor != widget->cursor) {
		widget->cursor = cursor;
		
		Display *xdisplay = window->display->xdisplay;
		Window xwindow = window->xwindow;
		Cursor xcursor;

		if (cursor >= CUSTOM_CURSOR_BASE) {
			int index = cursor - CUSTOM_CURSOR_BASE;
			if (index >= 0 && index < MAX_CUSTOM_CURSORS
				&& s_custom_cursors[index] != None) {
				XDefineCursor(xdisplay, xwindow, s_custom_cursors[index]);
				XFlush(xdisplay);
				return;
			}
		}
		
		switch (cursor) {
			case B_CURSOR_ID_SYSTEM_DEFAULT:
				xcursor = XCreateFontCursor(xdisplay, XC_left_ptr);
				break;
			case B_CURSOR_ID_I_BEAM:
				xcursor = XCreateFontCursor(xdisplay, XC_xterm);
				break;
			case B_CURSOR_ID_I_BEAM_HORIZONTAL:
				xcursor = XCreateFontCursor(xdisplay, XC_xterm);  /* X11 doesn't have vertical I-beam */
				break;
			case B_CURSOR_ID_CROSS_HAIR:
				xcursor = XCreateFontCursor(xdisplay, XC_crosshair);
				break;
			case B_CURSOR_ID_FOLLOW_LINK:
				xcursor = XCreateFontCursor(xdisplay, XC_hand2);
				break;
			case B_CURSOR_ID_GRABBING:
			case B_CURSOR_ID_MOVE:
				xcursor = XCreateFontCursor(xdisplay, XC_fleur);
				break;
			case B_CURSOR_ID_GRAB:
				xcursor = XCreateFontCursor(xdisplay, XC_hand1);
				break;
			case B_CURSOR_ID_RESIZE_EAST_WEST:
				xcursor = XCreateFontCursor(xdisplay, XC_sb_h_double_arrow);
				break;
			case B_CURSOR_ID_RESIZE_NORTH_SOUTH:
				xcursor = XCreateFontCursor(xdisplay, XC_sb_v_double_arrow);
				break;
			case B_CURSOR_ID_RESIZE_EAST:
				xcursor = XCreateFontCursor(xdisplay, XC_right_side);
				break;
			case B_CURSOR_ID_RESIZE_WEST:
				xcursor = XCreateFontCursor(xdisplay, XC_left_side);
				break;
			case B_CURSOR_ID_RESIZE_NORTH:
				xcursor = XCreateFontCursor(xdisplay, XC_top_side);
				break;
			case B_CURSOR_ID_RESIZE_SOUTH:
				xcursor = XCreateFontCursor(xdisplay, XC_bottom_side);
				break;
			case B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST:
				xcursor = XCreateFontCursor(xdisplay, XC_bottom_left_corner);
				break;
			case B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST:
				xcursor = XCreateFontCursor(xdisplay, XC_top_left_corner);
				break;
			case B_CURSOR_ID_NOT_ALLOWED:
				xcursor = XCreateFontCursor(xdisplay, XC_pirate);
				break;
			case B_CURSOR_ID_NO_CURSOR:
				/* Create an invisible cursor */
				{
					Pixmap pixmap = XCreatePixmap(xdisplay, xwindow, 1, 1, 1);
					XColor color = {0};
					xcursor = XCreatePixmapCursor(xdisplay, pixmap, pixmap, &color, &color, 0, 0);
					XFreePixmap(xdisplay, pixmap);
				}
				break;
			case B_CURSOR_ID_PROGRESS:
				xcursor = XCreateFontCursor(xdisplay, XC_watch);
				break;
			case B_CURSOR_ID_CONTEXT_MENU:
				xcursor = XCreateFontCursor(xdisplay, XC_question_arrow);
				break;
			case B_CURSOR_ID_COPY:
				xcursor = XCreateFontCursor(xdisplay, XC_plus);
				break;
			default:
				xcursor = XCreateFontCursor(xdisplay, XC_left_ptr);
				break;
		}
		
		XDefineCursor(xdisplay, xwindow, xcursor);
		XFreeCursor(xdisplay, xcursor);
		XFlush(xdisplay);
	}
}

static void
window_handle_client_message(struct window *window, XClientMessageEvent *event)
{
	struct display *display = window->display;
	
	X11_LOG("X11: ClientMessage received, type=%ld\n", event->message_type);
	if (event->message_type == display->wm_protocols) {
		if ((Atom)event->data.l[0] == display->wm_delete_window) {
			X11_LOG("X11: WM_DELETE_WINDOW received, calling close handler\n");
			if (window->close_handler) {
				window->close_handler(window->user_data);
			} else {
				window->deferred_destroy = true;
			}
		}
	}
}


int32_t
window_create_custom_cursor(const uint8_t* bits, size_t bitsLength,
	int32_t width, int32_t height, int32_t bytesPerRow,
	int32_t colorSpace, int32_t hotX, int32_t hotY)
{
	(void)colorSpace;

	if (s_custom_cursor_display == NULL || bits == NULL
		|| width <= 0 || height <= 0 || bytesPerRow <= 0)
		return -1;
	if (hotX < 0 || hotY < 0 || hotX >= width || hotY >= height)
		return -1;

	size_t minSize = (size_t)bytesPerRow * (size_t)height;
	if (bitsLength < minSize)
		return -1;

	int freeIndex = -1;
	for (int i = 0; i < MAX_CUSTOM_CURSORS; i++) {
		if (s_custom_cursors[i] == None) {
			freeIndex = i;
			break;
		}
	}
	if (freeIndex < 0)
		return -1;

	XcursorImage* image = XcursorImageCreate(width, height);
	if (image == NULL)
		return -1;

	image->xhot = hotX;
	image->yhot = hotY;

	for (int32_t y = 0; y < height; y++) {
		const uint8_t* srcRow = bits + (size_t)y * (size_t)bytesPerRow;
		for (int32_t x = 0; x < width; x++) {
			const uint8_t* px = srcRow + (size_t)x * 4;
			uint32_t b = px[0];
			uint32_t g = px[1];
			uint32_t r = px[2];
			uint32_t a = px[3];
			image->pixels[(size_t)y * (size_t)width + (size_t)x]
				= (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

	Cursor cursor = XcursorImageLoadCursor(s_custom_cursor_display, image);
	XcursorImageDestroy(image);
	if (cursor == None)
		return -1;

	s_custom_cursors[freeIndex] = cursor;
	return CUSTOM_CURSOR_BASE + freeIndex;
}


int
window_delete_custom_cursor(int32_t cursorID)
{
	if (cursorID < CUSTOM_CURSOR_BASE || s_custom_cursor_display == NULL)
		return -1;

	int index = cursorID - CUSTOM_CURSOR_BASE;
	if (index < 0 || index >= MAX_CUSTOM_CURSORS)
		return -1;

	if (s_custom_cursors[index] != None) {
		XFreeCursor(s_custom_cursor_display, s_custom_cursors[index]);
		s_custom_cursors[index] = None;
	}

	return 0;
}

static void
display_process_pending_operations(struct display *display)
{
	/* Handle deferred window destroys */
	for (int i = display->num_windows - 1; i >= 0; i--) {
		struct window *window = display->windows[i];
		if (window && window->deferred_destroy) {
			if (window->widget && window->widget->deferred_destroy) {
				widget_deferred_destroy(window->widget);
			}
			window_deferred_destroy(window);
		} else if (window && window->widget && window->widget->deferred_destroy) {
			widget_deferred_destroy(window->widget);
		}
	}
}

static void
display_handle_redraw(struct display *display)
{
	bool did_redraw = false;
	
	for (int i = 0; i < display->num_windows; i++) {
		struct window *window = display->windows[i];
		if (window && window->need_redraw && window->widget) {
			if (window->widget->redraw_handler) {
				window->widget->redraw_handler(window->widget,
							       window->widget->user_data);
				
				/* Copy the off-screen pixmap to the window for display */
				if (window->widget->pixmap && window->gc) {
					/* Ensure cairo has finished all drawing operations */
					cairo_surface_flush(window->widget->surface);
					
					XCopyArea(display->xdisplay, 
						window->widget->pixmap,
						window->xwindow,
						window->gc,
						0, 0,  /* source x, y */
						window->widget->allocation.width,
						window->widget->allocation.height,
						0, 0); /* dest x, y */
				}
				
				did_redraw = true;
			}
			window->need_redraw = false;
		}
	}
	
	/* Flush after all redraws complete */
	if (did_redraw) {
		XFlush(display->xdisplay);
	}
}

/* Handle SelectionRequest - another app wants to paste from us */
static void
display_handle_selection_request(struct display *display, XSelectionRequestEvent *request)
{
	XSelectionEvent response;
	Atom clipboard = XInternAtom(display->xdisplay, "CLIPBOARD", False);
	Atom utf8_string = XInternAtom(display->xdisplay, "UTF8_STRING", False);
	Atom targets = XInternAtom(display->xdisplay, "TARGETS", False);
	Atom clip_property = XInternAtom(display->xdisplay, "_COSMOE_CLIPBOARD", False);
	
	/* Prepare response event */
	response.type = SelectionNotify;
	response.display = request->display;
	response.requestor = request->requestor;
	response.selection = request->selection;
	response.target = request->target;
	response.property = None;
	response.time = request->time;
	
	if (request->selection != clipboard) {
		/* We only support CLIPBOARD, not PRIMARY */
		XSendEvent(display->xdisplay, request->requestor, False, 0, (XEvent*)&response);
		return;
	}
	
	/* Find which of our windows owns the selection */
	Window owner = XGetSelectionOwner(display->xdisplay, clipboard);
	struct window *owner_window = NULL;
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->xwindow == owner) {
			owner_window = display->windows[i];
			break;
		}
	}
	
	if (!owner_window) {
		/* We don't own it anymore */
		XSendEvent(display->xdisplay, request->requestor, False, 0, (XEvent*)&response);
		return;
	}
	
	/* Handle TARGETS request */
	if (request->target == targets) {
		Atom supported[] = { targets, utf8_string, XA_STRING };
		XChangeProperty(display->xdisplay, request->requestor, request->property,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char*)supported, sizeof(supported)/sizeof(Atom));
		response.property = request->property;
		XSendEvent(display->xdisplay, request->requestor, False, 0, (XEvent*)&response);
		return;
	}
	
	/* Handle UTF8_STRING or STRING request */
	if (request->target == utf8_string || request->target == XA_STRING) {
		/* Get our clipboard data */
		Atom actual_type;
		int actual_format;
		unsigned long nitems, bytes_after;
		unsigned char *prop_data = NULL;
		
		if (XGetWindowProperty(display->xdisplay, owner, clip_property,
				0, (~0L), False, AnyPropertyType,
				&actual_type, &actual_format, &nitems, &bytes_after,
				&prop_data) == Success && prop_data) {
			/* Send the data to the requestor */
			XChangeProperty(display->xdisplay, request->requestor, request->property,
				request->target, 8, PropModeReplace, prop_data, nitems);
			response.property = request->property;
			XFree(prop_data);
		}
	}
	
	XSendEvent(display->xdisplay, request->requestor, False, 0, (XEvent*)&response);
}

/* Handle SelectionNotify - response to our paste request from another app */
static void
display_handle_selection_notify(struct display *display, XSelectionEvent *event)
{
	pthread_mutex_lock(&display->clipboard_state_lock);
	if (display->clipboard_request_pending
		&& event->requestor == display->clipboard_requestor
		&& event->selection == display->clipboard_request_selection
		&& (event->property == display->clipboard_request_property
			|| event->property == None)) {
		display->clipboard_response_ready = true;
		display->clipboard_response_success = event->property != None;
	}
	pthread_mutex_unlock(&display->clipboard_state_lock);
}

/* Check if mouse has been idle long enough to trigger tooltip */
static void
display_check_idle(struct display *display)
{
	struct timespec now;
	long long elapsed_us;
	const long long IDLE_TIMEOUT_US = 750000; /* 750ms */
	
	if (!display->last_motion_widget || !display->last_motion_widget->idle_handler)
		return;
	
	/* Additional safety check: ensure widget's window is still valid */
	if (!display->last_motion_widget->window)
		return;
	
	if (display->idle_fired)
		return;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	elapsed_us = (now.tv_sec - display->last_motion_time.tv_sec) * 1000000LL +
	             (now.tv_nsec - display->last_motion_time.tv_nsec) / 1000;
	
	if (elapsed_us >= IDLE_TIMEOUT_US) {
		X11_LOG("X11: display_check_idle firing - widget=%p, user_data=%p, window=%p\n",
			display->last_motion_widget, display->last_motion_widget->user_data,
			display->last_motion_widget->window);
		X11_FLUSH();
		display->idle_fired = true;
		display->last_motion_widget->idle_handler(
			display->last_motion_widget,
			(struct input*)display->last_motion_widget,
			0,
			display->last_motion_x,
			display->last_motion_y,
			display->last_motion_widget->user_data);
	}
}

void
display_run(struct display *display)
{
	XEvent event;
	
	display->running = true;
	
	while (display->running && !display->exit_requested) {
		/* Process any pending redraws */
		display_handle_redraw(display);
		
		/* Process pending operations */
		display_process_pending_operations(display);
		
		/* Check for backend messages from BWindow via PortLink */
		/* The context contains a function pointer to the message processor */
		if (display->backend_port >= 0) {
			x11_process_backend_messages(display->backend_port, display->app_port);
		}
		
		/* Check for mouse idle (for tooltips) */
		display_check_idle(display);
		
		/* If no windows, just sleep and continue - don't exit */
		if (display->num_windows == 0) {
			usleep(10000); /* 10ms */
			continue;
		}
		
		/* Wait for and process events */
		if (XPending(display->xdisplay) > 0) {
			XNextEvent(display->xdisplay, &event);

			if (event.type == PropertyNotify
				&& event.xproperty.window
					== RootWindow(display->xdisplay, display->screen)
				&& event.xproperty.atom == display->net_client_list) {
				display_dispatch_app_list_changes(display);
				continue;
			}

			struct window *window = display_find_window(display, event.xany.window);
			if (!window) {
				continue;
			}
			
			switch (event.type) {
			case ConfigureNotify:
				/* Coalesce multiple ConfigureNotify events - skip to the last one */
				while (XCheckTypedWindowEvent(display->xdisplay, event.xany.window, 
							       ConfigureNotify, &event)) {
					/* Keep reading and discarding until we get the last one */
				}
				window_handle_configure_notify(window, &event.xconfigure);
				break;
			case Expose:
				/* Coalesce multiple Expose events - skip to the last one */
				while (XCheckTypedWindowEvent(display->xdisplay, event.xany.window, 
							       Expose, &event)) {
					/* Keep reading and discarding until we get the last one */
				}
				window_handle_expose(window, &event.xexpose);
				break;
			case KeyPress:
				window_handle_key_press(window, &event.xkey);
				break;
			case KeyRelease:
				window_handle_key_release(window, &event.xkey);
				break;
			case ButtonPress:
				window_handle_button_press(window, &event.xbutton);
				break;
			case ButtonRelease:
				window_handle_button_release(window, &event.xbutton);
				break;
			case MotionNotify:
				window_handle_motion_notify(window, &event.xmotion);
				break;
			case ClientMessage:
				window_handle_client_message(window, &event.xclient);
				break;
			case SelectionRequest:
				display_handle_selection_request(display, &event.xselectionrequest);
				break;
			case SelectionNotify:
				display_handle_selection_notify(display, &event.xselection);
				break;
			case FocusIn:
				if (window->focus_handler)
					window->focus_handler(window, true, window->focus_user_data);
				break;
			case FocusOut:
				if (window->focus_handler)
					window->focus_handler(window, false, window->focus_user_data);
				break;
			}
		} else {
			/* No events pending, sleep briefly */
			usleep(10000); /* 10ms */
		}
	}
	
	display->running = false;
}

void
display_exit(struct display *display)
{
	display->exit_requested = true;
	
	/* Destroy all windows */
	while (display->num_windows > 0) {
		struct window *window = display->windows[0];
		if (window->widget) {
			if (window->widget->surface) {
				cairo_surface_destroy(window->widget->surface);
			}
			if (window->widget->pixmap) {
				XFreePixmap(display->xdisplay, window->widget->pixmap);
			}
			free(window->widget);
		}
		if (window->gc) {
			XFreeGC(display->xdisplay, window->gc);
		}
		XDestroyWindow(display->xdisplay, window->xwindow);
		free(window->title);
		display_remove_window(display, window);
		free(window);
	}
	
	/* Clean up XKB */
	free(display->watched_app_teams);
	display->watched_app_teams = NULL;
	display->watched_app_team_count = 0;
	display->app_watcher = NULL;
	display->app_watcher_user_data = NULL;

	if (display->xkb_state)
		xkb_state_unref(display->xkb_state);
	if (display->xkb_keymap)
		xkb_keymap_unref(display->xkb_keymap);
	if (display->xkb_context)
		xkb_context_unref(display->xkb_context);

	for (int i = 0; i < MAX_CUSTOM_CURSORS; i++) {
		if (s_custom_cursors[i] != None) {
			XFreeCursor(display->xdisplay, s_custom_cursors[i]);
			s_custom_cursors[i] = None;
		}
	}
	s_custom_cursor_display = NULL;
	
	/* Close X display */
	pthread_mutex_destroy(&display->clipboard_state_lock);
	XCloseDisplay(display->xdisplay);
	free(display);
}

void
display_flush(struct display *display)
{
	if (display && display->xdisplay) {
		XFlush(display->xdisplay);
	}
}

void
display_trigger_redraw(struct display *display, struct window *window,
		       struct widget *widget)
{
	/* If no window was provided directly, get it from the widget */
	if (!window && widget)
		window = widget->window;

	if (window) {
		window->need_redraw = true;
	}
}

void
display_get_screen_dimensions(struct display *display, struct rectangle *allocation)
{
	if (!allocation)
		return;
	
	allocation->x = 0;
	allocation->y = 0;

	if (!display) {
		allocation->width = 0;
		allocation->height = 0;
		return;
	}
	
	allocation->width = XDisplayWidth(display->xdisplay, display->screen);
	allocation->height = XDisplayHeight(display->xdisplay, display->screen);
}

void
display_set_port(struct display *display, int32_t sender_port_id, int32_t receiver_port_id)
{
	if (!display)
		return;
	// sender_port_id is where backend RECEIVES commands (app sends to this)
	// receiver_port_id is where backend SENDS replies (app receives from this)
	display->backend_port = sender_port_id;
	display->app_port = receiver_port_id;
	X11_LOG("X11: display_set_port: backend reads from port %d, writes to port %d\n", 
	       (int)sender_port_id, (int)receiver_port_id);
}

/* Window functions */

struct window *
window_create(struct display *display)
{
	struct window *window;
	
	window = calloc(1, sizeof *window);
	if (!window)
		return NULL;
	
	window->display = display;
	window->width = 640;
	window->height = 480;
	window->min_width = 0;
	window->min_height = 0;
	window->max_width = 32767;
	window->max_height = 32767;
	window->mapped = false;
	window->hidden = true;  /* Start hidden; call window_show() to map */
	window->is_popup = false;  /* Regular window, not a popup */
	
	/* Create X11 window with attributes to prevent flicker */
	XSetWindowAttributes attrs;
	attrs.background_pixmap = None;  /* Don't auto-clear with background */
	attrs.bit_gravity = NorthWestGravity; /* Preserve top-left pixels during resize */
	
	window->xwindow = XCreateWindow(
		display->xdisplay,
		RootWindow(display->xdisplay, display->screen),
		0, 0, window->width, window->height, 0,
		CopyFromParent, InputOutput, CopyFromParent,
		CWBackPixmap | CWBitGravity, &attrs);
	
	if (!window->xwindow) {
		free(window);
		return NULL;
	}
	
	/* Select events */
	XSelectInput(display->xdisplay, window->xwindow,
		     ExposureMask | KeyPressMask | KeyReleaseMask |
		     ButtonPressMask | ButtonReleaseMask |
		     PointerMotionMask | StructureNotifyMask |
		     FocusChangeMask);
	
	/* Set WM_DELETE_WINDOW protocol */
	XSetWMProtocols(display->xdisplay, window->xwindow,
			&display->wm_delete_window, 1);

	/*
	 * Publish EWMH identity expected by display_get_app_list().
	 * Host WMs commonly set these on native apps, but Cosmoe windows must
	 * explicitly provide them so they are discoverable as running apps.
	 */
	Atom net_wm_pid = XInternAtom(display->xdisplay, "_NET_WM_PID", False);
	long pid = (long)getpid();
	XChangeProperty(display->xdisplay, window->xwindow,
		net_wm_pid, XA_CARDINAL, 32, PropModeReplace,
		(unsigned char*)&pid, 1);

	Atom net_wm_window_type = XInternAtom(display->xdisplay,
		"_NET_WM_WINDOW_TYPE", False);
	Atom net_wm_window_type_normal = XInternAtom(display->xdisplay,
		"_NET_WM_WINDOW_TYPE_NORMAL", False);
	XChangeProperty(display->xdisplay, window->xwindow,
		net_wm_window_type, XA_ATOM, 32, PropModeReplace,
		(unsigned char*)&net_wm_window_type_normal, 1);
	
	/* Create GC for copying pixmap to window */
	window->gc = XCreateGC(display->xdisplay, window->xwindow, 0, NULL);
	
	/* Don't map window yet - let window_schedule_resize() do that after setting correct size */

	/* Initialize stored position */
	window->x = 0;
	window->y = 0;
	
	display_add_window(display, window);
	
	return window;
}

/* Create an override-redirect popup window suitable for menus. It doesn't
   set WM protocols or decorations, as the window manager should not manage
   popup windows. */
struct window *
window_popup_create(struct display *display, struct window *parent_window, int x, int y)
{
	struct window *window;

	/* parent_window is ignored on X11 but kept for API compatibility */
	(void)parent_window;

	window = calloc(1, sizeof *window);
	if (!window)
		return NULL;

	window->display = display;
	window->width = 640;
	window->height = 480;
	window->min_width = 0;
	window->min_height = 0;
	window->max_width = 0;
	window->max_height = 0;
	window->is_popup = true;  /* Mark this as a popup window */

	/* Create an override-redirect X11 window (no window manager decorations) */
	XSetWindowAttributes attrs;
	attrs.override_redirect = True;
	attrs.background_pixmap = None;
	attrs.bit_gravity = NorthWestGravity;

	window->xwindow = XCreateWindow(
		display->xdisplay,
		RootWindow(display->xdisplay, display->screen),
		x, y, window->width, window->height, 0,
		CopyFromParent, InputOutput, CopyFromParent,
		CWOverrideRedirect | CWBackPixmap | CWBitGravity, &attrs);

	if (!window->xwindow) {
		free(window);
		return NULL;
	}

	/* Only select the events we need (mouse & exposure) */
	XSelectInput(display->xdisplay, window->xwindow,
				 ExposureMask | ButtonPressMask | ButtonReleaseMask |
				 PointerMotionMask | StructureNotifyMask);

	/* Create GC for copying pixmap to window */
	window->gc = XCreateGC(display->xdisplay, window->xwindow, 0, NULL);

	/* Do not set WM_DELETE_WINDOW or similar on popups */

	/* Store coordinates; do NOT map yet - wait for explicit window_show() */
	window->x = x;
	window->y = y;
	window->hidden = true;

	display_add_window(display, window);

	return window;
}

void window_activate(struct window *win, bool active)
{
	X11_LOG("X11: %s window %p (xwindow=%lu, mapped=%d)\n", 
			active ? "Activating" : "Deactivating", win,
			win ? win->xwindow : 0,
			win ? win->mapped : 0);
	
	/* Check if window is valid and mapped */
	if (win && win->xwindow && win->display && win->display->xdisplay) {
		if (!win->mapped) {
			X11_LOG("X11: Warning - cannot activate unmapped window\n");
		} else if (active) {
			XWindowAttributes attrs;
			Status gotAttributes = XGetWindowAttributes(win->display->xdisplay,
				win->xwindow, &attrs);
			if (!gotAttributes || attrs.map_state != IsViewable) {
				X11_LOG("X11: Warning - cannot set focus on non-viewable window (map_state=%d)\n",
					gotAttributes ? attrs.map_state : -1);
				XRaiseWindow(win->display->xdisplay, win->xwindow);
				XFlush(win->display->xdisplay);
				return;
			}
			/* Activate: Raise window and set input focus */
			X11_LOG("X11: Raising window and setting focus\n");
			XRaiseWindow(win->display->xdisplay, win->xwindow);
			XSetInputFocus(win->display->xdisplay, win->xwindow,
							RevertToParent, CurrentTime);
			XFlush(win->display->xdisplay);
			X11_LOG("X11: Window activated\n");
		} else {
			/* Deactivate: Lower window (optional - usually just lose focus naturally) */
			X11_LOG("X11: Deactivating window (lowering)\n");
			XLowerWindow(win->display->xdisplay, win->xwindow);
			XFlush(win->display->xdisplay);
			X11_LOG("X11: Window deactivated\n");
		}
	} else {
		X11_LOG("X11: Invalid window or display for activate operation\n");
	}
}

bool
window_is_front(struct window *window)
{
	if (window == NULL || window->display == NULL
		|| window->display->xdisplay == NULL || window->xwindow == 0) {
		return false;
	}

	Window focusedWindow = None;
	int revertTo = RevertToNone;
	XGetInputFocus(window->display->xdisplay, &focusedWindow, &revertTo);

	return focusedWindow == window->xwindow;
}

void
window_set_title(struct window *window, const char *title)
{
	XTextProperty property;
	char *titleList[1];
	const char *safeTitle;
	int status;

	if (window == NULL || window->display == NULL
		|| window->display->xdisplay == NULL || window->xwindow == 0) {
		return;
	}

	safeTitle = title != NULL ? title : "";

	if (window->title)
		free(window->title);

	window->title = strdup(safeTitle);
	
	/* Detect tooltip windows by their title */
	if (strcmp(safeTitle, "tool tip") == 0) {
		window->is_tooltip = true;
	}

	XChangeProperty(window->display->xdisplay, window->xwindow,
		window->display->net_wm_name, window->display->utf8_string, 8,
		PropModeReplace, (const unsigned char *)safeTitle,
		(int)strlen(safeTitle));

	titleList[0] = (char *)safeTitle;
	status = Xutf8TextListToTextProperty(window->display->xdisplay, titleList, 1,
		XStdICCTextStyle, &property);
	if (status >= Success) {
		XSetWMName(window->display->xdisplay, window->xwindow, &property);
		XFree(property.value);
	} else {
		XStoreName(window->display->xdisplay, window->xwindow, safeTitle);
	}

	XFlush(window->display->xdisplay);
}

/* Helper to load PNG icon and convert to _NET_WM_ICON format */
static unsigned long *
load_icon_from_png(const char *path, int *width, int *height)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	
	/* Read PNG signature */
	unsigned char sig[8];
	if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
		fclose(fp);
		return NULL;
	}
	
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fclose(fp);
		return NULL;
	}
	
	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, NULL, NULL);
		fclose(fp);
		return NULL;
	}
	
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}
	
	png_init_io(png, fp);
	png_set_sig_bytes(png, 8);
	png_read_info(png, info);
	
	*width = png_get_image_width(png, info);
	*height = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth = png_get_bit_depth(png, info);
	
	/* Convert to RGBA if needed */
	if (bit_depth == 16)
		png_set_strip_16(png);
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (color_type == PNG_COLOR_TYPE_RGB ||
	    color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	
	png_read_update_info(png, info);
	
	/* Allocate row pointers */
	png_bytep *row_pointers = malloc(sizeof(png_bytep) * (*height));
	if (!row_pointers) {
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}
	
	for (int y = 0; y < *height; y++)
		row_pointers[y] = malloc(png_get_rowbytes(png, info));
	
	png_read_image(png, row_pointers);
	
	/* Convert RGBA to ARGB (X11 _NET_WM_ICON format) */
	unsigned long *icon_data = malloc(sizeof(unsigned long) * (*width) * (*height));
	if (!icon_data) {
		for (int y = 0; y < *height; y++)
			free(row_pointers[y]);
		free(row_pointers);
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}
	
	for (int y = 0; y < *height; y++) {
		png_bytep row = row_pointers[y];
		for (int x = 0; x < *width; x++) {
			png_bytep px = &(row[x * 4]);
			/* ARGB format: (A << 24) | (R << 16) | (G << 8) | B */
			icon_data[y * (*width) + x] = 
				((unsigned long)px[3] << 24) |  /* A */
				((unsigned long)px[0] << 16) |  /* R */
				((unsigned long)px[1] << 8)  |  /* G */
				((unsigned long)px[2]);         /* B */
		}
		free(row_pointers[y]);
	}
	
	free(row_pointers);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(fp);
	
	return icon_data;
}

static void
canonicalize_icon_name(const char* input, char* output, size_t outputSize)
{
	if (!input || !output || outputSize == 0)
		return;

	size_t out = 0;
	for (size_t i = 0; input[i] != '\0' && out + 1 < outputSize; i++) {
		char c = input[i];
		if (c == '_')
			c = '-';
		output[out++] = (char)tolower((unsigned char)c);
	}
	output[out] = '\0';
}

static void
sanitize_wm_class_token(const char* input, char* output, size_t outputSize,
	bool preserveCase)
{
	if (!input || !output || outputSize == 0)
		return;

	size_t out = 0;
	for (size_t i = 0; input[i] != '\0' && out + 1 < outputSize; i++) {
		unsigned char uc = (unsigned char)input[i];
		char c = (char)uc;
		if (isalnum(uc) || c == '-' || c == '_') {
			output[out++] = preserveCase ? c : (char)tolower(uc);
		} else {
			output[out++] = '-';
		}
	}

	/* WM_CLASS fields should be non-empty for tools like wmctrl/xprop. */
	if (out == 0 && outputSize > 1) {
		output[0] = 'c';
		output[1] = '\0';
		return;
	}

	output[out] = '\0';
}

static void
derive_wm_class_names(const char* app_name, char* instanceName,
	size_t instanceSize, char* className, size_t classSize)
{
	const char* baseName = app_name;

	if (strncmp(baseName, "application/", 12) == 0)
		baseName += 12;
	if (strncmp(baseName, "x-vnd.", 6) == 0)
		baseName += 6;

	sanitize_wm_class_token(baseName, instanceName, instanceSize, false);
	sanitize_wm_class_token(baseName, className, classSize, true);
}

static bool
try_load_icon_for_name(const char* iconName, unsigned long** iconData,
	int* iconWidth, int* iconHeight)
{
	if (!iconName || iconName[0] == '\0' || !iconData || !iconWidth || !iconHeight)
		return false;

	const char *icon_dirs[] = {
		"/usr/local/share/icons/hicolor",
		"/usr/share/icons/hicolor",
		NULL
	};

	const int sizes[] = { 48, 32, 16, 0 };

	for (int d = 0; icon_dirs[d] != NULL && *iconData == NULL; d++) {
		for (int s = 0; sizes[s] != 0 && *iconData == NULL; s++) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%dx%d/apps/%s.png",
				icon_dirs[d], sizes[s], sizes[s], iconName);
			*iconData = load_icon_from_png(path, iconWidth, iconHeight);
		}
	}

	return *iconData != NULL;
}

static void
add_icon_candidate(char candidates[][128], int* candidateCount,
	const char* name)
{
	if (!candidates || !candidateCount || !name || name[0] == '\0')
		return;

	if (*candidateCount >= 8)
		return;

	char normalized[128];
	canonicalize_icon_name(name, normalized, sizeof(normalized));
	if (normalized[0] == '\0')
		return;

	for (int i = 0; i < *candidateCount; i++) {
		if (strcmp(candidates[i], normalized) == 0)
			return;
	}

	strncpy(candidates[*candidateCount], normalized,
		sizeof(candidates[*candidateCount]) - 1);
	candidates[*candidateCount][sizeof(candidates[*candidateCount]) - 1] = '\0';
	(*candidateCount)++;
}

void
window_set_appid(struct window *window, const char *app_name)
{
	if (!window || !app_name || !window->xwindow)
		return;

	char instance_name[128];
	char class_name[128];
	derive_wm_class_names(app_name, instance_name, sizeof(instance_name),
		class_name, sizeof(class_name));

	XClassHint class_hint;
	class_hint.res_name = instance_name;
	class_hint.res_class = class_name;
	XSetClassHint(window->display->xdisplay, window->xwindow, &class_hint);

	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == 0) {
		hostname[sizeof(hostname) - 1] = '\0';
		Atom wm_client_machine = XInternAtom(window->display->xdisplay,
			"WM_CLIENT_MACHINE", False);
		XChangeProperty(window->display->xdisplay, window->xwindow,
			wm_client_machine, XA_STRING, 8, PropModeReplace,
			(unsigned char*)hostname, (int)strlen(hostname));
	}

	X11_LOG("X11: Set WM_CLASS='%s'/'%s' and WM_CLIENT_MACHINE for app '%s'\n",
		instance_name, class_name, app_name);

	// Generate candidate icon names from either normalized names or full
	// application signatures (for example "application/x-vnd.Cosmoe-Showcase").
	char candidates[8][128];
	int candidateCount = 0;

	add_icon_candidate(candidates, &candidateCount, app_name);

	const char* typeName = app_name;
	if (strncmp(typeName, "application/", 12) == 0)
		typeName += 12;
	add_icon_candidate(candidates, &candidateCount, typeName);

	const char* vendorName = strstr(typeName, "x-vnd.");
	if (vendorName == typeName)
		vendorName += 6;
	else
		vendorName = typeName;
	add_icon_candidate(candidates, &candidateCount, vendorName);

	const char* appPart = strpbrk(vendorName, "-._");
	if (appPart != NULL)
		add_icon_candidate(candidates, &candidateCount, appPart + 1);

	unsigned long *icon_data = NULL;
	int icon_width = 0, icon_height = 0;
	const char* chosenName = NULL;
#ifndef DEBUG
	(void)chosenName;
#endif

	for (int i = 0; i < candidateCount && icon_data == NULL; i++) {
		if (try_load_icon_for_name(candidates[i], &icon_data, &icon_width, &icon_height))
			chosenName = candidates[i];
	}
	
	if (!icon_data) {
		X11_LOG("X11: No icon found for app '%s' (checked normalized candidates)\n", app_name);
		return;
	}
	
	/* Set _NET_WM_ICON property
	 * Format: width, height, ARGB data */
	unsigned long *prop_data = malloc(sizeof(unsigned long) * (2 + icon_width * icon_height));
	if (!prop_data) {
		free(icon_data);
		return;
	}
	
	prop_data[0] = icon_width;
	prop_data[1] = icon_height;
	memcpy(&prop_data[2], icon_data, sizeof(unsigned long) * icon_width * icon_height);
	
	Atom net_wm_icon = XInternAtom(window->display->xdisplay, "_NET_WM_ICON", False);
	XChangeProperty(window->display->xdisplay, window->xwindow,
		net_wm_icon, XA_CARDINAL, 32, PropModeReplace,
		(unsigned char *)prop_data, 2 + icon_width * icon_height);
	
	free(prop_data);
	free(icon_data);
	
	X11_LOG("X11: Set window icon for app '%s' using '%s' (%dx%d)\n",
		app_name, chosenName ? chosenName : app_name, icon_width, icon_height);
}

void
window_set_parent(struct window *window, struct window *parent)
{
	if (!window || !parent || !window->xwindow || !parent->xwindow)
		return;
	
	/* Set WM_TRANSIENT_FOR hint to establish parent-child relationship */
	XSetTransientForHint(window->display->xdisplay, window->xwindow, parent->xwindow);
	
	/* Set _NET_WM_STATE_MODAL to indicate this is a modal dialog */
	XChangeProperty(window->display->xdisplay, window->xwindow,
			window->display->net_wm_state,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&window->display->net_wm_state_modal, 1);
	
	XFlush(window->display->xdisplay);
	
	X11_LOG("X11: Set window %lu as modal child of window %lu\n",
			(unsigned long)window->xwindow, (unsigned long)parent->xwindow);
}

void
window_schedule_resize(struct window *window, int width, int height)
{
	if (!window || !window->xwindow)
		return;
	
	window->width = width;
	window->height = height;
	
	/* Maintain the 1-pixel difference between what BeOS/Haiku expects
		and what X11 expects regarding window size */
	XResizeWindow(window->display->xdisplay, window->xwindow, width + 1, height + 1);
	
	/* Update widget surface if it exists - grow in chunks if needed */
	if (window->widget && window->widget->surface) {
		const int SURFACE_GROW_CHUNK = 100;
		
		/* Only recreate surface if we need to grow it */
		if (width > window->widget->surface_width || 
		    height > window->widget->surface_height) {
			/* Grow only in the dimension(s) that need it, adding chunk to each */
			int new_width = width > window->widget->surface_width ? 
				width + SURFACE_GROW_CHUNK : window->widget->surface_width;
			int new_height = height > window->widget->surface_height ? 
				height + SURFACE_GROW_CHUNK : window->widget->surface_height;
			
			cairo_surface_destroy(window->widget->surface);
			
			/* Free old pixmap and create a new one at the larger size.
			 * We must draw over the pixmap (not the window directly) so that
			 * XCopyArea in display_handle_redraw copies valid content. */
			if (window->widget->pixmap) {
				XFreePixmap(window->display->xdisplay, window->widget->pixmap);
			}
			window->widget->pixmap = XCreatePixmap(
				window->display->xdisplay,
				window->xwindow,
				new_width, new_height,
				DefaultDepth(window->display->xdisplay, window->display->screen));
			
			window->widget->surface = cairo_xlib_surface_create(
				window->display->xdisplay,
				window->widget->pixmap,
				window->display->visual,
				new_width, new_height);
			
			/* Initialize new pixmap to background gray to avoid garbage display */
			cairo_t *init_cr = cairo_create(window->widget->surface);
			cairo_set_source_rgb(init_cr, 0.847, 0.847, 0.847);
			cairo_paint(init_cr);
			cairo_destroy(init_cr);
			cairo_surface_flush(window->widget->surface);
			
			window->widget->surface_width = new_width;
			window->widget->surface_height = new_height;
		}
		
		/* Update the drawable size to the actual window size */
		cairo_xlib_surface_set_size(window->widget->surface, width, height);
		window->widget->allocation.width = width;
		window->widget->allocation.height = height;
	}
	
	XFlush(window->display->xdisplay);
}

void
window_set_min_max_allocation(struct window *window,
			       int min_width, int min_height,
			       int max_width, int max_height)
{
	XSizeHints hints;
	long supplied_return;
	
	window->min_width = min_width;
	window->min_height = min_height;
	window->max_width = max_width;
	window->max_height = max_height;
	
	/* Get existing hints to preserve other properties */
	if (!XGetWMNormalHints(window->display->xdisplay, window->xwindow, &hints, &supplied_return)) {
		/* If no hints exist, initialize structure */
		memset(&hints, 0, sizeof(hints));
		hints.flags = 0;
	}
	
	/* Update min size */
	if (min_width > 0 && min_height > 0) {
		hints.flags |= PMinSize;
		hints.min_width = min_width;
		hints.min_height = min_height;
	} else {
		hints.flags &= ~PMinSize;
	}
	
	/* Update max size */
	if (max_width > 0 && max_height > 0) {
		hints.flags |= PMaxSize;
		hints.max_width = max_width;
		hints.max_height = max_height;
	} else {
		hints.flags &= ~PMaxSize;
	}
	
	/* Always set hints, even if removing constraints */
	XSetWMNormalHints(window->display->xdisplay, window->xwindow, &hints);
	XFlush(window->display->xdisplay);
}

void
window_set_resize_handler(struct window *window,
			  widget_resize_handler_t handler)
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
window_set_focus_handler(struct window *window, void (*handler)(struct window*, bool, void*), void *user_data)
{
	if (!window)
		return;
	window->focus_handler = handler;
	window->focus_user_data = user_data;
}

struct display *
window_get_display(struct window *window)
{
	return window->display;
}

Display *
window_get_xdisplay(struct window *window)
{
	return window ? window->display->xdisplay : NULL;
}

Window
window_get_xwindow(struct window *window)
{
	return window ? window->xwindow : None;
}


/* Wrapper functions for PortLink message handlers */

void
window_minimize(struct window *window, bool minimize)
{
	if (!window || !window->display || !window->xwindow)
		return;

	X11_LOG("X11: Minimizing window %p (xwindow=%lu, mapped=%d)\n", 
	       window, window->xwindow, window->mapped);

	if (minimize) {
		X11_LOG("X11: Calling XIconifyWindow\n");
		XIconifyWindow(window->display->xdisplay, window->xwindow, 
		               DefaultScreen(window->display->xdisplay));
	} else {
		X11_LOG("X11: Calling XMapWindow\n");
		XMapWindow(window->display->xdisplay, window->xwindow);
	}
	XFlush(window->display->xdisplay);
	
	X11_LOG("X11: Minimize/restore complete\n");
}

void
window_show(struct window *window)
{
	if (!window || !window->display || !window->xwindow) {
		X11_LOG("X11: window_show: NULL guard triggered\n");
		return;
	}
	if (!window->hidden) {
		X11_LOG("X11: window_show: window already visible (hidden=false), skipping\n");
		return;
	}
	X11_LOG("X11: window_show: mapping xwindow=%lu\n", (unsigned long)window->xwindow);
	window->hidden = false;
	window->mapped = true;
	XMapWindow(window->display->xdisplay, window->xwindow);
	XFlush(window->display->xdisplay);
}

void
window_hide(struct window *window)
{
	if (!window || !window->display || !window->xwindow)
		return;
	if (window->hidden)
		return;
	window->hidden = true;
	window->mapped = false;
	XUnmapWindow(window->display->xdisplay, window->xwindow);
	XFlush(window->display->xdisplay);
}

void
window_deferred_destroy(struct window *window)
{
	X11_LOG("X11: window_deferred_destroy called - window=%p, deferred=%d, widget=%p\n",
		window, window->deferred_destroy, window->widget);
	X11_FLUSH();
	
	/* Clear idle detection if it references this window's widget 
	 * Must be done BEFORE freeing the widget */
	if (window->widget && window->display->last_motion_widget == window->widget) {
		X11_LOG("X11: Clearing last_motion_widget for window %p\n", window);
		X11_FLUSH();
		window->display->last_motion_widget = NULL;
		window->display->last_motion_window = NULL;
	}
	
	if (window->deferred_destroy) {
		/* Actually destroy it now */
		if (window->widget) {
			if (window->widget->surface) {
				cairo_surface_destroy(window->widget->surface);
			}
			free(window->widget);
			window->widget = NULL;
		}
		
		XDestroyWindow(window->display->xdisplay, window->xwindow);
		display_remove_window(window->display, window);
		free(window->title);
		free(window);
	} else {
		/* Mark for deferred destruction */
		window->deferred_destroy = true;
	}
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

/* Widget functions */

struct widget *
window_add_widget(struct window *window, void *data)
{
	struct widget *widget;
	
	X11_LOG("X11: window_add_widget called for window %p\n", window);
	/* Only one widget per window in this simple implementation */
	if (window->widget) {
		return window->widget;
	}
	
	widget = calloc(1, sizeof *widget);
	if (!widget)
		return NULL;
	
	widget->window = window;
	widget->user_data = data;
	widget->allocation.x = 0;
	widget->allocation.y = 0;
	widget->allocation.width = window->width;
	widget->allocation.height = window->height;
	widget->cursor = 1;
	
	/* Create cairo surface with initial size plus growth chunk */
	const int SURFACE_GROW_CHUNK = 100;
	int initial_width = window->width + SURFACE_GROW_CHUNK;
	int initial_height = window->height + SURFACE_GROW_CHUNK;
	
	/* Create off-screen pixmap for double buffering */
	widget->pixmap = XCreatePixmap(
		window->display->xdisplay,
		window->xwindow,
		initial_width, initial_height,
		DefaultDepth(window->display->xdisplay, window->display->screen));
	
	/* Create cairo surface on the pixmap instead of the window */
	widget->surface = cairo_xlib_surface_create(
		window->display->xdisplay,
		widget->pixmap,
		window->display->visual,
		initial_width, initial_height);
	
	widget->surface_width = initial_width;
	widget->surface_height = initial_height;
	
	/* Set the drawable size to the actual window size */
	cairo_xlib_surface_set_size(widget->surface, window->width, window->height);
	
	/* Initialize pixmap to BeOS default gray to avoid garbage display before first draw */
	cairo_t *init_cr = cairo_create(widget->surface);
	cairo_set_source_rgb(init_cr, 0.847, 0.847, 0.847);  /* BeOS default: rgb(216,216,216) */
	cairo_paint(init_cr);
	cairo_destroy(init_cr);
	cairo_surface_flush(widget->surface);
	
	window->widget = widget;
	
	/* Trigger initial redraw */
	window->need_redraw = true;
	
	return widget;
}

void
widget_deferred_destroy(struct widget *widget)
{
	if (widget->deferred_destroy) {
		/* Actually destroy it now */
		if (widget->surface) {
			cairo_surface_destroy(widget->surface);
		}
		if (widget->pixmap && widget->window) {
			XFreePixmap(widget->window->display->xdisplay, widget->pixmap);
		}
		
		if (widget->window) {
			widget->window->widget = NULL;
		}
		
		free(widget);
	} else {
		/* Mark for deferred destruction */
		widget->deferred_destroy = true;
	}
}

void
widget_set_redraw_handler(struct widget *widget,
			  widget_redraw_handler_t handler)
{
	widget->redraw_handler = handler;
}

void
widget_set_resize_handler(struct widget *widget,
			  widget_resize_handler_t handler)
{
	widget->resize_handler = handler;
}

void
widget_set_button_handler(struct widget *widget,
			  widget_button_handler_t handler)
{
	widget->button_handler = handler;
}

void
widget_set_motion_handler(struct widget *widget,
			  widget_motion_handler_t handler)
{
	widget->motion_handler = handler;
}

void
widget_set_idle_handler(struct widget *widget,
			widget_idle_handler_t handler)
{
	widget->idle_handler = handler;
}

void
widget_set_axis_handler(struct widget *widget,
			widget_axis_handler_t handler)
{
	widget->axis_handler = handler;
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

cairo_t *
widget_cairo_create(struct widget *widget)
{
	if (!widget->surface)
		return NULL;
	
	return cairo_create(widget->surface);
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
	if (!widget || !widget->window)
		return;
	
	X11_LOG("X11: widget_schedule_redraw called for widget %p, window %p\n", widget, widget->window);
	widget->window->need_redraw = true;
}

void
window_set_move_handler(struct window *window, void (*handler)(struct window*, int, int, void*), void *user_data)
{
	if (!window)
		return;
	window->move_handler = handler;
	window->move_user_data = user_data;
}

void *window_get_focus_user_data(struct window *window)
{
	if (!window)
		return NULL;
	return window->focus_user_data;
}

/* Get decorator sizes (left border width, top tab height) for an X11 window
   by querying _NET_FRAME_EXTENTS. If not available, returns zero for each. */
void
window_get_decorator_size(struct window *window, int *borderWidth, int *tabHeight)
{
	if (!window || !window->xwindow) {
		if (borderWidth) *borderWidth = 0;
		if (tabHeight) *tabHeight = 0;
		return;
	}

	Display *xdisplay = window->display->xdisplay;
	Atom frameExtents = XInternAtom(xdisplay, "_NET_FRAME_EXTENTS", False);
	Atom actualType;
	int actualFormat;
	unsigned long nitems = 0, bytesAfter = 0;
	long *extents = NULL;
	int rc = XGetWindowProperty(xdisplay, window->xwindow, frameExtents, 0, 4, False,
								XA_CARDINAL, &actualType, &actualFormat, &nitems, &bytesAfter,
								(unsigned char**)&extents);
	if (rc == Success && extents != NULL && nitems >= 4) {
		if (borderWidth) *borderWidth = (int)extents[0];
		if (tabHeight) *tabHeight = (int)extents[2];
		XFree(extents);
		return;
	}

	if (extents) XFree(extents);
	// Fallback: no extents property found; set zeros
	if (borderWidth) *borderWidth = 0;
	if (tabHeight) *tabHeight = 0;
}


/* Clipboard implementation */

int
display_set_clipboard_text(struct display *display, const char *text, size_t length)
{
	if (!display || !text)
		return -1;
	
	Atom clipboard = XInternAtom(display->xdisplay, "CLIPBOARD", False);
	Atom utf8_string = XInternAtom(display->xdisplay, "UTF8_STRING", False);
	
	/* For simplicity, we'll use XA_STRING for now. In a full implementation,
	 * we'd need to handle selection requests and provide the data when requested.
	 * This is a simplified version that sets the selection to a dummy window. */
	
	/* Get the first window as the selection owner, or create a dummy if none exist */
	Window owner_window = None;
	if (display->num_windows > 0 && display->windows[0]) {
		owner_window = display->windows[0]->xwindow;
	} else {
		/* Create a dummy window for clipboard ownership */
		owner_window = XCreateSimpleWindow(display->xdisplay,
			DefaultRootWindow(display->xdisplay),
			0, 0, 1, 1, 0, 0, 0);
	}
	
	if (owner_window == None)
		return -1;
	
	/* Set the selection owner */
	XSetSelectionOwner(display->xdisplay, clipboard, owner_window, CurrentTime);
	
	/* Verify we own the selection */
	if (XGetSelectionOwner(display->xdisplay, clipboard) != owner_window) {
		return -1;
	}
	
	/* Store the text in a window property for later retrieval
	 * Note: This is a simplified approach. A proper implementation would
	 * handle SelectionRequest events and provide the data on demand. */
	Atom clip_property = XInternAtom(display->xdisplay, "_COSMOE_CLIPBOARD", False);
	XChangeProperty(display->xdisplay, owner_window, clip_property,
		utf8_string, 8, PropModeReplace,
		(unsigned char *)text, length);
	
	XFlush(display->xdisplay);
	return 0;
}

char *
display_get_clipboard_text(struct display *display, size_t *out_length)
{
	if (!display)
		return NULL;
	
	if (out_length)
		*out_length = 0;
	
	Atom clipboard = XInternAtom(display->xdisplay, "CLIPBOARD", False);
	Atom utf8_string = XInternAtom(display->xdisplay, "UTF8_STRING", False);
	
	/* Get the selection owner */
	Window owner = XGetSelectionOwner(display->xdisplay, clipboard);
	if (owner == None) {
		/* No clipboard content */
		return NULL;
	}
	
	/* Check if we own the selection - if so, retrieve from our property */
	bool we_own_it = false;
	for (int i = 0; i < display->num_windows; i++) {
		if (display->windows[i] && display->windows[i]->xwindow == owner) {
			we_own_it = true;
			break;
		}
	}
	
	if (we_own_it) {
		/* Retrieve from our stored property */
		Atom clip_property = XInternAtom(display->xdisplay, "_COSMOE_CLIPBOARD", False);
		Atom actual_type;
		int actual_format;
		unsigned long nitems, bytes_after;
		unsigned char *prop_data = NULL;
		
		if (XGetWindowProperty(display->xdisplay, owner, clip_property,
				0, (~0L), False, AnyPropertyType,
				&actual_type, &actual_format, &nitems, &bytes_after,
				&prop_data) == Success && prop_data) {
			char *result = malloc(nitems + 1);
			if (result) {
				memcpy(result, prop_data, nitems);
				result[nitems] = '\0';
				if (out_length)
					*out_length = nitems;
			}
			XFree(prop_data);
			return result;
		}
		return NULL;
	}
	
	/* Selection is owned by another application - request it via XConvertSelection */
	
	/* We need a window to receive the SelectionNotify event. Use first window. */
	if (display->num_windows == 0)
		return NULL;
	
	Window requestor = display->windows[0]->xwindow;
	Atom selection_property = XInternAtom(display->xdisplay, "_COSMOE_SELECTION", False);

	pthread_mutex_lock(&display->clipboard_state_lock);
	display->clipboard_request_pending = true;
	display->clipboard_response_ready = false;
	display->clipboard_response_success = false;
	display->clipboard_requestor = requestor;
	display->clipboard_request_selection = clipboard;
	display->clipboard_request_property = selection_property;
	pthread_mutex_unlock(&display->clipboard_state_lock);
	
	/* Request the clipboard content */
	XConvertSelection(display->xdisplay, clipboard, utf8_string, 
		selection_property, requestor, CurrentTime);
	XFlush(display->xdisplay);
	
	/* Wait for display loop to process SelectionNotify (with timeout). */
	int max_attempts = 100; /* 1 second total timeout */
	bool got_response = false;
	bool conversion_succeeded = false;
	
	while (max_attempts-- > 0) {
		pthread_mutex_lock(&display->clipboard_state_lock);
		if (display->clipboard_response_ready) {
			got_response = true;
			conversion_succeeded = display->clipboard_response_success;
			display->clipboard_request_pending = false;
			pthread_mutex_unlock(&display->clipboard_state_lock);
			break;
		}
		pthread_mutex_unlock(&display->clipboard_state_lock);
		usleep(10000); /* 10ms */
	}

	if (!got_response) {
		pthread_mutex_lock(&display->clipboard_state_lock);
		display->clipboard_request_pending = false;
		pthread_mutex_unlock(&display->clipboard_state_lock);
	}
	
	if (!got_response) {
		/* Timeout waiting for response */
		return NULL;
	}
	
	/* Check if the conversion succeeded */
	if (!conversion_succeeded) {
		/* Conversion failed */
		return NULL;
	}
	
	/* Read the property data */
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_data = NULL;
	
	if (XGetWindowProperty(display->xdisplay, requestor, selection_property,
			0, (~0L), True, /* Delete property after reading */
			AnyPropertyType, &actual_type, &actual_format,
			&nitems, &bytes_after, &prop_data) == Success && prop_data) {
		char *result = malloc(nitems + 1);
		if (result) {
			memcpy(result, prop_data, nitems);
			result[nitems] = '\0';
			if (out_length)
				*out_length = nitems;
		}
		XFree(prop_data);
		return result;
	}
	
	return NULL;
}


int32_t
display_get_app_list(struct display *display, int32_t *team_ids, int32_t max_count)
{
	if (display == NULL || display->xdisplay == NULL || team_ids == NULL
		|| max_count <= 0) {
		return 0;
	}

	int32_t *collected_team_ids = NULL;
	int32_t count = display_collect_app_list(display, &collected_team_ids);
	int32_t result_count = count < max_count ? count : max_count;
	for (int32_t i = 0; i < result_count; i++)
		team_ids[i] = collected_team_ids[i];

	free(collected_team_ids);
	return result_count;
}


status_t
display_set_app_watcher(struct display *display, display_app_watcher_t watcher,
	void *user_data)
{
	if (display == NULL || watcher == NULL)
		return B_BAD_VALUE;

	int32_t *team_ids = NULL;
	int32_t count = display_collect_app_list(display, &team_ids);
	free(display->watched_app_teams);
	display->watched_app_teams = team_ids;
	display->watched_app_team_count = count;
	display->app_watcher = watcher;
	display->app_watcher_user_data = user_data;
	return B_OK;
}


status_t
display_clear_app_watcher(struct display *display)
{
	if (display == NULL)
		return B_BAD_VALUE;

	free(display->watched_app_teams);
	display->watched_app_teams = NULL;
	display->watched_app_team_count = 0;
	display->app_watcher = NULL;
	display->app_watcher_user_data = NULL;
	return B_OK;
}
