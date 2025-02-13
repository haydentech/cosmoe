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


#include <Box.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ControlLook.h>
#include <Layout.h>
#include <LayoutUtils.h>
#include <Region.h>

//#include <binary_compatibility/Interface.h>


struct BBox::LayoutData {
	LayoutData()
		: valid(false)
	{
	}

	BRect	label_box;		// label box (label string or label view); in case
							// of a label string not including descent
	BRect	insets;			// insets induced by border and label
	BSize	min;
	BSize	max;
	BSize	preferred;
	BAlignment alignment;
	bool	valid;			// validity the other fields
};


BBox::BBox(BRect frame, const char* name, uint32 resizingMode, uint32 flags,
		border_style border)
	:
	BView(frame, name, resizingMode, flags  | B_WILL_DRAW | B_FRAME_EVENTS),
	  fStyle(border)
{
	_InitObject();
}


BBox::BBox(const char* name, uint32 flags, border_style border, BView* child)
	:
	BView(name, flags | B_WILL_DRAW | B_FRAME_EVENTS),
	fStyle(border)
{
	_InitObject();

	if (child)
		AddChild(child);
}


BBox::BBox(border_style border, BView* child)
	:
	BView(NULL, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE_JUMP),
	fStyle(border)
{
	_InitObject();

	if (child)
		AddChild(child);
}



BBox::~BBox()
{
	_ClearLabel();

	delete fLayoutData;
}


void
BBox::SetBorder(border_style border)
{
	if (border == fStyle)
		return;

	fStyle = border;

	InvalidateLayout();

	if (Window() != NULL && LockLooper()) {
		Invalidate();
		UnlockLooper();
	}
}


border_style
BBox::Border() const
{
	return fStyle;
}


//! This function is not part of the R5 API and is not yet finalized yet
float
BBox::TopBorderOffset()
{
	_ValidateLayoutData();

	if (fLabel != NULL || fLabelView != NULL)
		return fLayoutData->label_box.Height() / 2;

	return 0;
}


//! This function is not part of the R5 API and is not yet finalized yet
BRect
BBox::InnerFrame()
{
	_ValidateLayoutData();

	BRect frame(Bounds());
	frame.left += fLayoutData->insets.left;
	frame.top += fLayoutData->insets.top;
	frame.right -= fLayoutData->insets.right;
	frame.bottom -= fLayoutData->insets.bottom;

	return frame;
}


void
BBox::SetLabel(const char* string)
{
	_ClearLabel();

	if (string)
		fLabel = strdup(string);

	InvalidateLayout();

	if (Window())
		Invalidate();
}


status_t
BBox::SetLabel(BView* viewLabel)
{
	_ClearLabel();

	if (viewLabel) {
		fLabelView = viewLabel;
		fLabelView->MoveTo(10.0f, 0.0f);
		AddChild(fLabelView, ChildAt(0));
	}

	InvalidateLayout();

	if (Window())
		Invalidate();

	return B_OK;
}


const char*
BBox::Label() const
{
	return fLabel;
}


BView*
BBox::LabelView() const
{
	return fLabelView;
}


void
BBox::Draw(BRect updateRect)
{
	_ValidateLayoutData();

	PushState();

	BRect labelBox = BRect(0, 0, 0, 0);
	if (fLabel != NULL) {
		labelBox = fLayoutData->label_box;
		BRegion update(updateRect);
		update.Exclude(labelBox);

		ConstrainClippingRegion(&update);
	} else if (fLabelView != NULL)
		labelBox = fLabelView->Bounds();

	switch (fStyle) {
		case B_FANCY_BORDER:
			_DrawFancy(labelBox);
			break;

		case B_PLAIN_BORDER:
			_DrawPlain(labelBox);
			break;

		default:
			break;
	}

	if (fLabel != NULL) {
		ConstrainClippingRegion(NULL);

		font_height fontHeight;
		GetFontHeight(&fontHeight);

		// offset label up by 1/6 the font height
		float lineHeight = fontHeight.ascent + fontHeight.descent;
		float yOffset = roundf(lineHeight / 6.0f);

		SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		DrawString(fLabel, BPoint(10.0f, fontHeight.ascent - yOffset));
	}

	PopState();
}


void
BBox::AttachedToWindow()
{
	AdoptParentColors();

	// Force low color to match view color for proper label drawing.
	float viewTint = B_NO_TINT;
	float lowTint = B_NO_TINT;

	if (LowUIColor(&lowTint) != ViewUIColor(&viewTint) || viewTint != lowTint)
		SetLowUIColor(ViewUIColor(), viewTint);
	else if (LowColor() != ViewColor())
		SetLowColor(ViewColor());

	if (ViewColor() == B_TRANSPARENT_COLOR)
		AdoptSystemColors();

	// The box could have been resized in the mean time
	fBounds = Bounds().OffsetToCopy(0, 0);
}


void
BBox::DetachedFromWindow()
{
	BView::DetachedFromWindow();
}


void
BBox::AllAttached()
{
	BView::AllAttached();
}


void
BBox::AllDetached()
{
	BView::AllDetached();
}


void
BBox::FrameResized(float width, float height)
{
	BRect bounds(Bounds());

	// invalidate the regions that the app_server did not
	// (for removing the previous or drawing the new border)
	if (fStyle != B_NO_BORDER) {
		// TODO: this must be made part of the be_control_look stuff!
		int32 borderSize = fStyle == B_PLAIN_BORDER ? 0 : 2;

		// Horizontal
		BRect invalid(bounds);
		if (fBounds.Width() < bounds.Width()) {
			// enlarging
			invalid.left = bounds.left + fBounds.right - borderSize;
			invalid.right = bounds.left + fBounds.right;

			Invalidate(invalid);
		} else if (fBounds.Width() > bounds.Width()) {
			// shrinking
			invalid.left = bounds.left + bounds.right - borderSize;

			Invalidate(invalid);
		}

		// Vertical
		invalid = bounds;
		if (fBounds.Height() < bounds.Height()) {
			// enlarging
			invalid.top = bounds.top + fBounds.bottom - borderSize;
			invalid.bottom = bounds.top + fBounds.bottom;

			Invalidate(invalid);
		} else if (fBounds.Height() > bounds.Height()) {
			// shrinking
			invalid.top = bounds.top + bounds.bottom - borderSize;

			Invalidate(invalid);
		}
	}

	fBounds.right = width;
	fBounds.bottom = height;
}




void
BBox::MouseDown(BPoint point)
{
	BView::MouseDown(point);
}


void
BBox::MouseUp(BPoint point)
{
	BView::MouseUp(point);
}


void
BBox::WindowActivated(bool active)
{
	BView::WindowActivated(active);
}




void
BBox::FrameMoved(BPoint newLocation)
{
	BView::FrameMoved(newLocation);
}



void
BBox::ResizeToPreferred()
{
	float width, height;
	GetPreferredSize(&width, &height);

	// make sure the box don't get smaller than it already is
	if (width < Bounds().Width())
		width = Bounds().Width();
	if (height < Bounds().Height())
		height = Bounds().Height();

	BView::ResizeTo(width, height);
}


void
BBox::GetPreferredSize(float* _width, float* _height)
{
	_ValidateLayoutData();

	if (_width)
		*_width = fLayoutData->preferred.width;
	if (_height)
		*_height = fLayoutData->preferred.height;
}


void
BBox::MakeFocus(bool focused)
{
	BView::MakeFocus(focused);
}



BSize
BBox::MinSize()
{
	_ValidateLayoutData();

	BSize size = (GetLayout() ? GetLayout()->MinSize() : fLayoutData->min);
	if (size.width < fLayoutData->min.width)
		size.width = fLayoutData->min.width;
	return BLayoutUtils::ComposeSize(ExplicitMinSize(), size);
}


BSize
BBox::MaxSize()
{
	_ValidateLayoutData();

	BSize size = (GetLayout() ? GetLayout()->MaxSize() : fLayoutData->max);
	return BLayoutUtils::ComposeSize(ExplicitMaxSize(), size);
}


BSize
BBox::PreferredSize()
{
	_ValidateLayoutData();

	BSize size = (GetLayout() ? GetLayout()->PreferredSize()
		: fLayoutData->preferred);
	return BLayoutUtils::ComposeSize(ExplicitPreferredSize(), size);
}


BAlignment
BBox::LayoutAlignment()
{
	_ValidateLayoutData();

	BAlignment alignment = (GetLayout() ? GetLayout()->Alignment()
			: fLayoutData->alignment);
	return BLayoutUtils::ComposeAlignment(ExplicitAlignment(), alignment);
}


void
BBox::LayoutInvalidated(bool descendants)
{
	fLayoutData->valid = false;
}


void
BBox::DoLayout()
{
	// Bail out, if we shan't do layout.
	if (!(Flags() & B_SUPPORTS_LAYOUT))
		return;

	BLayout* layout = GetLayout();

	// If the user set a layout, let the base class version call its
	// hook. In case when we have BView as a label, remove it from child list
	// so it won't be layouted with the rest of views and add it again
	// after that.
	if (layout != NULL) {
		if (fLabelView)
			RemoveChild(fLabelView);

		BView::DoLayout();

		if (fLabelView != NULL) {
			DisableLayoutInvalidation();
				// don't trigger a relayout
			AddChild(fLabelView, ChildAt(0));
			EnableLayoutInvalidation();
		}
	}

	_ValidateLayoutData();

	// Even if the user set a layout, restore label view to it's
	// desired position.

	// layout the label view
	if (fLabelView != NULL) {
		fLabelView->MoveTo(fLayoutData->label_box.LeftTop());
		fLabelView->ResizeTo(fLayoutData->label_box.Size());
	}

	// If we have layout return here and do not layout the child
	if (layout != NULL)
		return;

	// layout the child
	BView* child = _Child();
	if (child != NULL) {
		BRect frame(Bounds());
		frame.left += fLayoutData->insets.left;
		frame.top += fLayoutData->insets.top;
		frame.right -= fLayoutData->insets.right;
		frame.bottom -= fLayoutData->insets.bottom;

		if ((child->Flags() & B_SUPPORTS_LAYOUT) != 0)
			BLayoutUtils::AlignInFrame(child, frame);
		else
			child->MoveTo(frame.LeftTop());
	}
}


void BBox::_ReservedBox1() {}
void BBox::_ReservedBox2() {}


BBox &
BBox::operator=(const BBox &)
{
	return *this;
}


void
BBox::_InitObject()
{
	fBounds = Bounds().OffsetToCopy(0, 0);

	fLabel = NULL;
	fLabelView = NULL;
	fLayoutData = new LayoutData;

	int32 flags = 0;

	BFont font(be_bold_font);

	// flags = B_FONT_FAMILY_AND_STYLE;

	// flags |= B_FONT_SIZE;

	if (flags != 0)
		SetFont(&font, flags);

	AdoptSystemColors();
}


void
BBox::_DrawPlain(BRect labelBox)
{
	BRect rect = Bounds();
	rect.top += TopBorderOffset();

	float lightTint;
	float shadowTint;
	lightTint = B_LIGHTEN_1_TINT;
	shadowTint = B_DARKEN_1_TINT;

	if (rect.Height() == 0.0 || rect.Width() == 0.0) {
		// used as separator
		rgb_color shadow = tint_color(ViewColor(), B_DARKEN_2_TINT);

		SetHighColor(shadow);
		StrokeLine(rect.LeftTop(),rect.RightBottom());
	} else {
		// used as box
		rgb_color light = tint_color(ViewColor(), lightTint);
		rgb_color shadow = tint_color(ViewColor(), shadowTint);

		BeginLineArray(4);
			AddLine(BPoint(rect.left, rect.bottom),
					BPoint(rect.left, rect.top), light);
			AddLine(BPoint(rect.left + 1.0f, rect.top),
					BPoint(rect.right, rect.top), light);
			AddLine(BPoint(rect.left + 1.0f, rect.bottom),
					BPoint(rect.right, rect.bottom), shadow);
			AddLine(BPoint(rect.right, rect.bottom - 1.0f),
					BPoint(rect.right, rect.top + 1.0f), shadow);
		EndLineArray();
	}
}


void
BBox::_DrawFancy(BRect labelBox)
{
	BRect rect = Bounds();
	rect.top += TopBorderOffset();

	rgb_color base = ViewColor();
	if (rect.Height() == 1.0) {
		// used as horizontal separator
		be_control_look->DrawGroupFrame(this, rect, rect, base,
			BControlLook::B_TOP_BORDER);
	} else if (rect.Width() == 1.0) {
		// used as vertical separator
		be_control_look->DrawGroupFrame(this, rect, rect, base,
			BControlLook::B_LEFT_BORDER);
	} else {
		// used as box
		be_control_look->DrawGroupFrame(this, rect, rect, base);
	}
}


void
BBox::_ClearLabel()
{
	if (fLabel) {
		free(fLabel);
		fLabel = NULL;
	} else if (fLabelView) {
		fLabelView->RemoveSelf();
		delete fLabelView;
		fLabelView = NULL;
	}
}


BView*
BBox::_Child() const
{
	for (int32 i = 0; BView* view = ChildAt(i); i++) {
		if (view != fLabelView)
			return view;
	}

	return NULL;
}


void
BBox::_ValidateLayoutData()
{
	if (fLayoutData->valid)
		return;

	// compute the label box, width and height
	bool label = true;
	float labelHeight = 0;	// height of the label (pixel count)
	if (fLabel) {
		// leave 6 pixels of the frame, and have a gap of 4 pixels between
		// the frame and the text on either side
		font_height fontHeight;
		GetFontHeight(&fontHeight);
		fLayoutData->label_box.Set(6.0f, 0, 14.0f + StringWidth(fLabel),
			ceilf(fontHeight.ascent));
		labelHeight = ceilf(fontHeight.ascent + fontHeight.descent) + 1;
	} else if (fLabelView) {
		// the label view is placed at (0, 10) at its preferred size
		BSize size = fLabelView->PreferredSize();
		fLayoutData->label_box.Set(10, 0, 10 + size.width, size.height);
		labelHeight = size.height + 1;
	} else
		label = false;

	// border
	switch (fStyle) {
		case B_PLAIN_BORDER:
			fLayoutData->insets.Set(1, 1, 1, 1);
			break;
		case B_FANCY_BORDER:
			fLayoutData->insets.Set(3, 3, 3, 3);
			break;
		case B_NO_BORDER:
		default:
			fLayoutData->insets.Set(0, 0, 0, 0);
			break;
	}

	// if there's a label, the top inset will be dictated by the label
	if (label && labelHeight > fLayoutData->insets.top)
		fLayoutData->insets.top = labelHeight;

	// total number of pixel the border adds
	float addWidth = fLayoutData->insets.left + fLayoutData->insets.right;
	float addHeight = fLayoutData->insets.top + fLayoutData->insets.bottom;

	// compute the minimal width induced by the label
	float minWidth;
	if (label)
		minWidth = fLayoutData->label_box.right + fLayoutData->insets.right;
	else
		minWidth = addWidth - 1;

	BAlignment alignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER);

	// finally consider the child constraints, if we shall support layout
	BView* child = _Child();
	if (child && (child->Flags() & B_SUPPORTS_LAYOUT)) {
		BSize min = child->MinSize();
		BSize max = child->MaxSize();
		BSize preferred = child->PreferredSize();

		min.width += addWidth;
		min.height += addHeight;
		preferred.width += addWidth;
		preferred.height += addHeight;
		max.width = BLayoutUtils::AddDistances(max.width, addWidth - 1);
		max.height = BLayoutUtils::AddDistances(max.height, addHeight - 1);

		if (min.width < minWidth)
			min.width = minWidth;
		BLayoutUtils::FixSizeConstraints(min, max, preferred);

		fLayoutData->min = min;
		fLayoutData->max = max;
		fLayoutData->preferred = preferred;

		BAlignment childAlignment = child->LayoutAlignment();
		if (childAlignment.horizontal == B_ALIGN_USE_FULL_WIDTH)
			alignment.horizontal = B_ALIGN_USE_FULL_WIDTH;
		if (childAlignment.vertical == B_ALIGN_USE_FULL_HEIGHT)
			alignment.vertical = B_ALIGN_USE_FULL_HEIGHT;

		fLayoutData->alignment = alignment;
	} else {
		fLayoutData->min.Set(minWidth, addHeight - 1);
		fLayoutData->max.Set(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED);
		fLayoutData->preferred = fLayoutData->min;
		fLayoutData->alignment = alignment;
	}

	fLayoutData->valid = true;
	ResetLayoutInvalidation();
}

