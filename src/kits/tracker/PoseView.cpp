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


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PoseView"


//	#pragma mark - BPoseView


BPoseView::BPoseView(Model* model, uint32 viewMode)
	:  BColumnListView("",0),
	fModel(model),
	fRefFilter(NULL),
	fLastKeyTime(0)
{
}


BPoseView::~BPoseView()
{
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



void
BPoseView::AttachedToWindow()
{
	AdoptSystemColors();

	BView::AttachedToWindow();
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
