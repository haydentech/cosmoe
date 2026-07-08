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


#include "Navigator.h"

#include <ControlLook.h>
#include <TextControl.h>
#include <Window.h>

#include "Bitmaps.h"
#include "Commands.h"
#include "FSUtils.h"
#include "Tracker.h"


namespace BPrivate {

static const int32 kMaxHistory = 32;

}


//	#pragma mark - BNavigator


BNavigator::BNavigator(const Model* model)
	:
	BToolBar(),
	fBackHistory(8),
	fForwHistory(8)
{
	// Get initial path
	model->GetPath(&fPath);

	fLocation = new BTextControl("Location", "", "",
		new BMessage(kNavigatorCommandLocation));
	fLocation->SetDivider(0);

	GroupLayout()->SetInsets(0.0f, 0.0f, B_USE_HALF_ITEM_INSETS, 1.0f);
		// 1px bottom inset used for border

	// Needed to draw the bottom border
	SetFlags(Flags() | B_WILL_DRAW);
	SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}


BNavigator::~BNavigator()
{
}


void
BNavigator::AttachedToWindow()
{
	BToolBar::AttachedToWindow();

	BSize iconSize;
	uint32 iconFlags = 0;
#ifdef __HAIKU__
	iconSize = be_control_look->ComposeIconSize(20);
#else
	iconSize = be_control_look->ComposeIconSize(40);
	iconFlags = B_BITMAP_HIDPI;
#endif

	const BRect iconRect(BPoint(0, 0), iconSize);

	// Set up toolbar items
	BBitmap* bmpBack = new BBitmap(iconRect, iconFlags, B_RGBA32);
	GetTrackerResources()->GetIconResource(R_ResBackNav, B_MINI_ICON, bmpBack);
	AddAction(kNavigatorCommandBackward, this, bmpBack);
	SetActionEnabled(kNavigatorCommandBackward, false);
	delete bmpBack;

	BBitmap* bmpForw = new BBitmap(iconRect, iconFlags, B_RGBA32);
	GetTrackerResources()->GetIconResource(R_ResForwNav, B_MINI_ICON, bmpForw);
	AddAction(kNavigatorCommandForward, this, bmpForw);
	SetActionEnabled(kNavigatorCommandForward, false);
	delete bmpForw;

	BBitmap* bmpUp = new BBitmap(iconRect, iconFlags, B_RGBA32);
	GetTrackerResources()->GetIconResource(R_ResUpNav, B_MINI_ICON, bmpUp);
	AddAction(kNavigatorCommandUp, this, bmpUp);
	SetActionEnabled(kNavigatorCommandUp, false);
	delete bmpUp;

	AddView(fLocation);
	fLocation->SetTarget(this);
}


void
BNavigator::AllAttached()
{
	// Inital setup of widget states
	UpdateLocation(0, kActionSet);
}


void
BNavigator::Draw(BRect updateRect)
{
	// Draw a 1px bottom border, like BMenuBar
	BRect rect(Bounds());
	rgb_color base = LowColor();
	uint32 flags = 0;

	be_control_look->DrawBorder(this, rect, updateRect, base,
		B_PLAIN_BORDER, flags, BControlLook::B_BOTTOM_BORDER);

	_inherited::Draw(rect & updateRect);
}


void
BNavigator::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kNavigatorCommandBackward:
			GoBackward((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandForward:
			GoForward((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandUp:
			GoUp((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandLocation:
			GoTo();
			break;

		case kNavigatorCommandSetFocus:
			fLocation->MakeFocus();
			break;

		default:
		{
			// Catch any dropped refs and try to switch to this new directory
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BMessage message(kSwitchDirectory);
				BEntry entry(&ref, true);
				if (!entry.IsDirectory()) {
					entry.GetRef(&ref);
					BPath path(&ref);
					path.GetParent(&path);
					get_ref_for_path(path.Path(), &ref);
				}
				message.AddRef("refs", &ref);
				message.AddInt32("action", kActionSet);
				Window()->PostMessage(&message);
			}
		}
	}
}


void
BNavigator::GoBackward(bool option)
{
	int32 itemCount = fBackHistory.CountItems();
	if (itemCount >= 2 && fBackHistory.ItemAt(itemCount - 2)) {
		BEntry entry;
		if (entry.SetTo(fBackHistory.ItemAt(itemCount - 2)->Path()) == B_OK)
			SendNavigationMessage(kActionBackward, &entry, option);
	}
}


void
BNavigator::GoForward(bool option)
{
	if (fForwHistory.CountItems() >= 1) {
		BEntry entry;
		if (entry.SetTo(fForwHistory.LastItem()->Path()) == B_OK)
			SendNavigationMessage(kActionForward, &entry, option);
	}
}


void
BNavigator::GoUp(bool option)
{
	BEntry entry;
	if (entry.SetTo(fPath.Path()) == B_OK) {
		BEntry parentEntry;
		if (entry.GetParent(&parentEntry) == B_OK) {
			SendNavigationMessage(kActionUp, &parentEntry, option);
		}
	}
}


void
BNavigator::SendNavigationMessage(NavigationAction action, BEntry* entry,
	bool option)
{
	entry_ref ref;

	if (entry->GetRef(&ref) == B_OK) {
		BMessage message;
		message.AddRef("refs", &ref);
		message.AddInt32("action", action);

		message.what = kSwitchDirectory;
		Window()->PostMessage(&message);
	}
}


void
BNavigator::GoTo()
{
	BString pathname = fLocation->Text();

	if (pathname.Compare("") == 0)
		pathname = "/";

	BEntry entry;
	entry_ref ref;

	if (entry.SetTo(pathname.String()) == B_OK
		&& entry.GetRef(&ref) == B_OK) {
		BMessage message(kSwitchDirectory);
		message.AddRef("refs", &ref);
		message.AddInt32("action", kActionLocation);
		Window()->PostMessage(&message);
	} else {
		BPath path;

		if (Window() && Window()->TargetModel()) {
			Window()->TargetModel()->GetPath(&path);
			fLocation->SetText(path.Path());
		}
	}
}


void
BNavigator::UpdateLocation(const Model* newmodel, int32 action)
{
	if (newmodel)
		newmodel->GetPath(&fPath);

	// Modify history according to commands
	switch (action) {
		case kActionBackward:
			fForwHistory.AddItem(fBackHistory.RemoveItemAt(
				fBackHistory.CountItems() - 1));
			break;

		case kActionForward:
			fBackHistory.AddItem(fForwHistory.RemoveItemAt(
				fForwHistory.CountItems() - 1));
			break;

		case kActionUpdatePath:
			break;

		default:
			fForwHistory.MakeEmpty();
			fBackHistory.AddItem(new BPath(fPath));

			while (fBackHistory.CountItems() > kMaxHistory)
				fBackHistory.RemoveItem(fBackHistory.FirstItem(), true);
			break;
	}

	// Enable Up button when there is any parent
	BEntry entry;
	if (entry.SetTo(fPath.Path()) == B_OK) {
		BEntry parentEntry;
		bool enable = entry.GetParent(&parentEntry) == B_OK;
		SetActionEnabled(kNavigatorCommandUp, enable);
	}

	// Enable history buttons if history contains something
	SetActionEnabled(kNavigatorCommandForward, fForwHistory.CountItems() > 0);
	SetActionEnabled(kNavigatorCommandBackward, fBackHistory.CountItems() > 1);

	// Avoid loss of selection and cursor position
	if (action != kActionLocation)
		fLocation->SetText(fPath.Path());
}
