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


#include "FilePanelPriv.h"

#include <string.h>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <BitmapButton.h>
#include <Button.h>
#include <ControlLook.h>
#include <Catalog.h>
#include <Debug.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <Messenger.h>
#include <Navigator.h>
#include <Path.h>
#include <SymLink.h>
#include <String.h>
#include <TextControl.h>

#include "AutoLock.h"
#include "Commands.h"
#include "FSUtils.h"

#include "Bitmaps.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "FilePanelPriv"

static uint32
GetLinkFlavor(const Model* model, bool resolve = true)
{
	if (model && model->IsSymLink()) {
		if (!resolve)
			return B_SYMLINK_NODE;
		model = model->LinkTo();
	}
	if (!model)
		return 0;

	if (model->IsDirectory())
		return B_DIRECTORY_NODE;

	return B_FILE_NODE;
}

//	#pragma mark - TFilePanel


TFilePanel::TFilePanel(file_panel_mode mode, BMessenger* target, const BEntry* startDir,
	uint32 nodeFlavors, bool multipleSelection, BMessage* message, BRefFilter* filter,
	uint32 openFlags, window_look look, window_feel feel, uint32 windowFlags, uint32 workspace,
	bool hideWhenDone)
	:
	BContainerWindow(0, openFlags, look, feel, windowFlags, workspace, false),
	fTextControl(NULL),
	fClientObject(NULL),
	fSelectionIterator(0),
	fMessage(NULL),
	fHideWhenDone(hideWhenDone),
	fDefaultStateRestored(false)
{
	fIsSavePanel = (mode == B_SAVE_PANEL);

	const float labelSpacing = be_control_look->DefaultLabelSpacing();
	// approximately (84, 50, 568, 296) with default sizing
	BRect windRect(labelSpacing * 1.0f, labelSpacing * 8.0f,
		labelSpacing * 125.0f, labelSpacing * 90.0f);
	MoveTo(windRect.LeftTop());
	ResizeTo(windRect.Width(), windRect.Height());

	fNodeFlavors = (nodeFlavors == 0) ? B_FILE_NODE : nodeFlavors;

	if (target)
		fTarget = *target;
	else
		fTarget = BMessenger(be_app);

	if (message)
		SetMessage(message);
	else if (fIsSavePanel)
		fMessage = new BMessage(B_SAVE_REQUESTED);
	else
		fMessage = new BMessage(B_REFS_RECEIVED);

	// check for legal starting directory
	Model* model = new Model();
	bool useRoot = true;

	if (startDir) {
		if (model->SetTo(startDir) == B_OK && model->IsDirectory())
			useRoot = false;
		else {
			delete model;
			model = new Model();
		}
	}

	if (useRoot) {
		BPath path;
		if (find_directory(B_USER_DIRECTORY, &path) == B_OK) {
			BEntry entry(path.Path(), true);
			if (entry.InitCheck() == B_OK && model->SetTo(&entry) == B_OK)
				useRoot = false;
		}
	}

	AutoLock<BWindow> lock(this);
	fBorderedView = new BorderedView;
	CreatePoseView(model);
	fBorderedView->GroupLayout()->SetInsets(1);

	fPoseContainer = new BGridView(0.0, 0.0);
	fPoseContainer->GridLayout()->AddView(fBorderedView, 0, 1);
	fPoseContainer->GridLayout()->AddView(fNavigator, 0, 0, 1);

	// Make room for the quick access buttons
	fPoseContainer->GridLayout()->SetInsets(146, 5, 0, 0);

	fPoseView->SetRefFilter(filter);
	if (!fIsSavePanel)
		fPoseView->SetMultipleSelection(multipleSelection);

	fPoseView->SetFlags(fPoseView->Flags() | B_NAVIGABLE);

	Init();
}


TFilePanel::~TFilePanel()
{
	delete fMessage;
}


void TFilePanel::AddQuickAccessButton(uint32 iconResource, const char* path, const char* name, const char* label, BRect rect)
{
	BSize largeIconSize = be_control_look->ComposeIconSize(32);
	BMessage* msg = new BMessage(kSwitchDirectory);
	entry_ref ref(0, 0, path);
	msg->AddRef("refs", &ref);
	BButton* quickAccessButton = new BButton(rect, name, label, msg);
	BBitmap* dirIcon = new BBitmap(BRect(BPoint(0, 0), largeIconSize), 0, B_RGBA32);
	GetTrackerResources()->GetIconResource(iconResource, B_LARGE_ICON, dirIcon);
	quickAccessButton->SetIcon(dirIcon);
	fBackView->AddChild(quickAccessButton);
}


filter_result
TFilePanel::MessageDropFilter(BMessage* message, BHandler**, BMessageFilter* filter)
{
	if (message == NULL || !message->WasDropped())
		return B_DISPATCH_MESSAGE;

	ASSERT(filter != NULL);
	if (filter == NULL)
		return B_DISPATCH_MESSAGE;

	TFilePanel* panel = dynamic_cast<TFilePanel*>(filter->Looper());
	ASSERT(panel != NULL);

	if (panel == NULL)
		return B_DISPATCH_MESSAGE;

	uint32 type;
	int32 count;
	if (message->GetInfo("refs", &type, &count) != B_OK)
		return B_SKIP_MESSAGE;

	if (count != 1)
		return B_SKIP_MESSAGE;

	entry_ref ref;
	if (message->FindRef("refs", &ref) != B_OK)
		return B_SKIP_MESSAGE;

	BEntry entry(&ref);
	if (entry.InitCheck() != B_OK)
		return B_SKIP_MESSAGE;

	// if the entry is a symlink
	// resolve it and see if it is a directory
	// pass it on if it is
	if (entry.IsSymLink()) {
		entry_ref resolvedRef;

		entry.GetRef(&resolvedRef);
		BEntry resolvedEntry(&resolvedRef, true);

		if (resolvedEntry.IsDirectory()) {
			// both entry and ref need to be the correct locations
			// for the last setto
			resolvedEntry.GetRef(&ref);
			entry.SetTo(&ref);
		}
	}

	// if not a directory, set to the parent, and select the child
	if (!entry.IsDirectory()) {
		node_ref child;
		if (entry.GetNodeRef(&child) != B_OK)
			return B_SKIP_MESSAGE;

		BPath path(&entry);

		if (entry.GetParent(&entry) != B_OK)
			return B_SKIP_MESSAGE;

		entry.GetRef(&ref);

		// also set the save name to the dragged in entry
		if (panel->IsSavePanel())
			panel->SetSaveText(path.Leaf());
	}

	panel->SwitchDirectory(&ref);

	return B_SKIP_MESSAGE;
}


void
TFilePanel::DispatchMessage(BMessage* message, BHandler* handler)
{
	_inherited::DispatchMessage(message, handler);
	if (message->what == B_KEY_DOWN || message->what == B_MOUSE_DOWN)
		AdjustButton();
}


BFilePanelPoseView*
TFilePanel::PoseView() const
{
	ASSERT(dynamic_cast<BFilePanelPoseView*>(fPoseView) != NULL);

	return static_cast<BFilePanelPoseView*>(fPoseView);
}


bool
TFilePanel::QuitRequested()
{
	// If we have a client object then this window will simply hide
	// itself, to be closed later when the client object itself is
	// destroyed. If we have no client then we must have been started
	// from the "easy" functions which simply instantiate a TFilePanel
	// and expect it to go away by itself

	if (fClientObject != NULL) {
		Hide();
		if (fClientObject != NULL)
			fClientObject->WasHidden();

		BMessage message(*fMessage);
		message.what = B_CANCEL;
		message.AddInt32("old_what", (int32)fMessage->what);
		message.AddPointer("source", fClientObject);
		fTarget.SendMessage(&message);

		return false;
	}

	return _inherited::QuitRequested();
}


BRefFilter*
TFilePanel::Filter() const
{
	return fPoseView->RefFilter();
}


void
TFilePanel::SetTarget(BMessenger target)
{
	fTarget = target;
}


void
TFilePanel::SetMessage(BMessage* message)
{
	delete fMessage;
	fMessage = new BMessage(*message);
}


void
TFilePanel::SetRefFilter(BRefFilter* filter)
{
	ASSERT(filter != NULL);
	if (filter == NULL)
		return;

	fPoseView->SetRefFilter(filter);
	//fPoseView->CommitActivePose();
	fPoseView->Refresh();
}


void
TFilePanel::SwitchDirectory(const entry_ref* ref)
{
	if (ref == NULL)
		return;

	entry_ref setToRef(*ref);
	BEntry entry(&setToRef, true);
	if (entry.InitCheck() != B_OK)
		return;

	if (!entry.Exists())
		return;

	_inherited::SwitchDirectory(&setToRef);
}


void
TFilePanel::Rewind()
{
	fSelectionIterator = 0;
}


void
TFilePanel::SetClientObject(BFilePanel* panel)
{
	fClientObject = panel;
}


void
TFilePanel::AdjustButton()
{
	// adjust button state
	BButton* button = dynamic_cast<BButton*>(FindView("default button"));
	if (button == NULL)
		return;

	BTextControl* textControl
		= dynamic_cast<BTextControl*>(FindView("text view"));
	PoseList* selectionList = fPoseView->SelectionList();
	BString buttonText = fButtonText;
	bool enabled = false;

	if (fIsSavePanel && textControl != NULL) {
		enabled = textControl->Text()[0] != '\0';
		if (fPoseView->IsFocus()) {
			fPoseView->ShowSelection(true);
			if (selectionList->CountItems() == 1) {
				Model* model = selectionList->FirstItem()->TargetModel();
				if (model->ResolveIfLink()->IsDirectory()) {
					enabled = true;
					buttonText = B_TRANSLATE("Open");
				} else {
					// insert the name of the selected model into
					// the text field, do not alter focus
					textControl->SetText(model->Name());
				}
			}
		} else
			fPoseView->ShowSelection(false);
	} else {
		int32 count = selectionList->CountItems();
		if (count) {
			enabled = true;

			// go through selection list looking at content
			for (int32 index = 0; index < count; index++) {
				Model* model = selectionList->ItemAt(index)->TargetModel();

				uint32 modelFlavor = GetLinkFlavor(model, false);
				uint32 linkFlavor = GetLinkFlavor(model, true);

				// if only one item is selected and we're not in dir
				// selection mode then we don't disable button ever
				if ((modelFlavor == B_DIRECTORY_NODE
						|| linkFlavor == B_DIRECTORY_NODE)
					&& count == 1) {
					break;
				}

				if ((fNodeFlavors & modelFlavor) == 0
					&& (fNodeFlavors & linkFlavor) == 0) {
					enabled = false;
					break;
				}
			}
		} else if ((fNodeFlavors & B_DIRECTORY_NODE) != 0) {
			// No selection, but the current directory could be opened.
			enabled = true;
		}
	}

	button->SetLabel(buttonText.String());
	button->SetEnabled(enabled);
}


void
TFilePanel::SelectionChanged()
{
	AdjustButton();

	if (fClientObject)
		fClientObject->SelectionChanged();
}


status_t
TFilePanel::GetNextEntryRef(entry_ref* ref)
{
	if (!ref)
		return B_ERROR;

	BPose* pose = fPoseView->SelectionList()->ItemAt(fSelectionIterator++);
	if (!pose)
		return B_ERROR;

	*ref = *pose->TargetModel()->EntryRef();
	return B_OK;
}


BPoseView*
TFilePanel::NewPoseView(Model* model, uint32)
{
	return new BFilePanelPoseView(model);
}


void
TFilePanel::Init(const BMessage*)
{
	BRect windRect(Bounds());
	fBackView = new BView(Bounds(), "View", B_FOLLOW_ALL, 0);
	fBackView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// BButton adopts the parent's high color instead of its own,
	// hence this mismatched color set here
	fBackView->SetHighUIColor(B_CONTROL_TEXT_COLOR);

	AddChild(fBackView);

	font_height ht;
	be_plain_font->GetHeight(&ht);
	const float f_height = ht.ascent + ht.descent + ht.leading;
	const float spacing = be_control_look->ComposeSpacing(B_USE_SMALL_SPACING);

	BRect rect;
	rect.top = spacing;
	rect.left = spacing;
	rect.right = rect.left + (spacing * 50);
	rect.bottom = rect.top + (f_height > 22 ? f_height : 22);

	// FIXME: Ignoring startDir for the moment
	const char* homeDir = getenv("HOME");
	BRect buttonRect(10, 10, 145, 50);

	if (homeDir != NULL) {

		AddQuickAccessButton(R_HomeDirIcon, homeDir, "home button", "Home", buttonRect);

		buttonRect.OffsetBy(0, 45);

		BString desktopDir(homeDir);
		desktopDir.Append("/Desktop");
		AddQuickAccessButton(R_DeskIcon, desktopDir.String(), "desktop button", "Desktop", buttonRect);

		buttonRect.OffsetBy(0, 45);

		BString documentsDir(homeDir);
		documentsDir.Append("/Documents");
		AddQuickAccessButton(R_HomeDirIcon, documentsDir.String(), "documents button", "Documents", buttonRect);

		buttonRect.OffsetBy(0, 45);

		BString picturesDir(homeDir);
		picturesDir.Append("/Pictures");
		AddQuickAccessButton(R_QueryDirIcon, picturesDir.String(), "pictures button", "Pictures", buttonRect);

		buttonRect.OffsetBy(0, 45);

		BString dlDir(homeDir);
		dlDir.Append("/Downloads");
		AddQuickAccessButton(R_DownloadDirIcon, dlDir.String(), "download button", "Downloads", buttonRect);

		buttonRect.OffsetBy(0, 45);
	}

	AddQuickAccessButton(R_RootIcon, "/", "drive button", "Hard Drive", buttonRect);

	// add buttons
	fButtonText = fIsSavePanel ? B_TRANSLATE("Save") : B_TRANSLATE("Open");
	BButton* default_button = new BButton(BRect(), "default button",
		fButtonText.String(), new BMessage(kDefaultButton),
		B_FOLLOW_RIGHT + B_FOLLOW_BOTTOM);
	BSize preferred = default_button->PreferredSize();
	const BRect defaultButtonRect = BRect(BPoint(
		windRect.Width() - (preferred.Width() + spacing + be_control_look->GetScrollBarWidth()),
		windRect.Height() - (preferred.Height() + spacing)),
		preferred);
	default_button->MoveTo(defaultButtonRect.LeftTop());
	default_button->ResizeTo(preferred);
	fBackView->AddChild(default_button);

	BButton* cancel_button = new BButton(BRect(), "cancel button",
		B_TRANSLATE("Cancel"), new BMessage(kCancelButton),
		B_FOLLOW_RIGHT + B_FOLLOW_BOTTOM);
	preferred = cancel_button->PreferredSize();
	cancel_button->MoveTo(defaultButtonRect.LeftTop()
		- BPoint(preferred.Width() + spacing, 0));
	cancel_button->ResizeTo(preferred);
	fBackView->AddChild(cancel_button);

	// add file name text view
	if (fIsSavePanel) {
		BRect rect(defaultButtonRect);
		rect.left = spacing;
		rect.right = rect.left + spacing * 28;

		fTextControl = new BTextControl(rect, "text view",
			B_TRANSLATE("save text"), "", NULL,
			B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
		// DisallowMetaKeys(fTextControl->TextView());
		// DisallowFilenameKeys(fTextControl->TextView());
		fBackView->AddChild(fTextControl);
		fTextControl->SetDivider(0.0f);
		fTextControl->TextView()->SetMaxBytes(B_FILE_NAME_LENGTH - 1);
	}

	// Add PoseView
	PoseView()->SetName("ActualPoseView");
	fPoseContainer->SetName("PoseView");
	fPoseContainer->SetResizingMode(B_FOLLOW_ALL);
	fBorderedView->EnableBorderHighlight(true);

	rect.left = spacing;
	rect.top = fNavigator->Frame().bottom + spacing;
	rect.right = windRect.Width() - spacing;
	rect.bottom = defaultButtonRect.top - spacing;
	fPoseContainer->MoveTo(rect.LeftTop());
	fPoseContainer->ResizeTo(rect.Size());

	// PoseView()->AddScrollBars();
	// PoseView()->SetDragEnabled(false);
	// PoseView()->SetDropEnabled(false);
	PoseView()->SetSelectionHandler(this);
	PoseView()->SetSelectionChangedHook(true);
	// PoseView()->DisableSaveLocation();

	if (fIsSavePanel)
		fBackView->AddChild(fPoseContainer, fTextControl);
	else
		fBackView->AddChild(fPoseContainer);

	// fShortcuts = new TShortcuts(this);

	AddShortcut('W', B_CONTROL_KEY, new BMessage(kCancelButton));
	AddShortcut('H', B_CONTROL_KEY, new BMessage(kSwitchToHome));
	AddShortcut('A', B_CONTROL_KEY | B_SHIFT_KEY, new BMessage(kShowSelectionWindow));
	AddShortcut('A', B_CONTROL_KEY, new BMessage(B_SELECT_ALL), this);
	// AddShortcut('S', B_COMMAND_KEY, new BMessage(kInvertSelection), PoseView());
	// AddShortcut('Y', B_COMMAND_KEY, new BMessage(kResizeToFit), PoseView());
	AddShortcut(B_DOWN_ARROW, B_CONTROL_KEY, new BMessage(kOpenDir));
	AddShortcut(B_DOWN_ARROW, B_CONTROL_KEY | B_OPTION_KEY, new BMessage(kOpenDir));
	AddShortcut(B_UP_ARROW, B_CONTROL_KEY, new BMessage(kOpenParentDir));
	AddShortcut(B_UP_ARROW, B_CONTROL_KEY | B_OPTION_KEY, new BMessage(kOpenParentDir));

	if (!fIsSavePanel && (fNodeFlavors & B_DIRECTORY_NODE) == 0)
		default_button->SetEnabled(false);

	default_button->MakeDefault(true);

	// Focus on text control initially, but do not alter focus afterwords
	// because pose view focus is needed for Cut/Copy/Paste to work.

	if (fIsSavePanel && fTextControl != NULL) {
		fTextControl->MakeFocus();
		fTextControl->TextView()->SelectAll();
	} else
		PoseView()->MakeFocus();

	// app_info info;
	BString title;
	// if (be_app->GetAppInfo(&info) == B_OK) {
	// 	if (!gLocalizedNamePreferred
	// 		|| BLocaleRoster::Default()->GetLocalizedFileName(
	// 			title, info.ref, false) != B_OK)
	// 		title = info.ref.name;
	// 	title << ": ";
	// }
	title << fButtonText;	// Open or Save

	SetTitle(title.String());

	SetSizeLimits(spacing * 60, 10000, spacing * 33, 10000);
}


void
TFilePanel::SaveState(bool)
{
}


void
TFilePanel::SaveState(BMessage &message) const
{
}


void
TFilePanel::SetButtonLabel(file_panel_button selector, const char* text)
{
	switch (selector) {
		case B_CANCEL_BUTTON:
			{
				BButton* button
					= dynamic_cast<BButton*>(FindView("cancel button"));
				if (button == NULL)
					break;

				float old_width = button->StringWidth(button->Label());
				button->SetLabel(text);
				float delta = old_width - button->StringWidth(text);
				if (delta) {
					button->MoveBy(delta, 0);
					button->ResizeBy(-delta, 0);
				}
			}
			break;

		case B_DEFAULT_BUTTON:
			{
				fButtonText = text;
				float delta = 0;
				BButton* button
					= dynamic_cast<BButton*>(FindView("default button"));
				if (button != NULL) {
					float old_width = button->StringWidth(button->Label());
					button->SetLabel(text);
					delta = old_width - button->StringWidth(text);
					if (delta) {
						button->MoveBy(delta, 0);
						button->ResizeBy(-delta, 0);
					}
				}

				// now must move cancel button
				button = dynamic_cast<BButton*>(FindView("cancel button"));
				if (button != NULL)
					button->MoveBy(delta, 0);
			}
			break;
	}
}


void
TFilePanel::SetSaveText(const char* text)
{
	if (text == NULL)
		return;

	BTextControl* textControl
		= dynamic_cast<BTextControl*>(FindView("text view"));
	if (textControl != NULL) {
		textControl->SetText(text);
		if (textControl->TextView() != NULL)
			textControl->TextView()->SelectAll();
	}
}


void
TFilePanel::MessageReceived(BMessage* message)
{
	entry_ref ref;

	switch (message->what) {
		case B_REFS_RECEIVED:
		{
			// item was double clicked in file panel (PoseView)
			if (message->FindRef("refs", &ref) != B_OK)
				break;

			BEntry entry(&ref, true);
			if (entry.InitCheck() != B_OK)
				break;

			// Double-click on dir or link-to-dir ALWAYS opens the dir.
			// If more than one dir is selected the first one is opened.
			if (entry.IsDirectory()) {
				SwitchDirectory(&ref);
			} else {
				// Otherwise, we have a file or a link to a file.
				// AdjustButton has already tested the flavor if it comes from the file
				// panel; all we have to do is see if the button is enabled.
				// In other cases, however, we can't rely on that. So first check for
				// TrackerViewToken in the message to see if it's coming from the pose view
				if (message->HasMessenger("TrackerViewToken")) {
					BButton* button = dynamic_cast<BButton*>(FindView("default button"));
					if (button == NULL || !button->IsEnabled())
						break;
				}

				if (IsSavePanel()) {
					int32 count = 0;
					type_code type;
					message->GetInfo("refs", &type, &count);

					// Don't allow saves of multiple files
					if (count > 1) {
						const char* sorry
							= B_TRANSLATE("Sorry, saving more than one item is not allowed.");
						ShowCenteredAlert(sorry, B_TRANSLATE("Cancel"));
					} else {
						// if we are a savepanel, set up the
						// filepanel correctly then pass control
						// so we follow the same path as if the user
						// clicked the save button

						// set the 'name' fld to the current ref's
						// name notify the panel that the default
						// button should be enabled
						SetSaveText(ref.name);
						SelectionChanged();

						HandleSaveButton();
					}
					break;
				}

				// send handler a message and close
				BMessage openMessage(*fMessage);
				for (int32 index = 0;; index++) {
					if (message->FindRef("refs", index, &ref) != B_OK)
						break;
					openMessage.AddRef("refs", &ref);
				}
				OpenSelectionCommon(&openMessage);
			}
			break;
		}

		case kSwitchDirectory:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) != B_OK)
				break;

			SwitchDirectory(&ref);
			break;
		}

		case kSwitchToHome:
		{
			BPath homePath;
			entry_ref ref;
			if (find_directory(B_USER_DIRECTORY, &homePath) != B_OK
				|| get_ref_for_path(homePath.Path(), &ref) != B_OK) {
				break;
			}

			SwitchDirectory(&ref);
			break;
		}

		case kCancelButton:
			PostMessage(B_QUIT_REQUESTED);
			break;

		case kResizeToFit:
			//ResizeToFit();
			break;

		case kOpenDir:
			OpenDirectory();
			break;

		case kOpenParentDir:
			OpenParent();
			break;

		case kDefaultButton:
			if (fIsSavePanel) {
				if (PoseView()->IsFocus()
					&& PoseView()->CountSelected() == 1) {
					Model* model = (PoseView()->SelectionList()->FirstItem())->TargetModel();
					if (model->ResolveIfLink()->IsDirectory()) {
						//PoseView()->CommitActivePose();
						PoseView()->OpenSelection();
						break;
					}
				}

				HandleSaveButton();
			} else
				HandleOpenButton();
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
TFilePanel::OpenDirectory()
{
	PoseList* list = PoseView()->SelectionList();
	if (list->CountItems() != 1)
		return;

	Model* model = list->FirstItem()->TargetModel();
	if (model->ResolveIfLink()->IsDirectory()) {
		BMessage message(B_REFS_RECEIVED);
		message.AddRef("refs", model->EntryRef());
		BMessenger(this).SendMessage(&message);
	}
}


void
TFilePanel::OpenParent()
{
	BEntry entry(TargetModel()->EntryRef());

	BEntry parentEntry;
	if (FSGetParentVirtualDirectoryAware(entry, parentEntry) != B_OK) {
		return;
	}

	entry_ref setToRef;
	parentEntry.GetRef(&setToRef);
	const entry_ref* parent = &setToRef;
	SwitchDirectory(parent);
}


int32
TFilePanel::ShowCenteredAlert(const char* text, const char* button1,
	const char* button2, const char* button3)
{
	BAlert* alert = new BAlert("", text, button1, button2, button3,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->MoveTo(Frame().left + 10, Frame().top + 10);

#if 0
	if (button1 != NULL && !strncmp(button1, "Cancel", 7))
		alert->SetShortcut(0, B_ESCAPE);
	else if (button2 != NULL && !strncmp(button2, "Cancel", 7))
		alert->SetShortcut(1, B_ESCAPE);
	else if (button3 != NULL && !strncmp(button3, "Cancel", 7))
		alert->SetShortcut(2, B_ESCAPE);
#endif

	return alert->Go();
}


void
TFilePanel::HandleSaveButton()
{
	BDirectory dir;

	// check for some illegal file names
	if (strcmp(fTextControl->Text(), ".") == 0
		|| strcmp(fTextControl->Text(), "..") == 0) {
		ShowCenteredAlert(
			B_TRANSLATE("The specified name is illegal. Please choose "
			"another name."),
			B_TRANSLATE("Cancel"));
		fTextControl->TextView()->SelectAll();
		return;
	}

	if (dir.SetTo(TargetModel()->EntryRef()) != B_OK) {
		ShowCenteredAlert(
			B_TRANSLATE("There was a problem trying to save in the folder "
			"you specified. Please try another one."),
			B_TRANSLATE("Cancel"));
		return;
	}

	if (dir.Contains(fTextControl->Text())) {
		if (dir.Contains(fTextControl->Text(), B_DIRECTORY_NODE)) {
			ShowCenteredAlert(
				B_TRANSLATE("The specified name is already used as the name "
				"of a folder. Please choose another name."),
				B_TRANSLATE("Cancel"));
			fTextControl->TextView()->SelectAll();
			return;
		} else {
			// if this was invoked by a dbl click, it is an explicit
			// replacement of the file.
			BString str(B_TRANSLATE("The file \"%name\" already exists in "
				"the specified folder. Do you want to replace it?"));
			str.ReplaceFirst("%name", fTextControl->Text());

			if (ShowCenteredAlert(str.String(),	B_TRANSLATE("Cancel"),
					B_TRANSLATE("Replace"))	== 0) {
				// user canceled
				fTextControl->TextView()->SelectAll();
				return;
			}
			// user selected "Replace" - let app deal with it
		}
	}

	BMessage message(*fMessage);
	message.AddRef("directory", TargetModel()->EntryRef());
	message.AddString("name", fTextControl->Text());

	if (fClientObject)
		fClientObject->SendMessage(&fTarget, &message);
	else
		fTarget.SendMessage(&message);

	// close window if we're dealing with standard message
	if (fHideWhenDone)
		PostMessage(B_QUIT_REQUESTED);
}


void
TFilePanel::OpenSelectionCommon(BMessage* openMessage)
{
	if (!openMessage->HasRef("refs"))
		return;

	for (int32 index = 0; ; index++) {
		entry_ref ref;
		if (openMessage->FindRef("refs", index, &ref) != B_OK)
			break;

		BEntry entry(&ref, true);
		if (entry.InitCheck() == B_OK) {
			//if (entry.IsDirectory())
			//	BRoster().AddToRecentFolders(&ref);
			//else
			//	BRoster().AddToRecentDocuments(&ref);
		}
	}

	//BRoster().AddToRecentFolders(TargetModel()->EntryRef());

	if (fClientObject)
		fClientObject->SendMessage(&fTarget, openMessage);
	else
		fTarget.SendMessage(openMessage);

	// close window if we're dealing with standard message
	if (fHideWhenDone)
		PostMessage(B_QUIT_REQUESTED);
}


void
TFilePanel::HandleOpenButton()
{
	PoseList* selection = PoseView()->SelectionList();

	// if we have only one directory and we're not opening dirs, enter.
	if ((fNodeFlavors & B_DIRECTORY_NODE) == 0
		&& selection->CountItems() == 1) {
		Model* model = selection->FirstItem()->TargetModel();

		if (model->IsDirectory()
			|| (model->IsSymLink() && !(fNodeFlavors & B_SYMLINK_NODE)
				&& model->ResolveIfLink()->IsDirectory())) {

			BMessage message(B_REFS_RECEIVED);
			message.AddRef("refs", model->EntryRef());
			PostMessage(&message);
			return;
		}
	}

	if (selection->CountItems()) {
			// there are items selected
			// message->fMessage->message from here to end
		BMessage message(*fMessage);
		// go through selection and add appropriate items
		for (int32 index = 0; index < selection->CountItems(); index++) {
			Model* model = selection->ItemAt(index)->TargetModel();

			if (((fNodeFlavors & B_DIRECTORY_NODE) != 0
					&& model->ResolveIfLink()->IsDirectory())
				|| ((fNodeFlavors & B_SYMLINK_NODE) != 0 && model->IsSymLink())
				|| ((fNodeFlavors & B_FILE_NODE) != 0
					&& model->ResolveIfLink()->IsFile())) {
				message.AddRef("refs", model->EntryRef());
			}
		}

		OpenSelectionCommon(&message);
	} else if ((fNodeFlavors & B_DIRECTORY_NODE) != 0) {
		// Open the current directory.
		BMessage message(*fMessage);
		message.AddRef("refs", TargetModel()->EntryRef());
		OpenSelectionCommon(&message);
	}
}


void
TFilePanel::WindowActivated(bool active)
{
	// force focus to update properly
	fBackView->Invalidate();
	_inherited::WindowActivated(active);
}
