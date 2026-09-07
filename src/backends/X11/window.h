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

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <stdint.h>
#include <SupportDefs.h>
#include <cairo.h>
#include <xkbcommon/xkbcommon.h>
#include <X11/Xlib.h>
#include "CosmoeBackendAPI.h"

// Pull in wl_fixed_t definition
#include "stubs/wayland-stubs.h"

struct window;
struct widget;
struct display;
struct input;

#include "rectangle.h"

/* Callback function types */
typedef void (*window_key_handler_t)(struct window *window, struct input* input, uint32_t time,
				     uint32_t key, uint32_t unicode,
				     enum xkb_key_direction state, void *data);

typedef void (*window_close_handler_t)(void *data);

typedef void (*window_screen_handler_t)(void *data);

typedef void (*widget_redraw_handler_t)(struct widget *widget, void *data);

typedef void (*widget_resize_handler_t)(struct widget *widget, int32_t width,
					int32_t height, void *data);

typedef void (*widget_button_handler_t)(struct widget *widget, struct input *input,
					uint32_t time, uint32_t button, uint32_t state,
					void *data);

typedef int (*widget_motion_handler_t)(struct widget *widget, struct input *input,
					uint32_t time, float x, float y, void *data);

typedef void (*widget_axis_handler_t)(struct widget *widget, struct input *input,
				      uint32_t time, uint32_t axis, wl_fixed_t value, void *data);

typedef void (*widget_idle_handler_t)(struct widget *widget, struct input *input,
				      uint32_t time, int32_t x, int32_t y, void *data);

enum {
	DISPLAY_APP_WATCH_LAUNCHED = 1,
	DISPLAY_APP_WATCH_QUIT = 2,
};

typedef void (*display_app_watcher_t)(struct display *display, int32_t event,
	int32_t team_id, void *data);

/* Display functions */
struct display *
display_create(int *argc, char **argv);

void
display_run(struct display *display);

void
display_exit(struct display *display);

void
display_flush(struct display *display);

void
display_trigger_redraw(struct display *display, struct window *window,
					   struct widget *widget, const struct rectangle *damage);

/* Get screen dimensions for the display (primary screen) */
void
display_get_screen_dimensions(struct display *display, struct rectangle *allocation);

/* Set the port ID for backend communication */
void
display_set_port(struct display *display, int32_t sender_port_id, int32_t receiver_port_id);

/* Find a window by its BWindow object token */
struct window *
display_find_window_by_token(struct display *display, int32_t token);

/* Set the BWindow object token on this window */
void
window_set_token(struct window *window, int32_t token);

/* Window functions */
struct window *
window_create(struct display *display, uint32_t look, uint32_t flags);

void
window_set_look(struct window *window, uint32_t look);

void
window_set_feel(struct window *window, uint32_t feel);

void
window_set_flags(struct window *window, uint32_t flags);

/* Create a popup (menu) window. This should be override-redirect and
	borderless to behave like a popup (menu) window. parent_window is
	ignored on X11 but kept for API compatibility. */
struct window *
window_popup_create(struct display *display, struct window *parent_window, int x, int y);

/* Get window position in screen coordinates */
void
window_get_position(struct window *window, int *x, int *y);

/* Move window to absolute coordinates. */
void
window_set_position(struct window *window, int x, int y);

void
window_set_title(struct window *window, const char *title);

void
window_set_appid(struct window *window, const char *app_name);

int32_t
window_create_custom_cursor(const uint8_t* bits, size_t bitsLength,
	int32_t width, int32_t height, int32_t bytesPerRow,
	int32_t colorSpace, int32_t hotX, int32_t hotY);

int
window_delete_custom_cursor(int32_t cursorID);

void
window_set_parent(struct window *window, struct window *parent);

void
window_schedule_resize(struct window *window, int width, int height);

void
window_set_min_max_allocation(struct window *window,
			       int min_width, int min_height,
			       int max_width, int max_height);

void
window_set_resize_handler(struct window *window,
			  widget_resize_handler_t handler);

void
window_set_key_handler(struct window *window, window_key_handler_t handler);

void
window_set_close_handler(struct window *window, window_close_handler_t handler);

void
window_set_screen_handler(struct window *window, window_screen_handler_t handler);

void
window_set_focus_handler(struct window *window, void (*handler)(struct window*, bool, void*), void *user_data);

void
window_set_move_handler(struct window *window, void (*handler)(struct window*, int, int, void*), void *user_data);

struct display *
window_get_display(struct window *window);

Display *
window_get_xdisplay(struct window *window);

Window
window_get_xwindow(struct window *window);

/* Wrapper functions for PortLink message handlers */
void
window_show(struct window *window);

void
window_hide(struct window *window);

void
window_minimize(struct window *window, bool minimize);

void
window_activate(struct window *window, bool active);

bool
window_is_front(struct window *window);

void
window_deferred_destroy(struct window *window);

void
window_set_user_data(struct window *window, void *data);

void *
window_get_user_data(struct window *window);

/* Get decorator sizes for X11 window: return left border width and top bar height in pixels */
void window_get_decorator_size(struct window *window, int *borderWidth, int *tabHeight);

/* Widget functions */
struct widget *
window_add_widget(struct window *window, void *data);

void
widget_deferred_destroy(struct widget *widget);

void
widget_set_redraw_handler(struct widget *widget,
			  widget_redraw_handler_t handler);

void
widget_set_resize_handler(struct widget *widget,
			  widget_resize_handler_t handler);

void
widget_set_button_handler(struct widget *widget,
			  widget_button_handler_t handler);

void
widget_set_motion_handler(struct widget *widget,
			  widget_motion_handler_t handler);

void
widget_set_axis_handler(struct widget *widget,
			widget_axis_handler_t handler);

void
widget_set_idle_handler(struct widget *widget,
			widget_idle_handler_t handler);

void
widget_get_allocation(struct widget *widget, struct rectangle *allocation);

void
widget_set_user_data(struct widget *widget, void *data);

cairo_t *
widget_cairo_create(struct widget *widget);

struct window *
widget_get_window(struct widget *widget);

void
window_get_mouse_position(struct window *window, int32_t *x, int32_t *y);

void
widget_schedule_redraw(struct widget *widget);


void *window_get_move_handler_data(struct window *window);
void *window_get_move_user_data(struct window *window);

void *window_get_focus_handler_data(struct window *window);
void *window_get_focus_user_data(struct window *window);

void
display_flush(struct display *display);

/* Clipboard functions */

/* Set clipboard content (text/plain format) */
int
display_set_clipboard_text(struct display *display, const char *text, size_t length);

status_t
display_set_app_watcher(struct display *display, display_app_watcher_t watcher,
	void *user_data);

status_t
display_clear_app_watcher(struct display *display);

status_t
display_get_app_info(struct display *display, int32_t team_id,
	cosmoe_backend_app_info *info);

int32_t
display_get_window_list(struct display *display, int32_t *window_ids,
	int32_t max_count);

status_t
display_get_window_info(struct display *display, int32_t window_id,
	struct cosmoe_backend_window_info *info);

status_t
display_activate_window(struct display *display, int32_t window_id);

status_t
display_minimize_window(struct display *display, int32_t window_id,
	bool minimize);

status_t
display_close_window(struct display *display, int32_t window_id);

/* Get clipboard content. Returns allocated string that caller must free(), or NULL if empty/unavailable */
char *
display_get_clipboard_text(struct display *display, size_t *length);

/* Fill up to max_count team IDs for app windows suitable for taskbar listing.
 * Returns the number of items written. */
int32_t
display_get_app_list(struct display *display, int32_t *team_ids, int32_t max_count);

#endif
