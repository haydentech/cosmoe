/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/


#include "ContainerWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <Debug.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <GroupLayout.h>
#include <Path.h>
#include <TextView.h>
#include <Volume.h>
#include <WindowPrivate.h>

#include <fs_attr.h>
#include <image.h>
#include <strings.h>
#include <stdlib.h>

#include <Autolock.h>
#include "Commands.h"
#include "FSUtils.h"
#include "Model.h"
#include "Navigator.h"
#include "PoseView.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ContainerWindow"


#ifdef _IMPEXP_BE
_IMPEXP_BE
#endif

BRect BContainerWindow::sNewWindRect;
static int32 sWindowStaggerBy;


namespace BPrivate {



//	#pragma mark - BContainerWindow


BContainerWindow::BContainerWindow(LockingList<BWindow>* list, uint32 openFlags, window_look look,
	window_feel feel, uint32 windowFlags, uint32 workspace, bool useLayout)
	:
	BWindow(InitialWindowRect(feel), "TrackerWindow", look, feel, windowFlags, workspace),
	fWindowList(list),
	fOpenFlags(openFlags),
	fUsesLayout(useLayout),
	fPoseContainer(NULL),
	fBorderedView(NULL),
	fNavigator(NULL),
	fPoseView(NULL),
	fStateNeedsSaving(false)
{
	if (list != NULL) {
		ASSERT(list->IsLocked());
		list->AddItem(this);
	}

	if (fUsesLayout) {
		SetFlags(Flags() | B_AUTO_UPDATE_SIZE_LIMITS);

		fRootLayout = new BGroupLayout(B_VERTICAL, 0);
		fRootLayout->SetInsets(0);
		SetLayout(fRootLayout);
		fRootLayout->Owner()->AdoptSystemColors();

		fPoseContainer = new BGridView(0.0, 0.0);
		fRootLayout->AddView(fPoseContainer);

		fBorderedView = new BorderedView;
		fPoseContainer->GridLayout()->AddView(fBorderedView, 0, 1);
	}

	Run();

	// ToDo: remove me once we have undo/redo menu items
	// (that is, move them to AddShortcuts())
	AddShortcut('Z', B_COMMAND_KEY, new BMessage(B_UNDO), this);
	AddShortcut('Z', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(B_REDO), this);
}


BContainerWindow::~BContainerWindow()
{
	ASSERT(IsLocked());
}


BRect
BContainerWindow::InitialWindowRect(window_feel feel)
{
	if (!sNewWindRect.IsValid()) {
		const float labelSpacing = be_control_look->DefaultLabelSpacing();
		// approximately (85, 50, 548, 280) with default spacing
		sNewWindRect = BRect(labelSpacing * 14, labelSpacing * 8,
			labelSpacing * 91, labelSpacing * 46);
		sWindowStaggerBy = (int32)(labelSpacing * 3.0f);
	}

	if (feel != kDesktopWindowFeel)
		return sNewWindRect;

	// do not offset desktop window
	BRect result = sNewWindRect;
	result.OffsetTo(0, 0);
	return result;
}


bool
BContainerWindow::QuitRequested()
{
	// this is a response to the DeskBar sending us a B_QUIT, when it really
	// means to say close all your windows. It might be better to have it
	// send a kCloseAllWindows message and have windowless apps stay running,
	// which is what we will do for the Tracker
	if (CurrentMessage() != NULL
		&& ((CurrentMessage()->FindInt32("modifiers") & B_CONTROL_KEY)) != 0) {
		be_app->PostMessage(kCloseAllWindows);
	}

	Hide();
		// this will close the window instantly, even if
		// the file system is very busy right now
	return true;
}


void
BContainerWindow::Quit()
{
	int32 windowCount = 0;

	// This is a deadlock code sequence - need to change this
	// to acquire the window list while this container window is unlocked
	if (fWindowList != NULL) {
		AutoLock<LockingList<BWindow> > lock(fWindowList);
		if (lock.IsLocked()) {
			fWindowList->RemoveItem(this, false);
			windowCount = fWindowList->CountItems();
		}
	}

	if (fWindowList != NULL && windowCount == 0)
		be_app->PostMessage(B_QUIT_REQUESTED);

	_inherited::Quit();
}


BPoseView*
BContainerWindow::NewPoseView(Model* model, uint32 viewMode)
{
	return new BPoseView(model, viewMode);
}


void
BContainerWindow::CreatePoseView(Model* model)
{
	fPoseView = NewPoseView(model, kListMode);
	fBorderedView->GroupLayout()->AddView(fPoseView);
	fBorderedView->GroupLayout()->SetInsets(1, 0, 1, 1);
	fBorderedView->EnableBorderHighlight(false);

	fNavigator = new BNavigator(model);
}


void
BContainerWindow::Init(const BMessage* message)
{
	// pose view is expected to be setup at this point
	if (PoseView() == NULL)
		return;

	Show();

	// done showing, turn the B_NO_WORKSPACE_ACTIVATION flag off;
	// it was on to prevent workspace jerking during boot
	SetFlags(Flags() & ~B_NO_WORKSPACE_ACTIVATION);
}


void
BContainerWindow::InitLayout()
{
	bool forFilePanel = PoseView()->IsFilePanel();
	if (!forFilePanel) {
		// Eliminate the extra borders
		fPoseContainer->GridLayout()->SetInsets(-1, 0, -1, -1);
	}
}


void
BContainerWindow::OpenParent()
{
	BEntry entry(TargetModel()->EntryRef());
	if (entry.InitCheck() != B_OK)
		return;

	BEntry parentEntry;
	if (FSGetParentVirtualDirectoryAware(entry, parentEntry) != B_OK)
		return;

	entry_ref setToRef;
	parentEntry.GetRef(&setToRef);
	const entry_ref* parent = &setToRef;

	// need to send switch message for spatial mode
	BMessage message(kSwitchDirectory);
	message.AddRef("refs", parent);
	MessageReceived(&message);
}


void
BContainerWindow::SwitchDirectory(const entry_ref* ref)
{
	// Update pose view and set directory type
	PoseView()->SwitchDir(ref);

	UpdateTitle();
}


void
BContainerWindow::UpdateTitle()
{
	if (Navigator() != NULL)
		Navigator()->UpdateLocation(TargetModel(), kActionUpdatePath);
}


Model*
BContainerWindow::TargetModel() const
{
	return PoseView()->TargetModel();
}


void
BContainerWindow::SelectionChanged()
{
}


void
BContainerWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_CUT:
		case B_COPY:
		case B_PASTE:
		case B_SELECT_ALL:
		{
			BView* view = CurrentFocus();
			if (dynamic_cast<BTextView*>(view) == NULL) {
				// The selected item is not a BTextView, so forward the
				// message to the PoseView.
				if (PoseView() != NULL)
					PostMessage(message, PoseView());
			} else {
				// Since we catch the generic clipboard shortcuts in a way that
				// means the BTextView will never get them, we must
				// manually forward them ourselves.
				//
				// However, we have to take care to not forward the custom
				// clipboard messages, else we would wind up in infinite
				// recursion.
				PostMessage(message, view);
			}
			break;
		}

		case kCutMoreSelectionToClipboard:
		case kCopyMoreSelectionToClipboard:
		case kPasteLinksFromClipboard:
			if (PoseView() != NULL)
				PostMessage(message, PoseView());
			break;

		case kOpenParentDir:
			OpenParent();
			break;

		case kNewFolder:
			PostMessage(message, PoseView());
			break;

		case kQuitTracker:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;

		case kSwitchDirectory:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) != B_OK)
				break;

			SwitchDirectory(&ref);

			if (Navigator() != NULL) {
				// update Navigation bar
				int32 action = message->GetInt32("action", kActionSet);
				Navigator()->UpdateLocation(TargetModel(), action);
			}
			break;
		}
		default:
			_inherited::MessageReceived(message);
			break;
	}
}


}


bool
BContainerWindow::IsShowing(const entry_ref* entry) const
{
	return PoseView()->Represents(entry);
}


//	#pragma mark - BorderedView


BorderedView::BorderedView()
	:
	BGroupView(B_VERTICAL, 0),
	fEnableBorderHighlight(true)
{
	GroupLayout()->SetInsets(1);
}


void
BorderedView::WindowActivated(bool active)
{
	BContainerWindow* window = dynamic_cast<BContainerWindow*>(Window());
	if (window == NULL)
		return;

	if (window->PoseView()->IsFocus())
		PoseViewFocused(active); // Update border color
}


void BorderedView::EnableBorderHighlight(bool enable)
{
	fEnableBorderHighlight = enable;
	PoseViewFocused(false);
}


void
BorderedView::PoseViewFocused(bool focused)
{
	BContainerWindow* window = dynamic_cast<BContainerWindow*>(Window());
	if (window == NULL)
		return;

	color_which base = B_DOCUMENT_BACKGROUND_COLOR;
	float tint = B_DARKEN_2_TINT;
	if (focused && window->IsActive() && fEnableBorderHighlight) {
		base = B_KEYBOARD_NAVIGATION_COLOR;
		tint = B_NO_TINT;
	}

	SetViewUIColor(base, tint);
	Invalidate();
}


void
BorderedView::Pulse()
{
}
