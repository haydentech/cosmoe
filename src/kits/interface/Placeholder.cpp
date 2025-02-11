/*
 * Copyright 2001-2022 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		DarkWyrm, bpmagic@columbus.rr.com
 *		Axel Dörfler, axeld@pinc-software.de
 *		Marc Flerackers, mflerackers@androme.be
 *		John Scipione, jscipione@gmail.com
 */


#include <Placeholder.h>
//#include <Alignment.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



BPlaceholder::BPlaceholder(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
	:
	BView(frame, name, resizingMode, flags | B_WILL_DRAW | B_FRAME_EVENTS)
{
	fBounds = Bounds().OffsetToCopy(0, 0);
}



BPlaceholder::~BPlaceholder()
{
}


void
BPlaceholder::Draw(BRect updateRect)
{
	BRect rect(Bounds());

	rgb_color light = tint_color(ViewColor(), B_LIGHTEN_1_TINT);
	rgb_color shadow = tint_color(ViewColor(), B_DARKEN_1_TINT);

	BeginLineArray(6);
		AddLine(BPoint(rect.left, rect.bottom),
				BPoint(rect.left, rect.top), light);
		AddLine(BPoint(rect.left + 1.0f, rect.top),
				BPoint(rect.right, rect.top), light);
		AddLine(BPoint(rect.left + 1.0f, rect.bottom),
				BPoint(rect.right, rect.bottom), shadow);
		AddLine(BPoint(rect.right, rect.bottom - 1.0f),
				BPoint(rect.right, rect.top + 1.0f), shadow);

		AddLine(BPoint(rect.right, rect.bottom - 1.0f),
				BPoint(rect.left, rect.top + 1.0f), shadow);
		AddLine(BPoint(rect.right, rect.top - 1.0f),
				BPoint(rect.left, rect.bottom + 1.0f), shadow);
	EndLineArray();
}


// void
// BPlaceholder::FrameResized(float width, float height)
// {
	// BRect bounds(Bounds());

	// // invalidate the regions that the app_server did not
	// // (for removing the previous or drawing the new border)
	// if (fStyle != B_NO_BORDER) {
	// 	// TODO: this must be made part of the be_control_look stuff!
	// 	int32 borderSize = fStyle == B_PLAIN_BORDER ? 0 : 2;

	// 	// Horizontal
	// 	BRect invalid(bounds);
	// 	if (fBounds.Width() < bounds.Width()) {
	// 		// enlarging
	// 		invalid.left = bounds.left + fBounds.right - borderSize;
	// 		invalid.right = bounds.left + fBounds.right;

	// 		Invalidate(invalid);
	// 	} else if (fBounds.Width() > bounds.Width()) {
	// 		// shrinking
	// 		invalid.left = bounds.left + bounds.right - borderSize;

	// 		Invalidate(invalid);
	// 	}

	// 	// Vertical
	// 	invalid = bounds;
	// 	if (fBounds.Height() < bounds.Height()) {
	// 		// enlarging
	// 		invalid.top = bounds.top + fBounds.bottom - borderSize;
	// 		invalid.bottom = bounds.top + fBounds.bottom;

	// 		Invalidate(invalid);
	// 	} else if (fBounds.Height() > bounds.Height()) {
	// 		// shrinking
	// 		invalid.top = bounds.top + bounds.bottom - borderSize;

	// 		Invalidate(invalid);
	// 	}
	// }

	// fBounds.right = width;
	// fBounds.bottom = height;
//}



