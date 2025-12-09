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

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <stdint.h>
#include <cairo/cairo.h>
#include <xkbcommon/xkbcommon.h>

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

typedef void (*widget_redraw_handler_t)(struct widget *widget, void *data);

typedef void (*widget_resize_handler_t)(struct widget *widget, int32_t width,
					int32_t height, void *data);

typedef void (*widget_button_handler_t)(struct widget *widget, struct input *input,
					uint32_t time, uint32_t button, uint32_t state,
					void *data);

typedef void (*widget_motion_handler_t)(struct widget *widget, struct input *input,
					uint32_t time, float x, float y, void *data);

typedef void (*widget_axis_handler_t)(struct widget *widget, struct input *input,
				      uint32_t time, uint32_t axis, double value, void *data);

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
		       struct widget *widget);

/* Window functions */
struct window *
window_create(struct display *display);

void
window_set_title(struct window *window, const char *title);

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

struct display *
window_get_display(struct window *window);

void
window_deferred_destroy(struct window *window);

void
window_set_user_data(struct window *window, void *data);

void *
window_get_user_data(struct window *window);

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
widget_get_allocation(struct widget *widget, struct rectangle *allocation);

cairo_t *
widget_cairo_create(struct widget *widget);

void *
widget_get_user_data(struct widget *widget);

struct window *
widget_get_window(struct widget *widget);

void
window_get_mouse_position(struct window *window, int32_t *x, int32_t *y);

void
widget_schedule_redraw(struct widget *widget);

void
display_flush(struct display *display);

#endif
