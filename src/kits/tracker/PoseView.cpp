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


#include "PoseView.h"

#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <InfoWindow.h>

#include "Commands.h"
#include "WidthBuffer.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PoseView"


//	#pragma mark - BPoseView


BPoseView::BPoseView(Model* model, uint32 viewMode)
	:  BColumnListView("",0),
	fSelectionHandler(be_app),
	fModel(model),
	fSelectionList(new PoseList()),
	fRefFilter(NULL),
	fLastKeyTime(0),
	fSelectionVisible(true),
	fSelectionChangedHook(false)
{
}


BPoseView::~BPoseView()
{
	delete fSelectionList;
	delete fModel;
}


void
BPoseView::Init(const BMessage &message)
{
}


void
BPoseView::AdoptSystemColors()
{
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

	SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
}


bool
BPoseView::HasSystemColors() const
{
	float tint = B_NO_TINT;
	float readOnlyTint = ReadOnlyTint(B_DOCUMENT_BACKGROUND_COLOR);

	return ViewUIColor(&tint) == B_DOCUMENT_BACKGROUND_COLOR
		&& (tint == B_NO_TINT || tint == readOnlyTint)
		&& LowUIColor(&tint) == B_DOCUMENT_BACKGROUND_COLOR
		&& (tint == B_NO_TINT || tint == readOnlyTint)
		&& HighUIColor(&tint) == B_DOCUMENT_TEXT_COLOR && tint == B_NO_TINT;
}


float
BPoseView::StringWidth(const char* str) const
{
	return BPrivate::gWidthBuffer->StringWidth(str, 0, (int32)strlen(str),
		be_plain_font);
}


float
BPoseView::StringWidth(const char* str, int32 len) const
{
	ASSERT(strlen(str) == (uint32)len);

	return BPrivate::gWidthBuffer->StringWidth(str, 0, len, be_plain_font);
}


void
BPoseView::AttachedToWindow()
{
	AdoptSystemColors();

	_inherited::AttachedToWindow();
}


void
BPoseView::MakeFocus(bool focused)
{
	bool invalidate = false;
	if (focused != IsFocus())
		invalidate = true;

	_inherited::MakeFocus(focused);

	if (invalidate) {
		BorderedView* view = dynamic_cast<BorderedView*>(Parent());
		if (view != NULL)
			view->PoseViewFocused(focused);
	}
}


BSize
BPoseView::MinSize()
{
	// Between the BTitleView, BCountView, and scrollbars,
	// we don't need any extra room.
	return BSize(0, 0);
}


void
BPoseView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kOpenSelection:
			OpenSelection();
			break;

		case kGetInfo:
			OpenInfoWindows();
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
BPoseView::SelectAll()
{
	// clear selection list
	fSelectionList->MakeEmpty();

	int32 startIndex = 0;

	PoseList* poseList = CurrentPoseList();
	int32 poseCount = poseList->CountItems();
	for (int32 index = startIndex; index < poseCount; index++) {
		BPose* pose = poseList->ItemAt(index);
		fSelectionList->AddItem(pose);
	}

	if (fSelectionChangedHook)
		ContainerWindow()->SelectionChanged();
}


void
BPoseView::OpenSelection()
{
	BPose* singleWindowBrowsePose = NULL;

	// get first selected pose in selection if none was clicked
	if (CountSelected() == 1) {
		singleWindowBrowsePose = fSelectionList->ItemAt(0);
	}

	if (singleWindowBrowsePose && singleWindowBrowsePose->ResolvedModel()
		&& singleWindowBrowsePose->ResolvedModel()->IsDirectory()) {
		// Switch to new directory
		BMessage msg(kSwitchDirectory);
		msg.AddRef("refs", singleWindowBrowsePose->ResolvedModel()->EntryRef());
		Window()->PostMessage(&msg);
	} else {
		// otherwise use standard method
		OpenSelectionCommon();
	}
}


void
BPoseView::OpenSelectionCommon()
{
	int32 selectCount = CountSelected();
	if (selectCount == 0)
		return;

	BMessage message(B_REFS_RECEIVED);

	for (int32 index = 0; index < selectCount; index++) {
		BPose* pose = fSelectionList->ItemAt(index);
		message.AddRef("refs", pose->TargetModel()->EntryRef());
	}

	// add a messenger to the launch message that will be used to
	// dispatch scripting calls from apps to the PoseView
	message.AddMessenger("TrackerViewToken", BMessenger(this));

	if (fSelectionHandler)
		fSelectionHandler->PostMessage(&message);
}


void
BPoseView::SwitchDir(const entry_ref* newDirRef)
{
	ASSERT(TargetModel() != NULL);
	if (*newDirRef == *TargetModel()->EntryRef())
		// no change
		return;

	Model* model = new Model(newDirRef, true);
	if (model->InitCheck() != B_OK || !model->IsDirectory()) {
		delete model;
		return;
	}

	delete fModel;
	fModel = model;

	AdoptSystemColors();

	// Refresh the ColumnListView
	Refresh();

	Invalidate();

	fLastKeyTime = 0;
}


void
BPoseView::OpenInfoWindows()
{
	int32 selectCount = CountSelected();
	if (selectCount <= 0)
		return;

	if (fSelectionList == NULL)
		return;

	for (int32 index = 0; index < selectCount; index++) {
		BPose* pose = fSelectionList->ItemAt(index);
		entry_ref ref;
		BEntry entry;
		if (entry.SetTo(&ref) == B_OK) {
			Model* model = new Model(&entry);
			if (model->InitCheck() != B_OK) {
				delete model;
				continue;
			}

			BInfoWindow* wind = new BInfoWindow(pose->TargetModel(), index);
		}
	}
}


void
BPoseView::ClearSelection()
{
	fSelectionList->MakeEmpty();
}


void
BPoseView::ShowSelection(bool show)
{
	if (fSelectionVisible == show)
		return;

	fSelectionVisible = show;

	if (CountSelected() <= 0)
		return;

	// Do other stuff we don't care about yet
}


bool
BPoseView::Represents(const entry_ref* ref) const
{
	return *fModel->EntryRef() == *ref;
}


void
BPoseView::Refresh()
{
	Invalidate();
}
