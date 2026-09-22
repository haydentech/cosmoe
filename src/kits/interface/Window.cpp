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

#include <DisplayScaleManager.h>

#include <ctype.h>
#include <math.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <unordered_map>
#include <vector>
#include <algorithm>

#include <Application.h>
#include <AppMisc.h>
#include <AppServerLink.h>
#include <ApplicationPrivate.h>
#include <Autolock.h>
#include <Bitmap.h>
#include <Button.h>
#include <Debug.h>
#include <Cursor.h>
#include <DirectMessageTarget.h>
#include <InputServerTypes.h>
#include <InterfaceDefs.h>
#include <Layout.h>
#include <LayoutUtils.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MenuPrivate.h>
#include <MessagePrivate.h>
#include <MessageQueue.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PortLink.h>
#include <PropertyInfo.h>
#include <Screen.h>
#include <ServerProtocol.h>
#include <String.h>
#include <TextView.h>
#include <TokenSpace.h>
#include <ToolTipManager.h>
#include <UnicodeChar.h>
#include <WindowPrivate.h>

#ifdef __linux__
#include <Keymap.h>
#endif

#include <CosmoeBackendAPI.h>

#include <BackendInputState.h>

#ifdef __linux__
#include <xkbcommon/xkbcommon-keysyms.h>
#endif

#include <binary_compatibility/Interface.h>
#include <input_globals.h>
#include <util/SplayTree.h>
#include <input_event_codes_compat.h>
#include <cairo.h>

#include <BitmapCairoUtils.h>

// Forward declaration for menu window check
class BMenuWindow;

//#define DEBUG_WIN
#ifdef DEBUG_WIN
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

#define B_HIDE_APPLICATION '_AHD'


extern void _set_key_state(uint32 key, bool pressed);
extern void _get_key_states(uint8 states[16]);
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


class BWindow::Shortcut : public SplayTreeLink<BWindow::Shortcut> {
public:
	struct TreeKey {
		uint32 key;
		uint32 prepared_modifiers;

		TreeKey(uint32 key, uint32 preparedModifiers)
			: key(key), prepared_modifiers(preparedModifiers) {}
	};

	struct SplayTreeDefinition {
		typedef TreeKey KeyType;
		typedef	BWindow::Shortcut NodeType;

		static KeyType GetKey(const NodeType* node)
		{
			return TreeKey(node->Key(), node->PreparedModifiers());
		}
		static SplayTreeLink<NodeType>* GetLink(NodeType* node)
		{
			return node;
		}

		static int Compare(const KeyType& key, const NodeType* node)
		{
			return node->Compare(key.key, key.prepared_modifiers);
		}
	};

	typedef ::SplayTree<SplayTreeDefinition> SplayTree;

public:
	static SplayTree* CastToTree(void** storage)
	{
		STATIC_ASSERT(sizeof(void*) == sizeof(SplayTree));
		return reinterpret_cast<SplayTree*>(storage);
	}

public:
							Shortcut(uint32 key, uint32 modifiers,
								BMenuItem* item);
							Shortcut(uint32 key, uint32 modifiers,
								BMessage* message, BHandler* target);
							~Shortcut();

			int				Compare(uint32 key, uint32 preparedModifiers) const;

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


int
BWindow::Shortcut::Compare(uint32 key, uint32 preparedModifiers) const
{
	if (fKey != key)
		return fKey - key;
	if (fPreparedModifiers != preparedModifiers)
		return fPreparedModifiers - preparedModifiers;
	return 0;
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
	#ifdef __linux__
	if ((modifiers & B_COMMAND_KEY) != 0) {
		BKeymap keymap;
		if (keymap.SetToCurrent() == B_OK) {
			const key_map& map = keymap.Map();
			if (map.left_command_key == map.left_control_key
				|| map.right_command_key == map.right_control_key) {
				modifiers &= ~B_CONTROL_KEY;
			}
		}
	}
	#endif

	if ((modifiers & B_NO_COMMAND_KEY) != 0)
		return (modifiers & AllowedModifiers()) & ~B_COMMAND_KEY;
	else
		return (modifiers & AllowedModifiers()) | B_COMMAND_KEY;
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
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* win = dynamic_cast<BWindow*>(handler);
	if (win == NULL)
		return;
	
	/* Additional safety: check if fWindowToken is B_NULL_TOKEN (window being destroyed).
	 * We don't use a lock here because the mutex might be destroyed during BWindow destruction. */
	if (win->fWindowToken == B_NULL_TOKEN)
		return;
		
 	// Getting the allocation for the window frame allows us to
	// find the "origin" for the top view
	rectangle allocation;

	if (widget) {
		cosmoe_widget_get_allocation((cosmoe_widget_t)widget, &allocation);
	} else {
		allocation.x = 0;
		allocation.y = 0;
		allocation.width = width;
		allocation.height = height;	
	}

	BMessage msg(B_WINDOW_RESIZED);
	msg.AddInt64("when", system_time());
	msg.AddInt32("width", allocation.width);
	msg.AddInt32("height", allocation.height);
	win->PostMessage(&msg, win);
}


static void
close_handler(void *data)
{
    STRACE("close_handler\n");
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* win = dynamic_cast<BWindow*>(handler);
	
	if (!win)
		return;
	
	BMessage message(B_QUIT_REQUESTED);
	status_t err = win->PostMessage(&message);
	if (err)
		STRACE(("close_handler PostMessage err: %d\n", err));
}


static void
screen_handler(void *data)
{
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* win = dynamic_cast<BWindow*>(handler);
	if (win == NULL)
		return;

	BScreen screen(win);
	BMessage update(B_SCREEN_CHANGED);
	update.AddInt64("when", real_time_clock_usecs());
	update.AddRect("frame", screen.Frame());
	update.AddInt32("mode", (int32)screen.ColorSpace());
	win->PostMessage(&update);
}


static inline BRect
_NormalizedRect(BRect rect)
{
	if (rect.left > rect.right)
		std::swap(rect.left, rect.right);
	if (rect.top > rect.bottom)
		std::swap(rect.top, rect.bottom);
	return rect;
}


void
BWindow::_DrawPointerTrackingOverlayLocked(cairo_t* cr)
{
	if (cr == NULL || fPointerTrackingMode == TRACKING_NONE) {
		return;
	}

	BRect rect = _NormalizedRect(fTrackingCurrentRect);
	if (!rect.IsValid())
		return;

	if (fPointerTrackingMode == TRACKING_DRAG
		&& fTrackingDragBitmap != NULL) {
		BBitmap* bitmap = fTrackingDragBitmap;
		cairo_surface_t* imageSurface = NULL;
		bool destroySurface = false;
		uint8* premultipliedBits = NULL;

		if ((bitmap->Flags() & B_BITMAP_ACCEPTS_VIEWS) != 0
			&& bitmap->fWindow != NULL
			&& bitmap->fWindow->fBackingSurface != NULL) {
			imageSurface = bitmap->fWindow->fBackingSurface;
		} else {
			uint8* bits = (uint8*)bitmap->Bits();
			if (bits != NULL) {
				BRect bounds = bitmap->Bounds();
				int32 width = (int32)bounds.IntegerWidth() + 1;
				int32 height = (int32)bounds.IntegerHeight() + 1;
				if (width > 0 && height > 0) {
					uint8* sourceBits = bits;
					if (!prepare_bitmap_bits_for_cairo_argb32(bits,
							CAIRO_FORMAT_ARGB32, bitmap->ColorSpace(), width,
							height, bitmap->BytesPerRow(),
							(const uint8**)&sourceBits,
							&premultipliedBits)) {
						return;
					}

					imageSurface = cairo_image_surface_create_for_data(sourceBits,
						CAIRO_FORMAT_ARGB32, width, height, bitmap->BytesPerRow());
					destroySurface = imageSurface != NULL;
				}
			}
		}

		if (imageSurface != NULL
			&& cairo_surface_status(imageSurface) == CAIRO_STATUS_SUCCESS) {
			cairo_set_source_surface(cr, imageSurface, rect.left, rect.top);
			// Always composite on top of the already-blitted backing surface.
			// Using SOURCE here clears destination pixels outside image coverage,
			// which makes the window contents appear to disappear during drag.
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			cairo_paint(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		}

		if (destroySurface)
			cairo_surface_destroy(imageSurface);
		if (premultipliedBits != NULL)
			free(premultipliedBits);
		return;
	}

	cairo_save(cr);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	double dashes[] = { 4.0, 4.0 };
	cairo_set_dash(cr, dashes, 2, 0.0);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_rectangle(cr, rect.left + 0.5, rect.top + 0.5,
		rect.Width(), rect.Height());
	cairo_stroke(cr);
	cairo_restore(cr);
}

static inline double _DisplayScaleFactor(int32 scalePercent);
static inline int32 _DisplayScaleBufferScale(int32 scalePercent);
static inline int32 _RefreshWindowDisplayScale(BWindow* window);


void
view_redraw_handler(struct widget *widget, void *data)
{
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* window = dynamic_cast<BWindow*>(handler);
	if (window == NULL || widget == NULL)
		return;

	BView* view = window->fTopView;
	if (view == NULL)
		return;

	if (!view->IsHidden() && window && !window->UpdatesDisabled()) {
		// Refresh display scale from backend output state before blitting.
		// This lets first paint/popup paints pick up HiDPI without waiting for input events.
		_RefreshWindowDisplayScale(window);

		// Simply copy the backing store to the Wayland surface
		// No message passing - the drawing has already been done
		if (window->fBackingSurface != NULL) {
			pthread_mutex_lock(&window->fBackingSurfaceLock);

			// Skip copying if backing surface hasn't been drawn to yet
			// This prevents flashing gray when resizing
			if (window->fBackingSurfaceValid) {
				cairo_t* cr = cosmoe_widget_cairo_create((cosmoe_widget_t)widget);
				if (cr) {
					int32_t offset_h, offset_v;
					cosmoe_window_get_topview_offset(be_app->Display(), window->fWindowToken, &offset_h, &offset_v);
					
					// The fBackingSurface is at physical resolution but the CGContext is already
					// scaled by Cocoa for Retina. Scale the source pattern to compensate.
					cairo_set_source_surface(cr, window->fBackingSurface, offset_h, offset_v);
					
					double scale = _DisplayScaleFactor(window->fDisplayScalePercent);
					if (scale != 1.0) {
						// Scale the source pattern down so physical pixels map to logical coordinates
						cairo_pattern_t* pattern = cairo_get_source(cr);
						cairo_matrix_t matrix;
						cairo_matrix_init(&matrix,
							scale, 0.0,
							0.0, scale,
							-(double)offset_h * scale,
							-(double)offset_v * scale);
						cairo_pattern_set_matrix(pattern, &matrix);
					}
					
					const char* backendName = cosmoe_backend_get_current_name();
					const bool cocoaBackend = backendName != NULL
						&& strcmp(backendName, "cocoa") == 0;
					const bool waylandBackend = backendName != NULL
						&& strcmp(backendName, "Wayland") == 0;
					
					const BRegion& dirtyRegion = window->fBackingSurfaceDirtyRegion;
					if (!cocoaBackend && !waylandBackend && dirtyRegion.CountRects() > 0) {
						cairo_save(cr);
						for (int32 i = 0; i < dirtyRegion.CountRects(); i++) {
							BRect rect = dirtyRegion.RectAt(i);
							cairo_rectangle(cr, rect.left, rect.top,
								rect.Width() + 1, rect.Height() + 1);
						}
						cairo_clip(cr);
						cairo_paint(cr);
						cairo_restore(cr);
					} else {
						// A Wayland shm buffer has no preserved contents when a
						// different buffer leaf is attached. Copy the complete,
						// opaque window backing store instead of treating this as
						// a partial repaint of a persistent target.
						// Cocoa's CGContext is already clipped to drawRect:'s dirty
						// region. Reapplying fBackingSurfaceDirtyRegion here is unsafe:
						// it is shared with asynchronous update requests and can no
						// longer describe the exact native dirty region. Let Quartz
						// perform the target clipping while it copies the source.
						cairo_paint(cr);
					}
					window->fBackingSurfaceDirtyRegion.MakeEmpty();
					window->_DrawPointerTrackingOverlayLocked(cr);
					cairo_destroy(cr);
				}
			}
			
			pthread_mutex_unlock(&window->fBackingSurfaceLock);
		}
	}
}

// Track currently pressed mouse buttons globally for motion events
static uint32_t sCurrentButtons = 0;
static const uint32_t kMsgApplyDisplayScale = 'dScl';

static inline double
_DisplayScaleFactor(int32 scalePercent)
{
	if (scalePercent < 100)
		return 1.0;
	return (double)scalePercent / 100.0;
}


static inline int32
_DisplayScaleBufferScale(int32 scalePercent)
{
	int32 bufferScale = (scalePercent + 50) / 100;
	if (bufferScale < 1)
		return 1;
	if (bufferScale > 4)
		return 4;
	return bufferScale;
}

static inline int32
_RefreshWindowDisplayScale(BWindow* window)
{
	if (!window)
		return 100;

	int32 detectedScale = BDisplayScaleManager::GetScaleForWindow(window);
	if (detectedScale < 100)
		detectedScale = 100;
	if (detectedScale > 400)
		detectedScale = 400;

	if (detectedScale != window->fDisplayScalePercent) {
		if (find_thread(NULL) == window->Thread()) {
			window->SetDisplayScale(detectedScale);
		} else {
			// Backend callbacks run on the display thread. Schedule scale updates
			// on the window thread to avoid cross-thread backend message deadlocks.
			BMessage applyScale(kMsgApplyDisplayScale);
			applyScale.AddInt32("scale", detectedScale);
			BMessenger(NULL, window).SendMessage(&applyScale);
		}
	}

	// Use detected scale immediately for input coordinate conversion.
	return detectedScale;
}

static inline bool
_PointerCoordsNeedScaleDivide()
{
	const char* backendName = cosmoe_backend_get_current_name();
	if (backendName != NULL && strcmp(backendName, "Wayland") == 0)
		return false;
	return true;
}

void view_mouse_idle_handler(struct widget *widget,
	struct input *input, uint32_t time,
	int32_t x, int32_t y, void *data)
{
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* window = dynamic_cast<BWindow*>(handler);
	if (window == NULL || window->fTopView == NULL) {
		STRACE("ERROR: view_mouse_idle_handler called with NULL window/topview!\n");
		return;
	}

	BView* view = window->fTopView;	// This is fTopView
	BView* subView;
	rectangle allocation;

	cosmoe_widget_get_allocation((cosmoe_widget_t)widget, &allocation);

	// Convert the coordinates to be window-relative
	x -= allocation.x;
	y -= allocation.y;

	int32 scalePercent = _RefreshWindowDisplayScale(window);
	double scale = _DisplayScaleFactor(scalePercent);
	
	// Widget surface is at physical resolution, so coordinates are in physical pixels
	// Divide by scale to get logical coordinates
	if (_PointerCoordsNeedScaleDivide() && scale > 1.0) {
		x /= scale;
		y /= scale;
	}

	// Backend callbacks run on the display thread. Avoid the public
	// FindView() here, since it takes the window lock and can deadlock against
	// UI-thread modal loops (for example BAlert::Go()).
	subView = window->_FindView(window->fTopView, BPoint(x, y));
	if (subView) {
		view = subView;
	}

	if (view && view->Window()) {
		BMessage* msg = new BMessage(B_MOUSE_IDLE);
		BMessage::Private messagePrivate(msg);
		messagePrivate.SetTarget(B_PREFERRED_TOKEN);
		msg->AddInt64("when", system_time());
		msg->AddPoint("window_where", BPoint(x, y));
		msg->AddInt32("_view_token", _get_object_token_(view));
		
		// Send the message directly to preserve B_PREFERRED_TOKEN target
		BMessenger messenger(NULL, view->Window());
		messenger.SendMessage(msg);
	}
}

void view_button_handler(struct widget *widget,
	struct input *input, uint32_t time,
	uint32_t button,
	enum wl_pointer_button_state state,
	void *data)
{
	STRACE(("view_button_handler: widget=%p, input=%p, data=%p\n", widget, input, data));

	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* window = dynamic_cast<BWindow*>(handler);
	if (window == NULL || window->fTopView == NULL) {
		STRACE("ERROR: view_button_handler called with NULL window/topview!\n");
		return;
	}

	BView* view = window->fTopView;
	BView* subView;
	BView* dropTarget = NULL;
	int32 dispatchViewToken = B_NULL_TOKEN;
	rectangle allocation;
	static uint32_t lastClickTime = 0;
	static uint32_t lastClickButton = 0;
	int32 clicks = 1;
	bool hadButtonsDown = sCurrentButtons != 0;
	bool releaseAllButtons = false;
	bool hasDropMessage = false;
	BMessage dropMessage;
	BPoint dropOffset;

	// FIXME - remove WL_ codes here and below
	if (time - lastClickTime < 250 && lastClickButton == button && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		clicks++;
	}

	lastClickTime = time;
	lastClickButton = button;

	cosmoe_widget_get_allocation((cosmoe_widget_t)widget, &allocation);

	// Convert the coordinates to be window-relative
	int32_t x, y;
	cosmoe_input_get_position(input, &x, &y);
	STRACE(("view_button_handler: position x=%d, y=%d (after alloc adjustment: x=%d, y=%d)\n", x, y, x - allocation.x, y - allocation.y));
	x -= allocation.x;
	y -= allocation.y;
	int32 scalePercent = _RefreshWindowDisplayScale(window);
	double scale = _DisplayScaleFactor(scalePercent);
	// Widget surface is at physical resolution, so coordinates are in physical pixels
	// Divide by scale to get logical coordinates
	if (_PointerCoordsNeedScaleDivide() && scale > 1.0) {
		x /= scale;
		y /= scale;
	}

	BMessage* msg = new BMessage((state == WL_POINTER_BUTTON_STATE_PRESSED) ? B_MOUSE_DOWN : B_MOUSE_UP);

	subView = window->_FindView(window->fTopView, BPoint(x, y));
	if (subView) {
		view = subView;
	}
	dispatchViewToken = _get_object_token_(view);

	BView* pointerView = view;

	int32 buttons = 0;
	if (button == BTN_LEFT)
		buttons = B_PRIMARY_MOUSE_BUTTON;
	else if (button == BTN_RIGHT)
		buttons = B_SECONDARY_MOUSE_BUTTON;
	else if (button == BTN_MIDDLE)
		buttons = B_TERTIARY_MOUSE_BUTTON;
	
	// Update global button state
	if (state == WL_POINTER_BUTTON_STATE_PRESSED)
		sCurrentButtons |= buttons;
	else
		sCurrentButtons &= ~buttons;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (!hadButtonsDown)
			window->fMouseDownViewToken = _get_object_token_(pointerView);
	} else if (window->fMouseDownViewToken != B_NULL_TOKEN) {
		dispatchViewToken = window->fMouseDownViewToken;
		BView* downView = window->_FindView(window->fMouseDownViewToken);
		if (downView != NULL)
			view = downView;

		if (sCurrentButtons == 0)
			window->fMouseDownViewToken = B_NULL_TOKEN;
	}

	releaseAllButtons = state != WL_POINTER_BUTTON_STATE_PRESSED
		&& sCurrentButtons == 0;
	if (releaseAllButtons) {
		pthread_mutex_lock(&window->fBackingSurfaceLock);
		if (window->fPointerTrackingMode == BWindow::TRACKING_DRAG
			&& window->fTrackingDragMessage != NULL) {
			dropMessage = *window->fTrackingDragMessage;
			dropOffset = window->fTrackingDragOffset;
			hasDropMessage = true;
		}
		pthread_mutex_unlock(&window->fBackingSurfaceLock);

		if (hasDropMessage) {
			dropTarget = window->_FindView(window->fTopView, BPoint(x, y));
			if (dropTarget == NULL)
				dropTarget = window->fTopView;
		}

		window->_StopPointerTracking();
	}
	
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);

	msg->AddInt64("when", system_time());


	msg->AddInt32("buttons", sCurrentButtons);
	msg->AddInt32("modifiers", modifiers());
	msg->AddPoint("window_where", BPoint(x, y));
	msg->AddInt32("clicks", clicks);
	if (dispatchViewToken <= B_NULL_TOKEN)
		dispatchViewToken = _get_object_token_(view);
	msg->AddInt32("_view_token", dispatchViewToken);
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		msg->AddInt32("_token", dispatchViewToken);
		msg->AddBool("_feed_focus", true);
	}
	
	// Send the message directly to preserve B_PREFERRED_TOKEN target
	BMessenger messenger(NULL, window);
	messenger.SendMessage(msg);

	if (hasDropMessage && dropTarget != NULL) {
		BMessage dropped(dropMessage);
		BMessage::Private droppedPrivate(&dropped);
		droppedPrivate.SetWasDropped(true);

		BPoint windowWhere(x, y);
		BPoint screenWhere(windowWhere + window->fFrame.LeftTop());
		dropped.RemoveName("_drop_point_");
		dropped.AddPoint("_drop_point_", screenWhere);
		dropped.RemoveName("_drop_offset_");
		dropped.AddPoint("_drop_offset_", dropOffset);

		window->PostMessage(&dropped, dropTarget);
	}
}


int view_pointer_motion_handler(struct widget *widget,
	struct input *input, uint32_t time,
	float x, float y, void *data)
{
	if (data == NULL)
		return 0;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return 0;
	}

	BWindow* window = dynamic_cast<BWindow*>(handler);
	if (window == NULL || window->fTopView == NULL) {
		STRACE("ERROR: view_pointer_motion_handler called with NULL window/topview!\n");
		return 0;
	}

	BView* view = window->fTopView;	// This is fTopView
	int32 cursor = -1;
	BView* subView;
	rectangle allocation;

	cosmoe_widget_get_allocation((cosmoe_widget_t)widget, &allocation);

	// Convert the coordinates to be window-relative
	x -= allocation.x;
	y -= allocation.y;

	int32 scalePercent = _RefreshWindowDisplayScale(window);
	double scale = _DisplayScaleFactor(scalePercent);
	// Widget surface is at physical resolution, so coordinates are in physical pixels
	// Divide by scale to get logical coordinates
	if (_PointerCoordsNeedScaleDivide() && scale > 1.0) {
		x /= scale;
		y /= scale;
	}

	window->_UpdatePointerTracking(BPoint(x, y));

	// Keep a cross-window mouse position in a global coordinate space so
	// BView::GetMouse() fallback works while tracking from menubar into popup menus.
	view->sLastMousePosition.Set(x + window->fFrame.left, y + window->fFrame.top);

	BMessage msg(B_MOUSE_MOVED);

	// Backend callbacks run on the display thread. Avoid lock-taking
	// FindView() to prevent cross-thread lock inversion/deadlock.
	subView = window->_FindView(window->fTopView, BPoint(x, y));
	if (subView) {
		view = subView;
		cursor = subView->CursorID();
	}

	// While dragging with a button held, keep routing mouse moved events
	// to the original mouse-down target view.
	if (sCurrentButtons != 0 && window->fMouseDownViewToken != B_NULL_TOKEN) {
		BView* downView = window->_FindView(window->fMouseDownViewToken);
		if (downView != NULL) {
			view = downView;
			cursor = downView->CursorID();
		}
	}

	if (view) {
		BMessage::Private messagePrivate(&msg);
		messagePrivate.SetTarget(B_PREFERRED_TOKEN);
		msg.AddInt64("when", system_time());
		msg.AddPoint("window_where", BPoint(x, y));
		msg.AddInt32("buttons", sCurrentButtons);
		msg.AddInt32("_view_token", _get_object_token_(view));

		bool hasDragMessage = false;
		BMessage dragMessage;
		pthread_mutex_lock(&window->fBackingSurfaceLock);
		if (window->fPointerTrackingMode == BWindow::TRACKING_DRAG
			&& window->fTrackingDragMessage != NULL) {
			dragMessage = *window->fTrackingDragMessage;
			hasDragMessage = true;
		}
		pthread_mutex_unlock(&window->fBackingSurfaceLock);
		if (hasDragMessage)
			msg.AddMessage("be:drag_message", &dragMessage);
		
		// Send the message directly to preserve B_PREFERRED_TOKEN target
		BMessenger messenger(NULL, window);
		messenger.SendMessage(&msg);
	}

	// If not, do we have an app cursor?
	if (cursor < 0)
		cursor = be_app->CursorID();

	// If neither, use the default cursor (cursor ID 0)
	if (cursor < 0)
		cursor = cosmoe_display_convert_cursor(0);
	else
		cursor = cosmoe_display_convert_cursor(cursor);

	return cursor;
}


void send_mouse_wheel(BWindow* window, BView* view, BPoint windowWhere,
	float deltaX, float deltaY)
{
	if (!view->IsHidden() && window != NULL && !window->UpdatesDisabled()) {
		BMessage msg(B_MOUSE_WHEEL_CHANGED);
		BMessage::Private messagePrivate(&msg);
		messagePrivate.SetTarget(B_PREFERRED_TOKEN);
		msg.AddInt64("when", system_time());
		msg.AddPoint("window_where", windowWhere);
		msg.AddInt32("buttons", sCurrentButtons);
		msg.AddInt32("modifiers", modifiers());
		msg.AddInt32("_view_token", _get_object_token_(view));
		msg.AddFloat("be:wheel_delta_x", deltaX);
		msg.AddFloat("be:wheel_delta_y", deltaY);

		BMessenger messenger(NULL, window);
		messenger.SendMessage(&msg);
	}
}


void view_axis_handler(struct widget *widget, struct input *input, uint32_t time,
	uint32_t axis, wl_fixed_t value, void *data)
{
	// FIXME: remove WL_ codes here
	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL || axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		if (data == NULL)
			return;

		intptr_t token = (intptr_t)data;
		BHandler* handler = NULL;
		if (token <= B_NULL_TOKEN
			|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
				(void**)&handler) != B_OK) {
			return;
		}

		BWindow* window = dynamic_cast<BWindow*>(handler);
		if (window == NULL || window->fTopView == NULL)
			return;

		BView* view = window->fTopView;
		BView* subView = view;  // Initialize to the main view
		rectangle allocation;
	
		cosmoe_widget_get_allocation((cosmoe_widget_t)widget, &allocation);
	
		// Convert the coordinates to be window-relative
		int32_t x, y;
		cosmoe_input_get_position(input, &x, &y);
		x -= allocation.x;
		y -= allocation.y;

		int32 scalePercent = _RefreshWindowDisplayScale(window);
		double scale = _DisplayScaleFactor(scalePercent);
		// Widget surface is at physical resolution, so coordinates are in physical pixels
		// Divide by scale to get logical coordinates
		if (_PointerCoordsNeedScaleDivide() && scale > 1.0) {
			x /= scale;
			y /= scale;
		}

		BView* foundView = window->_FindView(window->fTopView, BPoint(x, y));
		if (foundView) {
				subView = foundView;
		}

		float deltaX = (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ? cosmoe_fixed_to_double(value) : 0.0f;
		float deltaY = (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ? cosmoe_fixed_to_double(value) : 0.0f;

		send_mouse_wheel(window, subView, BPoint(x, y), deltaX, deltaY);
	}
}


void BWindow::SendModifiersEvent(BWindow* win, uint32 key, uint32 modifiers,
	uint32 oldModifiers)
{
	uint8 states[16];
	_get_key_states(states);

	BMessage* msg = new BMessage(B_MODIFIERS_CHANGED);
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);

	msg->AddInt64("when", real_time_clock());
	msg->AddData("states", B_UINT8_TYPE, states, sizeof(states));
	msg->AddInt32("key", key);
	msg->AddInt32("be:old_modifiers", oldModifiers);
	msg->AddInt32("modifiers", modifiers);

	win->AddMessage(msg);
}

void BWindow::SendKeyEvent(BWindow* win, uint32 key, uint32 sym, int32 what, uint32 modifiers)
{
	_set_key_state(key, what == B_KEY_DOWN);

	char string[2];
	string[0] = sym;
	string[1] = 0;
	uint8 states[16];
	_get_key_states(states);

	BMessage* msg = new BMessage(what);
	BMessage::Private messagePrivate(msg);
	messagePrivate.SetTarget(B_PREFERRED_TOKEN);

	msg->AddInt64("when", real_time_clock());
	msg->AddData("states", B_UINT8_TYPE, states, sizeof(states));
	msg->AddInt32("key", key);
	msg->AddInt32("modifiers", modifiers);
	msg->AddInt8("byte", (int8)string[0]);
	msg->AddData("bytes", B_STRING_TYPE, string, 2);
	msg->AddInt32("raw_char", sym);
	if (what == B_KEY_DOWN)
		msg->AddInt32("be:key_repeat", 1);

	win->AddMessage(msg);
}


static void
dispatch_keyboard_event_to_interested_views(BWindow* window, BMessage* message,
	BView* view)
{
	if (view == NULL)
		return;

	if (view != window->CurrentFocus()
		&& (view->EventMask() & B_KEYBOARD_EVENTS) != 0) {
		int32 what = message->what;
		if (what == B_KEY_DOWN)
			what = B_UNMAPPED_KEY_DOWN;
		else if (what == B_KEY_UP)
			what = B_UNMAPPED_KEY_UP;

		BMessage copy(*message);
		copy.what = what;
		copy.AddBool("be:forwarded_keyboard_event", true);
		window->PostMessage(&copy, view);
	}

	for (int32 index = 0; index < view->CountChildren(); index++)
		dispatch_keyboard_event_to_interested_views(window, message,
			view->ChildAt(index));
}


static void
dispatch_keyboard_event_to_interested_views(BWindow* window, BMessage* message)
{
	if (window == NULL || message == NULL)
		return;

	for (int32 index = 0; index < window->CountChildren(); index++)
		dispatch_keyboard_event_to_interested_views(window, message,
			window->ChildAt(index));
}


static bool
is_key_pressed(uint32 key)
{
	uint8 states[16] = {};
	_get_key_states(states);
	if (key >= sizeof(states) * 8)
		return false;

	return (states[key / 8] & (1 << (7 - (key & 7)))) != 0;
}


#ifdef __linux__
static void
apply_control_modifier_state(uint32& modifiers, bool pressed, bool left)
{
	const uint32 sideModifier = left ? B_LEFT_CONTROL_KEY : B_RIGHT_CONTROL_KEY;
	const uint32 otherSide = left ? B_RIGHT_CONTROL_KEY : B_LEFT_CONTROL_KEY;
	if (pressed)
		modifiers |= sideModifier | B_CONTROL_KEY;
	else {
		modifiers &= ~sideModifier;
		if ((modifiers & otherSide) == 0)
			modifiers &= ~B_CONTROL_KEY;
	}
}


static void
apply_command_modifier_state(uint32& modifiers, bool pressed, bool left)
{
	const uint32 commandSide = left ? B_LEFT_COMMAND_KEY : B_RIGHT_COMMAND_KEY;
	const uint32 otherCommandSide = left ? B_RIGHT_COMMAND_KEY : B_LEFT_COMMAND_KEY;

	if (pressed) {
		modifiers |= commandSide | B_COMMAND_KEY;
	} else {
		modifiers &= ~commandSide;
		if ((modifiers & otherCommandSide) == 0)
			modifiers &= ~B_COMMAND_KEY;
	}
}


static void
apply_option_modifier_state(uint32& modifiers, bool pressed, bool left)
{
	const uint32 optionSide = left ? B_LEFT_OPTION_KEY : B_RIGHT_OPTION_KEY;
	const uint32 otherOptionSide = left ? B_RIGHT_OPTION_KEY : B_LEFT_OPTION_KEY;

	if (pressed)
		modifiers |= optionSide | B_OPTION_KEY;
	else {
		modifiers &= ~optionSide;
		if ((modifiers & otherOptionSide) == 0)
			modifiers &= ~B_OPTION_KEY;
	}
}


static uint32
linux_shortcut_key_for_event(uint32 rawKey, uint32 modifiers, uint32 fallback)
{
	BKeymap keymap;
	if (keymap.SetToCurrent() != B_OK || keymap.IsModifierKey(rawKey))
		return fallback;

	char* chars = NULL;
	int32 numBytes = 0;
	keymap.GetChars(rawKey, modifiers & (B_SHIFT_KEY | B_CAPS_LOCK), 0,
		&chars, &numBytes);
	if (chars == NULL || numBytes <= 0)
		return fallback;

	return BUnicodeChar::ToUpper((unsigned char)chars[0]);
}


static bool
apply_linux_semantic_modifier_state(uint32 key, bool pressed, uint32& modifiers)
{
	BKeymap keymap;
	if (keymap.SetToCurrent() != B_OK)
		return false;

	const key_map& map = keymap.Map();
	bool handled = false;

	if (key == map.left_control_key) {
		apply_control_modifier_state(modifiers, pressed, true);
		handled = true;
	}
	if (key == map.right_control_key) {
		apply_control_modifier_state(modifiers, pressed, false);
		handled = true;
	}
	if (key == map.left_option_key) {
		apply_option_modifier_state(modifiers, pressed, true);
		handled = true;
	}
	if (key == map.right_option_key) {
		apply_option_modifier_state(modifiers, pressed, false);
		handled = true;
	}
	if (key == map.left_command_key) {
		apply_command_modifier_state(modifiers, pressed, true);
		handled = true;
	}
	if (key == map.right_command_key) {
		apply_command_modifier_state(modifiers, pressed, false);
		handled = true;
	}

	return handled;
}
#endif


void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
	if (data == NULL)
		return;

	intptr_t token = (intptr_t)data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* callbackWindow = dynamic_cast<BWindow*>(handler);
	if (callbackWindow == NULL)
		return;

	// Kept in interface.cpp
	uint32 newModifiers = modifiers();
	uint32 oldModifiers = newModifiers;
	uint32 backendLockModifiers = 0;
	const bool hasBackendLockModifiers
		= _get_backend_lock_modifiers(&backendLockModifiers) != 0;

	int32 what = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? B_KEY_DOWN : B_KEY_UP;
	const bool wasPressed = is_key_pressed(key);
	_set_key_state(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);

	#ifdef __linux__
	const bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
	bool handledByLinuxSemanticModifier = apply_linux_semantic_modifier_state(key, pressed, newModifiers);
	#endif

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
			#ifdef __linux__
			if (handledByLinuxSemanticModifier)
				break;
			#endif
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_LEFT_CONTROL_KEY | B_CONTROL_KEY;
			else {
				newModifiers &= ~B_LEFT_CONTROL_KEY;
				if ((newModifiers & B_RIGHT_CONTROL_KEY) == 0)
					newModifiers &= ~B_CONTROL_KEY;
			}
			break;

		case KEY_RIGHTCTRL:
			#ifdef __linux__
			if (handledByLinuxSemanticModifier)
				break;
			#endif
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_RIGHT_CONTROL_KEY | B_CONTROL_KEY;
				else {
				newModifiers &= ~B_RIGHT_CONTROL_KEY;
				if ((newModifiers & B_LEFT_CONTROL_KEY) == 0)
					newModifiers &= ~B_CONTROL_KEY;
			}
			break;

		case KEY_LEFTALT:
			#ifdef __linux__
			if (handledByLinuxSemanticModifier)
				break;
			#endif
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				newModifiers |= B_LEFT_OPTION_KEY | B_OPTION_KEY;
				#ifndef __APPLE__
				newModifiers |= B_LEFT_COMMAND_KEY | B_COMMAND_KEY;
				#endif
			}
			else {
				newModifiers &= ~B_LEFT_OPTION_KEY;
				#ifndef __APPLE__
				newModifiers &= ~B_LEFT_COMMAND_KEY;
				#endif
				if ((newModifiers & B_RIGHT_OPTION_KEY) == 0)
					newModifiers &= ~B_OPTION_KEY;
				#ifndef __APPLE__
				if ((newModifiers & B_RIGHT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
				#endif
			}
			break;

		case KEY_RIGHTALT:
			#ifdef __linux__
			if (handledByLinuxSemanticModifier)
				break;
			#endif
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				newModifiers |= B_RIGHT_OPTION_KEY | B_OPTION_KEY;
				#ifndef __APPLE__
				newModifiers |= B_RIGHT_COMMAND_KEY | B_COMMAND_KEY;
				#endif
			}
			else {
				newModifiers &= ~B_RIGHT_OPTION_KEY;
				#ifndef __APPLE__
				newModifiers &= ~B_RIGHT_COMMAND_KEY;
				#endif
				if ((newModifiers & B_LEFT_OPTION_KEY) == 0)
					newModifiers &= ~B_OPTION_KEY;
				#ifndef __APPLE__
				if ((newModifiers & B_LEFT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
				#endif
			}
			break;

		#ifdef __APPLE__
		case KEY_LEFTMETA:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_LEFT_COMMAND_KEY | B_COMMAND_KEY;
			else {
				newModifiers &= ~B_LEFT_COMMAND_KEY;
				if ((newModifiers & B_RIGHT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
			}
			break;

		case KEY_RIGHTMETA:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_RIGHT_COMMAND_KEY | B_COMMAND_KEY;
			else {
				newModifiers &= ~B_RIGHT_COMMAND_KEY;
				if ((newModifiers & B_LEFT_COMMAND_KEY) == 0)
					newModifiers &= ~B_COMMAND_KEY;
			}
			break;
		#endif

		case KEY_MENU:
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				newModifiers |= B_MENU_KEY;
			else
				newModifiers &= ~B_MENU_KEY;
			break;

		case KEY_CAPSLOCK:
			if (!hasBackendLockModifiers
				&& state == WL_KEYBOARD_KEY_STATE_PRESSED && !wasPressed) {
				if ((newModifiers & B_CAPS_LOCK) != 0)
					newModifiers &= ~B_CAPS_LOCK;
				else
					newModifiers |= B_CAPS_LOCK;
			}
			break;

		case KEY_SCROLLLOCK:
			if (!hasBackendLockModifiers
				&& state == WL_KEYBOARD_KEY_STATE_PRESSED && !wasPressed) {
				if ((newModifiers & B_SCROLL_LOCK) != 0)
					newModifiers &= ~B_SCROLL_LOCK;
				else
					newModifiers |= B_SCROLL_LOCK;
			}
			break;

		case KEY_NUMLOCK:
			if (!hasBackendLockModifiers
				&& state == WL_KEYBOARD_KEY_STATE_PRESSED && !wasPressed) {
				if ((newModifiers & B_NUM_LOCK) != 0)
					newModifiers &= ~B_NUM_LOCK;
				else
					newModifiers |= B_NUM_LOCK;
			}
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

		case KEY_ENTER:
		case KEY_KPENTER:
			sym = B_ENTER;
			break;

		case KEY_BACKSPACE:
			sym = B_BACKSPACE;
			break;

		default:
			// Key was not a modifier key or remapped key
			break;
	}

	if (hasBackendLockModifiers) {
		newModifiers &= ~(B_CAPS_LOCK | B_SCROLL_LOCK | B_NUM_LOCK);
		newModifiers |= backendLockModifiers
			& (B_CAPS_LOCK | B_SCROLL_LOCK | B_NUM_LOCK);
	}

	if (newModifiers != oldModifiers) {
		set_modifiers(newModifiers);
		BWindow::SendModifiersEvent(callbackWindow, key, newModifiers,
			oldModifiers);
	} else {
		BWindow::SendKeyEvent(callbackWindow, key, sym, what, newModifiers);
	}
}

// Callback invoked by the graphics backend when the window moves.
static void
window_move_handler(cosmoe_window_t _window, int32_t x, int32_t y, void* user_data)
{
	if (!user_data)
		return;

	intptr_t token = (intptr_t)user_data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* win = dynamic_cast<BWindow*>(handler);
	if (!win)
		return;

	const char* backendName = cosmoe_backend_get_current_name();
	if (backendName && strcmp(backendName, "Wayland") == 0) {
		x = 0;
		y = 0;
	}

	_RefreshWindowDisplayScale(win);

	BMessage msg(B_WINDOW_MOVED);
	msg.AddInt64("when", system_time());
	msg.AddPoint("where", BPoint((float)x, (float)y));
	win->PostMessage(&msg, win);
}

// Callback invoked by the graphics backend when the window gets/loses focus.
static void
window_focus_handler(cosmoe_window_t _window, bool focused, void* user_data)
{
	if (!user_data)
		return;

	intptr_t token = (intptr_t)user_data;
	BHandler* handler = NULL;
	if (token <= B_NULL_TOKEN
		|| gDefaultTokens.GetToken((int32)token, B_HANDLER_TOKEN,
			(void**)&handler) != B_OK) {
		return;
	}

	BWindow* win = dynamic_cast<BWindow*>(handler);
	if (!win)
		return;
	
	BMessage msg(B_WINDOW_ACTIVATED);
	msg.AddBool("active", focused);
	win->PostMessage(&msg, win);
}

thread_id BWindow::sDisplayThread = -1;

// Cosmoe: fix sticky mode handling in our appserver-less case
uint32 BWindow::sNonMenuClickSequence = 0;

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
	fTopView = NULL;

	// remove all remaining shortcuts
	Shortcut::SplayTree* shortcuts = Shortcut::CastToTree(&fShortcuts);
	while (!shortcuts->IsEmpty()) {
		Shortcut* shortcut = shortcuts->Root();
		shortcuts->Remove(shortcut);
		delete shortcut;
	}

	// TODO: release other dynamically-allocated objects
	free(fTitle);

	// disable pulsing
	SetPulseRate(0);

	// Fixme: combine this code with _SendShowOrHideMessage
	if (fWindowToken != B_NULL_TOKEN) {
		/* Set fWindowToken to B_NULL_TOKEN FIRST so handlers can detect destruction */
		int32_t tempToken = fWindowToken;
		fWindowToken = B_NULL_TOKEN;

		// tell app_server about our demise
		BEGIN_MESSAGE
		fLink->StartMessage(AS_DELETE_WINDOW);
		fLink->Attach<int32_t>(tempToken);
		fLink->Flush();
	}

	pthread_mutex_lock(&fBackingSurfaceLock);
	_ClearTrackingStateLocked();
	if (fBackingSurface != NULL) {
		cairo_surface_destroy(fBackingSurface);
		fBackingSurface = NULL;
	}

	pthread_mutex_unlock(&fBackingSurfaceLock);
	pthread_mutex_destroy(&fBackingSurfaceLock);
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
	if (ret == B_OK)
		ret = data->AddInt32("_wspace", (uint32)Workspaces());

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

		STRACE(("ERROR - you must Lock a looper before calling Quit(), team=%" B_PRId32 ", looper=%s\n", Team(), name));
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

	BEGIN_MESSAGE
	fLink->StartMessage(AS_MINIMIZE_WINDOW);
	fLink->Attach<int32_t>(fWindowToken);
	fLink->Attach<bool>(minimize);
	fLink->Flush();

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
	// no-op for Cosmoe
}


void
BWindow::Sync() const
{
	// no-op for Cosmoe
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

	if (fUpdateRequested && be_app && be_app->Display()
		&& fWindowToken != B_NULL_TOKEN) {
		fUpdateRequested = false;
		BEGIN_MESSAGE
		fLink->StartMessage(AS_FORCE_UPDATE);
		fLink->Attach<int32_t>(fWindowToken);
			fLink->Attach<BRect>(BRect(0, 0, -1, -1));
		fLink->Flush();
	}
}


bool
BWindow::UpdatesDisabled() const
{
	return fUpdatesDisabled;
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


bool
BWindow::IsFront() const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return false;

	BEGIN_MESSAGE
	fLink->StartMessage(AS_IS_FRONT_WINDOW);
	fLink->Attach<int32_t>(fWindowToken);

	status_t status;
	if (fLink->FlushWithReply(status) == B_OK)
		return status >= B_OK;

	return false;
}


void
BWindow::MessageReceived(BMessage* message)
{
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
			if (message->what == B_GET_PROPERTY) {
				replyMsg.AddInt32( "result", Workspaces());
				handled = true;
			} else {
				uint32 newWorkspaces;
				if (message->FindInt32("data", (int32*)&newWorkspaces) == B_OK) {
					SetWorkspaces(newWorkspaces);
					handled = true;
				}
			}
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
			if (message->what == B_GET_PROPERTY) {
				BMessage settings;
				if (GetDecoratorSettings(&settings) == B_OK) {
					BRect frame;
					if (settings.FindRect("tab frame", &frame) == B_OK) {
						replyMsg.AddRect("result", frame);
						handled = true;
					}
				}
			}
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

	// Cosmoe: fix sticky mode handling for appserver-less case
	// Track B_MOUSE_DOWN events on non-menu windows for menu tracking
	if (message->what == B_MOUSE_DOWN) {
		window_feel feel = Feel();
		if (feel != kMenuWindowFeel)
			sNonMenuClickSequence++;
	}

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

		case kMsgApplyDisplayScale:
		{
			int32 scalePercent = 100;
			if (message->FindInt32("scale", &scalePercent) == B_OK
				&& scalePercent >= 100 && scalePercent <= 400
				&& scalePercent != fDisplayScalePercent) {
				SetDisplayScale(scalePercent);
			}
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

				// Some backends can emit transient move coordinates while the user
				// is resizing (for example, decorator offsets). Refreshing from the
				// backend here keeps Frame().left/top properly in sync.  This should
				// be revisited later though, since it shouldn't be happening in the
				// first place.
				if (fParentWindow == NULL) {
					_UpdateFrame();
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
					ScreenChanged(frame, (color_space)mode);
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
			if (!message->HasBool("be:forwarded_keyboard_event"))
				dispatch_keyboard_event_to_interested_views(this, message);
			if (!_HandleKeyDown(message))
				target->MessageReceived(message);
			break;

		case B_KEY_UP:
			if (!message->HasBool("be:forwarded_keyboard_event"))
				dispatch_keyboard_event_to_interested_views(this, message);
			target->MessageReceived(message);
			break;

		case B_UNMAPPED_KEY_DOWN:
			if (!_HandleUnmappedKeyDown(message))
				target->MessageReceived(message);
			break;

		case B_MODIFIERS_CHANGED:
			if (!message->HasBool("be:forwarded_keyboard_event"))
				dispatch_keyboard_event_to_interested_views(this, message);
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
				BRect backingSurfaceDamage;

			{

				// read tokens for views that need to be drawn
				// NOTE: we need to read the tokens completely
				// first, we cannot draw views in between reading
				// the tokens, since other communication would likely
				// mess up the data in the link.
				struct ViewUpdateInfo {
					int32 token;
					BRect updateRect;
					int32 order;
				};
				BList infos(20);
				int32 index = 0;
				int32 order = 0;

				while (true) {
					// read next token and create/add ViewUpdateInfo

					ViewUpdateInfo* info = new(std::nothrow) ViewUpdateInfo;
					if (info == NULL) {
						break;
					}
					
					// Try to read token and updateRect at this index
					if (message->FindInt32("token", index, (int32*)&info->token) != B_OK
						|| message->FindRect("updateRect", index, (BRect*)&info->updateRect) != B_OK) {
						// No more tokens in this message
						delete info;
						break;
					}
					
					if (!infos.AddItem(info)) {
						delete info;
						break;
					}
					info->order = order++;
					
					index++;
				}
				
				// Now collect ALL other pending _UPDATE_ messages from the queue
				// This batches multiple invalidations into a single draw cycle
				BMessageQueue* queue = MessageQueue();
				if (queue) {
					queue->Lock();
					
					// Extract all _UPDATE_ messages from the queue
					for (int32 i = queue->CountMessages() - 1; i >= 0; i--) {
						BMessage* msg = queue->FindMessage(i);
						if (msg && msg->what == _UPDATE_) {
							// Read all tokens from this additional message
							int32 msgIndex = 0;
							while (true) {
								ViewUpdateInfo* batchInfo = new(std::nothrow) ViewUpdateInfo;
								if (batchInfo == NULL)
									break;
									
								if (msg->FindInt32("token", msgIndex, (int32*)&batchInfo->token) != B_OK
									|| msg->FindRect("updateRect", msgIndex, (BRect*)&batchInfo->updateRect) != B_OK) {
									delete batchInfo;
									break;
								}
								
								if (!infos.AddItem(batchInfo)) {
									delete batchInfo;
									break;
								}
								batchInfo->order = order++;
								
								msgIndex++;
							}
							
							// Remove this message from the queue
							queue->RemoveMessage(msg);
							delete msg;
						}
					}
					
					queue->Unlock();
				}

				// Lock the backing surface before drawing to prevent the display thread
				// from copying it to screen while we're in the middle of drawing
				pthread_mutex_lock(&fBackingSurfaceLock);

				// Coalesce duplicate tokens first to avoid re-drawing the same view
				// multiple times in a single batched _UPDATE_ cycle.
				struct CoalescedUpdate {
					int32 token;
					BRect updateRect;
					int32 order;
				};

				std::vector<CoalescedUpdate> sortedInfos;
				std::unordered_map<int32, size_t> tokenToIndex;
				int32 count = infos.CountItems();
				sortedInfos.reserve(count);
				tokenToIndex.reserve(count);

				for (int32 i = 0; i < count; i++) {
					ViewUpdateInfo* info = (ViewUpdateInfo*)infos.ItemAtFast(i);
					if (info == NULL)
						continue;

					auto found = tokenToIndex.find(info->token);
					if (found == tokenToIndex.end()) {
						tokenToIndex[info->token] = sortedInfos.size();
						sortedInfos.push_back({info->token, info->updateRect,
							info->order});
					} else {
						CoalescedUpdate& existing = sortedInfos[found->second];
						existing.updateRect = existing.updateRect | info->updateRect;
					}

					delete info;
				}

				auto viewDepth = [this](int32 token) -> int32 {
					BView* view = _FindView(token);
					int32 depth = 0;
					for (BView* parent = view != NULL ? view->fParent : NULL;
							parent != NULL; parent = parent->fParent) {
						depth++;
					}
					return depth;
				};

				std::stable_sort(sortedInfos.begin(), sortedInfos.end(),
					[&](const CoalescedUpdate& a, const CoalescedUpdate& b) {
						int32 depthA = viewDepth(a.token);
						int32 depthB = viewDepth(b.token);
						if (depthA != depthB)
							return depthA < depthB;
						return a.order < b.order;
					});

				for (size_t i = 0; i < sortedInfos.size(); i++) {
//bigtime_t drawStart = system_time();
					const CoalescedUpdate& info = sortedInfos[i];
					if (BView* view = _FindView(info.token)) {
						if (!view->IsHidden())
							view->_DrawBackground(info.updateRect);
					}
					else {
						STRACE(("_UPDATE_ - didn't find view by token: %" B_PRId32 "\n", info.token));
					}
					// If view not found, it was likely removed/destroyed before
					// this _UPDATE_ message was processed - just skip it silently
//drawTime += system_time() - drawStart;
				}

				for (size_t i = 0; i < sortedInfos.size(); i++) {
					const CoalescedUpdate& info = sortedInfos[i];
					if (BView* view = _FindView(info.token)) {
						if (!view->IsHidden())
							view->_Draw(info.updateRect);
					}
				}

				// DrawAfterChildren in reverse depth order.
				for (size_t i = sortedInfos.size(); i-- > 0;) {
					const CoalescedUpdate& info = sortedInfos[i];
					if (BView* view = _FindView(info.token)) {
						if (!view->IsHidden())
							view->_DrawAfterChildren(info.updateRect);
					}
				}

				for (const CoalescedUpdate& info : sortedInfos) {
					if (BView* view = _FindView(info.token))
						fBackingSurfaceDirtyRegion.Include(
							view->ConvertToWindow(info.updateRect));
				}
				backingSurfaceDamage = fBackingSurfaceDirtyRegion.Frame();

				// Mark backing surface as valid now that drawing is complete
				// This allows view_redraw_handler to copy it to the display
				fBackingSurfaceValid = true;

				// Unlock the backing surface now that drawing is complete
				pthread_mutex_unlock(&fBackingSurfaceLock);

//printf("  %ld views drawn, total Draw() time: %lld\n", count, drawTime);
			}

			fInTransaction = false;

			// Trigger backend redraw now that drawing is complete
			if (!fUpdatesDisabled
				&& be_app && be_app->Display() && fWindowToken != B_NULL_TOKEN) {
				fUpdateRequested = false;
				BEGIN_MESSAGE
				fLink->StartMessage(AS_FORCE_UPDATE);
				fLink->Attach<int32_t>(fWindowToken);
				fLink->Attach<BRect>(backingSurfaceDamage);
				fLink->Flush();
			} else if (fUpdatesDisabled) {
				fUpdateRequested = true;
			}

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

	if (fWindowToken == B_NULL_TOKEN)
		return;

	if (!Lock())
		return;

	// Propagate to backend and read back enforced limits/frame.
	BEGIN_MESSAGE
	fLink->StartMessage(AS_SET_SIZE_LIMITS);
	fLink->Attach<int32_t>(fWindowToken);
	fLink->Attach<BRect>(fFrame);
	fLink->Attach<float>(minWidth);
	fLink->Attach<float>(maxWidth);
	fLink->Attach<float>(minHeight);
	fLink->Attach<float>(maxHeight);

	int32 code;
	if (fLink->FlushWithReply(code) == B_OK
		&& code == B_OK) {
		// read the values that were really enforced on
		// the server side (the window frame could have
		// been changed, too)
		fLink->Read<BRect>(&fFrame);
		fLink->Read<float>(&fMinWidth);
		fLink->Read<float>(&fMaxWidth);
		fLink->Read<float>(&fMinHeight);
		fLink->Read<float>(&fMaxHeight);

		_AdoptResize();
			// TODO: the same has to be done for SetLook() (that can alter
			//		the size limits, and hence, the size of the window
	}
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


status_t
BWindow::GetDecoratorSettings(BMessage* settings) const
{
	return B_NOT_SUPPORTED;
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
BWindow::ScreenChanged(BRect screenSize, color_space depth)
{
	// Hook function
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

	if (!Shortcut::CastToTree(&fShortcuts)->Insert(shortcut))
		delete shortcut;
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

	if (!Shortcut::CastToTree(&fShortcuts)->Insert(shortcut))
		delete shortcut;
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
	if (shortcut != NULL && Shortcut::CastToTree(&fShortcuts)->Remove(shortcut))
		delete shortcut;
	else if (key == 'Q' && modifiers == B_COMMAND_KEY)
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
	if (fTopView == NULL)
		return NULL;

	return fTopView->FindView(viewName);
}


BView*
BWindow::FindView(BPoint point) const
{
	BAutolock locker(const_cast<BWindow*>(this));
	if (!locker.IsLocked())
		return NULL;
	if (fTopView == NULL)
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

		BEGIN_MESSAGE
		fLink->StartMessage(AS_ACTIVATE_WINDOW);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<bool>(active);
		fLink->Flush();
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
	if (point == NULL)
		return;

	point->x += fFrame.left;
	point->y += fFrame.top;
}


BPoint
BWindow::ConvertToScreen(BPoint point) const
{
	return point + fFrame.LeftTop();
}


void
BWindow::ConvertFromScreen(BPoint* point) const
{
	if (point == NULL)
		return;

	point->x -= fFrame.left;
	point->y -= fFrame.top;
}


BPoint
BWindow::ConvertFromScreen(BPoint point) const
{
	return point - fFrame.LeftTop();
}


void
BWindow::ConvertToScreen(BRect* rect) const
{
	if (rect == NULL)
		return;

	rect->OffsetBy(fFrame.LeftTop());
}


BRect
BWindow::ConvertToScreen(BRect rect) const
{
	return rect.OffsetByCopy(fFrame.LeftTop());
}


void
BWindow::ConvertFromScreen(BRect* rect) const
{
	if (rect == NULL)
		return;

	rect->OffsetBy(-fFrame.left, -fFrame.top);
}


BRect
BWindow::ConvertFromScreen(BRect rect) const
{
	return rect.OffsetByCopy(-fFrame.left, -fFrame.top);
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


BRect
BWindow::DecoratorFrame() const
{
	BRect decoratorFrame(Frame());
	BRect tabRect(0, 0, 0, 0);

	float borderWidth = 5.0;

	BMessage settings;
	if (GetDecoratorSettings(&settings) == B_OK) {
		settings.FindRect("tab frame", &tabRect);
		settings.FindFloat("border width", &borderWidth);
	} else {
		// probably no-border window look
		if (fLook == B_NO_BORDER_WINDOW_LOOK)
			borderWidth = 0.f;
		else if (fLook == B_BORDERED_WINDOW_LOOK)
			borderWidth = 1.f;
		// else use fall-back values from above
	}

	if (fLook == kLeftTitledWindowLook) {
		decoratorFrame.top -= borderWidth;
		decoratorFrame.left -= borderWidth + tabRect.Width();
		decoratorFrame.right += borderWidth;
		decoratorFrame.bottom += borderWidth;
	} else {
		decoratorFrame.top -= borderWidth + tabRect.Height();
		decoratorFrame.left -= borderWidth;
		decoratorFrame.right += borderWidth;
		decoratorFrame.bottom += borderWidth;
	}

	return decoratorFrame;
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

	// we notify the app_server so we can actually see the change
	if (Lock()) {
		BEGIN_MESSAGE
		fLink->StartMessage(AS_SET_WINDOW_TITLE);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->AttachString(fTitle);
		fLink->Flush();
		Unlock();
	}
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
BWindow::AddToSubset(BWindow* window)
{
	if (window == NULL || window->Feel() != B_NORMAL_WINDOW_FEEL
		|| (fFeel != B_MODAL_SUBSET_WINDOW_FEEL
			&& fFeel != B_FLOATING_SUBSET_WINDOW_FEEL))
		return B_BAD_VALUE;

	return B_OK;
}


status_t
BWindow::RemoveFromSubset(BWindow* window)
{
	if (window == NULL || window->Feel() != B_NORMAL_WINDOW_FEEL
		|| (fFeel != B_MODAL_SUBSET_WINDOW_FEEL
			&& fFeel != B_FLOATING_SUBSET_WINDOW_FEEL))
		return B_BAD_VALUE;

	return B_OK;
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

	if (Lock()) {
		BEGIN_MESSAGE
		fLink->StartMessage(AS_SET_LOOK);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<uint32>((uint32)fLook);
		fLink->Flush();
		Unlock();
	}

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

	if (Lock()) {
		BEGIN_MESSAGE
		fLink->StartMessage(AS_SET_FEEL);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<uint32>((uint32)fFeel);
		fLink->Flush();
		Unlock();
	}

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

	BEGIN_MESSAGE
	fLink->StartMessage(AS_SET_FLAGS);
	fLink->Attach<int32_t>(fWindowToken);
	fLink->Attach<uint32>(fFlags);
	fLink->Flush();

	return B_OK;
}


uint32
BWindow::Flags() const
{
	return fFlags;
}


status_t
BWindow::SetWindowAlignment(window_alignment mode,
	int32 h, int32 hOffset, int32 width, int32 widthOffset,
	int32 v, int32 vOffset, int32 height, int32 heightOffset)
{
	return B_OK;
}


uint32
BWindow::Workspaces() const
{
	if (!const_cast<BWindow*>(this)->Lock())
		return 0;

	uint32 workspaces = 0;

	const_cast<BWindow*>(this)->Unlock();
	return workspaces;
}


void
BWindow::SetWorkspaces(uint32 workspaces)
{
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

	const char* backend_name = cosmoe_backend_get_current_name();
	const bool isWayland = backend_name && strcmp(backend_name, "Wayland") == 0;

	if (fParentWindow != NULL) {
		if (isWayland) {
			const bool parentIsPanel = WindowIsPanel(fParentWindow->Flags());
			// Wayland popups are positioned relative to their parent surface.
			// Menu code passes screen-space coordinates, so convert to parent-local.
			BPoint parentOrigin = fParentWindow->fFrame.LeftTop();
			if (parentIsPanel) {
				BRect screenFrame = BScreen(fParentWindow).Frame();
				private_window_panel_placement placement =
					WindowPanelPlacement(fParentWindow->Flags());

				switch (placement) {
					case kWindowPanelRight:
					case kWindowPanelRightTop:
					case kWindowPanelRightBottom:
						parentOrigin.x = screenFrame.right
							- fParentWindow->fFrame.Width();
						break;

					case kWindowPanelLeft:
					case kWindowPanelLeftTop:
					case kWindowPanelLeftBottom:
						parentOrigin.x = screenFrame.left;
						break;

					default:
						break;
				}

				switch (placement) {
					case kWindowPanelBottom:
					case kWindowPanelLeftBottom:
					case kWindowPanelRightBottom:
						parentOrigin.y = screenFrame.bottom
							- fParentWindow->fFrame.Height();
						break;

					case kWindowPanelTop:
					case kWindowPanelLeftTop:
					case kWindowPanelRightTop:
						parentOrigin.y = screenFrame.top;
						break;

					default:
						break;
				}
			}
			fPopupPosition.Set(x - parentOrigin.x, y - parentOrigin.y);
		} else {
			// X11/Windows/Cocoa popup creation expects absolute screen coordinates.
			fPopupPosition.Set(x, y);
		}

		// Keep the logical window frame in sync for popup windows too. Several
		// menu hit-testing/conversion paths read Frame()/fFrame.
		if (fFrame.left != x || fFrame.top != y)
			fFrame.OffsetTo(x, y);
	} else if (fFeel == kMenuWindowFeel && !isWayland) {
		// Standalone popup on non-Wayland backends: use absolute coordinates.
		fPopupPosition.Set(x, y);
		if (fFrame.left != x || fFrame.top != y)
			fFrame.OffsetTo(x, y);
	}

	// If Show() already fired but backend doesn't exist yet, create it now at
	// the correct position. For Wayland, a popup parent is mandatory.
	if (fFeel == kMenuWindowFeel && fHadShow && fWindowToken == B_NULL_TOKEN) {
		if (isWayland && fParentWindow == NULL) {
			STRACE(("MoveTo: cannot create Wayland popup '%s' without parent window\n", Name()));
		} else {
			fHadShow = false;
			STRACE(("MoveTo: creating deferred popup backend for '%s' at (%.0f,%.0f)\n",
				Name(), fPopupPosition.x, fPopupPosition.y));
			int32_t parentToken = B_NULL_TOKEN;
			if (fParentWindow != NULL)
				parentToken = fParentWindow->fWindowToken;
			fWindowToken = _get_object_token_(this);
			void* callbackData = (void*)(intptr_t)_get_object_token_(this);
			{
				BEGIN_MESSAGE
				fLink->StartMessage(AS_CREATE_POPUP_WINDOW);
				fLink->Attach<void*>(be_app->Display());
				fLink->Attach<int32_t>(fWindowToken);
				fLink->Attach<int32_t>(parentToken);
				fLink->Attach<int32_t>((int32_t)fPopupPosition.x);
				fLink->Attach<int32_t>((int32_t)fPopupPosition.y);
				fLink->Attach<void*>(callbackData);
				const char* appSig = be_app->Signature();
				fLink->AttachString(appSig ? appSig : "");
				fLink->Attach<void*>(fTopView);
				fLink->Attach<int32_t>(Bounds().IntegerWidth());
				fLink->Attach<int32_t>(Bounds().IntegerHeight());
				fLink->Flush();
			}
			// Complete the show (registers callbacks, resizes)
			_SendShowOrHideMessage();
		}
	}

	// If backend window exists, update its position.
	// For popup windows, use fPopupPosition which includes the parent's screen
	// offset (correct for X11). For regular windows, use x,y directly.
	if (fWindowToken != B_NULL_TOKEN) {
		if (fParentWindow != NULL) {
			BEGIN_MESSAGE
			fLink->StartMessage(AS_WINDOW_MOVE);
			fLink->Attach<int32_t>(fWindowToken);
			fLink->Attach<float>(fPopupPosition.x);
			fLink->Attach<float>(fPopupPosition.y);
			fLink->Flush();
		} else {
			if (fFrame.left != x || fFrame.top != y) {
				BEGIN_MESSAGE
				fLink->StartMessage(AS_WINDOW_MOVE);
				fLink->Attach<int32_t>(fWindowToken);
				fLink->Attach<float>(x);
				fLink->Attach<float>(y);
				fLink->Flush();

				// Keep frame rect in sync (though Wayland frames don't support movement)
				if (!isWayland)
					fFrame.OffsetTo(x, y);
			}
		}
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
		if (fWindowToken == B_NULL_TOKEN) {
			fFrame.right = fFrame.left + width;
			fFrame.bottom = fFrame.top + height;
			_AdoptResize();
			Unlock();
			return;
		}

		BEGIN_MESSAGE
		fLink->StartMessage(AS_WINDOW_RESIZE);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<float>(width);
		fLink->Attach<float>(height);

		float actualWidth = width;
		float actualHeight = height;
		status_t status;
		if (fLink->FlushWithReply(status) == B_OK && status == B_OK) {
			fLink->Read<float>(&actualWidth);
			fLink->Read<float>(&actualHeight);

			fFrame.right = fFrame.left + actualWidth;
			fFrame.bottom = fFrame.top + actualHeight;
			_AdoptResize();
		}
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
	BAutolock locker(this);

	// Set size limits now if needed
	UpdateSizeLimits();

	MoveTo(BLayoutUtils::AlignInFrame(rect, Size(),
		BAlignment(B_ALIGN_HORIZONTAL_CENTER,
			B_ALIGN_VERTICAL_CENTER)).LeftTop());
	MoveOnScreen(B_DO_NOT_RESIZE_TO_FIT | B_MOVE_IF_PARTIALLY_OFFSCREEN);
}


void
BWindow::CenterOnScreen()
{
	CenterIn(BScreen(this).Frame());
}


// Centers the window on the screen with the passed in id.
void
BWindow::CenterOnScreen(screen_id id)
{
	CenterIn(BScreen(id).Frame());
}


void
BWindow::MoveOnScreen(uint32 flags)
{
	// Set size limits now if needed
	UpdateSizeLimits();

	BRect screenFrame = BScreen(this).Frame();
	BRect frame = Frame();

	float borderWidth;
	float tabHeight;
	_GetDecoratorSize(&borderWidth, &tabHeight);

	frame.InsetBy(-borderWidth, -borderWidth);
	frame.top -= tabHeight;

	if ((flags & B_DO_NOT_RESIZE_TO_FIT) == 0) {
		// Make sure the window fits on the screen
		if (frame.Width() > screenFrame.Width())
			frame.right -= frame.Width() - screenFrame.Width();
		if (frame.Height() > screenFrame.Height())
			frame.bottom -= frame.Height() - screenFrame.Height();

		BRect innerFrame = frame;
		innerFrame.top += tabHeight;
		innerFrame.InsetBy(borderWidth, borderWidth);
		ResizeTo(innerFrame.Width(), innerFrame.Height());
	}

	if (((flags & B_MOVE_IF_PARTIALLY_OFFSCREEN) == 0
			&& !screenFrame.Contains(frame))
		|| !frame.Intersects(screenFrame)) {
		// Off and away
		const char* backend_name = cosmoe_backend_get_current_name();
		if (backend_name && strcmp(backend_name, "Wayland") != 0) {
			// Avoid infinite loop on Wayland where centering has no effect
			// and the window would stay offscreen
			CenterOnScreen();
		}
		
		return;
	}

	// Move such that the upper left corner, and most of the window
	// will be visible.
	float left = frame.left;
	if (left < screenFrame.left)
		left = screenFrame.left;
	else if (frame.right > screenFrame.right)
		left = std::max(0.f, screenFrame.right - frame.Width());

	float top = frame.top;
	if (top < screenFrame.top)
		top = screenFrame.top;
	else if (frame.bottom > screenFrame.bottom)
		top = std::max(0.f, screenFrame.bottom - frame.Height());

	if (top != frame.top || left != frame.left)
		MoveTo(left + borderWidth, top + tabHeight + borderWidth);
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
		fUpdateRequested = false;

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

static int32 _DisplayLoopWindow(void *data)
{
	STRACE("***_DisplayLoopWindow::_DisplayLoop START\n");
	cosmoe_display_t display = (cosmoe_display_t)data;
	cosmoe_display_run(display);
	STRACE("***_DisplayLoopWindow::_DisplayLoop ENDED\n");
	return 0;
}

thread_id
BWindow::Run()
{
	// Display thread is now started in _InitData() to support Windows port
	EnableUpdates();
	
	STRACE(("Window Frame: %f %f %f %f\n", fFrame.left, fFrame.top, fFrame.right, fFrame.bottom));
	STRACE(("Window width: %d\n", fFrame.IntegerWidth()));
	STRACE(("Window height: %d\n", fFrame.IntegerHeight()));

	STRACE(("BWindow::Run display running\n"));

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


int32
BWindow::DisplayScale() const
{
	return fDisplayScalePercent;
}


void
BWindow::SetDisplayScale(int32 scalePercent)
{
	if (scalePercent < 100 || scalePercent > 400
		|| scalePercent == fDisplayScalePercent)
		return;
		
	fDisplayScalePercent = scalePercent;
	
	// Recreate backing surface with new scale
	_CreateBackingSurface();
	
	// Tell backend about the new buffer scale (Wayland needs this)
	if (fWindowToken != B_NULL_TOKEN) {
		const char* backend_name = cosmoe_backend_get_current_name();
		if (backend_name && strcmp(backend_name, "Wayland") == 0) {
			cosmoe_window_set_buffer_scale(be_app->Display(), fWindowToken,
				_DisplayScaleBufferScale(scalePercent));
			
			// Resize window to accommodate scaled content
			// The frame size stays logical, but backend needs to allocate physical pixels
			if (!fOffscreen) {
				BEGIN_MESSAGE
				fLink->StartMessage(AS_WINDOW_RESIZE);
				fLink->Attach<int32_t>(fWindowToken);
				fLink->Attach<float>(fFrame.IntegerWidth());
				fLink->Attach<float>(fFrame.IntegerHeight());
				status_t status = B_ERROR;
				if (fLink->FlushWithReply(status) == B_OK && status == B_OK) {
					float ignoredWidth = 0.0f;
					float ignoredHeight = 0.0f;
					fLink->Read<float>(&ignoredWidth);
					fLink->Read<float>(&ignoredHeight);
				}
			}
		}
	}
	
	// Invalidate everything so it redraws at new scale
	if (fTopView)
		fTopView->Invalidate();
}


void
BWindow::_CreateBackingSurface()
{
	_BackingSurfaceWillChange();

	pthread_mutex_lock(&fBackingSurfaceLock);

	cairo_surface_t* oldSurface = fBackingSurface;
	bool oldSurfaceValid = fBackingSurfaceValid;
	fBackingSurface = NULL;
	
	// Create surface at physical resolution (logical size * scale)
	double scale = _DisplayScaleFactor(fDisplayScalePercent);
	int physicalWidth = (int)((fFrame.IntegerWidth() + 1) * scale);
	int physicalHeight = (int)((fFrame.IntegerHeight() + 1) * scale);
	
	fBackingSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
		physicalWidth, physicalHeight);
	
	// Initialize visible windows to the current panel background color so
	// resize-time preserved content stays in the active theme. Offscreen bitmap
	// windows need a transparent backing surface so drag images and other
	// alpha bitmaps don't pick up an opaque panel-colored background.
	cairo_t* cr = cairo_create(fBackingSurface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	if (fOffscreen) {
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	} else {
		rgb_color panelColor = ui_color(B_PANEL_BACKGROUND_COLOR);
		cairo_set_source_rgb(cr, panelColor.red / 255.0, panelColor.green / 255.0,
			panelColor.blue / 255.0);
	}
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	// Preserve previously-rendered content during resize so backend redraw handlers
	// can keep blitting valid pixels until the next _UPDATE_ cycle finishes.
	if (oldSurface != NULL && oldSurfaceValid) {
		cairo_set_source_surface(cr, oldSurface, 0, 0);
		cairo_paint(cr);
		fBackingSurfaceValid = true;
	} else {
		fBackingSurfaceValid = false;
	}

	fBackingSurfaceDirtyRegion.Set(fTopView != NULL ? fTopView->Bounds()
		: BRect(0, 0, fFrame.Width(), fFrame.Height()));

	cairo_destroy(cr);

	if (oldSurface != NULL)
		cairo_surface_destroy(oldSurface);
	
	// Don't set device scale - we'll manually scale the Cairo context when drawing
	
	pthread_mutex_unlock(&fBackingSurfaceLock);

	_BackingSurfaceDidChange();
}


void
BWindow::_BackingSurfaceWillChange()
{
}


void
BWindow::_BackingSurfaceDidChange()
{
}


void
BWindow::_RequestTrackingRedraw()
{
	if (fUpdatesDisabled || be_app == NULL || be_app->Display() == NULL
		|| fWindowToken == B_NULL_TOKEN) {
		if (fUpdatesDisabled)
			fUpdateRequested = true;
		return;
	}

	BEGIN_MESSAGE
	fLink->StartMessage(AS_FORCE_UPDATE);
	fLink->Attach<int32_t>(fWindowToken);
	fLink->Attach<BRect>(BRect(0, 0, -1, -1));
	fLink->Flush();
}


void
BWindow::_UpdateTrackingRectLocked()
{
	if (fPointerTrackingMode == TRACKING_RECT) {
		BPoint delta = fTrackingCurrentMouse - fTrackingStartMouse;
		if (fRectTrackingStyle == B_TRACK_RECT_CORNER) {
			fTrackingCurrentRect = fRectTrackingStartRect;
			fTrackingCurrentRect.right += delta.x;
			fTrackingCurrentRect.bottom += delta.y;
		} else {
			fTrackingCurrentRect = fRectTrackingStartRect.OffsetByCopy(delta);
		}
		return;
	}

	if (fPointerTrackingMode == TRACKING_DRAG) {
		BPoint topLeft = fTrackingCurrentMouse - fTrackingDragOffset;
		if (fTrackingDragUsesRect || fTrackingDragBitmap != NULL) {
			fTrackingCurrentRect.left = topLeft.x;
			fTrackingCurrentRect.top = topLeft.y;
			fTrackingCurrentRect.right = topLeft.x + fTrackingDragRect.Width();
			fTrackingCurrentRect.bottom = topLeft.y + fTrackingDragRect.Height();
		} else {
			fTrackingCurrentRect = BRect(topLeft, topLeft);
		}
		return;
	}

	fTrackingCurrentRect = BRect();
}


void
BWindow::_ClearTrackingStateLocked()
{
	delete fTrackingDragMessage;
	fTrackingDragMessage = NULL;

	delete fTrackingDragBitmap;
	fTrackingDragBitmap = NULL;

	fPointerTrackingMode = TRACKING_NONE;
	fRectTrackingStyle = B_TRACK_WHOLE_RECT;
	fRectTrackingStartRect = BRect();
	fTrackingStartMouse = BPoint();
	fTrackingCurrentMouse = BPoint();
	fTrackingCurrentRect = BRect();
	fTrackingDragOffset = BPoint();
	fTrackingDragMode = B_OP_COPY;
	fTrackingDragUsesRect = false;
	fTrackingDragRect = BRect();
}


void
BWindow::_StartRectTracking(BRect startRect, uint32 style, BPoint mouseWindow)
{
	pthread_mutex_lock(&fBackingSurfaceLock);
	_ClearTrackingStateLocked();
	fPointerTrackingMode = TRACKING_RECT;
	fRectTrackingStartRect = startRect;
	fRectTrackingStyle = style;
	fTrackingStartMouse = mouseWindow;
	fTrackingCurrentMouse = mouseWindow;
	_UpdateTrackingRectLocked();
	pthread_mutex_unlock(&fBackingSurfaceLock);

	_RequestTrackingRedraw();
}


void
BWindow::_EndRectTracking()
{
	bool ended = false;
	pthread_mutex_lock(&fBackingSurfaceLock);
	if (fPointerTrackingMode == TRACKING_RECT) {
		_ClearTrackingStateLocked();
		ended = true;
	}
	pthread_mutex_unlock(&fBackingSurfaceLock);

	if (ended)
		_RequestTrackingRedraw();
}


void
BWindow::_StartMessageDrag(BMessage* message, BBitmap* image,
	drawing_mode dragMode, BPoint offset, BPoint mouseWindow, BRect dragRect)
{
	if (message == NULL) {
		delete image;
		return;
	}

	BMessage* messageCopy = new(std::nothrow) BMessage(*message);
	if (messageCopy == NULL) {
		delete image;
		return;
	}

	pthread_mutex_lock(&fBackingSurfaceLock);
	_ClearTrackingStateLocked();

	fPointerTrackingMode = TRACKING_DRAG;
	fTrackingDragMessage = messageCopy;
	fTrackingDragBitmap = image;
	fTrackingDragMode = dragMode;
	fTrackingDragOffset = offset;
	fTrackingStartMouse = mouseWindow;
	fTrackingCurrentMouse = mouseWindow;

	if (dragRect.IsValid()) {
		fTrackingDragUsesRect = true;
		fTrackingDragRect = BRect(0, 0, dragRect.Width(), dragRect.Height());
	} else if (image != NULL) {
		BRect bounds = image->Bounds();
		fTrackingDragRect = BRect(0, 0, bounds.Width(), bounds.Height());
	} else {
		fTrackingDragRect = BRect(0, 0, 0, 0);
	}

	_UpdateTrackingRectLocked();
	pthread_mutex_unlock(&fBackingSurfaceLock);

	_RequestTrackingRedraw();
}


void
BWindow::_UpdatePointerTracking(BPoint mouseWindow)
{
	bool changed = false;
	pthread_mutex_lock(&fBackingSurfaceLock);
	if (fPointerTrackingMode != TRACKING_NONE
		&& fTrackingCurrentMouse != mouseWindow) {
		fTrackingCurrentMouse = mouseWindow;
		_UpdateTrackingRectLocked();
		changed = true;
	}
	pthread_mutex_unlock(&fBackingSurfaceLock);

	if (changed)
		_RequestTrackingRedraw();
}


void
BWindow::_StopPointerTracking()
{
	bool changed = false;
	pthread_mutex_lock(&fBackingSurfaceLock);
	if (fPointerTrackingMode != TRACKING_NONE) {
		_ClearTrackingStateLocked();
		changed = true;
	}
	pthread_mutex_unlock(&fBackingSurfaceLock);

	if (changed)
		_RequestTrackingRedraw();
}


bool
BWindow::_IsDragTrackingActive() const
{
	BWindow* window = const_cast<BWindow*>(this);
	pthread_mutex_lock(&window->fBackingSurfaceLock);
	bool active = window->fPointerTrackingMode == TRACKING_DRAG
		&& window->fTrackingDragMessage != NULL;
	pthread_mutex_unlock(&window->fBackingSurfaceLock);
	return active;
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
		debugger("FATAL: You need a valid BApplication object to create a BWindow");
		return;
	}

	// Ensure display thread is running BEFORE creating any windows
	// Windows MUST be created on the display thread for proper message routing
	if (sDisplayThread < 0) {
		STRACE(("BWindow::_InitData: Display thread not yet started, starting it NOW...\n"));
		sDisplayThread = spawn_thread(&_DisplayLoopWindow, "Cosmoe Display Loop",
			B_NORMAL_PRIORITY, be_app->Display());
		if (sDisplayThread >= 0) {
			resume_thread(sDisplayThread);
			// Wait for the thread to actually start and set thread_id
			snooze(150000); // 150ms - ensure display->thread_id is set
			STRACE(("BWindow::_InitData: Display thread started on tid %d\n", (int)sDisplayThread));
		} else {
			STRACE(("BWindow::_InitData: FATAL - Failed to start display thread!\n"));
		}
	}

	frame.left = roundf(frame.left);
	frame.top = roundf(frame.top);
	frame.right = roundf(frame.right);
	frame.bottom = roundf(frame.bottom);

	// I've seen code in the wild that creates windows with inverted coordinates, and Haiku seems to 
	// handle it just fine, so we'll be forgiving and swap them back if needed instead of choking one
	// of our backends that might not be so forgiving.
	if (frame.left > frame.right) {
		float temp = frame.left;
		frame.left = frame.right;
		frame.right = temp;
	}

	if (frame.top > frame.bottom) {
		float temp = frame.top;
		frame.top = frame.bottom;
		frame.bottom = temp;
	}

	fFrame = frame;

	if (title == NULL)
		title = "";

	fTitle = strdup(title);

	_SetName(title);

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
	fMouseDownViewToken = B_NULL_TOKEN;
	fKeyMenuBar = NULL;
	fDefaultButton = NULL;
	fShortcuts = NULL;

	// Shortcut 'Q' is handled in _HandleKeyDown() directly, as its message
	// get sent to the application, and not one of our handlers.
	// It is only installed for non-modal windows, though.
	fNoQuitShortcut = IsModal();

	if ((fFlags & B_NOT_CLOSABLE) == 0 && !IsModal()) {
		// Modal windows default to non-closable, but you can add the
		// shortcut manually, if a different behaviour is wanted
		AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
	}

	// Edit modifier keys

	AddShortcut('X', B_COMMAND_KEY, new BMessage(B_CUT), NULL);
	AddShortcut('C', B_COMMAND_KEY, new BMessage(B_COPY), NULL);
	AddShortcut('V', B_COMMAND_KEY, new BMessage(B_PASTE), NULL);
	AddShortcut('A', B_COMMAND_KEY, new BMessage(B_SELECT_ALL), NULL);

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

	fParentWindow = NULL;  // Will be set via _SetParentWindow() for popup/tooltip windows
	fPopupPosition.Set(0, 0);  // Initialize popup position to origin
	fHadShow = false;

	// Initialize backend window variables
	fWindowToken = B_NULL_TOKEN;

	fOffscreen = (bitmapToken >= 0);

	pthread_mutexattr_t backingSurfaceLockAttributes;
	pthread_mutexattr_init(&backingSurfaceLockAttributes);
	pthread_mutexattr_settype(&backingSurfaceLockAttributes, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&fBackingSurfaceLock, &backingSurfaceLockAttributes);
	pthread_mutexattr_destroy(&backingSurfaceLockAttributes);

	// Initialize display scale
	fDisplayScalePercent = 100;  // Will be updated after window creation

	_CreateBackingSurface();

	_SetName(title);

	// Create the top view now so fTopView is valid when backend window creation
	// stores it as the topview widget user_data.
	_CreateTopView();

	// Avoid a round-trip into the backend for offscreen windows, since they don't
	// need a native backend window.  This also avoids deadlocks if the offscreen
	// window is created from the display thread.
	if (fOffscreen)
		return;

	if (fFeel == kMenuWindowFeel) {
		const char* backend_name = cosmoe_backend_get_current_name();
		const bool isWayland = backend_name && strcmp(backend_name, "Wayland") == 0;

		// For all popup windows, defer creation until position is set
		// This is because BMenu always calls Show() before MoveTo()
		// For Wayland: required because xdg_popup needs position at creation
		// For X11: ensures we don't create at (0,0) then reposition
		if (fPopupPosition.x == 0 && fPopupPosition.y == 0) {
			STRACE(("%s popup: deferring backend creation until position is set\n", 
					backend_name ? backend_name : "Unknown"));
			// Don't create backend window yet - wait for MoveTo() to be called
			// MoveTo() will trigger creation once position is known
			return;
		}
		
		// Detect scale before creating popup so position can be scaled
		int32 scalePercent = BDisplayScaleManager::GetScaleForWindow(this);
		double scale = _DisplayScaleFactor(scalePercent);
		
		// Use fPopupPosition instead of Frame() since fFrame is inaccurate on Wayland.
		// Wayland xdg_positioner uses logical parent-local coordinates; other
		// backends expect the popup position scaled to backend pixels here.
		int32_t popupX = (int32_t)(isWayland ? fPopupPosition.x
			: fPopupPosition.x * scale);
		int32_t popupY = (int32_t)(isWayland ? fPopupPosition.y
			: fPopupPosition.y * scale);
		
		// Get parent window's backend token if available
		int32_t parentToken = B_NULL_TOKEN;
		if (fParentWindow && fParentWindow->fWindowToken != B_NULL_TOKEN) {
			parentToken = fParentWindow->fWindowToken;
		}
		
		fWindowToken = _get_object_token_(this);
		void* callbackData = (void*)(intptr_t)_get_object_token_(this);
		BEGIN_MESSAGE
		fLink->StartMessage(AS_CREATE_POPUP_WINDOW);
		fLink->Attach<void*>(be_app->Display());
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<int32_t>(parentToken);
		fLink->Attach<int32_t>(popupX);
		fLink->Attach<int32_t>(popupY);
		fLink->Attach<void*>(callbackData);
		const char* appSig = be_app->Signature();
		fLink->AttachString(appSig ? appSig : "");
		fLink->Attach<void*>(fTopView);
		fLink->Attach<int32_t>(fFrame.IntegerWidth());
		fLink->Attach<int32_t>(fFrame.IntegerHeight());
		fLink->Flush();
		STRACE(("Created popup backend window token=%d at %d,%d (scale %d%%) with parent token=%d\n",
			(int)fWindowToken, popupX, popupY, scalePercent, (int)parentToken));
	} else  {
		// B_NOT_RESIZABLE only disables interactive resizing. Keep the
		// programmatic size limits unchanged unless explicit directional
		// limits were requested.
		if ((fFlags & B_NOT_H_RESIZABLE) && (fFlags & B_NOT_V_RESIZABLE)) {
			fMinWidth  = fMaxWidth  = (float)fFrame.IntegerWidth();
			fMinHeight = fMaxHeight = (float)fFrame.IntegerHeight();
		} else if (fFlags & B_NOT_H_RESIZABLE) {
			fMinWidth  = fMaxWidth  = (float)fFrame.IntegerWidth();
		} else if (fFlags & B_NOT_V_RESIZABLE) {
			fMinHeight = fMaxHeight = (float)fFrame.IntegerHeight();
		}
		fMaxZoomWidth  = fMaxWidth;
		fMaxZoomHeight = fMaxHeight;

		// Determine parent token for modal windows
		int32_t parentToken = B_NULL_TOKEN;
		if (Feel() == B_MODAL_APP_WINDOW_FEEL && fParentWindow
		    && fParentWindow->fWindowToken != B_NULL_TOKEN) {
			parentToken = fParentWindow->fWindowToken;
		}

		fWindowToken = _get_object_token_(this);
		void* callbackData = (void*)(intptr_t)_get_object_token_(this);
		BEGIN_MESSAGE
		fLink->StartMessage(AS_CREATE_WINDOW);
		fLink->Attach<BRect>(fFrame);
		fLink->Attach<uint32>((uint32)fLook);
		fLink->Attach<uint32>((uint32)fFeel);
		fLink->Attach<void*>(be_app->Display());
		fLink->Attach<bool>(fOffscreen);
		fLink->Attach<uint32>(fFlags);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<void*>(callbackData);
		fLink->Attach<int32_t>(parentToken);
		fLink->AttachString(title);
		const char* appSig = be_app->Signature();
		fLink->AttachString(appSig ? appSig : "");
		fLink->Attach<void*>(fTopView);
		status_t status = B_ERROR;
		if (fLink->FlushWithReply(status) == B_OK && status == B_OK) {
			BRect backendFrame;
			if (fLink->Read<BRect>(&backendFrame) == B_OK)
				fFrame = backendFrame;
		}
	}
	// For normal windows, the backend widget is now created from AS_CREATE_WINDOW
	// (fTopView is already valid here). AS_WINDOW_SHOW only wires handlers and
	// maps/shows the window.


	STRACE(("Window locked?: %s\n", IsLocked() ? "True" : "False"));
}


//! Rename the handler and its thread
void
BWindow::_SetName(const char* title)
{
	if (title == NULL)
		title = "";

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
			
			// SAFETY: Check if message pointer looks valid
			if (message != NULL && ((uintptr_t)message < 0x1000)) {
				STRACE(("[BUG] NextMessage() returned likely corrupt pointer: %p\n", message));
				message = NULL;
			}

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
				// SAFETY CHECK: Verify fLastMessage is still valid
				if (fLastMessage == NULL) {
					STRACE(("[BUG] fLastMessage became NULL after check! Thread race!\n"));
					dispatchNextMessage = false;
					Unlock();
					continue;
				}
				
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
	if (fTopView == NULL) {
		STRACE(("BUG: called _AdoptResize() with a NULL fTopView\n"));
		return;
	}

	// Recreate backing surface at new size with current scale
	_CreateBackingSurface();

	int32 deltaWidth = (int32)(fFrame.Width() - fTopView->Bounds().Width());
	int32 deltaHeight = (int32)(fFrame.Height() - fTopView->Bounds().Height());
	if (deltaWidth == 0 && deltaHeight == 0)
		return;

	fTopView->_ResizeBy(deltaWidth, deltaHeight);
	fTopView->_UpdateViewClippingRegion(true);

	// When the window grows, explicitly invalidate so newly exposed content
	// is repainted on the next update pass.
	if (deltaWidth > 0 || deltaHeight > 0)
		fTopView->Invalidate(fTopView->Bounds());
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
			int32 buttons = 0;
			if (message->FindInt32("buttons", &buttons) == B_OK) {
				BView::sLastButtonState[B_PRIMARY_MOUSE_BUTTON]
					= (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
				BView::sLastButtonState[B_SECONDARY_MOUSE_BUTTON]
					= (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
				BView::sLastButtonState[B_TERTIARY_MOUSE_BUTTON]
					= (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
			}

			BPoint where;
			if (message->FindPoint("window_where", &where) != B_OK)
				break;

			BView* view = dynamic_cast<BView*>(target);

			if (view == NULL || message->what == B_MOUSE_MOVED) {
				// add local window coordinates, only
				// for regular mouse moved messages
				message->AddPoint("where", where);
			}

			if (view != NULL) {
				// add local view coordinates
				BPoint viewWhere = view->ConvertFromWindow(where);
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
			// Graphics backends send window coordinates, so convert the point to
			// local view coordinates, then add the point in be:view_where
			BPoint where;
			if (message->FindPoint("window_where", &where) != B_OK)
				break;

			BView* view = dynamic_cast<BView*>(target);
			if (view != NULL) {
				// add local view coordinates
				message->AddPoint("be:view_where",
					view->ConvertFromWindow(where));
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

	unsigned char byte = (unsigned char)bytes[0];
	char key = Shortcut::PrepareKey(byte);

	uint32 rawKey;
	if (event->FindInt32("key", (int32*)&rawKey) != B_OK)
		rawKey = 0;

	uint32 modifiers;
	if (event->FindInt32("modifiers", (int32*)&modifiers) != B_OK)
		modifiers = 0;

	#ifdef __linux__
	if (rawKey != 0
		&& (modifiers & (B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY)) != 0) {
		key = linux_shortcut_key_for_event(rawKey, modifiers, key);
	} else if ((modifiers & B_COMMAND_KEY) != 0 && byte >= 1 && byte <= 26)
		key = Shortcut::PrepareKey(byte + 'a' - 1);
	#else
	if ((modifiers & B_COMMAND_KEY) != 0 && byte >= 1 && byte <= 26)
		key = Shortcut::PrepareKey(byte + 'a' - 1);
	#endif

	// handle BMenuBar key
	if (key == B_ESCAPE && (modifiers & B_COMMAND_KEY) != 0 && fKeyMenuBar != NULL) {
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

	// Optionally close window when the escape key is pressed
	if (key == B_ESCAPE && (Flags() & B_CLOSE_ON_ESCAPE) != 0) {
		BMessage message(B_QUIT_REQUESTED);
		message.AddBool("shortcut", true);
		PostMessage(&message);
		return true;
	}

	// Special handling for Command+q, Command+Left, Command+Right
	if ((modifiers & B_COMMAND_KEY) != 0) {
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
		// Only do this if Command key is down, it's too expensive to
		// do this on every key press.
		if ((modifiers & B_COMMAND_KEY) != 0)
			MenusBeginning();

		Shortcut* shortcut = _FindShortcut(key, modifiers
			| (((modifiers & B_COMMAND_KEY) == 0) ? B_NO_COMMAND_KEY : 0));
		if (shortcut != NULL) {
			// TODO: would be nice to move this functionality to
			//	a Shortcut::Invoke() method - but since BMenu::InvokeItem()
			//	(and BMenuItem::Invoke()) are private, I didn't want
			//	to mess with them (BMenuItem::Invoke() is public in
			//	Dano/Zeta, though, maybe we should just follow their
			//	example)
			if (shortcut->MenuItem() != NULL) {
				BMenu* menu = shortcut->MenuItem()->Menu();
				if (menu != NULL && shortcut->MenuItem()->IsEnabled()) {
					MenuPrivate(menu).InvokeItem(shortcut->MenuItem(), true);
				} else {
					// Process disabled shortcuts as if they did not exist.
					// (This lets B_NO_COMMAND_KEY shortcuts fall back to regular key events.)
					shortcut = NULL;
				}
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

		if ((modifiers & B_COMMAND_KEY) != 0)
			MenusEnded();

		if (shortcut != NULL)
			return true;
	}

	if ((modifiers & B_COMMAND_KEY) != 0) {
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


/*!
	\brief Return the position of the window centered horizontally to the passed
           in \a frame and vertically 3/4 from the top of \a frame.

	If the window is on the borders

	\param width The width of the window.
	\param height The height of the window.
	\param frame The \a frame to center the window in.

	\return The new window position.
*/
BPoint
BWindow::AlertPosition(const BRect& frame)
{
	float width = Bounds().Width();
	float height = Bounds().Height();

	BPoint point(frame.left + (frame.Width() / 2.0f) - (width / 2.0f),
		frame.top + (frame.Height() / 4.0f) - ceil(height / 3.0f));

	BRect screenFrame = BScreen(this).Frame();
	if (frame == screenFrame) {
		// reference frame is screen frame, skip the below adjustments
		return point;
	}

	float borderWidth;
	float tabHeight;
	_GetDecoratorSize(&borderWidth, &tabHeight);

	// clip the x position within the horizontal edges of the screen
	if (point.x < screenFrame.left + borderWidth)
		point.x = screenFrame.left + borderWidth;
	else if (point.x + width > screenFrame.right - borderWidth)
		point.x = screenFrame.right - borderWidth - width;

	// lower the window down if it is covering the window tab
	float tabPosition = frame.LeftTop().y + tabHeight + borderWidth;
	if (point.y < tabPosition)
		point.y = tabPosition;

	// clip the y position within the vertical edges of the screen
	if (point.y < screenFrame.top + borderWidth)
		point.y = screenFrame.top + borderWidth;
	else if (point.y + height > screenFrame.bottom - borderWidth)
		point.y = screenFrame.bottom - borderWidth - height;

	return point;
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

	Shortcut::SplayTree* shortcuts = Shortcut::CastToTree(&fShortcuts);
	Shortcut* shortcut = shortcuts->FindClosest(Shortcut::TreeKey(key, preparedModifiers),
		false, true);

	if (shortcut == NULL)
		return NULL;
	if (shortcut->Compare(key, preparedModifiers) == 0)
		return shortcut;
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
	if (view == NULL)
		return NULL;

	// point is assumed to be already in view's coordinates
	if (!view->IsHidden(view) && view->Bounds().Contains(point)) {
		if (view->fFirstChild == NULL)
			return view;
		else {
			BView* child = view->fFirstChild;
			while (child != NULL) {
				// Convert point from parent coordinates to child coordinates
				// This accounts for both frame position and scroll offset
				BPoint childPoint = point;
				childPoint.x += -child->fParentOffset.x + child->fBounds.left;
				childPoint.y += -child->fParentOffset.y + child->fBounds.top;
				
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
BWindow::_GetDecoratorSize(float* _borderWidth, float* _tabHeight) const
{
	// fallback in case retrieving the decorator settings fails
	// (highly unlikely)
	float borderWidth = 5.0;
	float tabHeight = 21.0;

	BMessage settings;
	if (GetDecoratorSettings(&settings) == B_OK) {
		BRect tabRect;
		if (settings.FindRect("tab frame", &tabRect) == B_OK)
			tabHeight = tabRect.Height();
		settings.FindFloat("border width", &borderWidth);
	} else {
		// probably no-border window look
		if (fLook == B_NO_BORDER_WINDOW_LOOK) {
			borderWidth = 0.0;
			tabHeight = 0.0;
		}
		// else use fall-back values from above
	}

	if (_borderWidth != NULL)
		*_borderWidth = borderWidth;
	if (_tabHeight != NULL)
		*_tabHeight = tabHeight;
}


void
BWindow::_SendShowOrHideMessage()
{
	if (IsHidden() && fWindowToken != B_NULL_TOKEN) {
		// Hide the backend window while keeping backend-owned window/widget state.

		DisableUpdates();

		// Send one-way hide message through the BMP queue.
		BEGIN_MESSAGE
		fLink->StartMessage(AS_WINDOW_HIDE);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Flush();

		STRACE(("Backend window hidden for '%s'\n", Name()));

	} else if (!IsHidden()) {
		// Show (or create-then-show for deferred popups).

		if (fWindowToken == B_NULL_TOKEN) {
			if (fFeel == kMenuWindowFeel && fParentWindow) {
				// Popup: backend will be created by MoveTo() once position is known.
				// Record that a show was requested so MoveTo() knows to complete it.
				STRACE(("_SendShowOrHideMessage: deferring popup creation for '%s' until MoveTo() sets position\n", Name()));
				fHadShow = true;
				return;
			} else {
				return; // No backend, not a popup — shouldn't happen
			}
		}

		// Detect and apply display scale before sending the show message.
		int32 detectedScale = BDisplayScaleManager::GetScaleForWindow(this);
		if (detectedScale != fDisplayScalePercent)
			SetDisplayScale(detectedScale);

		// Send AS_WINDOW_SHOW one-way with all handler pointers. The BMP will:
		//   - set all widget and window handlers
		//   - schedule resize, show the window
		void* frameResizeFn = (!fOffscreen && fFeel != kMenuWindowFeel)
			? (void*)windowframe_resize_handler : nullptr;

		{
			void* callbackData = (void*)(intptr_t)_get_object_token_(this);
			BEGIN_MESSAGE
			fLink->StartMessage(AS_WINDOW_SHOW);
			fLink->Attach<int32_t>(fWindowToken);
			fLink->Attach<void*>(fTopView);
			fLink->Attach<void*>(callbackData);
			fLink->Attach<void*>((void*)view_redraw_handler);
			fLink->Attach<void*>((void*)view_pointer_motion_handler);
			fLink->Attach<void*>((void*)view_button_handler);
			fLink->Attach<void*>((void*)view_axis_handler);
			fLink->Attach<void*>((void*)view_mouse_idle_handler);
			fLink->Attach<void*>(frameResizeFn);
			fLink->Attach<void*>((void*)close_handler);
			fLink->Attach<void*>((void*)key_handler);
			fLink->Attach<void*>((void*)screen_handler);
			fLink->Attach<void*>((void*)window_move_handler);
			fLink->Attach<void*>((void*)window_focus_handler);
			fLink->Attach<int32_t>(fFrame.IntegerWidth());
			fLink->Attach<int32_t>(fFrame.IntegerHeight());
			fLink->Attach<bool>(fOffscreen);
			fLink->Flush();  // One-way — sLock released when this block exits
		}

		EnableUpdates();

		if (fTopView)
			fTopView->Invalidate();
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

void
BWindow::_SetParentWindow(BWindow* parent)
{
	fParentWindow = parent;
}


void BWindow::_UpdateFrame()
{
	if (fWindowToken == B_NULL_TOKEN)
		return;

	int32_t x = 0;
	int32_t y = 0;
	BEGIN_MESSAGE
	fLink->StartMessage(AS_GET_POSITION);
	fLink->Attach<int32_t>(fWindowToken);
	status_t status = B_ERROR;
	if (fLink->FlushWithReply(status) == B_OK && status == B_OK) {
		fLink->Read<int32_t>(&x);
		fLink->Read<int32_t>(&y);
		fFrame.OffsetTo(BPoint((float)x, (float)y));
	}
}



// Cosmoe: fix sticky mode handling for appserver-less case
// Menu tracking support - detect clicks on non-menu windows
uint32
BWindow::GetNonMenuClickSequence()
{
	return sNonMenuClickSequence;
}


void BWindow::_ReservedWindow2() {}
void BWindow::_ReservedWindow3() {}
void BWindow::_ReservedWindow4() {}
void BWindow::_ReservedWindow5() {}
void BWindow::_ReservedWindow6() {}
void BWindow::_ReservedWindow7() {}
void BWindow::_ReservedWindow8() {}

