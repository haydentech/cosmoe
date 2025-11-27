/*
 * Copyright 2001-2025 Haiku, Inc. All rights reserved
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		Axel Dörfler, axeld@pinc-software.de
 *		Adrian Oanca, adioanca@cotty.iren.ro
 *		John Scipione, jscipione@gmail.com
 */


#include <Window.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <Application.h>
#include <AppMisc.h>
#include <ApplicationPrivate.h>
#include <Autolock.h>
#include <Bitmap.h>
#include <Button.h>
#include <Cursor.h>
#include <DirectMessageTarget.h>
#include <InputServerTypes.h>
#include <Layout.h>
#include <LayoutUtils.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MenuPrivate.h>
#include <MessagePrivate.h>
#include <MessageQueue.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <String.h>
#include <TextView.h>
#include <TokenSpace.h>
#include <ToolTipManager.h>
#include <UnicodeChar.h>
#include <WindowPrivate.h>

#include <binary_compatibility/Interface.h>
#include <input_globals.h>

#include <linux/input-event-codes.h>

//#define DEBUG_WIN
#ifdef DEBUG_WIN
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

#define B_HIDE_APPLICATION '_AHD'
	// if we ever move this to a public namespace, we should also move the
	// handling of this message into BApplication

#define _MINIMIZE_			'_WMZ'
#define _ZOOM_				'_WZO'
#define _SEND_BEHIND_		'_WSB'
#define _SEND_TO_FRONT_		'_WSF'


void do_minimize_team(BRect zoomRect, team_id team, bool zoom);


struct BWindow::unpack_cookie {
	unpack_cookie();

	BMessage*	message;
	int32		index;
	BHandler*	focus;
	int32		focus_token;
	int32		last_view_token;
	bool		found_focus;
	bool		tokens_scanned;
};


class BWindow::Shortcut {
public:
							Shortcut(uint32 key, uint32 modifiers,
								BMenuItem* item);
							Shortcut(uint32 key, uint32 modifiers,
								BMessage* message, BHandler* target);
							~Shortcut();

			bool			Matches(uint32 key, uint32 preparedModifiers) const;

			uint32			Key() const { return fKey; };
			uint32			Modifiers() const;
			uint32			PreparedModifiers() const { return fPreparedModifiers; };
			BMenuItem*		MenuItem() const { return fMenuItem; }
			BMessage*		Message() const { return fMessage; }
			BHandler*		Target() const { return fTarget; }

	static	uint32			AllowedModifiers();
	static	uint32			PrepareKey(uint32 key);
	static	uint32			PrepareModifiers(uint32 modifiers);

private:
			uint32			fKey;
			uint32			fPreparedModifiers;
			BMenuItem*		fMenuItem;
			BMessage*		fMessage;
			BHandler*		fTarget;
};


using BPrivate::gDefaultTokens;
using BPrivate::MenuPrivate;

static property_info sWindowPropInfo[] = {
	{
		"Active", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_BOOL_TYPE }
	},

	{
		"Feel", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_INT32_TYPE }
	},

	{
		"Flags", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_INT32_TYPE }
	},

	{
		"Frame", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_RECT_TYPE }
	},

	{
		"Hidden", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_BOOL_TYPE }
	},

	{
		"Look", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_INT32_TYPE }
	},

	{
		"Title", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_STRING_TYPE }
	},

	{
		"Workspaces", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_INT32_TYPE }
	},

	{
		"MenuBar", {},
		{ B_DIRECT_SPECIFIER }, NULL, 0, {}
	},

	{
		"View", { B_COUNT_PROPERTIES },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_INT32_TYPE }
	},

	{
		"View", {}, {}, NULL, 0, {}
	},

	{
		"Minimize", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_BOOL_TYPE }
	},

	{
		"TabFrame", { B_GET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, NULL, 0, { B_RECT_TYPE }
	},

	{ 0 }
};

static value_info sWindowValueInfo[] = {
	{
		"MoveTo", 'WDMT', B_COMMAND_KIND,
		"Moves to the position in the BPoint data"
	},

	{
		"MoveBy", 'WDMB', B_COMMAND_KIND,
		"Moves by the offsets in the BPoint data"
	},

	{
		"ResizeTo", 'WDRT', B_COMMAND_KIND,
		"Resize to the size in the BPoint data"
	},

	{
		"ResizeBy", 'WDRB', B_COMMAND_KIND,
		"Resize by the offsets in the BPoint data"
	},

	{ 0 }
};


void
_set_menu_sem_(BWindow* window, sem_id sem)
{
	if (window != NULL)
		window->fMenuSem = sem;
}


//	#pragma mark -


BWindow::unpack_cookie::unpack_cookie()
	:
	message((BMessage*)~0UL),
		// message == NULL is our exit condition
	index(0),
	focus_token(B_NULL_TOKEN),
	last_view_token(B_NULL_TOKEN),
	found_focus(false),
	tokens_scanned(false)
{
}


//	#pragma mark - BWindow::Shortcut


BWindow::Shortcut::Shortcut(uint32 key, uint32 modifiers, BMenuItem* item)
	:
	fKey(PrepareKey(key)),
	fPreparedModifiers(PrepareModifiers(modifiers)),
	fMenuItem(item),
	fMessage(NULL),
	fTarget(NULL)
{
}


BWindow::Shortcut::Shortcut(uint32 key, uint32 modifiers, BMessage* message,
	BHandler* target)
	:
	fKey(PrepareKey(key)),
	fPreparedModifiers(PrepareModifiers(modifiers)),
	fMenuItem(NULL),
	fMessage(message),
	fTarget(target)
{
}


BWindow::Shortcut::~Shortcut()
{
	// we own the message, if any
	delete fMessage;
}


bool
BWindow::Shortcut::Matches(uint32 key, uint32 preparedModifiers) const
{
	return fKey == key && fPreparedModifiers == preparedModifiers;
}


uint32
BWindow::Shortcut::Modifiers() const
{
	return fPreparedModifiers
		| (((fPreparedModifiers & B_COMMAND_KEY) == 0) ? B_NO_COMMAND_KEY : 0);
}


/*static*/
uint32
BWindow::Shortcut::AllowedModifiers()
{
	return B_COMMAND_KEY | B_OPTION_KEY | B_SHIFT_KEY | B_CONTROL_KEY | B_MENU_KEY;
}


/*static*/
uint32
BWindow::Shortcut::PrepareModifiers(uint32 modifiers)
{
	if ((modifiers & B_NO_COMMAND_KEY) != 0)
		return (modifiers & AllowedModifiers()) & ~B_CONTROL_KEY;
	else
		return (modifiers & AllowedModifiers()) | B_CONTROL_KEY;
}


/*static*/
uint32
BWindow::Shortcut::PrepareKey(uint32 key)
{
	return BUnicodeChar::ToUpper(key);
}


//	#pragma mark - BWindow


void
windowframe_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
    printf("windowframe_resize_handler w: %d h: %d\n", width, height);

	// Getting the allocation for the window frame allows us to
	// find the "origin" for the top view
	rectangle allocation;
	widget_get_allocation(widget, &allocation);

	BWindow* win = (BWindow*)data;

	if (win->fTopViewWidget) {
		widget_set_allocation(win->fTopViewWidget, WAYLAND_TOPVIEW_H_SLOP, WAYLAND_TOPVIEW_V_SLOP, allocation.width, allocation.height);
	}

	BMessage msg(B_WINDOW_RESIZED);
	msg.AddInt64("when", system_time());
	msg.AddInt32("width", allocation.width);
	msg.AddInt32("height", allocation.height);
	win->PostMessage(&msg, win);
}


static void
set_empty_input_region(struct widget *widget, struct display *display)
{
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	struct wl_region *region;

	compositor = display_get_compositor(display);
	surface = widget_get_wl_surface(widget);
	region = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(surface, region);
	wl_region_destroy(region);
}


static void
close_handler(void *data)
{
    printf("close_handler\n");
    BWindow* win = (BWindow*)data;

	BMessage message(B_QUIT_REQUESTED);
	status_t err = win->PostMessage(&message);
	if (err)
		printf("close_handler PostMessage err: %d\n", err);
}


void
view_redraw_handler(struct widget *widget, void *data)
{
    BView* view = (BView*)data;

	if (!view->IsHidden() && view->Window() && !view->Window()->UpdatesDisabled()) {
		if (view->ViewColor() != B_TRANSPARENT_COLOR) {
			rgb_color color = view->HighColor();
			view->SetHighColor(view->ViewColor());
			view->FillRect(view->Bounds());
			view->SetHighColor(color);
		}

		BMessage* msg = new BMessage(_UPDATE_);
		msg->AddInt64("when", system_time());
		msg->AddInt32("token", _get_object_token_(view));
		msg->AddRect("updateRect", view->Bounds());
		//view->Window()->AddMessage(msg);	// crashes
		view->Window()->DispatchMessage(msg, view->Window());

		// Get the widget's cairo surface
		if (view->Window()->fBackingSurface != NULL) {
			// Create a cairo context for the widget's surface
			cairo_t* cr = widget_cairo_create(widget);

			// Copy the contents of the BWindow's backing surface onto the widget's surface
			cairo_set_source_surface(cr, view->Window()->fBackingSurface, WAYLAND_TOPVIEW_H_SLOP, WAYLAND_TOPVIEW_V_SLOP);
			cairo_paint(cr);

			// Destroy the cairo context
			cairo_destroy(cr);
		}
	}
}

void view_button_handler(struct widget *widget,
	struct input *input, uint32_t time,
	uint32_t button,
	enum wl_pointer_button_state state,
	void *data)
{
	BView* view = (BView*)data;
	BView* subView;
	rectangle allocation;
	static uint32_t lastClickTime = 0;
	static uint32_t lastClickButton = 0;
	int32 clicks = 1;

	if (time - lastClickTime < 250 && lastClickButton == button && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		clicks++;
	}

	lastClickTime = time;
	lastClickButton = button;

	widget_get_allocation(widget, &allocation);

	// Convert the coordinates to be window-relative
	int32_t x, y;
	input_get_position(input, &x, &y);
	x -= allocation.x;
	y -= allocation.y;

	BMessage* msg = new BMessage((state == WL_POINTER_BUTTON_STATE_PRESSED) ? B_MOUSE_DOWN : B_MOUSE_UP);

	subView = view->fOwner->FindView(BPoint(x, y));
	if (subView) {
		view = subView;
	}

	int32 buttons = 0;
	if (button == BTN_LEFT)
		buttons = B_PRIMARY_MOUSE_BUTTON;
	else if (button == BTN_RIGHT)
		buttons = B_SECONDARY_MOUSE_BUTTON;
	else if (button == BTN_MIDDLE)
		buttons = B_TERTIARY_MOUSE_BUTTON;
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);
	msg->AddInt64("when", system_time());
	msg->AddInt32("waylandtime", time);
	msg->AddPointer("waylandinput", input);

	msg->AddInt32("buttons", buttons);
	msg->AddInt32("modifiers", modifiers());
	msg->AddPoint("screen_where", BPoint(x, y));
	msg->AddInt32("clicks", clicks);
	msg->AddInt32("_view_token", _get_object_token_(view));
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		msg->AddInt32("_token", _get_object_token_(view));
		msg->AddBool("_feed_focus", true);
	}
	view->Window()->AddMessage(msg);
}


int view_pointer_motion_handler(struct widget *widget,
	struct input *input, uint32_t time,
	float x, float y, void *data)
{
	BView* view = (BView*)data;	// This is fTopView
	int32 cursor = -1;
	BView* subView;
	rectangle allocation;

	widget_get_allocation(widget, &allocation);

	// Convert the coordinates to be window-relative
	x -= allocation.x;
	y -= allocation.y;

	view->sLastMousePosition.Set(x, y);

	BMessage* msg = new BMessage(B_MOUSE_MOVED);

	subView = view->fOwner->FindView(BPoint(x, y));
	if (subView) {
		view = subView;
		cursor = subView->CursorID();
	}

	if (view && view->Window()) {
		BMessage::Private messagePrivate(msg);
		messagePrivate.SetTarget(B_PREFERRED_TOKEN);
		msg->AddInt64("when", system_time());
		msg->AddPoint("screen_where", BPoint(x, y));
		msg->AddInt32("buttons", 0);
		msg->AddInt32("_view_token", _get_object_token_(view));
		view->Window()->AddMessage(msg);
	}

	// If not, do we have an app cursor?
	if (cursor < 0)
		cursor = be_app->CursorID();

	// If neither, use the default cursor
	if (cursor < 0)
		cursor = CURSOR_LEFT_PTR;
	else
		cursor = BCursorToWaylandCursor(cursor);

	return cursor;
}


void send_mouse_wheel(BView* view, float deltaX, float deltaY)
{
	printf("send_mouse_wheel(%f, %f)\n", deltaX, deltaY);
	if (!view->IsHidden() && view->Window() && !view->Window()->UpdatesDisabled()) {
		BMessage* msg = new BMessage(B_MOUSE_WHEEL_CHANGED);
		BMessage::Private messagePrivate(msg);
		messagePrivate.SetTarget(B_PREFERRED_TOKEN);
		msg->AddInt64("when", system_time());
		msg->AddFloat("be:wheel_delta_x", -1.0f * deltaX);
		msg->AddFloat("be:wheel_delta_y", -1.0f * deltaY);
		view->MessageReceived(msg);
		delete msg;
	}
}


void view_axis_handler(struct widget *widget, struct input *input, uint32_t time,
	uint32_t axis, wl_fixed_t value, void *data)
{
	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL || axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		BView* view = (BView*)data;
		BView* subView;
		rectangle allocation;
	
		widget_get_allocation(widget, &allocation);
	
		// Convert the coordinates to be window-relative
		int32_t x, y;
		input_get_position(input, &x, &y);
		x -= allocation.x;
		y -= allocation.y;
	
		subView = view->Window()->FindView(BPoint(x, y));
		if (subView) {
			view = subView;
		}

		float deltaX = (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ? wl_fixed_to_double(value) : 0.0f;
		float deltaY = (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ? wl_fixed_to_double(value) : 0.0f;

		send_mouse_wheel(subView, deltaX, deltaY);
	}
}


void BWindow::SendModifiersEvent(BWindow* win, uint32 modifiers, uint32 oldModifiers)
{
	BMessage* msg = new BMessage(B_MODIFIERS_CHANGED);
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);

	msg->AddInt64("when", real_time_clock());
	msg->AddInt32("be:old_modifiers", oldModifiers);
	msg->AddInt32("modifiers", modifiers);

	win->AddMessage(msg);
}

void BWindow::SendKeyEvent(BWindow* win, uint32 key, uint32 sym, int32 what, uint32 modifiers)
{
	char string[2];
	string[0] = sym;
	string[1] = 0;
	BMessage* msg = new BMessage(what);
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);

	msg->AddInt64("when", real_time_clock());
	msg->AddInt32("key", sym);
	msg->AddInt32("modifiers", modifiers);
	msg->AddInt8("byte", (int8)string[0]);
	msg->AddData("bytes", B_STRING_TYPE, string, 2);
	msg->AddInt8("raw_char", sym);
	if (what == B_KEY_DOWN)
		msg->AddInt32("be:key_repeat", 1);

	win->AddMessage(msg);
}


void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
	// Kept in interface.cpp
	uint32 newModifiers = modifiers();
	uint32 oldModifiers = newModifiers;

    printf("key_handler got key %d (%c)\n", sym, sym);

	int32 what = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? B_KEY_DOWN : B_KEY_UP;

	switch(key) {
		case KEY_LEFTSHIFT:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_LEFT_SHIFT_KEY | B_SHIFT_KEY;
			else {
				newModifiers &= ~B_LEFT_SHIFT_KEY;
				if ((newModifiers & B_RIGHT_SHIFT_KEY) == 0)
					newModifiers &= ~B_SHIFT_KEY;
			}
			break;

		case KEY_RIGHTSHIFT:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_RIGHT_SHIFT_KEY | B_SHIFT_KEY;
				else {
					newModifiers &= ~B_RIGHT_SHIFT_KEY;
				if ((newModifiers & B_LEFT_SHIFT_KEY) == 0)
					newModifiers &= ~B_SHIFT_KEY;
			}
			break;

		case KEY_LEFTCTRL:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_LEFT_CONTROL_KEY | B_CONTROL_KEY;
			else {
				newModifiers &= ~B_LEFT_CONTROL_KEY;
				if ((newModifiers & B_RIGHT_CONTROL_KEY) == 0)
					newModifiers &= ~B_CONTROL_KEY;
			}
			break;

		case KEY_RIGHTCTRL:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_RIGHT_CONTROL_KEY | B_CONTROL_KEY;
				else {
				newModifiers &= ~B_RIGHT_CONTROL_KEY;
				if ((newModifiers & B_LEFT_CONTROL_KEY) == 0)
					newModifiers &= ~B_CONTROL_KEY;
			}
			break;

		case KEY_LEFTALT:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				newModifiers |= B_LEFT_OPTION_KEY | B_OPTION_KEY;
				newModifiers |= B_LEFT_COMMAND_KEY | B_COMMAND_KEY;
			}
			else {
				newModifiers &= ~B_LEFT_OPTION_KEY;
				newModifiers &= ~B_LEFT_COMMAND_KEY;
				if ((newModifiers & B_RIGHT_OPTION_KEY) == 0)
					newModifiers &= ~B_OPTION_KEY;
				if ((newModifiers & B_RIGHT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
			}
			break;

		case KEY_RIGHTALT:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				newModifiers |= B_RIGHT_OPTION_KEY | B_OPTION_KEY;
				newModifiers |= B_RIGHT_COMMAND_KEY | B_COMMAND_KEY;
			}
			else {
				newModifiers &= ~B_RIGHT_OPTION_KEY;
				newModifiers &= ~B_RIGHT_COMMAND_KEY;
				if ((newModifiers & B_LEFT_OPTION_KEY) == 0)
					newModifiers &= ~B_OPTION_KEY;
				if ((newModifiers & B_LEFT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
			}
			break;

		case KEY_MENU:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_MENU_KEY;
			else
				newModifiers &= ~B_MENU_KEY;
			break;

		case KEY_CAPSLOCK:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_CAPS_LOCK;
			else
				newModifiers &= ~B_CAPS_LOCK;
			break;

		case KEY_SCROLLLOCK:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_SCROLL_LOCK;
			else
				newModifiers &= ~B_SCROLL_LOCK;
			break;

		case KEY_NUMLOCK:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_NUM_LOCK;
			else
				newModifiers &= ~B_NUM_LOCK;
			break;

		case KEY_RIGHT:
			sym = B_RIGHT_ARROW;
			break;

		case KEY_LEFT:
			sym = B_LEFT_ARROW;
			break;

		case KEY_UP:
			sym = B_UP_ARROW;
			break;

		case KEY_DOWN:
			sym = B_DOWN_ARROW;
			break;

		case KEY_DELETE:
			sym = B_DELETE;
			break;

		case KEY_HOME:
			sym = B_HOME;
			break;

		case KEY_END:
			sym = B_END;
			break;

		case KEY_PAGEUP:
			sym = B_PAGE_UP;
			break;

		case KEY_PAGEDOWN:
			sym = B_PAGE_DOWN;
			break;

		case KEY_INSERT:
			sym = B_INSERT;
			break;

		default:
			// Key was not a modifier key or remapped key
			break;
	}

	if (newModifiers != oldModifiers) {
		set_modifiers(newModifiers);
		BWindow::SendModifiersEvent((BWindow*)data, newModifiers, oldModifiers);
	} else {
		BWindow::SendKeyEvent((BWindow*)data, key, sym, what, newModifiers);
	}
}

thread_id BWindow::sDisplayThread = -1;

BWindow::BWindow(BRect frame, const char* title, window_type type,
		uint32 flags, uint32 workspace)
	:
	BLooper(title, B_DISPLAY_PRIORITY)
{
	window_look look;
	window_feel feel;
	_DecomposeType(type, &look, &feel);

	_InitData(frame, title, look, feel, flags, workspace);
}


BWindow::BWindow(BRect frame, const char* title, window_look look,
		window_feel feel, uint32 flags, uint32 workspace)
	:
	BLooper(title, B_DISPLAY_PRIORITY)
{
	_InitData(frame, title, look, feel, flags, workspace);
}


BWindow::BWindow(BMessage* data)
	:
	BLooper(data)
{
	data->FindRect("_frame", &fFrame);

	const char* title;
	data->FindString("_title", &title);

	window_look look;
	data->FindInt32("_wlook", (int32*)&look);

	window_feel feel;
	data->FindInt32("_wfeel", (int32*)&feel);

	if (data->FindInt32("_flags", (int32*)&fFlags) != B_OK)
		fFlags = 0;

	uint32 workspaces;
	data->FindInt32("_wspace", (int32*)&workspaces);

	uint32 type;
	if (data->FindInt32("_type", (int32*)&type) == B_OK)
		_DecomposeType((window_type)type, &fLook, &fFeel);

		// connect to app_server and initialize data
	_InitData(fFrame, title, look, feel, fFlags, workspaces);

	if (data->FindFloat("_zoom", 0, &fMaxZoomWidth) == B_OK
		&& data->FindFloat("_zoom", 1, &fMaxZoomHeight) == B_OK)
		SetZoomLimits(fMaxZoomWidth, fMaxZoomHeight);

	if (data->FindFloat("_sizel", 0, &fMinWidth) == B_OK
		&& data->FindFloat("_sizel", 1, &fMinHeight) == B_OK
		&& data->FindFloat("_sizel", 2, &fMaxWidth) == B_OK
		&& data->FindFloat("_sizel", 3, &fMaxHeight) == B_OK)
		SetSizeLimits(fMinWidth, fMaxWidth,
			fMinHeight, fMaxHeight);

	if (data->FindInt64("_pulse", &fPulseRate) == B_OK)
		SetPulseRate(fPulseRate);

	BMessage msg;
	int32 i = 0;
	while (data->FindMessage("_views", i++, &msg) == B_OK) {
		BArchivable* obj = instantiate_object(&msg);
		if (BView* child = dynamic_cast<BView*>(obj))
			AddChild(child);
	}
}


BWindow::BWindow(BRect frame, int32 bitmapToken)
	:
	BLooper("offscreen bitmap")
{
	_DecomposeType(B_UNTYPED_WINDOW, &fLook, &fFeel);
	_InitData(frame, "offscreen", fLook, fFeel, 0, 0, bitmapToken);
}


BWindow::~BWindow()
{
	if (BMenu* menu = dynamic_cast<BMenu*>(fFocus)) {
		MenuPrivate(menu).QuitTracking();
	}

	// The BWindow is locked when the destructor is called,
	// we need to unlock because the menubar thread tries
	// to post a message, which will deadlock otherwise.
	// TODO: I replaced Unlock() with UnlockFully() because the window
	// was kept locked after that in case it was closed using ALT-W.
	// There might be an extra Lock() somewhere in the quitting path...
	UnlockFully();

	// Wait if a menu is still tracking
	if (fMenuSem > 0) {
		while (acquire_sem(fMenuSem) == B_INTERRUPTED)
			;
	}

	Lock();

	fTopView->RemoveSelf();
	delete fTopView;

	// remove all remaining shortcuts
	int32 shortcutCount = fShortcuts.CountItems();
	for (int32 i = 0; i < shortcutCount; i++)
		delete (Shortcut*)fShortcuts.ItemAtFast(i);

	// TODO: release other dynamically-allocated objects
	free(fTitle);

	// disable pulsing
	SetPulseRate(0);

	// Fixme: combine this code with _SendShowOrHideMessage
	if (fWaylandWindow) {
		widget_deferred_destroy(fTopViewWidget);

		if (fWaylandWindowframeWidget) {
			widget_deferred_destroy(fWaylandWindowframeWidget);
			fWaylandWindowframeWidget = NULL;
		}

		window_deferred_destroy(fWaylandWindow);
		fWaylandWindow = NULL;
	}

	if (fBackingSurface != NULL) {
		cairo_surface_destroy(fBackingSurface);
		fBackingSurface = NULL;
	}
}


BArchivable*
BWindow::Instantiate(BMessage* data)
{
	if (!validate_instantiation(data, "BWindow"))
		return NULL;

	return new(std::nothrow) BWindow(data);
}


status_t
BWindow::Archive(BMessage* data, bool deep) const
{
	status_t ret = BLooper::Archive(data, deep);

	if (ret == B_OK)
		ret = data->AddRect("_frame", fFrame);
	if (ret == B_OK)
		ret = data->AddString("_title", fTitle);
	if (ret == B_OK)
		ret = data->AddInt32("_wlook", fLook);
	if (ret == B_OK)
		ret = data->AddInt32("_wfeel", fFeel);
	if (ret == B_OK && fFlags != 0)
		ret = data->AddInt32("_flags", fFlags);
	//if (ret == B_OK)
	//	ret = data->AddInt32("_wspace", (uint32)Workspaces());

	if (ret == B_OK && !_ComposeType(fLook, fFeel))
		ret = data->AddInt32("_type", (uint32)Type());

	if (fMaxZoomWidth != 32768.0 || fMaxZoomHeight != 32768.0) {
		if (ret == B_OK)
			ret = data->AddFloat("_zoom", fMaxZoomWidth);
		if (ret == B_OK)
			ret = data->AddFloat("_zoom", fMaxZoomHeight);
	}

	if (fMinWidth != 0.0 || fMinHeight != 0.0
		|| fMaxWidth != 32768.0 || fMaxHeight != 32768.0) {
		if (ret == B_OK)
			ret = data->AddFloat("_sizel", fMinWidth);
		if (ret == B_OK)
			ret = data->AddFloat("_sizel", fMinHeight);
		if (ret == B_OK)
			ret = data->AddFloat("_sizel", fMaxWidth);
		if (ret == B_OK)
			ret = data->AddFloat("_sizel", fMaxHeight);
	}

	if (ret == B_OK && fPulseRate != 500000)
		data->AddInt64("_pulse", fPulseRate);

	if (ret == B_OK && deep) {
		int32 noOfViews = CountChildren();
		for (int32 i = 0; i < noOfViews; i++){
			BMessage childArchive;
			ret = ChildAt(i)->Archive(&childArchive, true);
			if (ret == B_OK)
				ret = data->AddMessage("_views", &childArchive);
			if (ret != B_OK)
				break;
		}
	}

	return ret;
}


void
BWindow::Quit()
{
	if (!IsLocked()) {
		const char* name = Name();
		if (name == NULL)
			name = "no-name";

		printf("ERROR - you must Lock a looper before calling Quit(), "
			   "team=%" B_PRId32 ", looper=%s\n", Team(), name);
	}

	// Try to lock
	if (!Lock()){
		// We're toast already
		return;
	}

	while (!IsHidden())	{
		Hide();
	}

	if (fFlags & B_QUIT_ON_WINDOW_CLOSE)
		be_app->PostMessage(B_QUIT_REQUESTED);

	BLooper::Quit();
}


void
BWindow::AddChild(BView* child, BView* before)
{
	BAutolock locker(this);
	if (locker.IsLocked())
		fTopView->AddChild(child, before);
}


void
BWindow::AddChild(BLayoutItem* child)
{
	BAutolock locker(this);
	if (locker.IsLocked())
		fTopView->AddChild(child);
}


bool
BWindow::RemoveChild(BView* child)
{
	BAutolock locker(this);
	if (!locker.IsLocked())
		return false;

	return fTopView->RemoveChild(child);
}


int32
BWindow::CountChildren() const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return 0;

	return fTopView->CountChildren();
}


BView*
BWindow::ChildAt(int32 index) const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return NULL;

	return fTopView->ChildAt(index);
}


void
BWindow::Minimize(bool minimize)
{
	if (IsModal() || IsFloating() || IsHidden() || fMinimized == minimize
		|| !Lock())
		return;

	fMinimized = minimize;

	Unlock();
}


status_t
BWindow::SendBehind(const BWindow* window)
{
	return B_ERROR;
}


void
BWindow::Flush() const
{
	// no-op for Cosmoe on Wayland
}


void
BWindow::Sync() const
{
	// no-op for Cosmoe on Wayland
}


void
BWindow::DisableUpdates()
{
	fUpdatesDisabled = true;
}


void
BWindow::EnableUpdates()
{
	fUpdatesDisabled = false;
}


bool
BWindow::UpdatesDisabled() const
{
	return fUpdatesDisabled || fTopViewWidget == NULL;
}


void
BWindow::BeginViewTransaction()
{
	if (Lock()) {
		fInTransaction = true;
		Unlock();
	}
}


void
BWindow::EndViewTransaction()
{
	if (Lock()) {
		fInTransaction = false;
		Unlock();
	}
}


bool
BWindow::InViewTransaction() const
{
	BAutolock locker(const_cast<BWindow*>(this));
	return fInTransaction;
}

void
BWindow::MessageReceived(BMessage* message)
{
	//printf("*** BWindow::MessageReceived\n");
	//fflush(stdout);
	if (!message->HasSpecifiers()) {
		if (message->what == B_KEY_DOWN)
			_KeyboardNavigation();

		return BLooper::MessageReceived(message);
	}

	BMessage replyMsg(B_REPLY);
	bool handled = false;

	BMessage specifier;
	int32 what;
	const char* prop;
	int32 index;

	if (message->GetCurrentSpecifier(&index, &specifier, &what, &prop) != B_OK)
		return BLooper::MessageReceived(message);

	BPropertyInfo propertyInfo(sWindowPropInfo);
	switch (propertyInfo.FindMatch(message, index, &specifier, what, prop)) {
		case 0:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddBool("result", IsActive());
				handled = true;
			} else if (message->what == B_SET_PROPERTY) {
				bool newActive;
				if (message->FindBool("data", &newActive) == B_OK) {
					Activate(newActive);
					handled = true;
				}
			}
			break;
		case 1:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddInt32("result", (uint32)Feel());
				handled = true;
			} else {
				uint32 newFeel;
				if (message->FindInt32("data", (int32*)&newFeel) == B_OK) {
					SetFeel((window_feel)newFeel);
					handled = true;
				}
			}
			break;
		case 2:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddInt32("result", Flags());
				handled = true;
			} else {
				uint32 newFlags;
				if (message->FindInt32("data", (int32*)&newFlags) == B_OK) {
					SetFlags(newFlags);
					handled = true;
				}
			}
			break;
		case 3:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddRect("result", Frame());
				handled = true;
			} else {
				BRect newFrame;
				if (message->FindRect("data", &newFrame) == B_OK) {
					MoveTo(newFrame.LeftTop());
					ResizeTo(newFrame.Width(), newFrame.Height());
					handled = true;
				}
			}
			break;
		case 4:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddBool("result", IsHidden());
				handled = true;
			} else {
				bool hide;
				if (message->FindBool("data", &hide) == B_OK) {
					if (hide) {
						if (!IsHidden())
							Hide();
					} else if (IsHidden())
						Show();
					handled = true;
				}
			}
			break;
		case 5:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddInt32("result", (uint32)Look());
				handled = true;
			} else {
				uint32 newLook;
				if (message->FindInt32("data", (int32*)&newLook) == B_OK) {
					SetLook((window_look)newLook);
					handled = true;
				}
			}
			break;
		case 6:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddString("result", Title());
				handled = true;
			} else {
				const char* newTitle = NULL;
				if (message->FindString("data", &newTitle) == B_OK) {
					SetTitle(newTitle);
					handled = true;
				}
			}
			break;
		case 7:
			break;
		case 11:
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddBool("result", IsMinimized());
				handled = true;
			} else {
				bool minimize;
				if (message->FindBool("data", &minimize) == B_OK) {
					Minimize(minimize);
					handled = true;
				}
			}
			break;
		case 12:
			break;
		default:
			return BLooper::MessageReceived(message);
	}

	if (handled) {
		if (message->what == B_SET_PROPERTY)
			replyMsg.AddInt32("error", B_OK);
	} else {
		replyMsg.what = B_MESSAGE_NOT_UNDERSTOOD;
		replyMsg.AddInt32("error", B_BAD_SCRIPT_SYNTAX);
		replyMsg.AddString("message", "Didn't understand the specifier(s)");
	}
	message->SendReply(&replyMsg);
}


void
BWindow::DispatchMessage(BMessage* message, BHandler* target)
{
	if (message == NULL)
		return;

	// if (message->what != B_MOUSE_MOVED) {
	// 	printf("+++BWindow::DispatchMessage %c%c%c%c\n", message->what >> 24,
	// 		(message->what >> 16) & 0xFF, (message->what >> 8) & 0xFF,
	// 		message->what & 0xFF);
	// 	fflush(stdout);
	// }

	switch (message->what) {
		case B_ZOOM:
			Zoom();
			break;

		case _MINIMIZE_:
			// Used by the minimize shortcut
			if ((Flags() & B_NOT_MINIMIZABLE) == 0)
				Minimize(true);
			break;

		case _ZOOM_:
			// Used by the zoom shortcut
			if ((Flags() & B_NOT_ZOOMABLE) == 0)
				Zoom();
			break;

		case _SEND_BEHIND_:
			SendBehind(NULL);
			break;

		case _SEND_TO_FRONT_:
			Activate();
			break;

		case B_MINIMIZE:
		{
			bool minimize;
			if (message->FindBool("minimize", &minimize) == B_OK)
				Minimize(minimize);
			break;
		}

		case B_HIDE_APPLICATION:
		{
			// Hide all applications with the same signature
			// (ie. those that are part of the same group to be consistent
			// to what the Deskbar shows you).
			// app_info info;
			// be_app->GetAppInfo(&info);

			// BList list;
			// be_roster->GetAppList(info.signature, &list);

			// for (int32 i = 0; i < list.CountItems(); i++) {
			// 	do_minimize_team(BRect(), (team_id)(addr_t)list.ItemAt(i),
			// 		false);
			// }
			break;
		}

		case B_WINDOW_RESIZED:
		{
			int32 width, height;
			if (message->FindInt32("width", &width) == B_OK
				&& message->FindInt32("height", &height) == B_OK) {
				// combine with pending resize notifications
				BMessage* pendingMessage;
				while ((pendingMessage
						= MessageQueue()->FindMessage(B_WINDOW_RESIZED, 0))) {
					int32 nextWidth;
					if (pendingMessage->FindInt32("width", &nextWidth) == B_OK)
						width = nextWidth;

					int32 nextHeight;
					if (pendingMessage->FindInt32("height", &nextHeight)
							== B_OK) {
						height = nextHeight;
					}

					MessageQueue()->RemoveMessage(pendingMessage);
					delete pendingMessage;
						// this deletes the first *additional* message
						// fCurrentMessage is safe
				}
				if (width != fFrame.Width() || height != fFrame.Height()) {
					// NOTE: we might have already handled the resize
					// in an _UPDATE_ message
					fFrame.right = fFrame.left + width;
					fFrame.bottom = fFrame.top + height;

					_AdoptResize();
//					FrameResized(width, height);
				}
// call hook function anyways
// TODO: When a window is resized programmatically,
// it receives this message, and maybe it is wise to
// keep the asynchronous nature of this process to
// not risk breaking any apps.
FrameResized(width, height);
			}
			break;
		}

		case B_WINDOW_MOVED:
		{
			BPoint origin;
			if (message->FindPoint("where", &origin) == B_OK) {
				if (fFrame.LeftTop() != origin) {
					// NOTE: we might have already handled the move
					// in an _UPDATE_ message
					fFrame.OffsetTo(origin);

//					FrameMoved(origin);
				}
// call hook function anyways
// TODO: When a window is moved programmatically,
// it receives this message, and maybe it is wise to
// keep the asynchronous nature of this process to
// not risk breaking any apps.
FrameMoved(origin);
			}
			break;
		}

		case B_WINDOW_ACTIVATED:
			if (target != this) {
				target->MessageReceived(message);
				break;
			}

			bool active;
			if (message->FindBool("active", &active) != B_OK)
				break;

			// find latest activation message

			while (true) {
				BMessage* pendingMessage = MessageQueue()->FindMessage(
					B_WINDOW_ACTIVATED, 0);
				if (pendingMessage == NULL)
					break;

				bool nextActive;
				if (pendingMessage->FindBool("active", &nextActive) == B_OK)
					active = nextActive;

				MessageQueue()->RemoveMessage(pendingMessage);
				delete pendingMessage;
			}

			if (active != fActive) {
				fActive = active;

				WindowActivated(active);

				// call hook function 'WindowActivated(bool)' for all
				// views attached to this window.
				fTopView->_Activate(active);

				// we notify the input server if we are gaining or losing focus
				// from a view which has the B_INPUT_METHOD_AWARE on a window
				// activation
				if (!active)
					break;
				bool inputMethodAware = false;
				if (fFocus)
					inputMethodAware = fFocus->Flags() & B_INPUT_METHOD_AWARE;
				BMessage message(inputMethodAware ? IS_FOCUS_IM_AWARE_VIEW : IS_UNFOCUS_IM_AWARE_VIEW);
				BMessenger messenger(fFocus);
				BMessage reply;
				if (fFocus)
					message.AddMessenger("view", messenger);
				_control_input_server_(&message, &reply);
			}
			break;

		case B_SCREEN_CHANGED:
			if (target == this) {
				BRect frame;
				uint32 mode;
				if (message->FindRect("frame", &frame) == B_OK
					&& message->FindInt32("mode", (int32*)&mode) == B_OK) {
					_PropagateMessageToChildViews(message);
					// call hook method
					//ScreenChanged(frame, (color_space)mode);
				}
			} else
				target->MessageReceived(message);
			break;

		case B_WORKSPACE_ACTIVATED:
			if (target == this) {
				uint32 workspace;
				bool active;
				if (message->FindInt32("workspace", (int32*)&workspace) == B_OK
					&& message->FindBool("active", &active) == B_OK) {
					_PropagateMessageToChildViews(message);
					// call hook method
					WorkspaceActivated(workspace, active);
				}
			} else
				target->MessageReceived(message);
			break;

		case B_WORKSPACES_CHANGED:
			if (target == this) {
				uint32 oldWorkspace;
				uint32 newWorkspace;
				if (message->FindInt32("old", (int32*)&oldWorkspace) == B_OK
					&& message->FindInt32("new", (int32*)&newWorkspace) == B_OK) {
					_PropagateMessageToChildViews(message);
					// call hook method
					WorkspacesChanged(oldWorkspace, newWorkspace);
				}
			} else
				target->MessageReceived(message);
			break;

		case B_KEY_DOWN:
			if (!_HandleKeyDown(message))
				target->MessageReceived(message);
			break;

		case B_UNMAPPED_KEY_DOWN:
			if (!_HandleUnmappedKeyDown(message))
				target->MessageReceived(message);
			break;

		case B_PULSE:
			if (target == this && fPulseRunner) {
				fTopView->_Pulse();
				//fLink->Flush();
			} else
				target->MessageReceived(message);
			break;

		case _UPDATE_:
		{
//bigtime_t now = system_time();
//bigtime_t drawTime = 0;
			STRACE(("info:BWindow handling _UPDATE_.\n"));

			fInTransaction = true;

			{

				// read tokens for views that need to be drawn
				// NOTE: we need to read the tokens completely
				// first, we cannot draw views in between reading
				// the tokens, since other communication would likely
				// mess up the data in the link.
				struct ViewUpdateInfo {
					int32 token;
					BRect updateRect;
				};
				BList infos(20);
				while (true) {
					// read next token and create/add ViewUpdateInfo

					ViewUpdateInfo* info = new(std::nothrow) ViewUpdateInfo;
					if (info == NULL || !infos.AddItem(info)) {
						delete info;
						break;
					}

					if (message->FindInt32("token", (int32*)&info->token) != B_OK
						|| message->FindRect("updateRect", (BRect*)&info->updateRect) != B_OK) {
						printf("_UPDATE_ - error reading token or updateRect\n");
					}

					// Try to keep the multi-view code structure, even though we are
					// currently only doing 1 view at a time.
					break;
				}

				// draw
				int32 count = infos.CountItems();
				for (int32 i = 0; i < count; i++) {
//bigtime_t drawStart = system_time();
					ViewUpdateInfo* info
						= (ViewUpdateInfo*)infos.ItemAtFast(i);
					if (BView* view = _FindView(info->token))
						view->_Draw(info->updateRect);
					else {
						printf("_UPDATE_ - didn't find view by token: %"
							B_PRId32 "\n", info->token);
					}
//drawTime += system_time() - drawStart;
				}
				// NOTE: The tokens are actually hirachically sorted,
				// so traversing the list in revers and calling
				// child->_DrawAfterChildren() actually works like intended.
				for (int32 i = count - 1; i >= 0; i--) {
					ViewUpdateInfo* info
						= (ViewUpdateInfo*)infos.ItemAtFast(i);
					if (BView* view = _FindView(info->token))
						view->_DrawAfterChildren(info->updateRect);
					delete info;
				}

//printf("  %ld views drawn, total Draw() time: %lld\n", count, drawTime);
			}

			//fLink->StartMessage(AS_END_UPDATE);
			//fLink->Flush();
			fInTransaction = false;
			fUpdateRequested = false;

//printf("BWindow(%s) - UPDATE took %lld usecs\n", Title(), system_time() - now);
			break;
		}

		case _MENUS_DONE_:
			MenusEnded();
			break;

		// These two are obviously some kind of old scripting messages
		// this is NOT an app_server message and we have to be cautious
		case B_WINDOW_MOVE_BY:
		{
			BPoint offset;
			if (message->FindPoint("data", &offset) == B_OK)
				MoveBy(offset.x, offset.y);
			else
				message->SendReply(B_MESSAGE_NOT_UNDERSTOOD);
			break;
		}

		// this is NOT an app_server message and we have to be cautious
		case B_WINDOW_MOVE_TO:
		{
			BPoint origin;
			if (message->FindPoint("data", &origin) == B_OK)
				MoveTo(origin);
			else
				message->SendReply(B_MESSAGE_NOT_UNDERSTOOD);
			break;
		}

		case B_LAYOUT_WINDOW:
		{
			Layout(false);
			break;
		}

		case B_COLORS_UPDATED:
		{
			fTopView->_ColorsUpdated(message);
			target->MessageReceived(message);
			break;
		}

		case B_FONTS_UPDATED:
		{
			fTopView->_FontsUpdated(message);
			target->MessageReceived(message);
			break;
		}

		default:
			BLooper::DispatchMessage(message, target);
			break;
	}
}


void
BWindow::FrameMoved(BPoint newPosition)
{
	// does nothing
	// Hook function
}


void
BWindow::FrameResized(float newWidth, float newHeight)
{
	// does nothing
	// Hook function
}


void
BWindow::WorkspacesChanged(uint32 oldWorkspaces, uint32 newWorkspaces)
{
	// does nothing
	// Hook function
}


void
BWindow::WorkspaceActivated(int32 workspace, bool state)
{
	// does nothing
	// Hook function
}


void
BWindow::MenusBeginning()
{
	// does nothing
	// Hook function
}


void
BWindow::MenusEnded()
{
	// does nothing
	// Hook function
}


void
BWindow::SetSizeLimits(float minWidth, float maxWidth,
	float minHeight, float maxHeight)
{
	if (minWidth > maxWidth || minHeight > maxHeight)
		return;

	if (!Lock())
		return;

	if (fWaylandWindow) {
		window_set_min_max_allocation(fWaylandWindow,
			minWidth + WAYLAND_WINDOW_H_SLOP,
			minHeight + WAYLAND_WINDOW_V_SLOP,
			maxWidth + WAYLAND_WINDOW_H_SLOP,
			maxHeight + WAYLAND_WINDOW_V_SLOP);
	}

	_AdoptResize();
			// TODO: the same has to be done for SetLook() (that can alter
			//		the size limits, and hence, the size of the window
	Unlock();
}


void
BWindow::GetSizeLimits(float* _minWidth, float* _maxWidth, float* _minHeight,
	float* _maxHeight)
{
	// TODO: What about locking?!?
	if (_minHeight != NULL)
		*_minHeight = fMinHeight;
	if (_minWidth != NULL)
		*_minWidth = fMinWidth;
	if (_maxHeight != NULL)
		*_maxHeight = fMaxHeight;
	if (_maxWidth != NULL)
		*_maxWidth = fMaxWidth;
}


void
BWindow::UpdateSizeLimits()
{
	BAutolock locker(this);

	if ((fFlags & B_AUTO_UPDATE_SIZE_LIMITS) != 0) {
		// Get min/max constraints of the top view and enforce window
		// size limits respectively.
		BSize minSize = fTopView->MinSize();
		BSize maxSize = fTopView->MaxSize();
		SetSizeLimits(minSize.width, maxSize.width,
			minSize.height, maxSize.height);
	}
}


void
BWindow::SetZoomLimits(float maxWidth, float maxHeight)
{
	// TODO: What about locking?!?
	if (maxWidth > fMaxWidth)
		maxWidth = fMaxWidth;
	fMaxZoomWidth = maxWidth;

	if (maxHeight > fMaxHeight)
		maxHeight = fMaxHeight;
	fMaxZoomHeight = maxHeight;
}


void
BWindow::Zoom(BPoint origin, float width, float height)
{
	// the default implementation of this hook function
	// just does the obvious:
	MoveTo(origin);
	ResizeTo(width, height);
}


void
BWindow::Zoom()
{
	// TODO: What about locking?!?

	// From BeBook:
	// The dimensions that non-virtual Zoom() passes to hook Zoom() are deduced
	// from the smallest of three rectangles:

	// 1) the rectangle defined by SetZoomLimits() and,
	// 2) the rectangle defined by SetSizeLimits()
	//float maxZoomWidth = std::min(fMaxZoomWidth, fMaxWidth);
	//float maxZoomHeight = std::min(fMaxZoomHeight, fMaxHeight);

	// 3) the screen rectangle
	// BRect screenFrame = (BScreen(this)).Frame();
	// maxZoomWidth = std::min(maxZoomWidth, screenFrame.Width());
	// maxZoomHeight = std::min(maxZoomHeight, screenFrame.Height());

	// BRect zoomArea = screenFrame; // starts at screen size

	// BDeskbar deskbar;
	// BRect deskbarFrame = deskbar.Frame();
	// bool isShiftDown = (modifiers() & B_SHIFT_KEY) != 0;
	// if (!isShiftDown && !deskbar.IsAutoHide()) {
	// 	// remove area taken up by Deskbar unless hidden or shift is held down
	// 	switch (deskbar.Location()) {
	// 		case B_DESKBAR_TOP:
	// 			zoomArea.top = deskbarFrame.bottom + 2;
	// 			break;

	// 		case B_DESKBAR_BOTTOM:
	// 		case B_DESKBAR_LEFT_BOTTOM:
	// 		case B_DESKBAR_RIGHT_BOTTOM:
	// 			zoomArea.bottom = deskbarFrame.top - 2;
	// 			break;

	// 		// in vertical expando mode only if not always-on-top or auto-raise
	// 		case B_DESKBAR_LEFT_TOP:
	// 			if (!deskbar.IsExpanded())
	// 				zoomArea.top = deskbarFrame.bottom + 2;
	// 			else if (!deskbar.IsAlwaysOnTop() && !deskbar.IsAutoRaise())
	// 				zoomArea.left = deskbarFrame.right + 2;
	// 			break;

	// 		default:
	// 		case B_DESKBAR_RIGHT_TOP:
	// 			if (!deskbar.IsExpanded())
	// 				break;
	// 			else if (!deskbar.IsAlwaysOnTop() && !deskbar.IsAutoRaise())
	// 				zoomArea.right = deskbarFrame.left - 2;
	// 			break;
	// 	}
	// }

	// // TODO: Broken for tab on left side windows...
	// float borderWidth;
	// float tabHeight;
	// _GetDecoratorSize(&borderWidth, &tabHeight);

	// // remove the area taken up by the tab and border
	// zoomArea.left += borderWidth;
	// zoomArea.top += borderWidth + tabHeight;
	// zoomArea.right -= borderWidth;
	// zoomArea.bottom -= borderWidth;

	// // inset towards center vertically first to see if there will be room
	// // above or below Deskbar
	// if (zoomArea.Height() > maxZoomHeight)
	// 	zoomArea.InsetBy(0, roundf((zoomArea.Height() - maxZoomHeight) / 2));

	// if (zoomArea.top > deskbarFrame.bottom
	// 	|| zoomArea.bottom < deskbarFrame.top) {
	// 	// there is room above or below Deskbar, start from screen width
	// 	// minus borders instead of desktop width minus borders
	// 	zoomArea.left = screenFrame.left + borderWidth;
	// 	zoomArea.right = screenFrame.right - borderWidth;
	// }

	// // inset towards center
	// if (zoomArea.Width() > maxZoomWidth)
	// 	zoomArea.InsetBy(roundf((zoomArea.Width() - maxZoomWidth) / 2), 0);

	// Un-Zoom

	if (fPreviousFrame.IsValid()
		// NOTE: don't check for fFrame.LeftTop() == zoomArea.LeftTop()
		// -> makes it easier on the user to get a window back into place
		//&& fFrame.Width() == zoomArea.Width()
		//&& fFrame.Height() == zoomArea.Height()
		) {
		// already zoomed!
		Zoom(fPreviousFrame.LeftTop(), fPreviousFrame.Width(),
			fPreviousFrame.Height());
		return;
	}

	// Zoom

	// remember fFrame for later "unzooming"
	fPreviousFrame = fFrame;

	//Zoom(zoomArea.LeftTop(), zoomArea.Width(), zoomArea.Height());
}

void
BWindow::SetPulseRate(bigtime_t rate)
{
	// TODO: What about locking?!?
	if (rate < 0
		|| (rate == fPulseRate && !((rate == 0) ^ (fPulseRunner == NULL))))
		return;

	fPulseRate = rate;

	if (rate > 0) {
		if (fPulseRunner == NULL) {
			BMessage message(B_PULSE);
			fPulseRunner = new(std::nothrow) BMessageRunner(BMessenger(this),
				&message, rate);
		} else {
			fPulseRunner->SetInterval(rate);
		}
	} else {
		// rate == 0
		delete fPulseRunner;
		fPulseRunner = NULL;
	}
}


bigtime_t
BWindow::PulseRate() const
{
	return fPulseRate;
}


//! \brief Used by BMenuItem to add its shortcut to the window.
void
BWindow::_AddShortcut(uint32* _key, uint32* _modifiers, BMenuItem* item)
{
	Shortcut* shortcut = new(std::nothrow) Shortcut(*_key, *_modifiers, item);
	if (shortcut == NULL)
		return;

	// removes the shortcut if it already exists!
	RemoveShortcut(shortcut->Key(), shortcut->Modifiers());

	// pass the prepared key and modifiers back to caller
	*_key = shortcut->Key();
	*_modifiers = shortcut->Modifiers();

	fShortcuts.AddItem(shortcut);
}


void
BWindow::AddShortcut(uint32 key, uint32 modifiers, BMessage* message)
{
	AddShortcut(key, modifiers, message, this);
}


void
BWindow::AddShortcut(uint32 key, uint32 modifiers, BMessage* message, BHandler* target)
{
	if (message == NULL)
		return;

	Shortcut* shortcut = new(std::nothrow) Shortcut(key, modifiers, message, target);
	if (shortcut == NULL)
		return;

	// removes the shortcut if it already exists!
	RemoveShortcut(shortcut->Key(), shortcut->Modifiers());

	fShortcuts.AddItem(shortcut);
}


bool
BWindow::HasShortcut(uint32 key, uint32 modifiers)
{
	return _FindShortcut(key, modifiers) != NULL;
}


void
BWindow::RemoveShortcut(uint32 key, uint32 modifiers)
{
	Shortcut* shortcut = _FindShortcut(key, modifiers);
	if (shortcut != NULL && fShortcuts.RemoveItem(shortcut))
		delete shortcut;
	else if (key == 'Q' && modifiers == B_CONTROL_KEY)
		fNoQuitShortcut = true; // the quit shortcut is a fake shortcut
}


BButton*
BWindow::DefaultButton() const
{
	// TODO: What about locking?!?
	return fDefaultButton;
}


void
BWindow::SetDefaultButton(BButton* button)
{
	// TODO: What about locking?!?
	if (fDefaultButton == button)
		return;

	if (fDefaultButton != NULL) {
		// tell old button it's no longer the default one
		BButton* oldDefault = fDefaultButton;
		oldDefault->MakeDefault(false);
		oldDefault->Invalidate();
	}

	fDefaultButton = button;

	if (button != NULL) {
		// notify new default button
		fDefaultButton->MakeDefault(true);
		fDefaultButton->Invalidate();
	}
}


bool
BWindow::NeedsUpdate() const
{
	if (!const_cast<BWindow*>(this)->Lock())
		return false;

	// TODO Needed?

	return true;
}


void
BWindow::UpdateIfNeeded()
{
	// works only from the window thread
	if (find_thread(NULL) != Thread())
		return;

	// if the queue is already locked we are called recursivly
	// from our own dispatched update message
	if (((const BMessageQueue*)MessageQueue())->IsLocked())
		return;

	if (!Lock())
		return;

	// make sure all requests that would cause an update have
	// arrived at the server
	Sync();

	// Since we're blocking the event loop, we need to retrieve
	// all messages that are pending on the port.
	_DequeueAll();

	BMessageQueue* queue = MessageQueue();

	// First process and remove any _UPDATE_ message in the queue
	// With the current design, there can only be one at a time

	while (true) {
		queue->Lock();

		BMessage* message = queue->FindMessage(_UPDATE_, 0);
		queue->RemoveMessage(message);

		queue->Unlock();

		if (message == NULL)
			break;

		BWindow::DispatchMessage(message, this);
		delete message;
	}

	Unlock();
}


BView*
BWindow::FindView(const char* viewName) const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return NULL;

	return fTopView->FindView(viewName);
}


BView*
BWindow::FindView(BPoint point) const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return NULL;

	// point is assumed to be in window coordinates,
	// fTopView has same bounds as window
	return _FindView(fTopView, point);
}


BView*
BWindow::CurrentFocus() const
{
	return fFocus;
}


void
BWindow::Activate(bool active)
{
	if (!Lock())
		return;

	if (!IsHidden()) {
		fMinimized = false;
			// activating a window will also unminimize it
	}

	Unlock();
}


void
BWindow::WindowActivated(bool focus)
{
	// hook function
	// does nothing
}


void
BWindow::ConvertToScreen(BPoint* point) const
{

}


BPoint
BWindow::ConvertToScreen(BPoint point) const
{
	return point;
}


void
BWindow::ConvertFromScreen(BPoint* point) const
{
}


BPoint
BWindow::ConvertFromScreen(BPoint point) const
{
	return point;
}


void
BWindow::ConvertToScreen(BRect* rect) const
{
}


BRect
BWindow::ConvertToScreen(BRect rect) const
{
	return rect;
}


void
BWindow::ConvertFromScreen(BRect* rect) const
{
}


BRect
BWindow::ConvertFromScreen(BRect rect) const
{
	return rect;
}


bool
BWindow::IsMinimized() const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return false;

	return fMinimized;
}


BRect
BWindow::Bounds() const
{
	return BRect(0, 0, fFrame.Width(), fFrame.Height());
}


BRect
BWindow::Frame() const
{
	return fFrame;
}

BSize
BWindow::Size() const
{
	return BSize(fFrame.Width(), fFrame.Height());
}


const char*
BWindow::Title() const
{
	return fTitle;
}


void
BWindow::SetTitle(const char* title)
{
	if (title == NULL)
		title = "";

	free(fTitle);
	fTitle = strdup(title);

	_SetName(title);
}


bool
BWindow::IsActive() const
{
	return fActive;
}


void
BWindow::SetKeyMenuBar(BMenuBar* bar)
{
	fKeyMenuBar = bar;
}


BMenuBar*
BWindow::KeyMenuBar() const
{
	return fKeyMenuBar;
}


bool
BWindow::IsModal() const
{
	return fFeel == B_MODAL_SUBSET_WINDOW_FEEL
		|| fFeel == B_MODAL_APP_WINDOW_FEEL
		|| fFeel == B_MODAL_ALL_WINDOW_FEEL
		|| fFeel == kMenuWindowFeel;
}


bool
BWindow::IsFloating() const
{
	return fFeel == B_FLOATING_SUBSET_WINDOW_FEEL
		|| fFeel == B_FLOATING_APP_WINDOW_FEEL
		|| fFeel == B_FLOATING_ALL_WINDOW_FEEL;
}


status_t
BWindow::Perform(perform_code code, void* _data)
{
	switch (code) {
		case PERFORM_CODE_SET_LAYOUT:
		{
			perform_data_set_layout* data = (perform_data_set_layout*)_data;
			BWindow::SetLayout(data->layout);
			return B_OK;
}
	}

	return BLooper::Perform(code, _data);
}


status_t
BWindow::SetType(window_type type)
{
	window_look look;
	window_feel feel;
	_DecomposeType(type, &look, &feel);

	status_t status = SetLook(look);
	if (status == B_OK)
		status = SetFeel(feel);

	return status;
}


window_type
BWindow::Type() const
{
	return _ComposeType(fLook, fFeel);
}


status_t
BWindow::SetLook(window_look look)
{
	fLook = look;
	return B_OK;
}


window_look
BWindow::Look() const
{
	return fLook;
}


status_t
BWindow::SetFeel(window_feel feel)
{
	fFeel = feel;
	return B_OK;
}


window_feel
BWindow::Feel() const
{
	return fFeel;
}


status_t
BWindow::SetFlags(uint32 flags)
{
	BAutolock locker(this);
	if (!locker.IsLocked())
		return B_BAD_VALUE;

	fFlags = flags;

	return B_OK;
}


uint32
BWindow::Flags() const
{
	return fFlags;
}


uint32
BWindow::Workspaces() const
{
	if (!const_cast<BWindow*>(this)->Lock())
		return 0;

	uint32 workspaces = 0;

	return workspaces;
}


BView*
BWindow::LastMouseMovedView() const
{
	return fLastMouseMovedView;
}


void
BWindow::MoveBy(float dx, float dy)
{
	if ((dx != 0.0f || dy != 0.0f) && Lock()) {
		MoveTo(fFrame.left + dx, fFrame.top + dy);
		Unlock();
	}
}


void
BWindow::MoveTo(BPoint point)
{
	MoveTo(point.x, point.y);
}


void
BWindow::MoveTo(float x, float y)
{
	if (!Lock())
		return;

	x = roundf(x);
	y = roundf(y);

	if (fFrame.left != x || fFrame.top != y) {
		// TODO handle Wayland move
		// Also, for Wayland, our frame is always at 0,0

		// status_t status;
		// if (fLink->FlushWithReply(status) == B_OK && status == B_OK)
		// 	fFrame.OffsetTo(x, y);
	}

	Unlock();
}


void
BWindow::ResizeBy(float dx, float dy)
{
	if (Lock()) {
		ResizeTo(fFrame.Width() + dx, fFrame.Height() + dy);
		Unlock();
	}
}


void
BWindow::ResizeTo(float width, float height)
{
	if (!Lock())
		return;

	width = roundf(width);
	height = roundf(height);

	// stay in minimum & maximum frame limits
	if (width < fMinWidth)
		width = fMinWidth;
	else if (width > fMaxWidth)
		width = fMaxWidth;

	if (height < fMinHeight)
		height = fMinHeight;
	else if (height > fMaxHeight)
		height = fMaxHeight;

	if (width != fFrame.Width() || height != fFrame.Height()) {
		if (fWaylandWindowframeWidget) {
			widget_schedule_resize(fWaylandWindowframeWidget,
				width + WAYLAND_WINDOW_H_SLOP, height + WAYLAND_WINDOW_V_SLOP);
		}

		fFrame.right = fFrame.left + width;
		fFrame.bottom = fFrame.top + height;
		_AdoptResize();
	}

	Unlock();
}


void
BWindow::ResizeToPreferred()
{
	BAutolock locker(this);
	Layout(false);

	float width = fTopView->PreferredSize().width;
	width = std::min(width, fTopView->MaxSize().width);
	width = std::max(width, fTopView->MinSize().width);

	float height = fTopView->PreferredSize().height;
	height = std::min(height, fTopView->MaxSize().height);
	height = std::max(height, fTopView->MinSize().height);

	if (GetLayout()->HasHeightForWidth())
		GetLayout()->GetHeightForWidth(width, NULL, NULL, &height);

	ResizeTo(width, height);
}


void
BWindow::CenterIn(const BRect& rect)
{
	// Wayland says no.
}


void
BWindow::CenterOnScreen()
{
	// Wayland says no.
}


// Centers the window on the screen with the passed in id.
void
BWindow::CenterOnScreen(screen_id id)
{
	// Wayland says no.
}


void
BWindow::MoveOnScreen(uint32 flags)
{
	// Wayland says no.
}


void
BWindow::Show()
{
	bool runCalled = true;
	if (Lock()) {
		fShowLevel--;

		_SendShowOrHideMessage();

		runCalled = fRunCalled;

		Unlock();
	}

	if (!runCalled) {
		// This is the fist time Show() is called, which implicitly runs the
		// looper. NOTE: The window is still locked if it has not been
		// run yet, so accessing members is safe.
		Run();
	}
}


void
BWindow::Hide()
{
	if (Lock()) {
		// If we are minimized and are about to be hidden, unminimize
		if (IsMinimized() && fShowLevel == 0)
			Minimize(false);

		fShowLevel++;

		_SendShowOrHideMessage();

		Unlock();
	}
}


bool
BWindow::IsHidden() const
{
	return fShowLevel > 0;
}


bool
BWindow::QuitRequested()
{
	return BLooper::QuitRequested();
}

static int32 _WaylandDisplayLoopWindow(void *data)
{
	printf("***_WaylandDisplayLoopWindow::_WaylandDisplayLoop START\n");
	display* waylandDisplay = (display*)data;
	display_run(waylandDisplay);
	printf("***_WaylandDisplayLoopWindow::_WaylandDisplayLoop ENDED\n");
	return 0;
}

thread_id
BWindow::Run()
{
	EnableUpdates();
	
	printf("Window Frame: %f %f %f %f\n", fFrame.left, fFrame.top, fFrame.right, fFrame.bottom);
	printf("Window width: %d\n", fFrame.IntegerWidth());
	printf("Window height: %d\n", fFrame.IntegerHeight());

	printf("BWindow::Run display running\n");

	if (sDisplayThread < 0) {
		sDisplayThread = spawn_thread(&_WaylandDisplayLoopWindow, "Cosmoe Wayland Display Loop",
			B_NORMAL_PRIORITY, be_app->WaylandDisplay());
		if (sDisplayThread >= 0)
			resume_thread(sDisplayThread);
	}


	return BLooper::Run();
}


void
BWindow::SetLayout(BLayout* layout)
{
	// Adopt layout's colors for fTopView
	if (layout != NULL)
		fTopView->AdoptViewColors(layout->View());

	fTopView->SetLayout(layout);
}


BLayout*
BWindow::GetLayout() const
{
	return fTopView->GetLayout();
}


void
BWindow::InvalidateLayout(bool descendants)
{
	fTopView->InvalidateLayout(descendants);
}


void
BWindow::Layout(bool force)
{
	UpdateSizeLimits();

	// Do the actual layout
	fTopView->Layout(force);
}


bool
BWindow::IsOffscreenWindow() const
{
	return fOffscreen;
}


status_t
BWindow::GetSupportedSuites(BMessage* data)
{
	if (data == NULL)
		return B_BAD_VALUE;

	status_t status = data->AddString("suites", "suite/vnd.Be-window");
	if (status == B_OK) {
		BPropertyInfo propertyInfo(sWindowPropInfo, sWindowValueInfo);

		status = data->AddFlat("messages", &propertyInfo);
		if (status == B_OK)
			status = BLooper::GetSupportedSuites(data);
	}

	return status;
}


BHandler*
BWindow::ResolveSpecifier(BMessage* message, int32 index, BMessage* specifier,
	int32 what, const char* property)
{
	if (message->what == B_WINDOW_MOVE_BY
		|| message->what == B_WINDOW_MOVE_TO)
		return this;

	BPropertyInfo propertyInfo(sWindowPropInfo);
	if (propertyInfo.FindMatch(message, index, specifier, what, property) >= 0) {
		if (strcmp(property, "View") == 0) {
			// we will NOT pop the current specifier
			return fTopView;
		} else if (strcmp(property, "MenuBar") == 0) {
			if (fKeyMenuBar) {
				message->PopSpecifier();
				return fKeyMenuBar;
			} else {
				BMessage replyMsg(B_MESSAGE_NOT_UNDERSTOOD);
				replyMsg.AddInt32("error", B_NAME_NOT_FOUND);
				replyMsg.AddString("message",
					"This window doesn't have a main MenuBar");
				message->SendReply(&replyMsg);
				return NULL;
			}
		} else
			return this;
	}

	return BLooper::ResolveSpecifier(message, index, specifier, what, property);
}


//	#pragma mark - Private Methods


void
BWindow::_InitData(BRect frame, const char* title, window_look look,
	window_feel feel, uint32 flags,	uint32 workspace, int32 bitmapToken)
{
	STRACE(("BWindow::InitData()\n"));

	if (be_app == NULL) {
		debugger("You need a valid BApplication object before interacting with "
			"the app_server");
		return;
	}

	// For Cosmoe windows on Wayland, bounds and frame are the same since Wayland doesn't allow
	// window placement or even getting Window coordinates.
	frame.left = 0; //roundf(frame.left);
	frame.top = 0; //roundf(frame.top);
	frame.right = roundf(frame.right) - roundf(frame.left);
	frame.bottom = roundf(frame.bottom) - roundf(frame.top);

	fFrame = frame;

	if (title == NULL)
		title = "";

	fTitle = strdup(title);

	fFeel = feel;
	fLook = look;
	fFlags = flags | B_ASYNCHRONOUS_CONTROLS;

	fInTransaction = bitmapToken >= 0;
	fUpdatesDisabled = false;
	fUpdateRequested = false;
	fActive = false;
	fShowLevel = 1;

	fTopView = NULL;
	fFocus = NULL;
	fLastMouseMovedView	= NULL;
	fKeyMenuBar = NULL;
	fDefaultButton = NULL;

	// Shortcut 'Q' is handled in _HandleKeyDown() directly, as its message
	// get sent to the application, and not one of our handlers.
	// It is only installed for non-modal windows, though.
	fNoQuitShortcut = IsModal();

	if ((fFlags & B_NOT_CLOSABLE) == 0 && !IsModal()) {
		// Modal windows default to non-closable, but you can add the
		// shortcut manually, if a different behaviour is wanted
		AddShortcut('W', B_CONTROL_KEY, new BMessage(B_QUIT_REQUESTED));
	}

	// Edit modifier keys

	AddShortcut('X', B_CONTROL_KEY, new BMessage(B_CUT), NULL);
	AddShortcut('C', B_CONTROL_KEY, new BMessage(B_COPY), NULL);
	AddShortcut('V', B_CONTROL_KEY, new BMessage(B_PASTE), NULL);
	AddShortcut('A', B_CONTROL_KEY, new BMessage(B_SELECT_ALL), NULL);

	// Window modifier keys

	AddShortcut('M', B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(_MINIMIZE_), NULL);
	AddShortcut('Z', B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(_ZOOM_), NULL);
	AddShortcut('Z', B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(_ZOOM_), NULL);
	AddShortcut('H', B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(B_HIDE_APPLICATION), NULL);
	AddShortcut('F', B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(_SEND_TO_FRONT_), NULL);
	AddShortcut('B', B_COMMAND_KEY | B_CONTROL_KEY,
		new BMessage(_SEND_BEHIND_), NULL);

	// We set the default pulse rate, but we don't start the pulse
	fPulseRate = 500000;
	fPulseRunner = NULL;

	fIsFilePanel = false;

	fMenuSem = -1;

	fMinimized = false;

	fMaxZoomHeight = 32768.0;
	fMaxZoomWidth = 32768.0;
	fMinHeight = 0.0;
	fMinWidth = 0.0;
	fMaxHeight = 32768.0;
	fMaxWidth = 32768.0;

	fLastViewToken = B_NULL_TOKEN;


	port_id receivePort = create_port(B_LOOPER_PORT_DEFAULT_CAPACITY,
		"w<app_server");
	if (receivePort < B_OK) {
		// TODO: huh?
		printf("FATAL: Could not create BWindow's receive port\n");
		delete this;
		return;
	}

	fOffscreen = (bitmapToken >= 0);

	fBackingSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
		frame.IntegerWidth() * 2 + 1, frame.IntegerHeight() * 2 + 1);

	_SetName(title);

	STRACE(("Window locked?: %s\n", IsLocked() ? "True" : "False"));

	_CreateTopView();
}


//! Rename the handler and its thread
void
BWindow::_SetName(const char* title)
{
	if (title == NULL)
		title = "";

	if (fWaylandWindow)
		window_set_title(fWaylandWindow, title);

	// we will change BWindow's thread name to "w>window title"

	char threadName[B_OS_NAME_LENGTH];
	strcpy(threadName, "w>");
#ifdef __HAIKU__
	strlcat(threadName, title, B_OS_NAME_LENGTH);
#else
	int32 length = strlen(title);
	length = min_c(length, B_OS_NAME_LENGTH - 3);
	memcpy(threadName + 2, title, length);
	threadName[length + 2] = '\0';
#endif

	// change the handler's name
	SetName(threadName);

	// if the message loop has been started...
	if (Thread() >= B_OK)
		rename_thread(Thread(), threadName);
}


//!	Reads all pending messages from the window port and put them into the queue.
void
BWindow::_DequeueAll()
{
	//	Get message count from port
	int32 count = port_count(fMsgPort);

	for (int32 i = 0; i < count; i++) {
		BMessage* message = MessageFromPort(0);
		if (message != NULL)
			fDirectTarget->Queue()->AddMessage(message);
	}
}


/*!	This here is an almost complete code duplication to BLooper::task_looper()
	but with some important differences:
	 a)	it uses the _DetermineTarget() method to tell what the later target of
		a message will be, if no explicit target is supplied.
	 b)	it calls _UnpackMessage() and _SanitizeMessage() to duplicate the message
		to all of its intended targets, and to add all fields the target would
		expect in such a message.

	This is important because the app_server sends all input events to the
	preferred handler, and expects them to be correctly distributed to their
	intended targets.
*/
void
BWindow::task_looper()
{
	STRACE(("info: BWindow::task_looper() started.\n"));

	// Check that looper is locked (should be)
	AssertLocked();
	Unlock();

	if (IsLocked())
		debugger("window must not be locked!");

	while (!fTerminating) {
		// Did we get a message?
		BMessage* msg = MessageFromPort();
		if (msg)
			_AddMessagePriv(msg);

		//	Get message count from port
		int32 msgCount = port_count(fMsgPort);
		for (int32 i = 0; i < msgCount; ++i) {
			// Read 'count' messages from port (so we will not block)
			// We use zero as our timeout since we know there is stuff there
			msg = MessageFromPort(0);
			// Add messages to queue
			if (msg)
				_AddMessagePriv(msg);
		}

		bool dispatchNextMessage = true;
		while (!fTerminating && dispatchNextMessage) {
			// Get next message from queue (assign to fLastMessage after
			// locking)
			BMessage* message = fDirectTarget->Queue()->NextMessage();

			// Lock the looper
			if (!Lock()) {
				delete message;
				break;
			}

			fLastMessage = message;

			if (fLastMessage == NULL) {
				// No more messages: Unlock the looper and terminate the
				// dispatch loop.
				dispatchNextMessage = false;
			} else {
				// Get the target handler
				BMessage::Private messagePrivate(fLastMessage);
				bool usePreferred = messagePrivate.UsePreferredTarget();
				BHandler* handler = NULL;
				bool dropMessage = false;

				if (usePreferred) {
					handler = PreferredHandler();
					if (handler == NULL)
						handler = this;
				} else {
					gDefaultTokens.GetToken(messagePrivate.GetTarget(),
						B_HANDLER_TOKEN, (void**)&handler);

					// if this handler doesn't belong to us, we drop the message
					if (handler != NULL && handler->Looper() != this) {
						dropMessage = true;
						handler = NULL;
					}
				}

				if ((handler == NULL && !dropMessage) || usePreferred)
					handler = _DetermineTarget(fLastMessage, handler);

				unpack_cookie cookie;
				while (_UnpackMessage(cookie, &fLastMessage, &handler, &usePreferred)) {
					// if there is no target handler, the message is dropped
					if (handler != NULL) {
						_SanitizeMessage(fLastMessage, handler, usePreferred);

						// Is this a scripting message?
						if (fLastMessage->HasSpecifiers()) {
							int32 index = 0;
							// Make sure the current specifier is kosher
							if (fLastMessage->GetCurrentSpecifier(&index) == B_OK)
								handler = resolve_specifier(handler, fLastMessage);
						}

						if (handler != NULL)
							handler = _TopLevelFilter(fLastMessage, handler);

						if (handler != NULL)
							DispatchMessage(fLastMessage, handler);
					}

					// Delete the current message
					delete fLastMessage;
					fLastMessage = NULL;
				}
			}

			if (fTerminating) {
				// we leave the looper locked when we quit
				return;
			}

			Unlock();

			// Are any messages on the port?
			if (port_count(fMsgPort) > 0) {
				// Do outer loop
				dispatchNextMessage = false;
			}
		}
	}
}


window_type
BWindow::_ComposeType(window_look look, window_feel feel) const
{
	switch (feel) {
		case B_NORMAL_WINDOW_FEEL:
			switch (look) {
				case B_TITLED_WINDOW_LOOK:
					return B_TITLED_WINDOW;

				case B_DOCUMENT_WINDOW_LOOK:
					return B_DOCUMENT_WINDOW;

				case B_BORDERED_WINDOW_LOOK:
					return B_BORDERED_WINDOW;

				default:
					return B_UNTYPED_WINDOW;
			}
			break;

		case B_MODAL_APP_WINDOW_FEEL:
			if (look == B_MODAL_WINDOW_LOOK)
				return B_MODAL_WINDOW;
			break;

		case B_FLOATING_APP_WINDOW_FEEL:
			if (look == B_FLOATING_WINDOW_LOOK)
				return B_FLOATING_WINDOW;
			break;

		default:
			return B_UNTYPED_WINDOW;
	}

	return B_UNTYPED_WINDOW;
}


void
BWindow::_DecomposeType(window_type type, window_look* _look,
	window_feel* _feel) const
{
	switch (type) {
		case B_DOCUMENT_WINDOW:
			*_look = B_DOCUMENT_WINDOW_LOOK;
			*_feel = B_NORMAL_WINDOW_FEEL;
			break;

		case B_MODAL_WINDOW:
			*_look = B_MODAL_WINDOW_LOOK;
			*_feel = B_MODAL_APP_WINDOW_FEEL;
			break;

		case B_FLOATING_WINDOW:
			*_look = B_FLOATING_WINDOW_LOOK;
			*_feel = B_FLOATING_APP_WINDOW_FEEL;
			break;

		case B_BORDERED_WINDOW:
			*_look = B_BORDERED_WINDOW_LOOK;
			*_feel = B_NORMAL_WINDOW_FEEL;
			break;

		case B_TITLED_WINDOW:
		case B_UNTYPED_WINDOW:
		default:
			*_look = B_TITLED_WINDOW_LOOK;
			*_feel = B_NORMAL_WINDOW_FEEL;
			break;
	}
}


void
BWindow::_CreateTopView()
{
	STRACE(("_CreateTopView(): enter\n"));

	BRect frame = fFrame.OffsetToCopy(B_ORIGIN);
	// TODO: what to do here about std::nothrow?
	fTopView = new BView(frame, "fTopView", B_FOLLOW_ALL, B_WILL_DRAW);
	fTopView->fTopLevelView = true;

	//inhibit check_lock()
	fLastViewToken = _get_object_token_(fTopView);

	// set fTopView's owner, add it to window's eligible handler list
	// and also set its next handler to be this window.

	STRACE(("Calling setowner fTopView = %p this = %p.\n",
		fTopView, this));

	fTopView->_SetOwner(this);

	// we can't use AddChild() because this is the top view
	fTopView->_CreateSelf();
	STRACE(("BuildTopView ended\n"));
}


/*!
	Resizes the top view to match the window size. This will also
	adapt the size of all its child views as needed.
	This method has to be called whenever the frame of the window
	changes.
*/
void
BWindow::_AdoptResize()
{
	// Resize views according to their resize modes
	if (fTopView == NULL)
		return;

	int32 deltaWidth = (int32)(fFrame.Width() - fTopView->Bounds().Width());
	int32 deltaHeight = (int32)(fFrame.Height() - fTopView->Bounds().Height());
	if (deltaWidth == 0 && deltaHeight == 0)
		return;

	fTopView->_ResizeBy(deltaWidth, deltaHeight);
}


void
BWindow::_SetFocus(BView* focusView, bool notifyInputServer)
{
	if (fFocus == focusView)
		return;

	// we notify the input server if we are passing focus
	// from a view which has the B_INPUT_METHOD_AWARE to a one
	// which does not, or vice-versa
	if (notifyInputServer && fActive) {
		bool inputMethodAware = false;
		if (focusView)
			inputMethodAware = focusView->Flags() & B_INPUT_METHOD_AWARE;
		BMessage msg(inputMethodAware ? IS_FOCUS_IM_AWARE_VIEW : IS_UNFOCUS_IM_AWARE_VIEW);
		BMessenger messenger(focusView);
		BMessage reply;
		if (focusView)
			msg.AddMessenger("view", messenger);
		_control_input_server_(&msg, &reply);
	}

	fFocus = focusView;
	SetPreferredHandler(focusView);
}


/*!
	\brief Determines the target of a message received for the
		focus view.
*/
BHandler*
BWindow::_DetermineTarget(BMessage* message, BHandler* target)
{
	if (target == NULL)
		target = this;

	switch (message->what) {
		case B_KEY_DOWN:
		case B_KEY_UP:
		{
			// if we have a default button, it might want to hear
			// about pressing the <enter> key
			BButton* defaultButton = DefaultButton();
			if (defaultButton != NULL) {
				int32 rawChar = message->GetInt32("raw_char", 0);
				uint32 mods = modifiers();
				if (rawChar == B_ENTER && (mods & Shortcut::AllowedModifiers()) == 0)
					return defaultButton;
			}
			// supposed to fall through
		}
		case B_UNMAPPED_KEY_DOWN:
		case B_UNMAPPED_KEY_UP:
		case B_MODIFIERS_CHANGED:
			// these messages should be dispatched by the focus view
			if (CurrentFocus() != NULL)
				return CurrentFocus();
			break;

		case B_MOUSE_DOWN:
		case B_MOUSE_UP:
		case B_MOUSE_MOVED:
		case B_MOUSE_WHEEL_CHANGED:
		case B_MOUSE_IDLE:
		{
			// is there a token of the view that is currently under the mouse?
			int32 token;
			if (message->FindInt32("_view_token", &token) == B_OK) {
				BView* view = _FindView(token);
				if (view != NULL)
					return view;
			}

			// if there is no valid token in the message, we try our
			// luck with the last target, if available
			if (fLastMouseMovedView != NULL)
				return fLastMouseMovedView;
			break;
		}

		case B_PULSE:
		case B_QUIT_REQUESTED:
			// TODO: test whether R5 will let BView dispatch these messages
			return this;

		case _MESSAGE_DROPPED_:
			if (fLastMouseMovedView != NULL)
				return fLastMouseMovedView;
			break;

		default:
			break;
	}

	return target;
}


/*!	\brief Determines whether or not this message has targeted the focus view.

	This will return \c false only if the message did not go to the preferred
	handler, or if the packed message does not contain address the focus view
	at all.
*/
bool
BWindow::_IsFocusMessage(BMessage* message)
{
	BMessage::Private messagePrivate(message);
	if (!messagePrivate.UsePreferredTarget())
		return false;

	bool feedFocus;
	if (message->HasInt32("_token")
		&& (message->FindBool("_feed_focus", &feedFocus) != B_OK || !feedFocus))
		return false;

	return true;
}


/*!	\brief Distributes the message to its intended targets. This is done for
		all messages that should go to the preferred handler.

	Returns \c true in case the message should still be dispatched
*/
bool
BWindow::_UnpackMessage(unpack_cookie& cookie, BMessage** _message,
	BHandler** _target, bool* _usePreferred)
{
	if (cookie.message == NULL)
		return false;

	if (cookie.index == 0 && !cookie.tokens_scanned) {
		// We were called the first time for this message

		if (!*_usePreferred) {
			// only consider messages targeted at the preferred handler
			cookie.message = NULL;
			return true;
		}

		// initialize our cookie
		cookie.message = *_message;
		cookie.focus = *_target;

		if (cookie.focus != NULL)
			cookie.focus_token = _get_object_token_(*_target);

		if (fLastMouseMovedView != NULL && cookie.message->what == B_MOUSE_MOVED)
			cookie.last_view_token = _get_object_token_(fLastMouseMovedView);

		*_usePreferred = false;
	}

	_DequeueAll();

	// distribute the message to all targets specified in the
	// message directly (but not to the focus view)

	for (int32 token; !cookie.tokens_scanned
			&& cookie.message->FindInt32("_token", cookie.index, &token)
				== B_OK;
			cookie.index++) {
		// focus view is preferred and should get its message directly
		if (token == cookie.focus_token) {
			cookie.found_focus = true;
			continue;
		}
		if (token == cookie.last_view_token)
			continue;

		BView* target = _FindView(token);
		if (target == NULL)
			continue;

		*_message = new BMessage(*cookie.message);
		// the secondary copies of the message should not be treated as focus
		// messages, otherwise there will be unintended side effects, i.e.
		// keyboard shortcuts getting processed multiple times.
		(*_message)->RemoveName("_feed_focus");
		*_target = target;
		cookie.index++;
		return true;
	}

	cookie.tokens_scanned = true;

	// if there is a last mouse moved view, and the new focus is
	// different, the previous view wants to get its B_EXITED_VIEW
	// message
	if (cookie.last_view_token != B_NULL_TOKEN && fLastMouseMovedView != NULL
		&& fLastMouseMovedView != cookie.focus) {
		*_message = new BMessage(*cookie.message);
		*_target = fLastMouseMovedView;
		cookie.last_view_token = B_NULL_TOKEN;
		return true;
	}

	bool dispatchToFocus = true;

	// check if the focus token is still valid (could have been removed in the mean time)
	BHandler* handler;
	if (gDefaultTokens.GetToken(cookie.focus_token, B_HANDLER_TOKEN, (void**)&handler) != B_OK
		|| handler->Looper() != this)
		dispatchToFocus = false;

	if (dispatchToFocus && cookie.index > 0) {
		// should this message still be dispatched by the focus view?
		bool feedFocus;
		if (!cookie.found_focus
			&& (cookie.message->FindBool("_feed_focus", &feedFocus) != B_OK
				|| feedFocus == false))
			dispatchToFocus = false;
	}

	if (!dispatchToFocus) {
		delete cookie.message;
		cookie.message = NULL;
		return false;
	}

	*_message = cookie.message;
	*_target = cookie.focus;
	*_usePreferred = true;
	cookie.message = NULL;
	return true;
}


/*!	Some messages don't get to the window in a shape an application should see.
	This method is supposed to give a message the last grinding before
	it's acceptable for the receiving application.
*/
void
BWindow::_SanitizeMessage(BMessage* message, BHandler* target, bool usePreferred)
{
	if (target == NULL)
		return;

	switch (message->what) {
		case B_MOUSE_MOVED:
		case B_MOUSE_UP:
		case B_MOUSE_DOWN:
		{
			BPoint where;
			if (message->FindPoint("screen_where", &where) != B_OK)
				break;

			BView* view = dynamic_cast<BView*>(target);

			if (view == NULL || message->what == B_MOUSE_MOVED) {
				// add local window coordinates, only
				// for regular mouse moved messages
				message->AddPoint("where", ConvertFromScreen(where));
			}

			if (view != NULL) {
				// add local view coordinates
				BPoint viewWhere = view->ConvertFromScreen(where);
				if (message->what != B_MOUSE_MOVED) {
					// Yep, the meaning of "where" is different
					// for regular mouse moved messages versus
					// mouse up/down!
					message->AddPoint("where", viewWhere);
				}
				message->AddPoint("be:view_where", viewWhere);

				if (message->what == B_MOUSE_MOVED) {
					// is there a token of the view that is currently under
					// the mouse?
					BView* viewUnderMouse = NULL;
					int32 token;
					if (message->FindInt32("_view_token", &token) == B_OK)
						viewUnderMouse = _FindView(token);

					// add transit information
					uint32 transit
						= _TransitForMouseMoved(view, viewUnderMouse);
					message->AddInt32("be:transit", transit);

					if (usePreferred)
						fLastMouseMovedView = viewUnderMouse;
				}
			}
			break;
		}

		case B_MOUSE_IDLE:
		{
			// App Server sends screen coordinates, convert the point to
			// local view coordinates, then add the point in be:view_where
			BPoint where;
			if (message->FindPoint("screen_where", &where) != B_OK)
				break;

			BView* view = dynamic_cast<BView*>(target);
			if (view != NULL) {
				// add local view coordinates
				message->AddPoint("be:view_where",
					view->ConvertFromScreen(where));
			}
			break;
		}

		case _MESSAGE_DROPPED_:
		{
			uint32 originalWhat;
			if (message->FindInt32("_original_what",
					(int32*)&originalWhat) == B_OK) {
				message->what = originalWhat;
				message->RemoveName("_original_what");
			}
			break;
		}
	}
}


/*!
	This is called by BView::GetMouse() when a B_MOUSE_MOVED message
	is removed from the queue.
	It allows the window to update the last mouse moved view, and
	let it decide if this message should be kept. It will also remove
	the message from the queue.
	You need to hold the message queue lock when calling this method!

	\return true if this message can be used to get the mouse data from,
	\return false if this is not meant for the public.
*/
bool
BWindow::_StealMouseMessage(BMessage* message, bool& deleteMessage)
{
	BMessage::Private messagePrivate(message);
	if (!messagePrivate.UsePreferredTarget()) {
		// this message is targeted at a specific handler, so we should
		// not steal it
		return false;
	}

	int32 token;
	if (message->FindInt32("_token", 0, &token) == B_OK) {
		// This message has other targets, so we can't remove it;
		// just prevent it from being sent to the preferred handler
		// again (if it should have gotten it at all).
		bool feedFocus;
		if (message->FindBool("_feed_focus", &feedFocus) != B_OK || !feedFocus)
			return false;

		message->RemoveName("_feed_focus");
		deleteMessage = false;
	} else {
		deleteMessage = true;

		if (message->what == B_MOUSE_MOVED) {
			// We need to update the last mouse moved view, as this message
			// won't make it to _SanitizeMessage() anymore.
			BView* viewUnderMouse = NULL;
			int32 token;
			if (message->FindInt32("_view_token", &token) == B_OK)
				viewUnderMouse = _FindView(token);

			// Don't remove important transit messages!
			uint32 transit = _TransitForMouseMoved(fLastMouseMovedView,
				viewUnderMouse);
			if (transit == B_ENTERED_VIEW || transit == B_EXITED_VIEW)
				deleteMessage = false;
		}

		if (deleteMessage) {
			// The message is only thought for the preferred handler, so we
			// can just remove it.
			MessageQueue()->RemoveMessage(message);
		}
	}

	return true;
}


uint32
BWindow::_TransitForMouseMoved(BView* view, BView* viewUnderMouse) const
{
	uint32 transit;
	if (viewUnderMouse == view) {
		// the mouse is over the target view
		if (fLastMouseMovedView != view)
			transit = B_ENTERED_VIEW;
		else
			transit = B_INSIDE_VIEW;
	} else {
		// the mouse is not over the target view
		if (view == fLastMouseMovedView)
			transit = B_EXITED_VIEW;
		else
			transit = B_OUTSIDE_VIEW;
	}
	return transit;
}


/*!	Handles keyboard input before it gets forwarded to the target handler.
	This includes shortcut evaluation, keyboard navigation, etc.

	\return handled if true, the event was already handled, and will not
		be forwarded to the target handler.

	TODO: must also convert the incoming key to the font encoding of the target
*/
bool
BWindow::_HandleKeyDown(BMessage* event)
{
	// Only handle special functions when the event targeted the active focus
	// view
	if (!_IsFocusMessage(event))
		return false;

	const char* bytes;
	if (event->FindString("bytes", &bytes) != B_OK)
		return false;

	char key = Shortcut::PrepareKey(bytes[0]);

	uint32 modifiers;
	if (event->FindInt32("modifiers", (int32*)&modifiers) != B_OK)
		modifiers = 0;

	uint32 rawKey;
	if (event->FindInt32("key", (int32*)&rawKey) != B_OK)
		rawKey = 0;

	// handle BMenuBar key
	if (key == B_ESCAPE && (modifiers & B_CONTROL_KEY) != 0 && fKeyMenuBar != NULL) {
		fKeyMenuBar->StartMenuBar(0, true, false, NULL);
		return true;
	}

	// Keyboard navigation through views
	// (B_OPTION_KEY makes BTextViews and friends navigable, even in editing
	// mode)
	if (key == B_TAB && (modifiers & B_OPTION_KEY) != 0) {
		_KeyboardNavigation();
		return true;
	}

	// Deskbar's Switcher
	//if ((key == B_TAB || rawKey == 0x11) && (modifiers & B_CONTROL_KEY) != 0) {
	//	_Switcher(rawKey, modifiers, event->HasInt32("be:key_repeat"));
	//	return true;
	//}

	printf("BWindow::_HandleKeyDown() - key: %c, rawKey: %d, modifiers: %u, escape: %d\n",
		key, rawKey, modifiers, B_ESCAPE);
	// Optionally close window when the escape key is pressed
	if (key == B_ESCAPE && (Flags() & B_CLOSE_ON_ESCAPE) != 0) {
		BMessage message(B_QUIT_REQUESTED);
		message.AddBool("shortcut", true);
		PostMessage(&message);
		return true;
	}

	// PrtScr key takes a screenshot
	if (key == B_FUNCTION_KEY && rawKey == B_PRINT_KEY) {
		// With no modifier keys the best way to get a screenshot is by
		// calling the screenshot CLI
		//if (modifiers == 0) {
		//	be_roster->Launch("application/x-vnd.haiku-screenshot-cli");
		//	return true;
		//}

		// Prepare a message based on the modifier keys pressed and launch the
		// screenshot GUI
		//BMessage message(B_ARGV_RECEIVED);
		//int32 argc = 1;
		//message.AddString("argv", "Screenshot");
		//if ((modifiers & B_CONTROL_KEY) != 0) {
		//	argc++;
		//	message.AddString("argv", "--clipboard");
		//}
		//if ((modifiers & B_SHIFT_KEY) != 0) {
		//	argc++;
		//	message.AddString("argv", "--silent");
		//}
		//message.AddInt32("argc", argc);
		//be_roster->Launch("application/x-vnd.haiku-screenshot", &message);
		//return true;
	}

	// Special handling for Command+q, Command+Left, Command+Right
	if ((modifiers & B_CONTROL_KEY) != 0) {
		// Command+q has been pressed, so, we will quit
		// the shortcut mechanism doesn't allow handlers outside the window
		if (!fNoQuitShortcut && key == 'Q') {
			BMessage message(B_QUIT_REQUESTED);
			message.AddBool("shortcut", true);
			be_app->PostMessage(&message);
			return true;
		}

		// Send Command+Left and Command+Right to textview if it has focus
		if (key == B_LEFT_ARROW || key == B_RIGHT_ARROW) {
			// check key before doing expensive dynamic_cast
			BTextView* textView = dynamic_cast<BTextView*>(CurrentFocus());
			if (textView != NULL) {
				textView->KeyDown(bytes, modifiers);
				return true;
			}
		}
	}

	// Handle shortcuts
	{
		// Pretend that the user opened a menu, to give the subclass a
		// chance to update its menus. This may install new shortcuts,
		// which is why we have to call it here, before trying to find
		// a shortcut for the given key.
		MenusBeginning();

		Shortcut* shortcut = _FindShortcut(key, modifiers
			| (((modifiers & B_CONTROL_KEY) == 0) ? B_NO_COMMAND_KEY : 0));
		if (shortcut != NULL) {
			// TODO: would be nice to move this functionality to
			//	a Shortcut::Invoke() method - but since BMenu::InvokeItem()
			//	(and BMenuItem::Invoke()) are private, I didn't want
			//	to mess with them (BMenuItem::Invoke() is public in
			//	Dano/Zeta, though, maybe we should just follow their
			//	example)
			if (shortcut->MenuItem() != NULL) {
				BMenu* menu = shortcut->MenuItem()->Menu();
				if (menu != NULL)
					MenuPrivate(menu).InvokeItem(shortcut->MenuItem(), true);
			} else {
				BHandler* target = shortcut->Target();
				if (target == NULL)
					target = CurrentFocus();

				if (shortcut->Message() != NULL) {
					BMessage message(*shortcut->Message());
					if (message.ReplaceInt64("when", system_time()) != B_OK)
						message.AddInt64("when", system_time());
					if (message.ReplaceBool("shortcut", true) != B_OK)
						message.AddBool("shortcut", true);
					PostMessage(&message, target);
				}
			}
		}

		MenusEnded();

		if (shortcut != NULL)
			return true;
	}

	if ((modifiers & B_CONTROL_KEY) != 0) {
		// we always eat the event if the command key was pressed
		return true;
	}

	// TODO: convert keys to the encoding of the target view

	return false;
}


bool
BWindow::_HandleUnmappedKeyDown(BMessage* event)
{
	// Only handle special functions when the event targeted the active focus
	// view
	if (!_IsFocusMessage(event))
		return false;

	uint32 modifiers;
	int32 rawKey;
	if (event->FindInt32("modifiers", (int32*)&modifiers) != B_OK
		|| event->FindInt32("key", &rawKey))
		return false;

	// Deskbar's Switcher
	//if (rawKey == 0x11 && (modifiers & B_CONTROL_KEY) != 0) {
	//	_Switcher(rawKey, modifiers, event->HasInt32("be:key_repeat"));
	//	return true;
	//}

	return false;
}


void
BWindow::_KeyboardNavigation()
{
	BMessage* message = CurrentMessage();
	if (message == NULL)
		return;

	const char* bytes;
	if (message->FindString("bytes", &bytes) != B_OK || bytes[0] != B_TAB)
		return;

	uint32 modifiers;
	if (message->FindInt32("modifiers", (int32*)&modifiers) != B_OK)
		modifiers = 0;

	BView* nextFocus;
	int32 jumpGroups = (modifiers & B_OPTION_KEY) != 0 ? B_NAVIGABLE_JUMP : B_NAVIGABLE;
	if ((modifiers & B_SHIFT_KEY) != 0)
		nextFocus = _FindPreviousNavigable(fFocus, jumpGroups);
	else
		nextFocus = _FindNextNavigable(fFocus, jumpGroups);

	if (nextFocus != NULL && nextFocus != fFocus)
		nextFocus->MakeFocus(true);
}


BMessage*
BWindow::ConvertToMessage(void* raw, int32 code)
{
	return BLooper::ConvertToMessage(raw, code);
}


BWindow::Shortcut*
BWindow::_FindShortcut(uint32 key, uint32 modifiers)
{
	key = Shortcut::PrepareKey(key);
	uint32 preparedModifiers = Shortcut::PrepareModifiers(modifiers);

	int32 shortcutCount = fShortcuts.CountItems();
	for (int32 index = 0; index < shortcutCount; index++) {
		Shortcut* shortcut = (Shortcut*)fShortcuts.ItemAt(index);
		if (shortcut != NULL && shortcut->Matches(key, preparedModifiers))
			return shortcut;
	}

	return NULL;
}


BView*
BWindow::_FindView(int32 token)
{
	BHandler* handler;
	if (gDefaultTokens.GetToken(token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return NULL;
	}

	// the view must belong to us in order to be found by this method
	BView* view = dynamic_cast<BView*>(handler);
	if (view != NULL && view->Window() == this)
		return view;

	return NULL;
}


BView*
BWindow::_FindView(BView* view, BPoint point) const
{
	// point is assumed to be already in view's coordinates
	if (!view->IsHidden(view) && view->Bounds().Contains(point)) {
		if (view->fFirstChild == NULL)
			return view;
		else {
			BView* child = view->fFirstChild;
			while (child != NULL) {
				BPoint childPoint = point - child->Frame().LeftTop();
				BView* subView  = _FindView(child, childPoint);
				if (subView != NULL)
					return subView;

				child = child->fNextSibling;
			}
		}
		return view;
	}
	return NULL;
}


BView*
BWindow::_FindNextNavigable(BView* focus, uint32 flags)
{
	if (focus == NULL)
		focus = fTopView;

	BView* nextFocus = focus;

	// Search the tree for views that accept focus (depth search)
	while (true) {
		if (nextFocus->fFirstChild)
			nextFocus = nextFocus->fFirstChild;
		else if (nextFocus->fNextSibling)
			nextFocus = nextFocus->fNextSibling;
		else {
			// go to the nearest parent with a next sibling
			while (!nextFocus->fNextSibling && nextFocus->fParent) {
				nextFocus = nextFocus->fParent;
			}

			if (nextFocus == fTopView) {
				// if we started with the top view, we traversed the whole tree already
				if (nextFocus == focus)
					return NULL;

				nextFocus = nextFocus->fFirstChild;
			} else
				nextFocus = nextFocus->fNextSibling;
		}

		if (nextFocus == focus || nextFocus == NULL) {
			// When we get here it means that the hole tree has been
			// searched and there is no view with B_NAVIGABLE(_JUMP) flag set!
			return NULL;
		}

		if (!nextFocus->IsHidden() && (nextFocus->Flags() & flags) != 0)
			return nextFocus;
	}
}


BView*
BWindow::_FindPreviousNavigable(BView* focus, uint32 flags)
{
	if (focus == NULL)
		focus = fTopView;

	BView* previousFocus = focus;

	// Search the tree for the previous view that accept focus
	while (true) {
		if (previousFocus->fPreviousSibling) {
			// find the last child in the previous sibling
			previousFocus = _LastViewChild(previousFocus->fPreviousSibling);
		} else {
			previousFocus = previousFocus->fParent;
			if (previousFocus == fTopView)
				previousFocus = _LastViewChild(fTopView);
		}

		if (previousFocus == focus || previousFocus == NULL) {
			// When we get here it means that the hole tree has been
			// searched and there is no view with B_NAVIGABLE(_JUMP) flag set!
			return NULL;
		}

		if (!previousFocus->IsHidden() && (previousFocus->Flags() & flags) != 0)
			return previousFocus;
	}
}


/*!
	Returns the last child in a view hierarchy.
	Needed only by _FindPreviousNavigable().
*/
BView*
BWindow::_LastViewChild(BView* parent)
{
	while (true) {
		BView* last = parent->fFirstChild;
		if (last == NULL)
			return parent;

		while (last->fNextSibling) {
			last = last->fNextSibling;
		}

		parent = last;
	}
}


void
BWindow::SetIsFilePanel(bool isFilePanel)
{
	fIsFilePanel = isFilePanel;
}


bool
BWindow::IsFilePanel() const
{
	return fIsFilePanel;
}


void
BWindow::_SendShowOrHideMessage()
{
	if (IsHidden() && fWaylandWindow) {
		// Destroy our Wayland backing window

		printf("Destroying Wayland window for '%s'\n", Name());

		DisableUpdates();

		widget_set_redraw_handler(fTopViewWidget, NULL);
		widget_set_motion_handler(fTopViewWidget, NULL);
		widget_set_button_handler(fTopViewWidget, NULL);
		widget_set_axis_handler(fTopViewWidget, NULL);
		widget_set_redraw_handler(fWaylandWindowframeWidget, NULL);

		if (fWaylandWindowframeWidget) {
			widget_deferred_destroy(fWaylandWindowframeWidget);
			fWaylandWindowframeWidget = NULL;
		}

		if (fTopViewWidget) {
			widget_deferred_destroy(fTopViewWidget);
			fTopViewWidget = NULL;
		}

		window_deferred_destroy(fWaylandWindow);
		fWaylandWindow = NULL;

		printf("Wayland window destroyed for '%s'\n", Name());

	} else if (!IsHidden() && !fWaylandWindow) {
		// Create our Wayland backing window

		printf("Creating Wayland window for '%s'\n", Name());

		fWaylandWindow = window_create(be_app->WaylandDisplay());

		if (!fOffscreen) {
	
			fWaylandWindowframeWidget = window_frame_create(fWaylandWindow, this);
			set_empty_input_region(fWaylandWindowframeWidget, window_get_display(fWaylandWindow));

			// FIXME: determine the windowframe widget size dynamically and stop using the SLOP defines

			BRect frame = Frame();

			if (fFlags & B_NOT_RESIZABLE)
				window_set_min_max_allocation(fWaylandWindow,
					frame.IntegerWidth() + WAYLAND_WINDOW_H_SLOP,
					frame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP,
					frame.IntegerWidth() + WAYLAND_WINDOW_H_SLOP,
					frame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP);
			else {
				if (fFlags & B_NOT_H_RESIZABLE)
					window_set_min_max_allocation(fWaylandWindow,
						frame.IntegerWidth() + WAYLAND_WINDOW_H_SLOP,
						0,
						frame.IntegerWidth() + WAYLAND_WINDOW_H_SLOP,
						0);

				if (fFlags & B_NOT_V_RESIZABLE)
					window_set_min_max_allocation(fWaylandWindow,
						0,
						frame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP,
						0,
						frame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP);
			}
		}

		window_set_appid(fWaylandWindow, "org.haydentech.cosmoe");
		window_set_user_data(fWaylandWindow, this);

		// The handler name is prefixed with "w>", so skip those two characters
		window_set_title(fWaylandWindow, Name() + 2);

		// FIXME: not sure whether this should be SUBSURFACE_SYNCHRONIZED or SUBSURFACE_DESYNCHRONIZED
		fTopViewWidget = window_add_subsurface(fWaylandWindow, fTopView, SUBSURFACE_SYNCHRONIZED);
		//printf("TopView widget %p\n", fTopViewWidget);
		widget_set_allocation(fTopViewWidget, WAYLAND_TOPVIEW_H_SLOP, WAYLAND_TOPVIEW_V_SLOP, Bounds().IntegerWidth() + 1, Bounds().IntegerHeight() + 1);

		/* We set the input region of the subsurface where the image is draw as
		* NULL, as the input region of the parent surface is automatically set
		* by the toytoolkit. But as the window that finds the widget in a
		* certain (x, y) position looks for surfaces that are on top first, it
		* will call the image_widget handlers for input related stuff. */
		set_empty_input_region(fTopViewWidget, window_get_display(fWaylandWindow));
		widget_set_redraw_handler(fTopViewWidget, view_redraw_handler);
		widget_set_motion_handler(fTopViewWidget, view_pointer_motion_handler);
		widget_set_button_handler(fTopViewWidget, view_button_handler);
		widget_set_axis_handler(fTopViewWidget, view_axis_handler);

		if (!fOffscreen) {
			widget_set_resize_handler(fWaylandWindowframeWidget, windowframe_resize_handler);
			widget_schedule_resize(fWaylandWindowframeWidget, fFrame.IntegerWidth()  + WAYLAND_WINDOW_H_SLOP,
				fFrame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP);
			widget_schedule_redraw(fWaylandWindowframeWidget);
		}

		// window_set_keyboard_focus_handler(window, keyboard_focus_handler);
		// window_set_fullscreen_handler(window, fullscreen_handler);
		window_set_close_handler(fWaylandWindow, close_handler);
		window_set_key_handler(fWaylandWindow, key_handler);

		widget_schedule_redraw(fTopViewWidget);
		window_schedule_redraw(fWaylandWindow);

		display_trigger_fake_event(be_app->WaylandDisplay());
	}
}


void
BWindow::_PropagateMessageToChildViews(BMessage* message)
{
	int32 childrenCount = CountChildren();
	for (int32 index = 0; index < childrenCount; index++) {
		BView* view = ChildAt(index);
		if (view != NULL)
			PostMessage(message, view);
	}
}

void BWindow::_ReservedWindow2() {}
void BWindow::_ReservedWindow3() {}
void BWindow::_ReservedWindow4() {}
void BWindow::_ReservedWindow5() {}
void BWindow::_ReservedWindow6() {}
void BWindow::_ReservedWindow7() {}
void BWindow::_ReservedWindow8() {}

