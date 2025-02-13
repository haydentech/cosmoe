/*
 * Copyright 2001-2019 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		Axel Dörfler, axeld@pinc-software.de
 *		Adrian Oanca, adioanca@cotty.iren.ro
 *		Ingo Weinhold. ingo_weinhold@gmx.de
 *		Julian Harnath, julian.harnath@rwth-aachen.de
 *		Joseph Groover, looncraz@looncraz.net
 */


#include <View.h>

#include <algorithm>
#include <new>

#include <math.h>
#include <stdio.h>

#include <Application.h>
//#include <Bitmap.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientRadialFocus.h>
#include <GradientDiamond.h>
#include <GradientConic.h>
#include <InterfaceDefs.h>
#include <Layout.h>
#include <LayoutContext.h>
#include <LayoutUtils.h>
#include <ObjectList.h>
#include <Point.h>
#include <Region.h>
#include <Shape.h>
#include <String.h>
#include <Window.h>

#include <ShapePrivate.h>
#include <ViewPrivate.h>

#include <pango/pango-layout.h>
#include <pango/pangocairo.h>

static double rgb_to_cairo_color(uint8_t rgb) {
    return (double)rgb / 255.0;
}

static cairo_operator_t drawing_mode_to_cairo_operator(drawing_mode mode)
{
	switch(mode)
	{
		case B_OP_SELECT:
			// Unhandled, fall through
		case B_OP_COPY:
			return CAIRO_OPERATOR_SOURCE;
		case B_OP_OVER:
			return CAIRO_OPERATOR_OVER;
		case B_OP_ERASE:
			return CAIRO_OPERATOR_CLEAR;
		case B_OP_ADD:
			return CAIRO_OPERATOR_ADD;
		case B_OP_SUBTRACT:
			return CAIRO_OPERATOR_DIFFERENCE;
		case B_OP_BLEND:
			return CAIRO_OPERATOR_OVERLAY;
		case B_OP_MIN:
			return CAIRO_OPERATOR_LIGHTEN;
		case B_OP_MAX:
			return CAIRO_OPERATOR_DARKEN;
		case B_OP_ALPHA:
			return CAIRO_OPERATOR_ATOP;
		case B_OP_INVERT:
			// This will work with the addition of a white source
			// e.g. cairo_set_source_rgb (cr, 1., 1., 1.);
			return CAIRO_OPERATOR_DIFFERENCE;
	}

	return CAIRO_OPERATOR_SOURCE;
}


using std::nothrow;

#define DEBUG_BVIEW
#ifdef DEBUG_BVIEW
#	include <stdio.h>
#	define STRACE(x) printf x
#	define BVTRACE _PrintToStream()
#else
#	define STRACE(x) ;
#	define BVTRACE ;
#endif

#define WAYLAND_TOPVIEW_H_SLOP 39
#define WAYLAND_TOPVIEW_V_SLOP 60

static void
view_redraw_handler(struct widget *widget, void *data)
{
    //printf("view_redraw_handler\n");
    BView* view = (BView*)data;
    view->Draw(view->Bounds());
}

// void
// view_resize_handler(struct widget *widget, int32_t width, int32_t height, void *data)
// {
//     printf("view_resize_handler\n");
//     BView* view = (BView*)data;
//     view->_ResizeBy(width - view->Bounds().IntegerWidth(), height - view->Bounds().IntegerHeight());
// }

static void
set_empty_input_region(struct widget *widget, struct display *display)
{
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	struct wl_region *region;

	compositor = display_get_compositor(display);
	surface = widget_get_wl_surface(widget);
	region = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(surface, region);
	wl_region_destroy(region);
}


//	#pragma mark -


namespace BPrivate {

ViewState::ViewState()
{
	pen_location.Set(0, 0);
	pen_size = 1.0;

	// NOTE: the clipping_region is empty
	// on construction but it is not used yet,
	// we avoid having to keep track of it via
	// this flag
	clipping_region_used = false;

	high_color = (rgb_color){ 0, 0, 0, 255 };
	low_color = (rgb_color){ 255, 255, 255, 255 };
	view_color = low_color;
	which_view_color = B_NO_COLOR;
	which_view_color_tint = B_NO_TINT;

	which_high_color = B_NO_COLOR;
	which_high_color_tint = B_NO_TINT;

	which_low_color = B_NO_COLOR;
	which_low_color_tint = B_NO_TINT;

	pattern = B_SOLID_HIGH;
	drawing_mode = B_OP_COPY;

	origin.Set(0, 0);

	line_join = B_MITER_JOIN;
	line_cap = B_BUTT_CAP;
	miter_limit = B_DEFAULT_MITER_LIMIT;
	fill_rule = B_NONZERO;

	alpha_source_mode = B_PIXEL_ALPHA;
	alpha_function_mode = B_ALPHA_OVERLAY;

	scale = 1.0;

	font = *be_plain_font;
	font_flags = font.Flags();
	font_aliasing = false;

	//parent_composite_transform.Reset();
	parent_composite_scale = 1.0f;
	parent_composite_origin.Set(0, 0);

	// We only keep the B_VIEW_CLIP_REGION_BIT flag invalidated,
	// because we should get the clipping region from app_server.
	// The other flags do not need to be included because the data they
	// represent is already in sync with app_server - app_server uses the
	// same init (default) values.
	valid_flags = ~B_VIEW_CLIP_REGION_BIT;

	archiving_flags = B_VIEW_FRAME_BIT | B_VIEW_RESIZE_BIT;
}




}	// namespace BPrivate


//	#pragma mark -


// archiving constants
namespace {
	const char* const kSizesField = "BView:sizes";
		// kSizesField = {min, max, pref}
	const char* const kAlignmentField = "BView:alignment";
	const char* const kLayoutField = "BView:layout";
}


struct BView::LayoutData {
	LayoutData()
		:
		fMinSize(),
		fMaxSize(),
		fPreferredSize(),
		fAlignment(),
		fLayoutInvalidationDisabled(0),
		fLayout(NULL),
		fLayoutContext(NULL),
		fLayoutItems(5, false),
		fLayoutValid(true),		// TODO: Rethink these initial values!
		fMinMaxValid(true),		//
		fLayoutInProgress(false),
		fNeedsRelayout(true)
	{
	}

	// status_t
	// AddDataToArchive(BMessage* archive)
	// {
	// 	status_t err = archive->AddSize(kSizesField, fMinSize);

	// 	if (err == B_OK)
	// 		err = archive->AddSize(kSizesField, fMaxSize);

	// 	if (err == B_OK)
	// 		err = archive->AddSize(kSizesField, fPreferredSize);

	// 	if (err == B_OK)
	// 		err = archive->AddAlignment(kAlignmentField, fAlignment);

	// 	return err;
	// }

	// void
	// PopulateFromArchive(BMessage* archive)
	// {
	// 	archive->FindSize(kSizesField, 0, &fMinSize);
	// 	archive->FindSize(kSizesField, 1, &fMaxSize);
	// 	archive->FindSize(kSizesField, 2, &fPreferredSize);
	// 	archive->FindAlignment(kAlignmentField, &fAlignment);
	// }

	BSize			fMinSize;
	BSize			fMaxSize;
	BSize			fPreferredSize;
	BAlignment		fAlignment;
	int				fLayoutInvalidationDisabled;
	BLayout*		fLayout;
	BLayoutContext*	fLayoutContext;
	BObjectList<BLayoutItem> fLayoutItems;
	bool			fLayoutValid;
	bool			fMinMaxValid;
	bool			fLayoutInProgress;
	bool			fNeedsRelayout;
};


BView::BView(const char* name, uint32 flags, BLayout* layout)
	:
	BHandler(name)
{
	_InitData(BRect(0, 0, -1, -1), name, B_FOLLOW_NONE,
		flags | B_SUPPORTS_LAYOUT);
	SetLayout(layout);
}


BView::BView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
	:
	BHandler(name)
{
	_InitData(frame, name, resizingMode, flags);
}

BView::~BView()
{
	STRACE(("BView(%s)::~BView()\n", this->Name()));

	if (fOwner != NULL) {
		debugger("Trying to delete a view that belongs to a window. "
			"Call RemoveSelf first.");
	}

	// we also delete all our children

	BView* child = fFirstChild;
	while (child) {
		BView* nextChild = child->fNextSibling;

		delete child;
		child = nextChild;
	}

	SetLayout(NULL);
	_RemoveLayoutItemsFromLayout(true);

	delete fLayoutData;

	_RemoveSelf();

	SetName(NULL);

	_RemoveCommArray();
	delete fState;
}


BRect
BView::Bounds() const
{
	_CheckLock();

	if (fIsPrinting)
		return fState->print_rect;

	return fBounds;
}


void
BView::_ConvertToParent(BPoint* point, bool checkLock) const
{
	if (!fParent)
		return;

	if (checkLock)
		_CheckLock();

	// - our scrolling offset
	// + our bounds location within the parent
	point->x += -fBounds.left + fParentOffset.x;
	point->y += -fBounds.top + fParentOffset.y;
}


void
BView::ConvertToParent(BPoint* point) const
{
	_ConvertToParent(point, true);
}


BPoint
BView::ConvertToParent(BPoint point) const
{
	ConvertToParent(&point);

	return point;
}


void
BView::_ConvertFromParent(BPoint* point, bool checkLock) const
{
	if (!fParent)
		return;

	if (checkLock)
		_CheckLock();

	// - our bounds location within the parent
	// + our scrolling offset
	point->x += -fParentOffset.x + fBounds.left;
	point->y += -fParentOffset.y + fBounds.top;
}


void
BView::ConvertFromParent(BPoint* point) const
{
	_ConvertFromParent(point, true);
}


BPoint
BView::ConvertFromParent(BPoint point) const
{
	ConvertFromParent(&point);

	return point;
}


void
BView::ConvertToParent(BRect* rect) const
{
	if (!fParent)
		return;

	_CheckLock();

	// - our scrolling offset
	// + our bounds location within the parent
	rect->OffsetBy(-fBounds.left + fParentOffset.x,
		-fBounds.top + fParentOffset.y);
}


BRect
BView::ConvertToParent(BRect rect) const
{
	ConvertToParent(&rect);

	return rect;
}


void
BView::ConvertFromParent(BRect* rect) const
{
	if (!fParent)
		return;

	_CheckLock();

	// - our bounds location within the parent
	// + our scrolling offset
	rect->OffsetBy(-fParentOffset.x + fBounds.left,
		-fParentOffset.y + fBounds.top);
}


BRect
BView::ConvertFromParent(BRect rect) const
{
	ConvertFromParent(&rect);

	return rect;
}


uint32
BView::Flags() const
{
	_CheckLock();
	return fFlags & ~_RESIZE_MASK_;
}


void
BView::SetFlags(uint32 flags)
{
	if (Flags() == flags)
		return;

	if (fOwner) {
		if (flags & B_PULSE_NEEDED) {
			_CheckLock();
			//if (fOwner->fPulseRunner == NULL)
			//	fOwner->SetPulseRate(fOwner->PulseRate());
		}
	}

	/* Some useful info:
		fFlags is a unsigned long (32 bits)
		* bits 1-16 are used for BView's flags
		* bits 17-32 are used for BView' resize mask
		* _RESIZE_MASK_ is used for that. Look into View.h to see how
			it's defined
	*/
	fFlags = (flags & ~_RESIZE_MASK_) | (fFlags & _RESIZE_MASK_);

	fState->archiving_flags |= B_VIEW_FLAGS_BIT;
}


BRect
BView::Frame() const
{
	return Bounds().OffsetToCopy(fParentOffset.x, fParentOffset.y);
}


void
BView::Hide()
{
	fShowLevel++;

	if (fShowLevel == 1)
		_InvalidateParentLayout();
}


void
BView::Show()
{
	fShowLevel--;

	if (fShowLevel == 0)
		_InvalidateParentLayout();
}


bool
BView::IsFocus() const
{
	if (fOwner) {
		_CheckLock();
		return fOwner->CurrentFocus() == this;
	} else
		return false;
}


bool
BView::IsHidden(const BView* lookingFrom) const
{
	if (fShowLevel > 0)
		return true;

	// may we be egocentric?
	if (lookingFrom == this)
		return false;

	// we have the same visibility state as our
	// parent, if there is one
	if (fParent)
		return fParent->IsHidden(lookingFrom);

	// if we're the top view, and we're interested
	// in the "global" view, we're inheriting the
	// state of the window's visibility
	if (fOwner && lookingFrom == NULL)
		return fOwner->IsHidden();

	return false;
}


bool
BView::IsHidden() const
{
	return IsHidden(NULL);
}


BPoint
BView::LeftTop() const
{
	return Bounds().LeftTop();
}


void
BView::SetResizingMode(uint32 mode)
{
	// look at SetFlags() for more info on the below line
	fFlags = (fFlags & ~_RESIZE_MASK_) | (mode & _RESIZE_MASK_);
}


uint32
BView::ResizingMode() const
{
	return fFlags & _RESIZE_MASK_;
}



void
BView::Flush() const
{
	//if (fOwner)
	//	fOwner->Flush();
}


void
BView::Sync() const
{
	//_CheckOwnerLock();
	//if (fOwner)
	//	fOwner->Sync();
}


BWindow*
BView::Window() const
{
	return fOwner;
}


//	#pragma mark - Hook Functions


void
BView::AttachedToWindow()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::AttachedToWindow()\n", Name()));
}


void
BView::AllAttached()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::AllAttached()\n", Name()));
}


void
BView::DetachedFromWindow()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::DetachedFromWindow()\n", Name()));
}


void
BView::AllDetached()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::AllDetached()\n", Name()));
}


void
BView::Draw(BRect updateRect)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::Draw()\n", Name()));

    // Unlike Haiku, we actually draw the default background here
    if (fTopLevelView) {
        cairo_t *cr;
        rgb_color color = ViewColor();

        cr = widget_cairo_create(view_widget);
        cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                    rgb_to_cairo_color(color.green),
                                    rgb_to_cairo_color(color.blue), 1);
        cairo_paint(cr);
        cairo_destroy(cr);

        // DEBUG: Draw a red X through the view
        // BRect rect(Bounds());

        // rgb_color light = (rgb_color){ 200, 0, 0, 255 };
        // rgb_color shadow = tint_color(light, B_DARKEN_1_TINT);

        // BeginLineArray(6);
        //     AddLine(BPoint(rect.left, rect.bottom),
        //             BPoint(rect.left, rect.top), light);
        //     AddLine(BPoint(rect.left + 1.0f, rect.top),
        //             BPoint(rect.right, rect.top), light);
        //     AddLine(BPoint(rect.left + 1.0f, rect.bottom),
        //             BPoint(rect.right, rect.bottom), shadow);
        //     AddLine(BPoint(rect.right, rect.bottom - 1.0f),
        //             BPoint(rect.right, rect.top + 1.0f), shadow);

        //     AddLine(BPoint(rect.right, rect.bottom - 1.0f),
        //             BPoint(rect.left, rect.top + 1.0f), shadow);
        //     AddLine(BPoint(rect.right, rect.top - 1.0f),
        //             BPoint(rect.left, rect.bottom + 1.0f), shadow);
        // EndLineArray();

        // END DEBUG
    }
}


void
BView::DrawAfterChildren(BRect updateRect)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::DrawAfterChildren()\n", Name()));
}


void
BView::FrameMoved(BPoint newPosition)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::FrameMoved()\n", Name()));
}


void
BView::FrameResized(float newWidth, float newHeight)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::FrameResized()\n", Name()));
}


void
BView::GetPreferredSize(float* _width, float* _height)
{
	STRACE(("\tHOOK: BView(%s)::GetPreferredSize()\n", Name()));

	if (_width != NULL)
		*_width = fBounds.Width();
	if (_height != NULL)
		*_height = fBounds.Height();
}


void
BView::ResizeToPreferred()
{
	STRACE(("\tHOOK: BView(%s)::ResizeToPreferred()\n", Name()));

	float width;
	float height;
	GetPreferredSize(&width, &height);

	ResizeTo(width, height);
}


void
BView::KeyDown(const char* bytes, int32 numBytes)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::KeyDown()\n", Name()));

	//if (Window())
	//	Window()->_KeyboardNavigation();
}


void
BView::KeyUp(const char* bytes, int32 numBytes)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::KeyUp()\n", Name()));
}


void
BView::MouseDown(BPoint where)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::MouseDown()\n", Name()));
}


void
BView::MouseUp(BPoint where)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::MouseUp()\n", Name()));
}


void
BView::Pulse()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::Pulse()\n", Name()));
}

void
BView::WindowActivated(bool active)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::WindowActivated()\n", Name()));
}


//	#pragma mark - Input Functions


void
BView::MakeFocus(bool focus)
{
	if (fOwner == NULL)
		return;

	// TODO: If this view has focus and focus == false,
	// will there really be no other view with focus? No
	// cycling to the next one?
	BView* focusView = fOwner->CurrentFocus();
	if (focus) {
		// Unfocus a previous focus view
		if (focusView != NULL && focusView != this)
			focusView->MakeFocus(false);

		// if we want to make this view the current focus view
		fOwner->_SetFocus(this, true);
	} else {
		// we want to unfocus this view, but only if it actually has focus
		if (focusView == this)
			fOwner->_SetFocus(NULL, true);
	}
}


status_t
BView::SetEventMask(uint32 mask, uint32 options)
{
	if (fEventMask == mask && fEventOptions == options)
		return B_OK;

	// don't change the mask if it's zero and we've got options
	if (mask != 0 || options == 0)
		fEventMask = mask | (fEventMask & 0xffff0000);
	fEventOptions = options;

	fState->archiving_flags |= B_VIEW_EVENT_MASK_BIT;

	return B_OK;
}


uint32
BView::EventMask()
{
	return fEventMask;
}


status_t
BView::SetMouseEventMask(uint32 mask, uint32 options)
{
	// Just don't do anything if the view is not yet attached
	// or we were called outside of BView::MouseDown()
	if (fOwner != NULL) {
		//&& fOwner->CurrentMessage() != NULL
		//&& fOwner->CurrentMessage()->what == B_MOUSE_DOWN) {
		_CheckLockAndSwitchCurrent();
		fMouseEventOptions = options;

		return B_OK;
	}

	return B_ERROR;
}


//	#pragma mark - Graphic State Functions


void
BView::PushState()
{
	//_CheckOwnerLockAndSwitchCurrent();

	fState->valid_flags &= ~B_VIEW_PARENT_COMPOSITE_BIT;

	// initialize origin, scale and transform, new states start "clean".
	fState->valid_flags |= B_VIEW_SCALE_BIT | B_VIEW_ORIGIN_BIT
		| B_VIEW_TRANSFORM_BIT;
	fState->scale = 1.0f;
	fState->origin.Set(0, 0);
	//fState->transform.Reset();
}


void
BView::PopState()
{
	//_CheckOwnerLockAndSwitchCurrent();

	//fOwner->fLink->StartMessage(AS_VIEW_POP_STATE);
	//_FlushIfNotInTransaction();

	// invalidate all flags (except those that are not part of pop/push)
	//fState->valid_flags = B_VIEW_VIEW_COLOR_BIT;
}


void
BView::SetOrigin(BPoint where)
{
	SetOrigin(where.x, where.y);
}


void
BView::SetOrigin(float x, float y)
{
	fState->origin.x = x;
	fState->origin.y = y;

	// our local coord system origin has changed, so when archiving we'll add
	// this too
	fState->archiving_flags |= B_VIEW_ORIGIN_BIT;
}


BPoint
BView::Origin() const
{
	return fState->origin;
}


void
BView::SetScale(float scale) const
{
	fState->scale = scale;
	fState->archiving_flags |= B_VIEW_SCALE_BIT;
}


float
BView::Scale() const
{
	return fState->scale;
}


void
BView::SetLineMode(cap_mode lineCap, join_mode lineJoin, float miterLimit)
{
	fState->line_cap = lineCap;
	fState->line_join = lineJoin;
	fState->miter_limit = miterLimit;

	fState->archiving_flags |= B_VIEW_LINE_MODES_BIT;
}


join_mode
BView::LineJoinMode() const
{
	// This will update the current state, if necessary
	if (!fState->IsValid(B_VIEW_LINE_MODES_BIT))
		LineMiterLimit();

	return fState->line_join;
}


cap_mode
BView::LineCapMode() const
{
	// This will update the current state, if necessary
	if (!fState->IsValid(B_VIEW_LINE_MODES_BIT))
		LineMiterLimit();

	return fState->line_cap;
}


float
BView::LineMiterLimit() const
{
	return fState->miter_limit;
}


void
BView::SetFillRule(int32 fillRule)
{
	fState->fill_rule = fillRule;
}


int32
BView::FillRule() const
{
	return fState->fill_rule;
}


void
BView::SetDrawingMode(drawing_mode mode)
{
	fState->drawing_mode = mode;
}


drawing_mode
BView::DrawingMode() const
{
	return fState->drawing_mode;
}


void
BView::SetBlendingMode(source_alpha sourceAlpha, alpha_function alphaFunction)
{
	if (fState->IsValid(B_VIEW_BLENDING_BIT)
		&& sourceAlpha == fState->alpha_source_mode
		&& alphaFunction == fState->alpha_function_mode)
		return;

	fState->alpha_source_mode = sourceAlpha;
	fState->alpha_function_mode = alphaFunction;

	fState->archiving_flags |= B_VIEW_BLENDING_BIT;
}


void
BView::GetBlendingMode(source_alpha* _sourceAlpha,
	alpha_function* _alphaFunction) const
{
	if (_sourceAlpha)
		*_sourceAlpha = fState->alpha_source_mode;

	if (_alphaFunction)
		*_alphaFunction = fState->alpha_function_mode;
}


void
BView::MovePenTo(BPoint point)
{
	MovePenTo(point.x, point.y);
}


void
BView::MovePenTo(float x, float y)
{
	fState->pen_location.x = x;
	fState->pen_location.y = y;

	fState->archiving_flags |= B_VIEW_PEN_LOCATION_BIT;
}


void
BView::MovePenBy(float x, float y)
{
	// this will update the pen location if necessary
	if (!fState->IsValid(B_VIEW_PEN_LOCATION_BIT))
		PenLocation();

	MovePenTo(fState->pen_location.x + x, fState->pen_location.y + y);
}


BPoint
BView::PenLocation() const
{
	return fState->pen_location;
}


void
BView::SetPenSize(float size)
{
	fState->pen_size = size;
	fState->archiving_flags	|= B_VIEW_PEN_SIZE_BIT;
}


float
BView::PenSize() const
{
	return fState->pen_size;
}


void
BView::SetHighColor(rgb_color color)
{
	SetHighUIColor(B_NO_COLOR);
	fState->high_color = color;

	fState->archiving_flags |= B_VIEW_HIGH_COLOR_BIT;
}


rgb_color
BView::HighColor() const
{
	return fState->high_color;
}


void
BView::SetHighUIColor(color_which which, float tint)
{
	if (fState->IsValid(B_VIEW_WHICH_HIGH_COLOR_BIT)
		&& fState->which_high_color == which
		&& fState->which_high_color_tint == tint)
		return;

	fState->which_high_color = which;
	fState->which_high_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_HIGH_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_HIGH_COLOR_BIT;
		fState->valid_flags |= B_VIEW_HIGH_COLOR_BIT;

		fState->high_color = tint_color(ui_color(which), tint);
	} else {
		fState->valid_flags &= ~B_VIEW_HIGH_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_WHICH_HIGH_COLOR_BIT;
	}
}


color_which
BView::HighUIColor(float* tint) const
{
	if (tint != NULL)
		*tint = fState->which_high_color_tint;

	return fState->which_high_color;
}


void
BView::SetLowColor(rgb_color color)
{
	SetLowUIColor(B_NO_COLOR);

	fState->low_color = color;

	fState->archiving_flags |= B_VIEW_LOW_COLOR_BIT;
}


rgb_color
BView::LowColor() const
{
	return fState->low_color;
}


void
BView::SetLowUIColor(color_which which, float tint)
{
	if (fState->IsValid(B_VIEW_WHICH_LOW_COLOR_BIT)
		&& fState->which_low_color == which
		&& fState->which_low_color_tint == tint)
		return;

	fState->which_low_color = which;
	fState->which_low_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_LOW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_LOW_COLOR_BIT;
		fState->valid_flags |= B_VIEW_LOW_COLOR_BIT;

		fState->low_color = tint_color(ui_color(which), tint);
	} else {
		fState->valid_flags &= ~B_VIEW_LOW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_WHICH_LOW_COLOR_BIT;
	}
}


color_which
BView::LowUIColor(float* tint) const
{
	if (tint != NULL)
		*tint = fState->which_low_color_tint;

	return fState->which_low_color;
}


bool
BView::HasDefaultColors() const
{
	// If we don't have any of these flags, then we have default colors
	uint32 testMask = B_VIEW_VIEW_COLOR_BIT | B_VIEW_HIGH_COLOR_BIT
		| B_VIEW_LOW_COLOR_BIT | B_VIEW_WHICH_VIEW_COLOR_BIT
		| B_VIEW_WHICH_HIGH_COLOR_BIT | B_VIEW_WHICH_LOW_COLOR_BIT;

	return (fState->archiving_flags & testMask) == 0;
}


bool
BView::HasSystemColors() const
{
	return fState->which_view_color == B_PANEL_BACKGROUND_COLOR
		&& fState->which_high_color == B_PANEL_TEXT_COLOR
		&& fState->which_low_color == B_PANEL_BACKGROUND_COLOR
		&& fState->which_view_color_tint == B_NO_TINT
		&& fState->which_high_color_tint == B_NO_TINT
		&& fState->which_low_color_tint == B_NO_TINT;
}


void
BView::AdoptParentColors()
{
	AdoptViewColors(Parent());
}


void
BView::AdoptSystemColors()
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	SetHighUIColor(B_PANEL_TEXT_COLOR);
}


void
BView::AdoptViewColors(BView* view)
{
	if (view == NULL)// || (view->Window() != NULL && !view->LockLooper()))
		return;

	float tint = B_NO_TINT;
	float viewTint = tint;
	color_which viewWhich = view->ViewUIColor(&viewTint);

	// View color
	if (viewWhich != B_NO_COLOR)
		SetViewUIColor(viewWhich, viewTint);
	else
		SetViewColor(view->ViewColor());

	// Low color
	color_which which = view->LowUIColor(&tint);
	if (which != B_NO_COLOR)
		SetLowUIColor(which, tint);
	else if (viewWhich != B_NO_COLOR)
		SetLowUIColor(viewWhich, viewTint);
	else
		SetLowColor(view->LowColor());

	// High color
	which = view->HighUIColor(&tint);
	if (which != B_NO_COLOR)
		SetHighUIColor(which, tint);
	else
		SetHighColor(view->HighColor());

	if (view->Window() != NULL)
		view->UnlockLooper();
}


void
BView::SetViewColor(rgb_color color)
{
	SetViewUIColor(B_NO_COLOR);

	fState->view_color = color;

	fState->archiving_flags |= B_VIEW_VIEW_COLOR_BIT;
}


rgb_color
BView::ViewColor() const
{
	return fState->view_color;
}


void
BView::SetViewUIColor(color_which which, float tint)
{
	if (fState->IsValid(B_VIEW_WHICH_VIEW_COLOR_BIT)
		&& fState->which_view_color == which
		&& fState->which_view_color_tint == tint)
		return;

	fState->which_view_color = which;
	fState->which_view_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_VIEW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_VIEW_COLOR_BIT;
		fState->valid_flags |= B_VIEW_VIEW_COLOR_BIT;

		fState->view_color = tint_color(ui_color(which), tint);
	} else {
		fState->valid_flags &= ~B_VIEW_VIEW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_WHICH_VIEW_COLOR_BIT;
	}

	if (!fState->IsValid(B_VIEW_WHICH_LOW_COLOR_BIT))
		SetLowUIColor(which, tint);
}


color_which
BView::ViewUIColor(float* tint) const
{
	if (tint != NULL)
		*tint = fState->which_view_color_tint;

	return fState->which_view_color;
}


void
BView::ForceFontAliasing(bool enable)
{
	fState->font_aliasing = enable;
	fState->archiving_flags |= B_VIEW_FONT_ALIASING_BIT;
}


void
BView::SetFont(const BFont* font, uint32 mask)
{
	if (!font || mask == 0)
		return;

	if (mask == B_FONT_ALL) {
		fState->font = *font;
	} else {
		// TODO: move this into a BFont method
		if (mask & B_FONT_FAMILY_AND_STYLE)
			fState->font.SetFamilyAndStyle(font->FamilyAndStyle());

		if (mask & B_FONT_SIZE)
			fState->font.SetSize(font->Size());

		if (mask & B_FONT_SHEAR)
			fState->font.SetShear(font->Shear());

		if (mask & B_FONT_ROTATION)
			fState->font.SetRotation(font->Rotation());

		if (mask & B_FONT_FALSE_BOLD_WIDTH)
			fState->font.SetFalseBoldWidth(font->FalseBoldWidth());

		if (mask & B_FONT_SPACING)
			fState->font.SetSpacing(font->Spacing());

		if (mask & B_FONT_ENCODING)
			fState->font.SetEncoding(font->Encoding());

		if (mask & B_FONT_FACE)
			fState->font.SetFace(font->Face());

		if (mask & B_FONT_FLAGS)
			fState->font.SetFlags(font->Flags());
	}

	fState->font_flags |= mask;

	fState->archiving_flags |= B_VIEW_FONT_BIT;
	// TODO: InvalidateLayout() here for convenience?
}


void
BView::GetFont(BFont* font) const
{
	*font = fState->font;
}


void
BView::GetFontHeight(font_height* height) const
{
	fState->font.GetHeight(height);
}


void
BView::SetFontSize(float size)
{
	BFont font;
	font.SetSize(size);

	SetFont(&font, B_FONT_SIZE);
}


float
BView::StringWidth(const char* string) const
{
	return fState->font.StringWidth(string);
}


float
BView::StringWidth(const char* string, int32 length) const
{
	float len =  fState->font.StringWidth(string, length);
	printf("StringWidth: %f\n", len);
	return len;
}


void
BView::GetStringWidths(char* stringArray[], int32 lengthArray[],
	int32 numStrings, float widthArray[]) const
{
	fState->font.GetStringWidths(const_cast<const char**>(stringArray),
		const_cast<const int32*>(lengthArray), numStrings, widthArray);
}


void
BView::TruncateString(BString* string, uint32 mode, float width) const
{
	fState->font.TruncateString(string, mode, width);
}

void
BView::GetClippingRegion(BRegion* region) const
{
	if (!region)
		return;

	// NOTE: the client has no idea when the clipping in the server
	// changed, so it is always read from the server
	region->MakeEmpty();


	// TODO return clip region
}


void
BView::ConstrainClippingRegion(BRegion* region)
{
	// TODO set clip region
}


void
BView::ClipToRect(BRect rect)
{
	_ClipToRect(rect, false);
}


void
BView::ClipToInverseRect(BRect rect)
{
	_ClipToRect(rect, true);
}

//	#pragma mark - Drawing Functions


// void
// BView::DrawBitmapAsync(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect,
// 	uint32 options)
// {
// 	if (bitmap == NULL || fOwner == NULL
// 		|| !bitmapRect.IsValid() || !viewRect.IsValid())
// 		return;

// 	_CheckLockAndSwitchCurrent();

// 	// TODO
// }


// void
// BView::DrawBitmapAsync(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect)
// {
// 	DrawBitmapAsync(bitmap, bitmapRect, viewRect, 0);
// }


// void
// BView::DrawBitmapAsync(const BBitmap* bitmap, BRect viewRect)
// {
// 	if (bitmap && fOwner) {
// 		DrawBitmapAsync(bitmap, bitmap->Bounds().OffsetToCopy(B_ORIGIN),
// 			viewRect, 0);
// 	}
// }


// void
// BView::DrawBitmapAsync(const BBitmap* bitmap, BPoint where)
// {
// 	if (bitmap == NULL || fOwner == NULL)
// 		return;

// 	_CheckLockAndSwitchCurrent();

// 	// TODO
// }


// void
// BView::DrawBitmapAsync(const BBitmap* bitmap)
// {
// 	DrawBitmapAsync(bitmap, PenLocation());
// }


// void
// BView::DrawBitmap(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect,
// 	uint32 options)
// {
// 	if (fOwner) {
// 		DrawBitmapAsync(bitmap, bitmapRect, viewRect, options);
// 		Sync();
// 	}
// }


// void
// BView::DrawBitmap(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect)
// {
// 	if (fOwner) {
// 		DrawBitmapAsync(bitmap, bitmapRect, viewRect, 0);
// 		Sync();
// 	}
// }


// void
// BView::DrawBitmap(const BBitmap* bitmap, BRect viewRect)
// {
// 	if (bitmap && fOwner) {
// 		DrawBitmap(bitmap, bitmap->Bounds().OffsetToCopy(B_ORIGIN), viewRect,
// 			0);
// 	}
// }


// void
// BView::DrawBitmap(const BBitmap* bitmap, BPoint where)
// {
// 	if (fOwner) {
// 		DrawBitmapAsync(bitmap, where);
// 		Sync();
// 	}
// }


// void
// BView::DrawBitmap(const BBitmap* bitmap)
// {
// 	DrawBitmap(bitmap, PenLocation());
// }


void
BView::DrawChar(char c)
{
	DrawString(&c, 1, PenLocation());
}


void
BView::DrawChar(char c, BPoint location)
{
	DrawString(&c, 1, location);
}


void
BView::DrawString(const char* string, escapement_delta* delta)
{
	if (string == NULL)
		return;

	DrawString(string, strlen(string), PenLocation(), delta);
}


void
BView::DrawString(const char* string, BPoint location, escapement_delta* delta)
{
	if (string == NULL)
		return;

	DrawString(string, strlen(string), location, delta);
}


void
BView::DrawString(const char* string, int32 length, escapement_delta* delta)
{
	DrawString(string, length, PenLocation(), delta);
}


void
BView::DrawString(const char* string, int32 length, BPoint location,
	escapement_delta* delta)
{
	if (fOwner == NULL || string == NULL || length < 1)
		return;

	_CheckLockAndSwitchCurrent();

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
    cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));

	/* Create a PangoLayout, set the font and draw the text */
	PangoLayout *layout = pango_cairo_create_layout(cr);

	pango_layout_set_text(layout, string, length);

	PangoFontDescription *desc;
	desc = pango_font_description_from_string("Sans");
	pango_font_description_set_size (desc, fState->font.Size() * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	cairo_move_to(cr, allocation.x + location.x, allocation.y + location.y);
	pango_cairo_show_layout(cr, layout);

	cairo_destroy(cr);

	/* free the layout object */
	g_object_unref (layout);
}


void
BView::DrawString(const char* string, const BPoint* locations,
	int32 locationCount)
{
	if (string == NULL)
		return;

	DrawString(string, strlen(string), locations, locationCount);
}


void
BView::DrawString(const char* string, int32 length, const BPoint* locations,
	int32 locationCount)
{
	if (fOwner == NULL || string == NULL || length < 1 || locations == NULL)
		return;

	// TODO: Draw the strings
}


void
BView::StrokeEllipse(BPoint center, float xRadius, float yRadius,
	::pattern pattern)
{
	StrokeEllipse(BRect(center.x - xRadius, center.y - yRadius,
		center.x + xRadius, center.y + yRadius), pattern);
}


void
BView::StrokeEllipse(BRect rect, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();
	double radius = rect.Width() / 2;

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
    cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));

	cairo_arc(cr, allocation.x + rect.left + radius, allocation.y + rect.top + radius,
				radius, 0, 2*M_PI);
	cairo_stroke_preserve(cr);
	cairo_stroke(cr);

	cairo_destroy(cr);
}


void
BView::FillEllipse(BPoint center, float xRadius, float yRadius,
	::pattern pattern)
{
	FillEllipse(BRect(center.x - xRadius, center.y - yRadius,
		center.x + xRadius, center.y + yRadius), pattern);
}


void
BView::FillEllipse(BPoint center, float xRadius, float yRadius,
	const BGradient& gradient)
{
	FillEllipse(BRect(center.x - xRadius, center.y - yRadius,
		center.x + xRadius, center.y + yRadius), gradient);
}


void
BView::FillEllipse(BRect rect, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();
	double radius = rect.Width() / 2;

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
    cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));

	cairo_arc(cr, allocation.x + rect.left + radius, allocation.y + rect.top + radius,
				radius, 0, 2*M_PI);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);

	cairo_destroy(cr);
}


void
BView::FillEllipse(BRect rect, const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	// TODO -- for now just fill
	//FillEllipse(rect, B_SOLID_HIGH);
}
void
BView::StrokeRect(BRect rect, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
    cairo_rectangle(cr, allocation.x + rect.left, allocation.y + rect.top, rect.IntegerWidth(), rect.IntegerHeight());
    cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_stroke(cr);
    cairo_destroy(cr);
}


void
BView::FillRect(BRect rect, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	// NOTE: ensuring compatibility with R5,
	// invalid rects are not filled, they are stroked though!
	if (!rect.IsValid())
		return;

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
    cairo_rectangle(cr, allocation.x + rect.left, allocation.y + rect.top, rect.IntegerWidth(), rect.IntegerHeight());
    cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_fill(cr);
    cairo_destroy(cr);
}


void
BView::FillRect(BRect rect, const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	// NOTE: ensuring compatibility with R5,
	// invalid rects are not filled, they are stroked though!
	if (!rect.IsValid())
		return;

	// TODO -- for now just fill
	//FillRect(rect, B_SOLID_HIGH);
}


void
BView::StrokeRoundRect(BRect rect, float xRadius, float yRadius,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
	cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));

	double x = rect.left + allocation.x;
	double y = rect.top + allocation.y;
	double w = rect.IntegerWidth();
	double h = rect.IntegerHeight();
	double r = (xRadius + yRadius) / 2; // fudge average radius for now

    cairo_move_to(cr, x+r, y);								// Move to A
    cairo_line_to(cr, x+w-r, y);							// Straight line to B
    cairo_curve_to(cr, x+w, y, x+w, y, x+w, y+r);			// Curve to C, Control points are both at Q
    cairo_line_to(cr, x+w, y+h-r);							// Move to D
    cairo_curve_to(cr, x+w, y+h, x+w, y+h, x+w-r, y+h);		// Curve to E
    cairo_line_to(cr, x+r, y+h);							// Line to F
    cairo_curve_to(cr, x, y+h, x, y+h, x, y+h-r);			// Curve to G
    cairo_line_to(cr, x, y+r);								// Line to H
    cairo_curve_to(cr, x, y, x, y, x+r, y);					// Curve to A
    cairo_stroke(cr);

    cairo_destroy(cr);
}


void
BView::FillRoundRect(BRect rect, float xRadius, float yRadius,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
    rgb_color color = HighColor();

    widget_get_allocation(view_widget, &allocation);

    cr = widget_cairo_create(view_widget);
	cairo_set_source_rgba(cr, rgb_to_cairo_color(color.red),
                                rgb_to_cairo_color(color.green),
                                rgb_to_cairo_color(color.blue),
                                rgb_to_cairo_color(color.alpha));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));

	double x = rect.left + allocation.x;
	double y = rect.top + allocation.y;
	double w = rect.IntegerWidth();
	double h = rect.IntegerHeight();
	double r = (xRadius + yRadius) / 2; // fudge average radius for now
	
    cairo_move_to(cr, x+r, y);								// Move to A
    cairo_line_to(cr, x+w-r, y);							// Straight line to B
    cairo_curve_to(cr, x+w, y, x+w, y, x+w, y+r);			// Curve to C, Control points are both at Q
    cairo_line_to(cr, x+w, y+h-r);							// Move to D
    cairo_curve_to(cr, x+w, y+h, x+w, y+h, x+w-r, y+h);		// Curve to E
    cairo_line_to(cr, x+r, y+h);							// Line to F
    cairo_curve_to(cr, x, y+h, x, y+h, x, y+h-r);			// Curve to G
    cairo_line_to(cr, x, y+r);								// Line to H
    cairo_curve_to(cr, x, y, x, y, x+r, y);					// Curve to A
    cairo_fill(cr);

    cairo_destroy(cr);
}

void
BView::FillRoundRect(BRect rect, float xRadius, float yRadius,
	const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO -- for now just fill
	//FillRoundRect(rect, xRadius, yRadius, B_SOLID_HIGH);
}


void
BView::FillRegion(BRegion* region, ::pattern pattern)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
	rgb_color color = HighColor();

	widget_get_allocation(view_widget, &allocation);

	cr = widget_cairo_create(view_widget);
	cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_set_line_width(cr, fState->pen_size);
	cairo_set_source_rgba(cr,
		rgb_to_cairo_color(color.red),
		rgb_to_cairo_color(color.green),
		rgb_to_cairo_color(color.blue),
		rgb_to_cairo_color(color.alpha));

	uint32 rects = region->CountRects();

	for (uint32 i = 0; i < rects; i++) {
		cairo_rectangle(cr, region->RectAt(i).left + allocation.x,
			region->RectAt(i).top + allocation.y,
			region->RectAt(i).IntegerWidth(),
			region->RectAt(i).IntegerHeight());
	}
	cairo_fill(cr);

	cairo_destroy(cr);
}


void
BView::FillRegion(BRegion* region, const BGradient& gradient)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO -- for now just fill
	//FillRegion(region, B_SOLID_HIGH);
}


void
BView::StrokeTriangle(BPoint point1, BPoint point2, BPoint point3, BRect bounds,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
	rgb_color color = HighColor();

	widget_get_allocation(view_widget, &allocation);

	cr = widget_cairo_create(view_widget);
	cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_set_line_width(cr, fState->pen_size);
	cairo_set_source_rgba(cr,
		rgb_to_cairo_color(color.red),
		rgb_to_cairo_color(color.green),
		rgb_to_cairo_color(color.blue),
		rgb_to_cairo_color(color.alpha));

	cairo_move_to(cr, point1.x + allocation.x, point1.y + allocation.y);
	cairo_line_to(cr, point2.x + allocation.x, point2.y + allocation.y);
	cairo_line_to(cr, point3.x + allocation.x, point3.y + allocation.y);
	cairo_close_path(cr);

	cairo_stroke_preserve(cr);
	cairo_stroke(cr);

	cairo_destroy(cr);
}


void
BView::StrokeTriangle(BPoint point1, BPoint point2, BPoint point3,
	::pattern pattern)
{
	if (fOwner) {
		// we construct the smallest rectangle that contains the 3 points
		// for the 1st point
		BRect bounds(point1, point1);

		// for the 2nd point
		if (point2.x < bounds.left)
			bounds.left = point2.x;

		if (point2.y < bounds.top)
			bounds.top = point2.y;

		if (point2.x > bounds.right)
			bounds.right = point2.x;

		if (point2.y > bounds.bottom)
			bounds.bottom = point2.y;

		// for the 3rd point
		if (point3.x < bounds.left)
			bounds.left = point3.x;

		if (point3.y < bounds.top)
			bounds.top = point3.y;

		if (point3.x > bounds.right)
			bounds.right = point3.x;

		if (point3.y > bounds.bottom)
			bounds.bottom = point3.y;

		StrokeTriangle(point1, point2, point3, bounds, pattern);
	}
}


void
BView::FillTriangle(BPoint point1, BPoint point2, BPoint point3,
	::pattern pattern)
{
	if (fOwner) {
		// we construct the smallest rectangle that contains the 3 points
		// for the 1st point
		BRect bounds(point1, point1);

		// for the 2nd point
		if (point2.x < bounds.left)
			bounds.left = point2.x;

		if (point2.y < bounds.top)
			bounds.top = point2.y;

		if (point2.x > bounds.right)
			bounds.right = point2.x;

		if (point2.y > bounds.bottom)
			bounds.bottom = point2.y;

		// for the 3rd point
		if (point3.x < bounds.left)
			bounds.left = point3.x;

		if (point3.y < bounds.top)
			bounds.top = point3.y;

		if (point3.x > bounds.right)
			bounds.right = point3.x;

		if (point3.y > bounds.bottom)
			bounds.bottom = point3.y;

		FillTriangle(point1, point2, point3, bounds, pattern);
	}
}


void
BView::FillTriangle(BPoint point1, BPoint point2, BPoint point3,
	const BGradient& gradient)
{
	if (fOwner) {
		// we construct the smallest rectangle that contains the 3 points
		// for the 1st point
		BRect bounds(point1, point1);

		// for the 2nd point
		if (point2.x < bounds.left)
			bounds.left = point2.x;

		if (point2.y < bounds.top)
			bounds.top = point2.y;

		if (point2.x > bounds.right)
			bounds.right = point2.x;

		if (point2.y > bounds.bottom)
			bounds.bottom = point2.y;

		// for the 3rd point
		if (point3.x < bounds.left)
			bounds.left = point3.x;

		if (point3.y < bounds.top)
			bounds.top = point3.y;

		if (point3.x > bounds.right)
			bounds.right = point3.x;

		if (point3.y > bounds.bottom)
			bounds.bottom = point3.y;

		FillTriangle(point1, point2, point3, bounds, gradient);
	}
}


void
BView::FillTriangle(BPoint point1, BPoint point2, BPoint point3,
	BRect bounds, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

    cairo_t *cr;
    rectangle allocation;
	rgb_color color = HighColor();

	widget_get_allocation(view_widget, &allocation);

	cr = widget_cairo_create(view_widget);
	cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_set_line_width(cr, fState->pen_size);
	cairo_set_source_rgba(cr,
		rgb_to_cairo_color(color.red),
		rgb_to_cairo_color(color.green),
		rgb_to_cairo_color(color.blue),
		rgb_to_cairo_color(color.alpha));

	cairo_move_to(cr, point1.x + allocation.x, point1.y + allocation.y);
	cairo_line_to(cr, point2.x + allocation.x, point2.y + allocation.y);
	cairo_line_to(cr, point3.x + allocation.x, point3.y + allocation.y);
	cairo_close_path(cr);

	cairo_stroke_preserve(cr);
	cairo_fill(cr);

	cairo_destroy(cr);
}


void
BView::FillTriangle(BPoint point1, BPoint point2, BPoint point3, BRect bounds,
	const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO -- for now just fill
	//FillTriangle(point1, point2, point3, bounds, B_SOLID_HIGH);
}


void
BView::StrokeLine(BPoint toPoint, ::pattern pattern)
{
	StrokeLine(PenLocation(), toPoint, pattern);
}


void
BView::StrokeLine(BPoint start, BPoint end, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

    rgb_color color = HighColor();
    cairo_t *cr;
    rectangle allocation;

	widget_get_allocation(view_widget, &allocation);

	cr = widget_cairo_create(view_widget);
	cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_set_line_width(cr, fState->pen_size);
    cairo_set_source_rgba(cr,
        rgb_to_cairo_color(color.red),
        rgb_to_cairo_color(color.green),
        rgb_to_cairo_color(color.blue),
        rgb_to_cairo_color(color.alpha));

    start += BPoint(allocation.x, allocation.y);
    end += BPoint(allocation.x, allocation.y);
    cairo_move_to(cr, start.x, start.y);
    cairo_line_to(cr, end.x, end.y);
    cairo_stroke(cr);

	cairo_destroy(cr);
}


void
BView::StrokeShape(BShape* shape, ::pattern pattern)
{
	if (shape == NULL || fOwner == NULL)
		return;

	shape_data* sd = (shape_data*)shape->fPrivateData;
	if (sd->opCount == 0 || sd->ptCount == 0)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

	// TODO
}


void
BView::FillShape(BShape* shape, ::pattern pattern)
{
	if (shape == NULL || fOwner == NULL)
		return;

	shape_data* sd = (shape_data*)(shape->fPrivateData);
	if (sd->opCount == 0 || sd->ptCount == 0)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

	// TODO
}


void
BView::FillShape(BShape* shape, const BGradient& gradient)
{
	if (shape == NULL || fOwner == NULL)
		return;

	shape_data* sd = (shape_data*)(shape->fPrivateData);
	if (sd->opCount == 0 || sd->ptCount == 0)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO -- for now just fill
	//FillShape(shape, B_SOLID_HIGH);
}


void
BView::BeginLineArray(int32 count)
{
	if (fOwner == NULL)
		return;

	if (count <= 0)
		debugger("Calling BeginLineArray with a count <= 0");

	_CheckLock();

	if (fCommArray) {
		debugger("Can't nest BeginLineArray calls");
			// not fatal, but it helps during
			// development of your app and is in
			// line with R5...
		delete[] fCommArray->array;
		delete fCommArray;
	}

	// TODO: since this method cannot return failure, and further AddLine()
	//	calls with a NULL fCommArray would drop into the debugger anyway,
	//	we allow the possible std::bad_alloc exceptions here...
	fCommArray = new _array_data_;
	fCommArray->count = 0;

	// Make sure the fCommArray is initialized to reasonable values in cases of
	// bad_alloc. At least the exception can be caught and EndLineArray won't
	// crash.
	fCommArray->array = NULL;
	fCommArray->maxCount = 0;

	fCommArray->array = new ViewLineArrayInfo[count];
	fCommArray->maxCount = count;
}


void
BView::AddLine(BPoint start, BPoint end, rgb_color color)
{
	if (fOwner == NULL)
		return;

	if (!fCommArray)
		debugger("BeginLineArray must be called before using AddLine");

	_CheckLock();

	const uint32 &arrayCount = fCommArray->count;
	if (arrayCount < fCommArray->maxCount) {
		fCommArray->array[arrayCount].startPoint = start;
		fCommArray->array[arrayCount].endPoint = end;
		fCommArray->array[arrayCount].color = color;

		fCommArray->count++;
	}
}


void
BView::EndLineArray()
{
	if (fOwner == NULL)
		return;

	if (fCommArray == NULL)
		debugger("Can't call EndLineArray before BeginLineArray");

    cairo_t *cr;
    rectangle allocation;

	widget_get_allocation(view_widget, &allocation);

	cr = widget_cairo_create(view_widget);
	cairo_set_operator(cr, drawing_mode_to_cairo_operator(DrawingMode()));
    cairo_set_line_width(cr, fState->pen_size);

	for (uint32 i = 0; i < fCommArray->count; i++) {
        cairo_set_source_rgb(cr,
            rgb_to_cairo_color(fCommArray->array[i].color.red),
            rgb_to_cairo_color(fCommArray->array[i].color.green),
            rgb_to_cairo_color(fCommArray->array[i].color.blue));

        BPoint start = fCommArray->array[i].startPoint + BPoint(allocation.x, allocation.y);
        BPoint end = fCommArray->array[i].endPoint + BPoint(allocation.x, allocation.y);
        cairo_move_to(cr, start.x, start.y);
        cairo_line_to(cr, end.x, end.y);
        
	}
	cairo_stroke(cr);

	cairo_destroy(cr);

	_RemoveCommArray();
}

void
BView::Invalidate(BRect invalRect)
{
	if (fOwner == NULL)
		return;

	// NOTE: This rounding of the invalid rect is to stay compatible with BeOS.
	// On the server side, the invalid rect will be converted to a BRegion,
	// which rounds in a different manner, so that it really includes the
	// fractional coordinates of a BRect (ie ceilf(rect.right) &
	// ceilf(rect.bottom)), which is also what BeOS does. So we have to do the
	// different rounding here to stay compatible in both ways.
	invalRect.left = (int)invalRect.left;
	invalRect.top = (int)invalRect.top;
	invalRect.right = (int)invalRect.right;
	invalRect.bottom = (int)invalRect.bottom;
	if (!invalRect.IsValid())
		return;

// 	_CheckLockAndSwitchCurrent();

// 	fOwner->fLink->StartMessage(AS_VIEW_INVALIDATE_RECT);
// 	fOwner->fLink->Attach<BRect>(invalRect);

// // TODO: determine why this check isn't working correctly.
// #if 0
// 	if (!fOwner->fUpdateRequested) {
// 		fOwner->fLink->Flush();
// 		fOwner->fUpdateRequested = true;
// 	}
// #else
// 	fOwner->fLink->Flush();
// #endif
}


void
BView::Invalidate(const BRegion* region)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO
}


void
BView::Invalidate()
{
	Invalidate(Bounds());
}


void
BView::InvertRect(BRect rect)
{
	//TODO
}


//	#pragma mark - View Hierarchy Functions


void
BView::AddChild(BView* child, BView* before)
{
	STRACE(("BView(%s)::AddChild(child '%s', before '%s')\n",
		this->Name(),
		child != NULL && child->Name() ? child->Name() : "NULL",
		before != NULL && before->Name() ? before->Name() : "NULL"));

	if (!_AddChild(child, before))
		return;

	if (fLayoutData->fLayout)
		fLayoutData->fLayout->AddView(child);
}


bool
BView::AddChild(BLayoutItem* child)
{
	if (!fLayoutData->fLayout)
		return false;
	return fLayoutData->fLayout->AddItem(child);
}


bool
BView::_AddChild(BView* child, BView* before)
{
	if (!child)
		return false;

	if (child->fParent != NULL) {
		debugger("AddChild failed - the view already has a parent.");
		return false;
	}

	if (child == this) {
		debugger("AddChild failed - cannot add a view to itself.");
		return false;
	}

	bool lockedOwner = false;
	if (fOwner && !fOwner->IsLocked()) {
		fOwner->Lock();
		lockedOwner = true;
	}

	if (!_AddChildToList(child, before)) {
		debugger("AddChild failed!");
		if (lockedOwner)
			fOwner->Unlock();
		return false;
	}

	if (fOwner) {
		_CheckLockAndSwitchCurrent();

		child->_SetOwner(fOwner);
		child->_CreateSelf();
		child->_Attach();

		if (lockedOwner)
			fOwner->Unlock();
	}

	InvalidateLayout();

	return true;
}


bool
BView::RemoveChild(BView* child)
{
	STRACE(("BView(%s)::RemoveChild(%s)\n", Name(), child->Name()));

	if (!child)
		return false;

	if (child->fParent != this)
		return false;

	return child->RemoveSelf();
}


int32
BView::CountChildren() const
{
	_CheckLock();

	uint32 count = 0;
	BView* child = fFirstChild;

	while (child != NULL) {
		count++;
		child = child->fNextSibling;
	}

	return count;
}


BView*
BView::ChildAt(int32 index) const
{
	_CheckLock();

	BView* child = fFirstChild;
	while (child != NULL && index-- > 0) {
		child = child->fNextSibling;
	}

	return child;
}


BView*
BView::NextSibling() const
{
	return fNextSibling;
}


BView*
BView::PreviousSibling() const
{
	return fPreviousSibling;
}


bool
BView::RemoveSelf()
{
	_RemoveLayoutItemsFromLayout(false);

	return _RemoveSelf();
}


bool
BView::_RemoveSelf()
{
	STRACE(("BView(%s)::_RemoveSelf()\n", Name()));

	// Remove this child from its parent

	BWindow* owner = fOwner;
	_CheckLock();

	if (owner != NULL) {
		// _UpdateStateForRemove();
		_Detach();
	}

	BView* parent = fParent;
	if (!parent || !parent->_RemoveChildFromList(this))
		return false;

	if (owner != NULL && !fTopLevelView) {
		// the top level view is deleted by the app_server automatically
		// owner->fLink->StartMessage(AS_VIEW_DELETE);
		// owner->fLink->Attach<int32>(_get_object_token_(this));
	}

	parent->InvalidateLayout();

    widget_destroy(view_widget);

	STRACE(("DONE: BView(%s)::_RemoveSelf()\n", Name()));

	return true;
}


void
BView::_RemoveLayoutItemsFromLayout(bool deleteItems)
{
	if (fParent == NULL || fParent->fLayoutData->fLayout == NULL)
		return;

	int32 index = fLayoutData->fLayoutItems.CountItems();
	while (index-- > 0) {
		BLayoutItem* item = fLayoutData->fLayoutItems.ItemAt(index);
		item->RemoveSelf();
			// Removes item from fLayoutItems list
		if (deleteItems)
			delete item;
	}
}


BView*
BView::Parent() const
{
	if (fParent && fParent->fTopLevelView)
		return NULL;

	return fParent;
}


BView*
BView::FindView(const char* name) const
{
	if (name == NULL)
		return NULL;

	if (Name() != NULL && !strcmp(Name(), name))
		return const_cast<BView*>(this);

	BView* child = fFirstChild;
	while (child != NULL) {
		BView* view = child->FindView(name);
		if (view != NULL)
			return view;

		child = child->fNextSibling;
	}

	return NULL;
}


void
BView::MoveBy(float deltaX, float deltaY)
{
	MoveTo(fParentOffset.x + roundf(deltaX), fParentOffset.y + roundf(deltaY));
}


void
BView::MoveTo(BPoint where)
{
	MoveTo(where.x, where.y);
}


void
BView::MoveTo(float x, float y)
{
	if (x == fParentOffset.x && y == fParentOffset.y)
		return;

	// BeBook says we should do this. And it makes sense.
	x = roundf(x);
	y = roundf(y);

	_MoveTo((int32)x, (int32)y);
}


void
BView::ResizeBy(float deltaWidth, float deltaHeight)
{
	// BeBook says we should do this. And it makes sense.
	deltaWidth = roundf(deltaWidth);
	deltaHeight = roundf(deltaHeight);

	if (deltaWidth == 0 && deltaHeight == 0)
		return;

	_ResizeBy((int32)deltaWidth, (int32)deltaHeight);
}


void
BView::ResizeTo(float width, float height)
{
	ResizeBy(width - fBounds.Width(), height - fBounds.Height());
}


void
BView::ResizeTo(BSize size)
{
	ResizeBy(size.width - fBounds.Width(), size.height - fBounds.Height());
}


//	#pragma mark - Inherited Methods (from BHandler)

// #pragma mark - Layout Functions


BSize
BView::MinSize()
{
	// TODO: make sure this works correctly when some methods are overridden
	float width, height;
	GetPreferredSize(&width, &height);

	return BLayoutUtils::ComposeSize(fLayoutData->fMinSize,
		(fLayoutData->fLayout ? fLayoutData->fLayout->MinSize()
			: BSize(width, height)));
}


BSize
BView::MaxSize()
{
	return BLayoutUtils::ComposeSize(fLayoutData->fMaxSize,
		(fLayoutData->fLayout ? fLayoutData->fLayout->MaxSize()
			: BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED)));
}


BSize
BView::PreferredSize()
{
	// TODO: make sure this works correctly when some methods are overridden
	float width, height;
	GetPreferredSize(&width, &height);

	return BLayoutUtils::ComposeSize(fLayoutData->fPreferredSize,
		(fLayoutData->fLayout ? fLayoutData->fLayout->PreferredSize()
			: BSize(width, height)));
}


BAlignment
BView::LayoutAlignment()
{
	return BLayoutUtils::ComposeAlignment(fLayoutData->fAlignment,
		(fLayoutData->fLayout ? fLayoutData->fLayout->Alignment()
			: BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER)));
}


void
BView::SetExplicitMinSize(BSize size)
{
	fLayoutData->fMinSize = size;
	InvalidateLayout();
}


void
BView::SetExplicitMaxSize(BSize size)
{
	fLayoutData->fMaxSize = size;
	InvalidateLayout();
}


void
BView::SetExplicitPreferredSize(BSize size)
{
	fLayoutData->fPreferredSize = size;
	InvalidateLayout();
}


void
BView::SetExplicitSize(BSize size)
{
	fLayoutData->fMinSize = size;
	fLayoutData->fMaxSize = size;
	fLayoutData->fPreferredSize = size;
	InvalidateLayout();
}


void
BView::SetExplicitAlignment(BAlignment alignment)
{
	fLayoutData->fAlignment = alignment;
	InvalidateLayout();
}


BSize
BView::ExplicitMinSize() const
{
	return fLayoutData->fMinSize;
}


BSize
BView::ExplicitMaxSize() const
{
	return fLayoutData->fMaxSize;
}


BSize
BView::ExplicitPreferredSize() const
{
	return fLayoutData->fPreferredSize;
}


BAlignment
BView::ExplicitAlignment() const
{
	return fLayoutData->fAlignment;
}


bool
BView::HasHeightForWidth()
{
	return (fLayoutData->fLayout
		? fLayoutData->fLayout->HasHeightForWidth() : false);
}


void
BView::GetHeightForWidth(float width, float* min, float* max, float* preferred)
{
	if (fLayoutData->fLayout)
		fLayoutData->fLayout->GetHeightForWidth(width, min, max, preferred);
}


void
BView::SetLayout(BLayout* layout)
{
	if (layout == fLayoutData->fLayout)
		return;

	if (layout && layout->Layout())
		debugger("BView::SetLayout() failed, layout is already in use.");

	fFlags |= B_SUPPORTS_LAYOUT;

	// unset and delete the old layout
	if (fLayoutData->fLayout) {
		fLayoutData->fLayout->RemoveSelf();
		fLayoutData->fLayout->SetOwner(NULL);
		delete fLayoutData->fLayout;
	}

	fLayoutData->fLayout = layout;

	if (fLayoutData->fLayout) {
		fLayoutData->fLayout->SetOwner(this);

		// add all children
		int count = CountChildren();
		for (int i = 0; i < count; i++)
			fLayoutData->fLayout->AddView(ChildAt(i));
	}

	InvalidateLayout();
}


BLayout*
BView::GetLayout() const
{
	return fLayoutData->fLayout;
}


void
BView::InvalidateLayout(bool descendants)
{
	// printf("BView(%p)::InvalidateLayout(%i), valid: %i, inProgress: %i\n",
	//	this, descendants, fLayoutData->fLayoutValid,
	//	fLayoutData->fLayoutInProgress);

	if (!fLayoutData->fMinMaxValid || fLayoutData->fLayoutInProgress
 			|| fLayoutData->fLayoutInvalidationDisabled > 0) {
		return;
	}
	fLayoutData->fLayoutValid = false;
	fLayoutData->fMinMaxValid = false;
	LayoutInvalidated(descendants);

	if (descendants) {
		for (BView* child = fFirstChild;
			child; child = child->fNextSibling) {
			child->InvalidateLayout(descendants);
		}
	}

	if (fLayoutData->fLayout)
		fLayoutData->fLayout->InvalidateLayout(descendants);
	else
		_InvalidateParentLayout();

	//if (fTopLevelView
	//	&& fOwner != NULL)
	//	fOwner->PostMessage(B_LAYOUT_WINDOW);
}


void
BView::EnableLayoutInvalidation()
{
	if (fLayoutData->fLayoutInvalidationDisabled > 0)
		fLayoutData->fLayoutInvalidationDisabled--;
}


void
BView::DisableLayoutInvalidation()
{
	fLayoutData->fLayoutInvalidationDisabled++;
}


bool
BView::IsLayoutInvalidationDisabled()
{
	if (fLayoutData->fLayoutInvalidationDisabled > 0)
		return true;
	return false;
}


bool
BView::IsLayoutValid() const
{
	return fLayoutData->fLayoutValid;
}


void
BView::ResetLayoutInvalidation()
{
	fLayoutData->fMinMaxValid = true;
}


BLayoutContext*
BView::LayoutContext() const
{
	return fLayoutData->fLayoutContext;
}


void
BView::Layout(bool force)
{
	BLayoutContext context;
	_Layout(force, &context);
}


void
BView::Relayout()
{
	if (fLayoutData->fLayoutValid && !fLayoutData->fLayoutInProgress) {
		fLayoutData->fNeedsRelayout = true;
		if (fLayoutData->fLayout)
			fLayoutData->fLayout->RequireLayout();

		// Layout() is recursive, that is if the parent view is currently laid
		// out, we don't call layout() on this view, but wait for the parent's
		// Layout() to do that for us.
		if (!fParent || !fParent->fLayoutData->fLayoutInProgress)
			Layout(false);
	}
}


void
BView::LayoutInvalidated(bool descendants)
{
	// hook method
}


void
BView::DoLayout()
{
	if (fLayoutData->fLayout)
		fLayoutData->fLayout->_LayoutWithinContext(false, LayoutContext());
}



void
BView::LayoutChanged()
{
	// hook method
}


void
BView::_Layout(bool force, BLayoutContext* context)
{
//printf("%p->BView::_Layout(%d, %p)\n", this, force, context);
//printf("  fNeedsRelayout: %d, fLayoutValid: %d, fLayoutInProgress: %d\n",
//fLayoutData->fNeedsRelayout, fLayoutData->fLayoutValid,
//fLayoutData->fLayoutInProgress);
	if (fLayoutData->fNeedsRelayout || !fLayoutData->fLayoutValid || force) {
		fLayoutData->fLayoutValid = false;

		if (fLayoutData->fLayoutInProgress)
			return;

		BLayoutContext* oldContext = fLayoutData->fLayoutContext;
		fLayoutData->fLayoutContext = context;

		fLayoutData->fLayoutInProgress = true;
		DoLayout();
		fLayoutData->fLayoutInProgress = false;

		fLayoutData->fLayoutValid = true;
		fLayoutData->fMinMaxValid = true;
		fLayoutData->fNeedsRelayout = false;

		// layout children
		for(BView* child = fFirstChild; child; child = child->fNextSibling) {
			if (!child->IsHidden(child))
				child->_Layout(force, context);
		}

		LayoutChanged();

		fLayoutData->fLayoutContext = oldContext;

		// invalidate the drawn content, if requested
		if (fFlags & B_INVALIDATE_AFTER_LAYOUT)
			Invalidate();
	}
}


void
BView::_LayoutLeft(BLayout* deleted)
{
	// If our layout is added to another layout (via BLayout::AddItem())
	// then we share ownership of our layout. In the event that our layout gets
	// deleted by the layout it has been added to, this method is called so
	// that we don't double-delete our layout.
	if (fLayoutData->fLayout == deleted)
		fLayoutData->fLayout = NULL;
	InvalidateLayout();
}


void
BView::_InvalidateParentLayout()
{
	if (!fParent)
		return;

	BLayout* layout = fLayoutData->fLayout;
	BLayout* layoutParent = layout ? layout->Layout() : NULL;
	if (layoutParent) {
		layoutParent->InvalidateLayout();
	} else if (fLayoutData->fLayoutItems.CountItems() > 0) {
		int32 count = fLayoutData->fLayoutItems.CountItems();
		for (int32 i = 0; i < count; i++) {
			fLayoutData->fLayoutItems.ItemAt(i)->Layout()->InvalidateLayout();
		}
	} else {
		fParent->InvalidateLayout();
	}
}


//	#pragma mark - Private Functions


void
BView::_InitData(BRect frame, const char* name, uint32 resizingMode,
	uint32 flags)
{
	// Info: The name of the view is set by BHandler constructor

	STRACE(("BView::_InitData: enter\n"));

	// initialize members
	if ((resizingMode & ~_RESIZE_MASK_) || (flags & _RESIZE_MASK_))
		printf("%s BView::_InitData(): resizing mode or flags swapped\n", name);

	// There are applications that swap the resize mask and the flags in the
	// BView constructor. This does not cause problems under BeOS as it just
	// ors the two fields to one 32bit flag.
	// For now we do the same but print the above warning message.
	// TODO: this should be removed at some point and the original
	// version restored:
	// fFlags = (resizingMode & _RESIZE_MASK_) | (flags & ~_RESIZE_MASK_);
	fFlags = resizingMode | flags;

	// handle rounding
	frame.left = roundf(frame.left);
	frame.top = roundf(frame.top);
	frame.right = roundf(frame.right);
	frame.bottom = roundf(frame.bottom);

	fParentOffset.Set(frame.left, frame.top);

	fOwner = NULL;
	fParent = NULL;
	fNextSibling = NULL;
	fPreviousSibling = NULL;
	fFirstChild = NULL;

	fShowLevel = 0;
	fTopLevelView = false;

	fCommArray = NULL;

	fIsPrinting = false;
	fAttached = false;

	// TODO: Since we cannot communicate failure, we don't use std::nothrow here
	// TODO: Maybe we could auto-delete those views on AddChild() instead?
	fState = new BPrivate::ViewState;

	fBounds = frame.OffsetToCopy(B_ORIGIN);

	fEventMask = 0;
	fEventOptions = 0;
	fMouseEventOptions = 0;

	fLayoutData = new LayoutData;

	if ((flags & B_SUPPORTS_LAYOUT) != 0) {
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetLowUIColor(ViewUIColor());
		SetHighUIColor(B_PANEL_TEXT_COLOR);
	}
}


void
BView::_RemoveCommArray()
{
	if (fCommArray) {
		delete [] fCommArray->array;
		delete fCommArray;
		fCommArray = NULL;
	}
}


void
BView::_SetOwner(BWindow* newOwner)
{
	if (!newOwner)
		_RemoveCommArray();

	if (fOwner != newOwner && fOwner) {
	// 	if (fOwner->fFocus == this)
	// 		MakeFocus(false);

	// 	if (fOwner->fLastMouseMovedView == this)
	// 		fOwner->fLastMouseMovedView = NULL;

		fOwner->RemoveHandler(this);
	// 	if (fShelf)
	// 		fOwner->RemoveHandler(fShelf);
	}

	if (newOwner && newOwner != fOwner) {
		newOwner->AddHandler(this);
	// 	if (fShelf)
	// 		newOwner->AddHandler(fShelf);

		if (fTopLevelView)
			SetNextHandler(newOwner);
		else
			SetNextHandler(fParent);
	}

	fOwner = newOwner;

	for (BView* child = fFirstChild; child != NULL; child = child->fNextSibling)
		child->_SetOwner(newOwner);
}


void
BView::_ClipToRect(BRect rect, bool inverse)
{
}



bool
BView::_RemoveChildFromList(BView* child)
{
	if (child->fParent != this)
		return false;

	if (fFirstChild == child) {
		// it's the first view in the list
		fFirstChild = child->fNextSibling;
	} else {
		// there must be a previous sibling
		child->fPreviousSibling->fNextSibling = child->fNextSibling;
	}

	if (child->fNextSibling)
		child->fNextSibling->fPreviousSibling = child->fPreviousSibling;

	child->fParent = NULL;
	child->fNextSibling = NULL;
	child->fPreviousSibling = NULL;

	return true;
}


bool
BView::_AddChildToList(BView* child, BView* before)
{
	if (!child)
		return false;
	if (child->fParent != NULL) {
		debugger("View already belongs to someone else");
		return false;
	}
	if (before != NULL && before->fParent != this) {
		debugger("Invalid before view");
		return false;
	}

	if (before != NULL) {
		// add view before this one
		child->fNextSibling = before;
		child->fPreviousSibling = before->fPreviousSibling;
		if (child->fPreviousSibling != NULL)
			child->fPreviousSibling->fNextSibling = child;

		before->fPreviousSibling = child;
		if (fFirstChild == before)
			fFirstChild = child;
	} else {
		// add view to the end of the list
		BView* last = fFirstChild;
		while (last != NULL && last->fNextSibling != NULL) {
			last = last->fNextSibling;
		}

		if (last != NULL) {
			last->fNextSibling = child;
			child->fPreviousSibling = last;
		} else {
			fFirstChild = child;
			child->fPreviousSibling = NULL;
		}

		child->fNextSibling = NULL;
	}

	child->fParent = this;
	return true;
}


/*!	\brief Creates the server counterpart of this view.
	This is only done for views that are part of the view hierarchy, ie. when
	they are attached to a window.
	RemoveSelf() deletes the server object again.
*/
bool
BView::_CreateSelf()
{
	// we create all its children, too

    view_widget = window_add_subsurface(fOwner->window, this, SUBSURFACE_SYNCHRONIZED);

	if (fTopLevelView) {
    	widget_set_allocation(view_widget, WAYLAND_TOPVIEW_H_SLOP, WAYLAND_TOPVIEW_V_SLOP, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	} else {
		// Position our Wayland widget based on the parent widget's position
		rectangle allocation;
		widget_get_allocation(fParent->view_widget, &allocation);
		widget_set_allocation(view_widget, fParentOffset.x + allocation.x, fParentOffset.y + allocation.y, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	}

    printf("Bounds: %f %f %f %f\n", Bounds().left, Bounds().top, Bounds().right, Bounds().bottom);
	/* We set the input region of the subsurface where the image is draw as
	 * NULL, as the input region of the parent surface is automatically set
	 * by the toytoolkit. But as the window that finds the widget in a
	 * certain (x, y) position looks for surfaces that are on top first, it
	 * will call the image_widget handlers for input related stuff. */
	set_empty_input_region(view_widget, window_get_display(fOwner->window));
	widget_set_redraw_handler(view_widget, view_redraw_handler);
	// widget_set_resize_handler(view_widget, view_resize_handler);
	// widget_set_enter_handler(image->image_widget, image_enter_handler);
	// widget_set_motion_handler(image->image_widget, image_motion_handler);
	// widget_set_button_handler(image->image_widget, image_button_handler);
	// widget_set_axis_handler(image->image_widget, image_axis_handler);

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		child->_CreateSelf();
	}

	return true;
}


/*!	Sets the new view position.
	It doesn't contact the server, though - the only case where this
	is called outside of MoveTo() is as reaction of moving a view
	in the server (a.k.a. B_WINDOW_RESIZED).
	It also calls the BView's FrameMoved() hook.
*/
void
BView::_MoveTo(int32 x, int32 y)
{
	fParentOffset.Set(x, y);

	// Keep the Wayland widget size in sync with the view
	if (fParent != NULL) {
		rectangle allocation;
		widget_get_allocation(fParent->view_widget, &allocation);
		widget_set_allocation(view_widget, x + allocation.x, y + allocation.y, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	}

	if (Window() != NULL && fFlags & B_FRAME_EVENTS) {
	// 	BMessage moved(B_VIEW_MOVED);
	// 	moved.AddInt64("when", system_time());
	// 	moved.AddPoint("where", BPoint(x, y));

	// 	BMessenger target(this);
	// 	target.SendMessage(&moved);
	}
}


/*!	Computes the actual new frame size and recalculates the size of
	the children as well.
	It doesn't contact the server, though - the only case where this
	is called outside of ResizeBy() is as reaction of resizing a view
	in the server (a.k.a. B_WINDOW_RESIZED).
	It also calls the BView's FrameResized() hook.
*/
void
BView::_ResizeBy(int32 deltaWidth, int32 deltaHeight)
{
	fBounds.right += deltaWidth;
	fBounds.bottom += deltaHeight;

	if (Window() == NULL) {
		// we're not supposed to exercise the resizing code in case
		// we haven't been attached to a window yet
		return;
	}

	// Keep the Wayland widget size in sync with the view
	rectangle allocation;
	widget_get_allocation(view_widget, &allocation);
	widget_set_allocation(view_widget, allocation.x, allocation.y,
			allocation.width + deltaWidth, allocation.height + deltaHeight);

	// layout the children
	if ((fFlags & B_SUPPORTS_LAYOUT) != 0) {
		Relayout();
	} else {
		for (BView* child = fFirstChild; child; child = child->fNextSibling)
			child->_ParentResizedBy(deltaWidth, deltaHeight);
	}

	if (fFlags & B_FRAME_EVENTS) {
	// 	BMessage resized(B_VIEW_RESIZED);
	// 	resized.AddInt64("when", system_time());
	// 	resized.AddInt32("width", fBounds.IntegerWidth());
	// 	resized.AddInt32("height", fBounds.IntegerHeight());

	// 	BMessenger target(this);
	// 	target.SendMessage(&resized);
	}
}


/*!	Relayouts the view according to its resizing mode. */
void
BView::_ParentResizedBy(int32 x, int32 y)
{
	uint32 resizingMode = fFlags & _RESIZE_MASK_;
	BRect newFrame = Frame();

	printf("_ParentResizedBy %d x %d for %s\n", x, y, Name());

	// follow with left side
	if ((resizingMode & 0x0F00U) == _VIEW_RIGHT_ << 8)
		newFrame.left += x;
	else if ((resizingMode & 0x0F00U) == _VIEW_CENTER_ << 8)
		newFrame.left += x / 2;

	// follow with right side
	if ((resizingMode & 0x000FU) == _VIEW_RIGHT_)
		newFrame.right += x;
	else if ((resizingMode & 0x000FU) == _VIEW_CENTER_)
		newFrame.right += x / 2;

	// follow with top side
	if ((resizingMode & 0xF000U) == _VIEW_BOTTOM_ << 12)
		newFrame.top += y;
	else if ((resizingMode & 0xF000U) == _VIEW_CENTER_ << 12)
		newFrame.top += y / 2;

	// follow with bottom side
	if ((resizingMode & 0x00F0U) == _VIEW_BOTTOM_ << 4)
		newFrame.bottom += y;
	else if ((resizingMode & 0x00F0U) == _VIEW_CENTER_ << 4)
		newFrame.bottom += y / 2;

	if (newFrame.LeftTop() != fParentOffset) {
		printf("Moving %s to %f %f\n", Name(), newFrame.left, newFrame.top);
		_MoveTo((int32)roundf(newFrame.left), (int32)roundf(newFrame.top));
	}

	if (newFrame != Frame()) {
		// resize view
		printf("Resizing %s to %f %f %f %f\n", Name(), newFrame.left, newFrame.top, newFrame.right, newFrame.bottom);
		int32 widthDiff = (int32)(newFrame.Width() - fBounds.Width());
		int32 heightDiff = (int32)(newFrame.Height() - fBounds.Height());
		_ResizeBy(widthDiff, heightDiff);
	}
}


void
BView::_Activate(bool active)
{
	WindowActivated(active);

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		child->_Activate(active);
	}
}


void
BView::_Attach()
{
	if (fOwner != NULL) {
		// unmask state flags to force [re]syncing with the app_server
		fState->valid_flags &= ~(B_VIEW_WHICH_VIEW_COLOR_BIT
			| B_VIEW_WHICH_LOW_COLOR_BIT | B_VIEW_WHICH_HIGH_COLOR_BIT);

		if (fState->which_view_color != B_NO_COLOR)
			SetViewUIColor(fState->which_view_color,
				fState->which_view_color_tint);

		if (fState->which_high_color != B_NO_COLOR)
			SetHighUIColor(fState->which_high_color,
				fState->which_high_color_tint);

		if (fState->which_low_color != B_NO_COLOR)
			SetLowUIColor(fState->which_low_color,
				fState->which_low_color_tint);
	}

	AttachedToWindow();

	fAttached = true;

	// after giving the view a chance to do this itself,
	// check for the B_PULSE_NEEDED flag and make sure the
	// window set's up the pulse messaging
	if (fOwner) {
		if (fFlags & B_PULSE_NEEDED) {
			_CheckLock();
			// if (fOwner->fPulseRunner == NULL)
			// 	fOwner->SetPulseRate(fOwner->PulseRate());
		}

		if (!fOwner->IsHidden())
			Invalidate();
	}

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		// we need to check for fAttached as new views could have been
		// added in AttachedToWindow() - and those are already attached
		if (!child->fAttached)
			child->_Attach();
	}

	AllAttached();
}


void
BView::_Detach()
{
	DetachedFromWindow();
	fAttached = false;

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		child->_Detach();
	}

	AllDetached();

	if (fOwner) {
		_CheckLock();

		if (!fOwner->IsHidden())
			Invalidate();

		// make sure our owner doesn't need us anymore

		_SetOwner(NULL);
	}
}


void
BView::_Draw(BRect updateRect)
{
	if (IsHidden(this) || !(Flags() & B_WILL_DRAW))
		return;

	// NOTE: if ViewColor() == B_TRANSPARENT_COLOR and no B_WILL_DRAW
	// -> View is simply not drawn at all

	//_SwitchServerCurrentView();

	//ConvertFromScreen(&updateRect);

	// TODO: make states robust (the hook implementation could
	// mess things up if it uses non-matching Push- and PopState(),
	// we would not be guaranteed to still have the same state on
	// the stack after having called Draw())
	PushState();
	Draw(updateRect);
	PopState();
	//Flush();
}


void
BView::_DrawAfterChildren(BRect updateRect)
{
	if (IsHidden(this) || !(Flags() & B_WILL_DRAW)
		|| !(Flags() & B_DRAW_ON_CHILDREN))
		return;

	// _SwitchServerCurrentView();

	// ConvertFromScreen(&updateRect);

	// // TODO: make states robust (see above)
	PushState();
	DrawAfterChildren(updateRect);
	PopState();
	// Flush();
}


void
BView::_Pulse()
{
	if ((Flags() & B_PULSE_NEEDED) != 0)
		Pulse();

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		child->_Pulse();
	}
}

inline void
BView::_UpdatePattern(::pattern pattern)
{
	fState->pattern = pattern;
}


void
BView::_CheckLockAndSwitchCurrent() const
{
	STRACE(("BView(%s)::_CheckLockAndSwitchCurrent()\n", Name()));

	if (!fOwner)
		return;

	//fOwner->check_lock();

	//_SwitchServerCurrentView();
}


void
BView::_CheckLock() const
{
	// if (fOwner)
	// 	fOwner->check_lock();
}

void BView::_ReservedView13() {}
void BView::_ReservedView14() {}
void BView::_ReservedView15() {}
void BView::_ReservedView16() {}

void
BView::_PrintToStream()
{
	printf("BView::_PrintToStream()\n");
	printf("\tName: %s\n"
		"\tParent: %s\n"
		"\tFirstChild: %s\n"
		"\tNextSibling: %s\n"
		"\tPrevSibling: %s\n"
		"\tOwner(Window): %s\n"
		// "\tToken: %" B_PRId32 "\n"
		"\tFlags: %" B_PRId32 "\n"
		"\tView origin: (%f,%f)\n"
		"\tView Bounds rectangle: (%f,%f,%f,%f)\n"
		"\tShow level: %d\n"
		"\tTopView?: %s\n"
		// "\tBPicture: %s\n"
		// "\tVertical Scrollbar %s\n"
		// "\tHorizontal Scrollbar %s\n"
		"\tIs Printing?: %s\n"
		// "\tShelf?: %s\n"
		"\tEventMask: %" B_PRId32 "\n"
		"\tEventOptions: %" B_PRId32 "\n",
	Name(),
	fParent ? fParent->Name() : "NULL",
	fFirstChild ? fFirstChild->Name() : "NULL",
	fNextSibling ? fNextSibling->Name() : "NULL",
	fPreviousSibling ? fPreviousSibling->Name() : "NULL",
	fOwner ? fOwner->Name() : "NULL",
	// _get_object_token_(this),
	fFlags,
	fParentOffset.x, fParentOffset.y,
	fBounds.left, fBounds.top, fBounds.right, fBounds.bottom,
	fShowLevel,
	fTopLevelView ? "YES" : "NO",
	// fCurrentPicture? "YES" : "NULL",
	// fVerScroller? "YES" : "NULL",
	// fHorScroller? "YES" : "NULL",
	fIsPrinting? "YES" : "NO",
	// fShelf? "YES" : "NO",
	fEventMask,
	fEventOptions);

	printf("\tState status:\n"
		"\t\tLocalCoordianteSystem: (%f,%f)\n"
		"\t\tPenLocation: (%f,%f)\n"
		"\t\tPenSize: %f\n"
		"\t\tHighColor: [%d,%d,%d,%d]\n"
		"\t\tLowColor: [%d,%d,%d,%d]\n"
		"\t\tViewColor: [%d,%d,%d,%d]\n"
		"\t\tPattern: %" B_PRIx64 "\n"
		"\t\tDrawingMode: %d\n"
		"\t\tLineJoinMode: %d\n"
		"\t\tLineCapMode: %d\n"
		"\t\tMiterLimit: %f\n"
		"\t\tAlphaSource: %d\n"
		"\t\tAlphaFuntion: %d\n"
		"\t\tScale: %f\n"
		"\t\t(Print)FontAliasing: %s\n"
		"\t\tFont Info:\n",
	fState->origin.x, fState->origin.y,
	fState->pen_location.x, fState->pen_location.y,
	fState->pen_size,
	fState->high_color.red, fState->high_color.blue, fState->high_color.green, fState->high_color.alpha,
	fState->low_color.red, fState->low_color.blue, fState->low_color.green, fState->low_color.alpha,
	fState->view_color.red, fState->view_color.blue, fState->view_color.green, fState->view_color.alpha,
	*((uint64*)&(fState->pattern)),
	fState->drawing_mode,
	fState->line_join,
	fState->line_cap,
	fState->miter_limit,
	fState->alpha_source_mode,
	fState->alpha_function_mode,
	fState->scale,
	fState->font_aliasing? "YES" : "NO");

	// fState->font.PrintToStream();

	// TODO: also print the line array.
}


void
BView::_PrintTree()
{
	int32 spaces = 2;
	BView* c = fFirstChild; //c = short for: current
	printf( "'%s'\n", Name() );
	if (c != NULL) {
		while(true) {
			// action block
			{
				for (int i = 0; i < spaces; i++)
					printf(" ");

				printf( "'%s'\n", c->Name() );
			}

			// go deep
			if (c->fFirstChild) {
				c = c->fFirstChild;
				spaces += 2;
			} else {
				// go right
				if (c->fNextSibling) {
					c = c->fNextSibling;
				} else {
					// go up
					while (!c->fParent->fNextSibling && c->fParent != this) {
						c = c->fParent;
						spaces -= 2;
					}

					// that enough! We've reached this view.
					if (c->fParent == this)
						break;

					c = c->fParent->fNextSibling;
					spaces -= 2;
				}
			}
		}
	}
}


// #pragma mark -


BLayoutItem*
BView::Private::LayoutItemAt(int32 index)
{
	return fView->fLayoutData->fLayoutItems.ItemAt(index);
}


int32
BView::Private::CountLayoutItems()
{
	return fView->fLayoutData->fLayoutItems.CountItems();
}


void
BView::Private::RegisterLayoutItem(BLayoutItem* item)
{
	fView->fLayoutData->fLayoutItems.AddItem(item);
}


void
BView::Private::DeregisterLayoutItem(BLayoutItem* item)
{
	fView->fLayoutData->fLayoutItems.RemoveItem(item);
}


bool
BView::Private::MinMaxValid()
{
	return fView->fLayoutData->fMinMaxValid;
}


bool
BView::Private::WillLayout()
{
	BView::LayoutData* data = fView->fLayoutData;
	if (data->fLayoutInProgress)
		return false;
	if (data->fNeedsRelayout || !data->fLayoutValid || !data->fMinMaxValid)
		return true;
	return false;
}
