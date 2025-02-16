/*
 * Copyright 2001-2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_WINDOW_H
#define	_WINDOW_H

extern "C" {
#include "window.h"
}

#include <Looper.h>
#include <View.h>


class BButton;
class BMenuBar;
class BMenuItem;
class BView;


enum window_type {
	B_UNTYPED_WINDOW					= 0,
	B_TITLED_WINDOW						= 1,
	B_MODAL_WINDOW						= 3,
	B_DOCUMENT_WINDOW					= 11,
	B_BORDERED_WINDOW					= 20,
	B_FLOATING_WINDOW					= 21
};

enum window_look {
	B_BORDERED_WINDOW_LOOK				= 20,
	B_NO_BORDER_WINDOW_LOOK				= 19,
	B_TITLED_WINDOW_LOOK				= 1,
	B_DOCUMENT_WINDOW_LOOK				= 11,
	B_MODAL_WINDOW_LOOK					= 3,
	B_FLOATING_WINDOW_LOOK				= 7
};

enum window_feel {
	B_NORMAL_WINDOW_FEEL				= 0,
	B_MODAL_SUBSET_WINDOW_FEEL			= 2,
	B_MODAL_APP_WINDOW_FEEL				= 1,
	B_MODAL_ALL_WINDOW_FEEL				= 3,
	B_FLOATING_SUBSET_WINDOW_FEEL		= 5,
	B_FLOATING_APP_WINDOW_FEEL			= 4,
	B_FLOATING_ALL_WINDOW_FEEL			= 6
};

enum window_alignment {
	B_BYTE_ALIGNMENT	= 0,
	B_PIXEL_ALIGNMENT	= 1
};

// window flags
enum {
	B_NOT_MOVABLE						= 0x00000001,
	B_NOT_CLOSABLE						= 0x00000020,
	B_NOT_ZOOMABLE						= 0x00000040,
	B_NOT_MINIMIZABLE					= 0x00004000,
	B_NOT_RESIZABLE						= 0x00000002,
	B_NOT_H_RESIZABLE					= 0x00000004,
	B_NOT_V_RESIZABLE					= 0x00000008,
	B_AVOID_FRONT						= 0x00000080,
	B_AVOID_FOCUS						= 0x00002000,
	B_WILL_ACCEPT_FIRST_CLICK			= 0x00000010,
	B_OUTLINE_RESIZE					= 0x00001000,
	B_NO_WORKSPACE_ACTIVATION			= 0x00000100,
	B_NOT_ANCHORED_ON_ACTIVATE			= 0x00020000,
	B_ASYNCHRONOUS_CONTROLS				= 0x00080000,
	B_QUIT_ON_WINDOW_CLOSE				= 0x00100000,
	B_SAME_POSITION_IN_ALL_WORKSPACES	= 0x00200000,
	B_AUTO_UPDATE_SIZE_LIMITS			= 0x00400000,
	B_CLOSE_ON_ESCAPE					= 0x00800000,
	B_NO_SERVER_SIDE_WINDOW_MODIFIERS	= 0x00000200
};

#define B_CURRENT_WORKSPACE				0
#define B_ALL_WORKSPACES				0xffffffff

// MoveOnScreen() flags
enum {
	B_DO_NOT_RESIZE_TO_FIT				= 0x0001,
	B_MOVE_IF_PARTIALLY_OFFSCREEN		= 0x0002
};


class BWindow : public BLooper {
public:
								BWindow(BRect frame, const char* title,
									window_type type, uint32 flags,
									uint32 workspace = B_CURRENT_WORKSPACE);
								BWindow(BRect frame, const char* title,
									window_look look, window_feel feel,
									uint32 flags, uint32 workspace
										= B_CURRENT_WORKSPACE);
	virtual						~BWindow();

	virtual	void				Quit();
			void				Close() { Quit(); }

			void				AddChild(BView* child, BView* before = NULL);
			void				AddChild(BLayoutItem* child);
			bool				RemoveChild(BView* child);
			int32				CountChildren() const;
			BView*				ChildAt(int32 index) const;

	virtual	void				FrameMoved(BPoint newPosition);

	virtual	void				FrameResized(float newWidth, float newHeight);
	virtual void				Minimize(bool minimize);
			void				SetPulseRate(bigtime_t rate);
			bigtime_t			PulseRate() const;

			void				SetDefaultButton(BButton* button);
			BButton*			DefaultButton() const;

	virtual	void				MenusBeginning();
	virtual	void				MenusEnded();

			bool				NeedsUpdate() const;
			void				UpdateIfNeeded();

			BView*				FindView(const char* viewName) const;
			BView*				FindView(BPoint) const;
			BView*				CurrentFocus() const;

			void				Activate(bool = true);
	virtual	void				WindowActivated(bool focus);

			void				ConvertToScreen(BPoint* point) const;
			BPoint				ConvertToScreen(BPoint point) const;
			void				ConvertFromScreen(BPoint* point) const;
			BPoint				ConvertFromScreen(BPoint point) const;
			void				ConvertToScreen(BRect* rect) const;
			BRect				ConvertToScreen(BRect rect) const;
			void				ConvertFromScreen(BRect* rect) const;
			BRect				ConvertFromScreen(BRect rect) const;

	virtual	void				Show();
	virtual	void				Hide();
			bool				IsHidden() const;
			bool				IsMinimized() const;

			BRect				Bounds() const;
			BRect				Frame() const;
			//BRect				DecoratorFrame() const;
			BSize				Size() const;
			const char*			Title() const;
			void				SetTitle(const char* title);
			bool				IsActive() const;

			void				SetKeyMenuBar(BMenuBar* bar);
			BMenuBar*			KeyMenuBar() const;

			status_t			SetType(window_type type);
			window_type			Type() const;

			status_t			SetLook(window_look look);
			window_look			Look() const;

			status_t			SetFeel(window_feel feel);
			window_feel			Feel() const;

			status_t			SetFlags(uint32);
			uint32				Flags() const;

			bool				IsModal() const;
			bool				IsFloating() const;

	virtual	bool				QuitRequested();
	virtual thread_id			Run();

	virtual	void				SetLayout(BLayout* layout);
			BLayout*			GetLayout() const;

			void				InvalidateLayout(bool descendants = false);
			void				Layout(bool force);
private:
	// FBC padding and forbidden methods
	virtual	void				_ReservedWindow2();
	virtual	void				_ReservedWindow3();
	virtual	void				_ReservedWindow4();
	virtual	void				_ReservedWindow5();
	virtual	void				_ReservedWindow6();
	virtual	void				_ReservedWindow7();
	virtual	void				_ReservedWindow8();

private:
	typedef BLooper inherited;
	struct unpack_cookie;
	class Shortcut;

	friend class BAlert;
	friend class BApplication;
	friend class BBitmap;
	friend class BView;
	friend class BMenuItem;
	friend class BWindowScreen;
	friend class BDirectWindow;
	friend class BFilePanel;
	friend class BWindowStack;

			void				_InitData(BRect frame, const char* title,
									window_look look, window_feel feel,
									uint32 flags, uint32 workspace,
									int32 bitmapToken = -1);
			window_type			_ComposeType(window_look look,
									window_feel feel) const;
			void				_DecomposeType(window_type type,
									window_look* look,
									window_feel* feel) const;

			void				SetIsFilePanel(bool yes);
			bool				IsFilePanel() const;

			void				_CreateTopView();
			void				_AdoptResize();
			void				_SetFocus(BView* focusView,
									bool notifyIputServer = false);
			void				_SetName(const char* title);

			BView*				_FindView(BView* view, BPoint point) const;
			BView*				_LastViewChild(BView* parent);

			BView*				_FindNextNavigable(BView* focus, uint32 flags);
			BView*				_FindPreviousNavigable(BView* focus,
									uint32 flags);
			void				_SendShowOrHideMessage();

private:
            friend void windowframe_resize_handler(struct widget *widget, int32_t width, int32_t height, void *data);
            friend void key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data);
			char*				fTitle;
			int32				_unused0;
			bool				fInTransaction;
			bool				fActive;
			short				fShowLevel;
			uint32				fFlags;

			BView*				fTopView;
			BView*				fFocus;
			BView*				fLastMouseMovedView;
			uint32				_unused1;
			BMenuBar*			fKeyMenuBar;
			BButton*			fDefaultButton;
			BList				fShortcuts;
			int32				fTopViewToken;
			bool				fUpdateRequested;
			bool				fOffscreen;
			bool				fIsFilePanel;
			bool				_unused4;
			bigtime_t			fPulseRate;
			bool				_unused5;
			bool				fMinimized;
			bool				fNoQuitShortcut;
			bool				_unused6;
			sem_id				fMenuSem;
			float				fMaxZoomHeight;
			float				fMaxZoomWidth;
			float				fMinHeight;
			float				fMinWidth;
			float				fMaxHeight;
			float				fMaxWidth;
			BRect				fFrame;
			window_look			fLook;
			window_feel			fFeel;
			int32				fLastViewToken;
			BRect				fPreviousFrame;

			uint32				_reserved[9];


            // Wayland/Weston support

            static inline struct display *d = NULL;

            struct window *window = NULL;
            struct widget *windowframe_widget = NULL;
};


#endif // _WINDOW_H
