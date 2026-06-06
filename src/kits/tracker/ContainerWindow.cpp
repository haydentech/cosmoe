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
#include <MenuBar.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <TextView.h>
#include <Volume.h>
#include <WindowPrivate.h>

#include <fs_attr.h>
#include <image.h>
#include <strings.h>
#include <stdlib.h>

#include "Attributes.h"
#include "AutoLock.h"
#include "Commands.h"
#include "FSUtils.h"
#include "Model.h"
#include "Navigator.h"
#include "PoseView.h"
#include "Shortcuts.h"

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
	window_feel feel, uint32 windowFlags, uint32 workspace, bool useLayout, bool runIt)
	:
	BWindow(InitialWindowRect(feel), "TrackerWindow", look, feel, windowFlags, workspace),
	fWindowList(list),
	fOpenFlags(openFlags),
	fUsesLayout(useLayout),
	fMenuContainer(NULL),
	fPoseContainer(NULL),
	fBorderedView(NULL),
	fShortcuts(NULL),
	fContextMenu(NULL),
	fWindowContextMenu(NULL),
	fMenuBar(NULL),
	fNavigator(NULL),
	fPoseView(NULL),
	fAttrMenu(NULL),
	fWindowMenu(NULL),
	fFileMenu(NULL),
	fStateNeedsSaving(false)
{
	InitIconPreloader();

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

		fMenuContainer = new BGroupView(B_HORIZONTAL, 0);
		fRootLayout->AddView(fMenuContainer);

		fPoseContainer = new BGridView(0.0, 0.0);
		fRootLayout->AddView(fPoseContainer);

		fBorderedView = new BorderedView;
		fPoseContainer->GridLayout()->AddView(fBorderedView, 0, 1);
	}

	if (runIt)
		Run();
	else {
		// The window is created locked and unlocked when first run.
		// When not running the thread, this constructor should still exit with the window unlocked
		// so that Lock() can be called later and not block.
		Unlock();
	}

	// ToDo: remove me once we have undo/redo menu items
	// (that is, move them to AddShortcuts())
	AddShortcut('Z', B_COMMAND_KEY, new BMessage(B_UNDO), this);
	AddShortcut('Z', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(B_REDO), this);
}


BContainerWindow::~BContainerWindow()
{
	ASSERT(IsLocked());

	delete fShortcuts;
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
	delete fWindowContextMenu;

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
BContainerWindow::AddContextMenus()
{
	fWindowContextMenu = new BPopUpMenu("WindowContext");
	AddWindowContextMenu(fWindowContextMenu);
}


void
BContainerWindow::DetachSubmenus()
{
}




void
BContainerWindow::RepopulateMenus()
{
	DetachSubmenus();

	if (fMenuBar != NULL) {
		if (fFileMenu != NULL) {
			fMenuBar->RemoveItem(fFileMenu);
			delete fFileMenu;
		}

		if (fWindowMenu != NULL) {
			fMenuBar->RemoveItem(fWindowMenu);
			delete fWindowMenu;
		}

		if (fAttrMenu != NULL) {
			fMenuBar->RemoveItem(fAttrMenu);
			delete fAttrMenu;
		}

		if (ShouldAddMenus()) {
			AddMenus();
			if (PoseView()->ViewMode() == kListMode)
				fMenuBar->AddItem(fAttrMenu, 2);
		}
	}

	delete fWindowContextMenu;
	fWindowContextMenu = new BPopUpMenu("WindowContext");
	AddWindowContextMenu(fWindowContextMenu);
}


void
BContainerWindow::Init(const BMessage* message)
{
	// pose view is expected to be setup at this point
	if (PoseView() == NULL)
		return;

	fShortcuts = new TShortcuts(this);

	if (ShouldAddMenus()) {
		fMenuBar = new BMenuBar("MenuBar");
		fMenuContainer->GroupLayout()->AddView(fMenuBar);
		AddMenus();
	} else {
		// add equivalents of the menu shortcuts to the menuless
		// desktop window
		AddShortcuts();
	}

	AddContextMenus();

	if (message != NULL)
		RestoreState(*message);
	else
		RestoreState();

	bool isListMode = PoseView()->ViewMode() == kListMode;
	if (ShouldAddMenus() && isListMode) {
		// for now only show attributes in list view
		// eventually enable attribute menu to allow users to select
		// using different attributes as titles in icon view modes
		ShowAttributesMenu();
	}

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
BContainerWindow::RestoreState()
{
	UpdateTitle();

	RestoreStateCommon();
}


void
BContainerWindow::RestoreState(const BMessage &message)
{
	UpdateTitle();

	RestoreWindowState(message);

	RestoreStateCommon();
}


void
BContainerWindow::RestoreStateCommon()
{
	if (fUsesLayout)
		InitLayout();
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


void
BContainerWindow::SaveState(bool hide)
{
}


void
BContainerWindow::SaveState(BMessage& message) const
{
}


bool
BContainerWindow::ShouldAddMenus() const
{
	return true;
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


void
BContainerWindow::AddMenus()
{
// TODO
}


void
BContainerWindow::AddFileMenu(BMenu* menu)
{
}


void
BContainerWindow::AddWindowMenu(BMenu* menu)
{
	BMenuItem* item = new BMenuItem(B_TRANSLATE("Preferences" B_UTF8_ELLIPSIS),
		new BMessage(kShowSettingsWindow), ',');
	item->SetTarget(be_app);
	menu->AddItem(item);
}


void
BContainerWindow::AddShortcuts()
{
	// add equivalents of the menu shortcuts to the menuless desktop window
}


void
BContainerWindow::MenusBeginning()
{
	if (fMenuBar == NULL)
		return;


	if (fFileMenu != NULL)
		UpdateMenu(fFileMenu, kFileMenuContext);

	if (fWindowMenu != NULL)
		UpdateMenu(fWindowMenu, kWindowMenuContext);
}


void
BContainerWindow::MenusEnded()
{
}


void
BContainerWindow::AddWindowContextMenu(BMenu* menu)
{
	// create context sensitive menu for empty area of window
	// since we check view mode before display, this should be a radio
	// mode menu

	// else "Arrange by >" menu inserted here,
	// see UpdateMenu() and SetupArrangeByMenu()
	menu->AddItem(Shortcuts()->SelectItem());
	menu->AddItem(Shortcuts()->SelectAllItem());
	menu->AddItem(Shortcuts()->OpenParentItem());
	menu->AddSeparatorItem();

	// "Mount >" menu and "Unmount" are inserted here,
	// see UpdateMenu() and SetupMountMenu().


#if DEBUG
	menu->AddSeparatorItem();
	BMenuItem* testing = new BMenuItem("Test icon cache",
		new BMessage(kTestIconCache));
	menu->AddItem(testing);
	testing->SetTarget(PoseView());
#endif
}

void
BContainerWindow::UpdateMenu(BMenu* menu, MenuContext context, const entry_ref* ref)
{
	// update shared shortcut item's target and enabled state
	Shortcuts()->Update(menu);

	if (context == kFileMenuContext)
		UpdateFileMenu(menu);
	else if (context == kWindowMenuContext)
		UpdateWindowMenu(menu);
	else if (context == kWindowPopUpContext)
		UpdateWindowContextMenu(menu);
}


void
BContainerWindow::UpdateFileMenu(BMenu* menu)
{
	UpdateFileMenuOrPoseContextMenu(menu, kFileMenuContext);
}


void
BContainerWindow::UpdateFileMenuOrPoseContextMenu(BMenu* menu, MenuContext context,
	const entry_ref* ref)
{
}


void
BContainerWindow::UpdateWindowMenu(BMenu* menu)
{
	UpdateWindowMenuOrWindowContextMenu(menu, kWindowMenuContext);
}


void
BContainerWindow::UpdateWindowContextMenu(BMenu* menu)
{
	UpdateWindowMenuOrWindowContextMenu(menu, kWindowPopUpContext);
}


void
BContainerWindow::UpdateWindowMenuOrWindowContextMenu(BMenu* menu, MenuContext context)
{
}


BMenuItem*
BContainerWindow::NewAttributeMenuItem(const char* label, const char* name,
	int32 type, float width, int32 align, bool editable, bool statField)
{
	return NewAttributeMenuItem(label, name, type, NULL, width, align,
		editable, statField);
}


BMenuItem*
BContainerWindow::NewAttributeMenuItem(const char* label, const char* name,
	int32 type, const char* displayAs, float width, int32 align,
	bool editable, bool statField)
{
	BMessage* message = new BMessage(kAttributeItem);
	message->AddString("attr_name", name);
	message->AddInt32("attr_type", type);
	message->AddInt32("attr_hash", (int32)AttrHashString(name, (uint32)type));
	message->AddFloat("attr_width", width);
	message->AddInt32("attr_align", align);
	if (displayAs != NULL)
		message->AddString("attr_display_as", displayAs);
	message->AddBool("attr_editable", editable);
	message->AddBool("attr_statfield", statField);

	BMenuItem* menuItem = new BMenuItem(label, message);
	menuItem->SetTarget(PoseView());

	return menuItem;
}


void
BContainerWindow::NewAttributesMenu()
{
	if (fAttrMenu != NULL)
		delete fAttrMenu;

	fAttrMenu = new BMenu(B_TRANSLATE("Attributes"));

	NewAttributesMenu(fAttrMenu);
}


void
BContainerWindow::NewAttributesMenu(BMenu* menu)
{
	ASSERT(PoseView() != NULL);

	// empty menu
	BMenuItem* item;
	while ((item = menu->RemoveItem((int32)0)) != NULL)
		delete item;

	menu->AddItem(item = new BMenuItem(B_TRANSLATE("Copy layout"),
		new BMessage(kCopyAttributes)));
	item->SetTarget(PoseView());
	menu->AddItem(item = new BMenuItem(B_TRANSLATE("Paste layout"),
		new BMessage(kPasteAttributes)));
	item->SetTarget(PoseView());
	menu->AddSeparatorItem();

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Name"),
		kAttrStatName, B_STRING_TYPE, 145, B_ALIGN_LEFT, true, true));

	if (gLocalizedNamePreferred) {
		menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Real name"),
			kAttrRealName, B_STRING_TYPE, 145, B_ALIGN_LEFT, true, true));
	}

	menu->AddItem(NewAttributeMenuItem (B_TRANSLATE("Size"), kAttrStatSize,
		B_OFF_T_TYPE, 80, B_ALIGN_RIGHT, false, true));

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Modified"),
		kAttrStatModified, B_TIME_TYPE, 150, B_ALIGN_LEFT, false, true));

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Created"),
		kAttrStatCreated, B_TIME_TYPE, 150, B_ALIGN_LEFT, false, true));

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Kind"),
		kAttrMIMEType, B_MIME_STRING_TYPE, 145, B_ALIGN_LEFT, false, false));

	if (TargetModel()->IsTrash() || TargetModel()->InTrash()) {
		menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Original name"),
			kAttrOriginalPath, B_STRING_TYPE, 225, B_ALIGN_LEFT, false,
			false));
	} else {
		menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Location"), kAttrPath,
			B_STRING_TYPE, 225, B_ALIGN_LEFT, false, false));
	}

#ifdef OWNER_GROUP_ATTRIBUTES
	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Owner"), kAttrStatOwner,
		B_STRING_TYPE, 60, B_ALIGN_LEFT, false, true));

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Group"), kAttrStatGroup,
		B_STRING_TYPE, 60, B_ALIGN_LEFT, false, true));
#endif

	menu->AddItem(NewAttributeMenuItem(B_TRANSLATE("Permissions"),
		kAttrStatMode, B_STRING_TYPE, 80, B_ALIGN_LEFT, false, true));

	MarkAttributesMenu(menu);
}


void
BContainerWindow::ShowAttributesMenu()
{
	ASSERT(fAttrMenu != NULL);
	fMenuBar->AddItem(fAttrMenu, 2);
}


void
BContainerWindow::HideAttributesMenu()
{
	ASSERT(fAttrMenu != NULL);
	fMenuBar->RemoveItem(fAttrMenu);
}


void
BContainerWindow::MarkAttributesMenu()
{
	MarkAttributesMenu(fAttrMenu);
}


void
BContainerWindow::MarkAttributesMenu(BMenu* menu)
{
	if (menu == NULL)
		return;
}


void
BContainerWindow::RestoreWindowState(AttributeStreamNode* node)
{
}


void
BContainerWindow::RestoreWindowState(const BMessage& message)
{
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
