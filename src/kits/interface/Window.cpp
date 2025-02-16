/*
 * Copyright 2001-2016 Haiku, Inc. All rights reserved
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
#include <Button.h>
#include <Layout.h>
#include <LayoutUtils.h>
#include <MenuBar.h>
#include <WindowPrivate.h>

#define DEBUG_WIN
#ifdef DEBUG_WIN
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

#define WAYLAND_WINDOW_H_SLOP 76
#define WAYLAND_WINDOW_V_SLOP 97

// static void
// windowframe_redraw_handler(struct widget *widget, void *data)
// {
//     printf("windowframe_redraw_handler\n");
// 	//struct rectangle allocation;
// 	// cairo_t *cr;

// 	// widget_get_allocation(widget, &allocation);

// 	// cr = widget_cairo_create(widget);
// 	// cairo_rectangle(cr, allocation.x, allocation.y,
// 	// 		allocation.width, allocation.height);
// 	// cairo_set_source_rgba(cr, 0, 0.8, 0, 0.8);
// 	// cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
// 	// cairo_fill(cr);
// 	// cairo_destroy(cr);
// }


void
windowframe_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
    printf("windowframe_resize_handler w: %d h: %d\n", width, height);

    rectangle allocation;

    // Getting the allocation for the window frame allows us to
    // find the "origin" for the top view
    widget_get_allocation(widget, &allocation);

    BWindow* win = (BWindow*)data;

    if (win->fTopView != NULL && win->fTopView->view_widget != NULL) {
        // By syncing fFrame with the Wayland topview allocation, 
        // we keep the Wayland and BeOS world in harmony
        widget_set_allocation(win->fTopView->view_widget, allocation.x, allocation.y, width, height);
        win->fFrame.Set(allocation.x, allocation.y, allocation.x + width, allocation.y + height);
        win->_AdoptResize();
    }
}

static void
close_handler(void *data)
{
    printf("close_handler\n");
    BWindow* win = (BWindow*)data;
    win->Quit();
}

void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
    printf("key_handler\n");
}

BWindow::BWindow(BRect frame, const char* title, window_type type,
		uint32 flags, uint32 workspace)
	:
	BLooper(title, B_DISPLAY_PRIORITY)
{
	window_look look;
	window_feel feel;
	_DecomposeType(type, &look, &feel);

	_InitData(frame, title, look, feel, workspace, 0);
}


BWindow::BWindow(BRect frame, const char* title, window_look look,
		window_feel feel, uint32 flags, uint32 workspace)
	:
	BLooper(title, B_DISPLAY_PRIORITY)
{
	_InitData(frame, title, look, feel, flags, workspace, 0);
}

BWindow::~BWindow()
{
	fTopView->RemoveSelf();
	delete fTopView;

	// TODO: release other dynamically-allocated objects
	free(fTitle);

	// disable pulsing
	SetPulseRate(0);
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
}


BView*
BWindow::FindView(const char* viewName) const
{
	//BAutolock locker(const_cast<BWindow*>(this));
	//if (!locker.IsLocked())
	//	return NULL;

	return fTopView->FindView(viewName);
}


BView*
BWindow::FindView(BPoint point) const
{
	//BAutolock locker(const_cast<BWindow*>(this));
	//if (!locker.IsLocked())
	//	return NULL;

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
	rect;
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
	// TODO

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
	//BAutolock locker(this);
	//if (!locker.IsLocked())
	//	return B_BAD_VALUE;

	fFlags = flags;

	return B_OK;
}


uint32
BWindow::Flags() const
{
	return fFlags;
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
	fUpdateRequested = false;
	fActive = false;
	fShowLevel = 1;

	fTopView = NULL;
	fFocus = NULL;
	fLastMouseMovedView	= NULL;
	fDefaultButton = NULL;

    // Weston Start
    bool firstWindow = false;

    if (d == NULL) {
        d = display_create(NULL, NULL);
        firstWindow = true;
        printf("BWindow::BWindow display created (%p)\n", d);
    }

    printf("BWindow::BWindow 1\n");
	window = window_create(d);
	window_set_appid(window, "org.haydentech.cow");
	window_set_user_data(window, this);

    windowframe_widget = window_frame_create(window, this);
 	//widget_set_redraw_handler(windowframe_widget, windowframe_redraw_handler);


    // Weston End

	_SetName(title);

	// fKeyMenuBar = NULL;
	// fDefaultButton = NULL;

	// // Shortcut 'Q' is handled in _HandleKeyDown() directly, as its message
	// // get sent to the application, and not one of our handlers.
	// // It is only installed for non-modal windows, though.
	// fNoQuitShortcut = IsModal();

	// if ((fFlags & B_NOT_CLOSABLE) == 0 && !IsModal()) {
	// 	// Modal windows default to non-closable, but you can add the
	// 	// shortcut manually, if a different behaviour is wanted
	// 	AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
	// }

	// Edit modifier keys

	// AddShortcut('X', B_COMMAND_KEY, new BMessage(B_CUT), NULL);
	// AddShortcut('C', B_COMMAND_KEY, new BMessage(B_COPY), NULL);
	// AddShortcut('V', B_COMMAND_KEY, new BMessage(B_PASTE), NULL);
	// AddShortcut('A', B_COMMAND_KEY, new BMessage(B_SELECT_ALL), NULL);

	// // Window modifier keys

	// AddShortcut('M', B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(_MINIMIZE_), NULL);
	// AddShortcut('Z', B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(_ZOOM_), NULL);
	// AddShortcut('Z', B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(_ZOOM_), NULL);
	// AddShortcut('H', B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(B_HIDE_APPLICATION), NULL);
	// AddShortcut('F', B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(_SEND_TO_FRONT_), NULL);
	// AddShortcut('B', B_COMMAND_KEY | B_CONTROL_KEY,
	// 	new BMessage(_SEND_BEHIND_), NULL);

	// We set the default pulse rate, but we don't start the pulse
	fPulseRate = 500000;
	//fPulseRunner = NULL;

	fIsFilePanel = false;

	fMenuSem = -1;

	fMinimized = false;

	fMaxZoomHeight = 32768.0;
	fMaxZoomWidth = 32768.0;
	fMinHeight = 0.0;
	fMinWidth = 0.0;
	fMaxHeight = 32768.0;
	fMaxWidth = 32768.0;

	//fLastViewToken = B_NULL_TOKEN;

	// TODO: other initializations!
	fOffscreen = false;

	// Create the server-side window

	// port_id receivePort = create_port(B_LOOPER_PORT_DEFAULT_CAPACITY,
	// 	"w<app_server");
	// if (receivePort < B_OK) {
	// 	// TODO: huh?
	// 	debugger("Could not create BWindow's receive port, used for "
	// 			 "interacting with the app_server!");
	// 	delete this;
	// 	return;
	// }

	// STRACE(("BWindow::InitData(): contacting app_server...\n"));

	// // let app_server know that a window has been created.
	// fLink = new(std::nothrow) BPrivate::PortLink(
	// 	BApplication::Private::ServerLink()->SenderPort(), receivePort);
	// if (fLink == NULL) {
	// 	// Zombie!
	// 	return;
	// }

	// {
	// 	BPrivate::AppServerLink lockLink;
	// 		// we're talking to the server application using our own
	// 		// communication channel (fLink) - we better make sure no one
	// 		// interferes by locking that channel (which AppServerLink does
	// 		// implicetly)

	// 	if (bitmapToken < 0) {
	// 		fLink->StartMessage(AS_CREATE_WINDOW);
	// 	} else {
	// 		fLink->StartMessage(AS_CREATE_OFFSCREEN_WINDOW);
	// 		fLink->Attach<int32>(bitmapToken);
	// 		fOffscreen = true;
	// 	}

	// 	fLink->Attach<BRect>(fFrame);
	// 	fLink->Attach<uint32>((uint32)fLook);
	// 	fLink->Attach<uint32>((uint32)fFeel);
	// 	fLink->Attach<uint32>(fFlags);
	// 	fLink->Attach<uint32>(workspace);
	// 	fLink->Attach<int32>(_get_object_token_(this));
	// 	fLink->Attach<port_id>(receivePort);
	// 	fLink->Attach<port_id>(fMsgPort);
	// 	fLink->AttachString(title);

	// 	port_id sendPort;
	// 	int32 code;
	// 	if (fLink->FlushWithReply(code) == B_OK
	// 		&& code == B_OK
	// 		&& fLink->Read<port_id>(&sendPort) == B_OK) {
	// 		// read the frame size and its limits that were really
	// 		// enforced on the server side

	// 		fLink->Read<BRect>(&fFrame);
	// 		fLink->Read<float>(&fMinWidth);
	// 		fLink->Read<float>(&fMaxWidth);
	// 		fLink->Read<float>(&fMinHeight);
	// 		fLink->Read<float>(&fMaxHeight);

	// 		fMaxZoomWidth = fMaxWidth;
	// 		fMaxZoomHeight = fMaxHeight;
	// 	} else
	// 		sendPort = -1;

	// 	// Redirect our link to the new window connection
	// 	fLink->SetSenderPort(sendPort);
	// 	STRACE(("Server says that our send port is %ld\n", sendPort));
	// }

	STRACE(("Window locked?: %s\n", IsLocked() ? "True" : "False"));

	_CreateTopView();
}


//! Rename the handler and its thread
void
BWindow::_SetName(const char* title)
{
	if (title == NULL)
		title = "";

    window_set_title(window, title);
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
	//fLastViewToken = _get_object_token_(fTopView);

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

	fprintf(stderr, "_AdoptResize(): dw = %d, dh = %d\n", deltaWidth, deltaHeight);

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
		// bool inputMethodAware = false;
		// if (focusView)
		// 	inputMethodAware = focusView->Flags() & B_INPUT_METHOD_AWARE;
		// BMessage msg(inputMethodAware ? IS_FOCUS_IM_AWARE_VIEW : IS_UNFOCUS_IM_AWARE_VIEW);
		// BMessenger messenger(focusView);
		// BMessage reply;
		// if (focusView)
		// 	msg.AddMessenger("view", messenger);
		// _control_input_server_(&msg, &reply);
	}

	fFocus = focusView;
	SetPreferredHandler(focusView);
}


void BWindow::Quit()
{
    widget_destroy(windowframe_widget);
	window_destroy(window);

    display_destroy(d);
	display_exit(d);
}

void
BWindow::AddChild(BView* child, BView* before)
{
	//BAutolock locker(this);
	// if (locker.IsLocked())
		fTopView->AddChild(child, before);
}


bool
BWindow::RemoveChild(BView* child)
{
	//BAutolock locker(this);
	// if (!locker.IsLocked())
	// 	return false;

	return fTopView->RemoveChild(child);
}


int32
BWindow::CountChildren() const
{
	//BAutolock locker(const_cast<BWindow*>(this));
	// if (!locker.IsLocked())
	// 	return 0;

	return fTopView->CountChildren();
}


BView*
BWindow::ChildAt(int32 index) const
{
	// BAutolock locker(const_cast<BWindow*>(this));
	// if (!locker.IsLocked())
	// 	return NULL;

	return fTopView->ChildAt(index);
}


thread_id BWindow::Run()
{
    printf("BWindow::Run\n");
    printf("display (%p)\n", d);

    if (d) {
        widget_set_resize_handler(windowframe_widget, windowframe_resize_handler);

        // window_set_keyboard_focus_handler(window,
        // 				  keyboard_focus_handler);
        // window_set_fullscreen_handler(window, fullscreen_handler);
        window_set_close_handler(window, close_handler);
        window_set_key_handler(window, key_handler);
        printf("Window Frame: %f %f %f %f\n", fFrame.left, fFrame.top, fFrame.right, fFrame.bottom);
        printf("Window width: %d\n", fFrame.IntegerWidth());
        printf("Window height: %d\n", fFrame.IntegerHeight());

        widget_schedule_resize(windowframe_widget, fFrame.IntegerWidth() + WAYLAND_WINDOW_H_SLOP,
                fFrame.IntegerHeight() + WAYLAND_WINDOW_V_SLOP);
        display_run(d);
        printf("BWindow::Run display running\n");

    }

    printf("BWindow::Run end\n");
    return B_ERROR;
}


void
BWindow::SetPulseRate(bigtime_t rate)
{
	// TODO: What about locking?!?
	if (rate < 0)
		// || (rate == fPulseRate && !((rate == 0) ^ (fPulseRunner == NULL))))
		return;

	fPulseRate = rate;

	// if (rate > 0) {
	// 	if (fPulseRunner == NULL) {
	// 		// BMessage message(B_PULSE);
	// 		// fPulseRunner = new(std::nothrow) BMessageRunner(BMessenger(this),
	// 		// 	&message, rate);
	// 	} else {
	// 		fPulseRunner->SetInterval(rate);
	// 	}
	// } else {
		// rate == 0
		// delete fPulseRunner;
		// fPulseRunner = NULL;
	//}
}


bigtime_t
BWindow::PulseRate() const
{
	return fPulseRate;
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

}

void BWindow::_ReservedWindow2() {}
void BWindow::_ReservedWindow3() {}
void BWindow::_ReservedWindow4() {}
void BWindow::_ReservedWindow5() {}
void BWindow::_ReservedWindow6() {}
void BWindow::_ReservedWindow7() {}
void BWindow::_ReservedWindow8() {}

