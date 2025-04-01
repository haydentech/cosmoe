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
#include <Bitmap.h>
#include <Button.h>
#include <Cursor.h>
#include <File.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientRadialFocus.h>
#include <GradientDiamond.h>
#include <GradientConic.h>
#include <InterfaceDefs.h>
#include <InterfacePrivate.h>
#include <Layout.h>
#include <LayoutContext.h>
#include <LayoutUtils.h>
#include <MenuBar.h>
#include <Message.h>
#include <MessageQueue.h>
#include <ObjectList.h>
#include <Picture.h>
#include <Point.h>
#include <Polygon.h>
#include <PropertyInfo.h>
#include <Region.h>
#include <ScrollBar.h>
#include <Shape.h>
#include <Shelf.h>
#include <String.h>
#include <Window.h>

#include <AppMisc.h>
#include <binary_compatibility/Interface.h>
#include <binary_compatibility/Support.h>
#include <MessagePrivate.h>
#include <MessageUtils.h>
#include <ShapePrivate.h>
#include <ToolTip.h>
#include <ToolTipManager.h>
#include <TokenSpace.h>
#include <ViewPrivate.h>

#include <linux/input-event-codes.h>

#include <pango/pango-layout.h>
#include <pango/pangocairo.h>
#include <cairo-util.h>

#include <CairoHelpers.h>



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

#define DRAW 1


static property_info sViewPropInfo[] = {
	{ "Frame", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER, 0 }, "The view's frame rectangle.", 0,
		{ B_RECT_TYPE }
	},
	{ "Hidden", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER, 0 }, "Whether or not the view is hidden.",
		0, { B_BOOL_TYPE }
	},
	{ "Shelf", { 0 },
		{ B_DIRECT_SPECIFIER, 0 }, "Directs the scripting message to the "
			"shelf.", 0
	},
	{ "View", { B_COUNT_PROPERTIES, 0 },
		{ B_DIRECT_SPECIFIER, 0 }, "Returns the number of child views.", 0,
		{ B_INT32_TYPE }
	},
	{ "View", { 0 },
		{ B_INDEX_SPECIFIER, B_REVERSE_INDEX_SPECIFIER, B_NAME_SPECIFIER, 0 },
		"Directs the scripting message to the specified view.", 0
	},

	{ 0 }
};


#define WAYLAND_TOPVIEW_H_SLOP 38
#define WAYLAND_TOPVIEW_V_SLOP 59

void
view_redraw_handler(struct widget *widget, void *data)
{
    //printf("view_redraw_handler\n");
    BView* view = (BView*)data;

	if (!view->IsHidden() && view->Window() && !view->Window()->UpdatesDisabled()) {
		if (view->ViewColor() != B_TRANSPARENT_COLOR) {
			rgb_color color = view->HighColor();
			view->SetHighColor(view->ViewColor());
			view->FillRegion(&view->fLocalClipping, B_SOLID_HIGH);
			view->SetHighColor(color);
		}

		BMessage msg(_UPDATE_);
		msg.AddInt64("when", system_time());
		msg.AddInt32("token", _get_object_token_(view));
		msg.AddRect("updateRect", view->Bounds());
		view->Window()->DispatchMessage(&msg, view->Window());
	}
}

void view_button_handler(struct widget *widget,
	struct input *input, uint32_t time,
	uint32_t button,
	enum wl_pointer_button_state state,
	void *data)
{
	BView* view = (BView*)data;

	printf("view_button_handler, got button %d at %.0f, %.0f\n",
		(button == BTN_LEFT) ? B_PRIMARY_MOUSE_BUTTON : B_SECONDARY_MOUSE_BUTTON,
		view->fLastMousePosition.x, view->fLastMousePosition.y);


	switch(button) {
		case BTN_LEFT:
			view->fLastButtonState[B_PRIMARY_MOUSE_BUTTON] = (state == WL_POINTER_BUTTON_STATE_PRESSED);
			break;

		case BTN_RIGHT:
			view->fLastButtonState[B_SECONDARY_MOUSE_BUTTON] = (state == WL_POINTER_BUTTON_STATE_PRESSED);
			break;

		case BTN_MIDDLE:
			view->fLastButtonState[B_TERTIARY_MOUSE_BUTTON] = (state == WL_POINTER_BUTTON_STATE_PRESSED);
			break;
	}

	if (!view->IsHidden() && !view->Window()->UpdatesDisabled()) {
		int32 buttons = 0;
		if (view->fLastButtonState[B_PRIMARY_MOUSE_BUTTON])
			buttons |= B_PRIMARY_MOUSE_BUTTON;
		if (view->fLastButtonState[B_SECONDARY_MOUSE_BUTTON])
			buttons |= B_SECONDARY_MOUSE_BUTTON;
		if (view->fLastButtonState[B_TERTIARY_MOUSE_BUTTON])
			buttons |= B_TERTIARY_MOUSE_BUTTON;

		BMessage* msg = new BMessage((state == WL_POINTER_BUTTON_STATE_PRESSED) ? B_MOUSE_DOWN : B_MOUSE_UP);
		msg->AddInt64("when", system_time());
		msg->AddInt32("buttons", buttons);
		msg->AddPoint("be:view_where", view->fLastMousePosition);
		view->Window()->SetOOBMessage(msg);
		view->MessageReceived(msg);
		view->Window()->SetOOBMessage(NULL);
		delete msg;
	}
}

void send_mouse_moved(BView* view, int32 transit, BPoint& where)
{
	if (!view->IsHidden() && view->Window() && !view->Window()->UpdatesDisabled()) {
		BMessage* msg = new BMessage(B_MOUSE_MOVED);
		msg->AddInt64("when", system_time());
		msg->AddPoint("be:view_where", where);
		msg->AddInt32("be:transit", transit);
		view->MessageReceived(msg);
		delete msg;
	}
}

void send_mouse_wheel(BView* view, float deltaX, float deltaY)
{
	printf("send_mouse_wheel(%f, %f)\n", deltaX, deltaY);
	if (!view->IsHidden() && view->Window() && !view->Window()->UpdatesDisabled()) {
		BMessage* msg = new BMessage(B_MOUSE_WHEEL_CHANGED);
		msg->AddInt64("when", system_time());
		msg->AddFloat("be:wheel_delta_x", -1.0f * deltaX);
		msg->AddFloat("be:wheel_delta_y", -1.0f * deltaY);
		view->MessageReceived(msg);
		delete msg;
	}
}

int view_pointer_motion_handler(struct widget *widget,
	struct input *input, uint32_t time,
	float x, float y, void *data)
{
	BView* view = (BView*)data;
	//printf("view_pointer_motion_handler(%s)\n", view->Name());

	if (view->ToolTip() != NULL) {
		BTextToolTip* tip = dynamic_cast<BTextToolTip*>(view->ToolTip());
		if (tip != NULL) {
			widget_set_tooltip(widget, (char*)tip->Text(), x, y);
		}
	} else if (view->Name() != NULL) {
		widget_set_tooltip(widget, (char*)view->Name(), x, y);
	}

	rectangle allocation;
	widget_get_allocation(widget, &allocation);

	view->fLastMousePosition.Set(x - allocation.x,
									y - allocation.y);

	//printf("view_pointer_motion_handler, got mouse move at %.0f, %.0f\n", x, y);
	//printf("view_pointer_motion_handler, fLastMousePosition is %.0f, %.0f\n", view->fLastMousePosition.x, view->fLastMousePosition.y);

	send_mouse_moved(view, B_INSIDE_VIEW, view->fLastMousePosition);

	// Do we have a view cursor?
	int32 cursor = view->CursorID();

	// If not, do we have an app cursor?
	if (cursor < 0)
		cursor = be_app->CursorID();

	// If neither, use the default cursor
	if (cursor < 0)
		cursor = CURSOR_LEFT_PTR;
	else
		cursor = BCursorToWaylandCursor(cursor);

	return cursor;
}

int view_pointer_enter_handler(struct widget *widget,
	struct input *input,
	float x, float y, void *data)
{
	printf("view_pointer_enter_handler\n");
	BView* view = (BView*)data;

	rectangle allocation;
	widget_get_allocation(widget, &allocation);

	view->fLastMousePosition.Set(x - allocation.x,
									y - allocation.y);

	send_mouse_moved(view, B_ENTERED_VIEW, view->fLastMousePosition);

	return 0;
}

void view_pointer_leave_handler(struct widget *widget,
	struct input *input, void *data)
{
	printf("view_pointer_leave_handler\n");
	BView* view = (BView*)data;

	widget_destroy_tooltip(widget);
	widget_schedule_redraw(widget);

	send_mouse_moved(view, B_EXITED_VIEW, view->fLastMousePosition);
}

void view_axis_handler(struct widget *widget, struct input *input, uint32_t time,
	uint32_t axis, wl_fixed_t value, void *data)
{
	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL || axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		BView* view = (BView*)data;

		float deltaX = (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ? wl_fixed_to_double(value) : 0.0f;
		float deltaY = (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ? wl_fixed_to_double(value) : 0.0f;

		send_mouse_wheel(view, deltaX, deltaY);
	}
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


static inline uint32
get_uint32_color(rgb_color color)
{
	return B_BENDIAN_TO_HOST_INT32(*(uint32*)&color);
		// rgb_color is always in rgba format, no matter what endian;
		// we always return the int32 value in host endian.
}


static inline rgb_color
get_rgb_color(uint32 value)
{
	value = B_HOST_TO_BENDIAN_INT32(value);
	return *(rgb_color*)&value;
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

	parent_composite_transform.Reset();
	parent_composite_scale = 1.0f;
	parent_composite_origin.Set(0, 0);

	previous_state = NULL;
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

	status_t
	AddDataToArchive(BMessage* archive)
	{
		status_t err = archive->AddSize(kSizesField, fMinSize);

		if (err == B_OK)
			err = archive->AddSize(kSizesField, fMaxSize);

		if (err == B_OK)
			err = archive->AddSize(kSizesField, fPreferredSize);

		if (err == B_OK)
			err = archive->AddAlignment(kAlignmentField, fAlignment);

		return err;
	}

	void
	PopulateFromArchive(BMessage* archive)
	{
		archive->FindSize(kSizesField, 0, &fMinSize);
		archive->FindSize(kSizesField, 1, &fMaxSize);
		archive->FindSize(kSizesField, 2, &fPreferredSize);
		archive->FindAlignment(kAlignmentField, &fAlignment);
	}

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


BView::BView(BMessage* archive)
	:
	BHandler(BUnarchiver::PrepareArchive(archive))
{
	BUnarchiver unarchiver(archive);
	if (!archive)
		debugger("BView cannot be constructed from a NULL archive.");

	BRect frame;
	archive->FindRect("_frame", &frame);

	uint32 resizingMode;
	if (archive->FindInt32("_resize_mode", (int32*)&resizingMode) != B_OK)
		resizingMode = 0;

	uint32 flags;
	if (archive->FindInt32("_flags", (int32*)&flags) != B_OK)
		flags = 0;

	_InitData(frame, Name(), resizingMode, flags);

	font_family family;
	font_style style;
	if (archive->FindString("_fname", 0, (const char**)&family) == B_OK
		&& archive->FindString("_fname", 1, (const char**)&style) == B_OK) {
		BFont font;
		font.SetFamilyAndStyle(family, style);

		float size;
		if (archive->FindFloat("_fflt", 0, &size) == B_OK)
			font.SetSize(size);

		float shear;
		if (archive->FindFloat("_fflt", 1, &shear) == B_OK
			&& shear >= 45.0 && shear <= 135.0)
			font.SetShear(shear);

		float rotation;
		if (archive->FindFloat("_fflt", 2, &rotation) == B_OK
			&& rotation >=0 && rotation <= 360)
			font.SetRotation(rotation);

		SetFont(&font, B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE
			| B_FONT_SHEAR | B_FONT_ROTATION);
	}

	int32 color = 0;
	if (archive->FindInt32("_color", 0, &color) == B_OK)
		SetHighColor(get_rgb_color(color));
	if (archive->FindInt32("_color", 1, &color) == B_OK)
		SetLowColor(get_rgb_color(color));
	if (archive->FindInt32("_color", 2, &color) == B_OK)
		SetViewColor(get_rgb_color(color));

	float tint = B_NO_TINT;
	if (archive->FindInt32("_uicolor", 0, &color) == B_OK
		&& color != B_NO_COLOR) {
		if (archive->FindFloat("_uitint", 0, &tint) != B_OK)
			tint = B_NO_TINT;

		SetHighUIColor((color_which)color, tint);
	}
	if (archive->FindInt32("_uicolor", 1, &color) == B_OK
		&& color != B_NO_COLOR) {
		if (archive->FindFloat("_uitint", 1, &tint) != B_OK)
			tint = B_NO_TINT;

		SetLowUIColor((color_which)color, tint);
	}
	if (archive->FindInt32("_uicolor", 2, &color) == B_OK
		&& color != B_NO_COLOR) {
		if (archive->FindFloat("_uitint", 2, &tint) != B_OK)
			tint = B_NO_TINT;

		SetViewUIColor((color_which)color, tint);
	}

	uint32 evMask;
	uint32 options;
	if (archive->FindInt32("_evmask", 0, (int32*)&evMask) == B_OK
		&& archive->FindInt32("_evmask", 1, (int32*)&options) == B_OK)
		SetEventMask(evMask, options);

	BPoint origin;
	if (archive->FindPoint("_origin", &origin) == B_OK)
		SetOrigin(origin);

	float scale;
	if (archive->FindFloat("_scale", &scale) == B_OK)
		SetScale(scale);

	BAffineTransform transform;
	if (archive->FindFlat("_transform", &transform) == B_OK)
		SetTransform(transform);

	float penSize;
	if (archive->FindFloat("_psize", &penSize) == B_OK)
		SetPenSize(penSize);

	BPoint penLocation;
	if (archive->FindPoint("_ploc", &penLocation) == B_OK)
		MovePenTo(penLocation);

	int16 lineCap;
	int16 lineJoin;
	float lineMiter;
	if (archive->FindInt16("_lmcapjoin", 0, &lineCap) == B_OK
		&& archive->FindInt16("_lmcapjoin", 1, &lineJoin) == B_OK
		&& archive->FindFloat("_lmmiter", &lineMiter) == B_OK)
		SetLineMode((cap_mode)lineCap, (join_mode)lineJoin, lineMiter);

	int16 fillRule;
	if (archive->FindInt16("_fillrule", &fillRule) == B_OK)
		SetFillRule(fillRule);

	int16 alphaBlend;
	int16 modeBlend;
	if (archive->FindInt16("_blend", 0, &alphaBlend) == B_OK
		&& archive->FindInt16("_blend", 1, &modeBlend) == B_OK)
		SetBlendingMode( (source_alpha)alphaBlend, (alpha_function)modeBlend);

	uint32 drawingMode;
	if (archive->FindInt32("_dmod", (int32*)&drawingMode) == B_OK)
		SetDrawingMode((drawing_mode)drawingMode);

	fLayoutData->PopulateFromArchive(archive);

	if (archive->FindInt16("_show", &fShowLevel) != B_OK)
		fShowLevel = 0;

	if (BUnarchiver::IsArchiveManaged(archive)) {
		int32 i = 0;
		while (unarchiver.EnsureUnarchived("_views", i++) == B_OK)
				;
		unarchiver.EnsureUnarchived(kLayoutField);

	} else {
		BMessage msg;
		for (int32 i = 0; archive->FindMessage("_views", i, &msg) == B_OK;
			i++) {
			BArchivable* object = instantiate_object(&msg);
			if (BView* child = dynamic_cast<BView*>(object))
				AddChild(child);
		}
	}
}


BArchivable*
BView::Instantiate(BMessage* data)
{
	if (!validate_instantiation(data , "BView"))
		return NULL;

	return new(std::nothrow) BView(data);
}


status_t
BView::Archive(BMessage* data, bool deep) const
{
	BArchiver archiver(data);
	status_t ret = BHandler::Archive(data, deep);

	if (ret != B_OK)
		return ret;

	if ((fState->archiving_flags & B_VIEW_FRAME_BIT) != 0)
		ret = data->AddRect("_frame", Bounds().OffsetToCopy(fParentOffset));

	if (ret == B_OK)
		ret = data->AddInt32("_resize_mode", ResizingMode());

	if (ret == B_OK)
		ret = data->AddInt32("_flags", Flags());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_EVENT_MASK_BIT) != 0) {
		ret = data->AddInt32("_evmask", fEventMask);
		if (ret == B_OK)
			ret = data->AddInt32("_evmask", fEventOptions);
	}

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_FONT_BIT) != 0) {
		BFont font;
		GetFont(&font);

		font_family family;
		font_style style;
		font.GetFamilyAndStyle(&family, &style);
		ret = data->AddString("_fname", family);
		if (ret == B_OK)
			ret = data->AddString("_fname", style);
		if (ret == B_OK)
			ret = data->AddFloat("_fflt", font.Size());
		if (ret == B_OK)
			ret = data->AddFloat("_fflt", font.Shear());
		if (ret == B_OK)
			ret = data->AddFloat("_fflt", font.Rotation());
	}

	// colors
	if (ret == B_OK)
		ret = data->AddInt32("_color", get_uint32_color(HighColor()));
	if (ret == B_OK)
		ret = data->AddInt32("_color", get_uint32_color(LowColor()));
	if (ret == B_OK)
		ret = data->AddInt32("_color", get_uint32_color(ViewColor()));

	if (ret == B_OK)
		ret = data->AddInt32("_uicolor", (int32)HighUIColor());
	if (ret == B_OK)
		ret = data->AddInt32("_uicolor", (int32)LowUIColor());
	if (ret == B_OK)
		ret = data->AddInt32("_uicolor", (int32)ViewUIColor());

	if (ret == B_OK)
		ret = data->AddFloat("_uitint", fState->which_high_color_tint);
	if (ret == B_OK)
		ret = data->AddFloat("_uitint", fState->which_low_color_tint);
	if (ret == B_OK)
		ret = data->AddFloat("_uitint", fState->which_view_color_tint);

//	NOTE: we do not use this flag any more
//	if ( 1 ){
//		ret = data->AddInt32("_dbuf", 1);
//	}

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_ORIGIN_BIT) != 0)
		ret = data->AddPoint("_origin", Origin());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_SCALE_BIT) != 0)
		ret = data->AddFloat("_scale", Scale());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_TRANSFORM_BIT) != 0) {
		BAffineTransform transform = Transform();
		ret = data->AddFlat("_transform", &transform);
	}

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_PEN_SIZE_BIT) != 0)
		ret = data->AddFloat("_psize", PenSize());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_PEN_LOCATION_BIT) != 0)
		ret = data->AddPoint("_ploc", PenLocation());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_LINE_MODES_BIT) != 0) {
		ret = data->AddInt16("_lmcapjoin", (int16)LineCapMode());
		if (ret == B_OK)
			ret = data->AddInt16("_lmcapjoin", (int16)LineJoinMode());
		if (ret == B_OK)
			ret = data->AddFloat("_lmmiter", LineMiterLimit());
	}

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_FILL_RULE_BIT) != 0)
		ret = data->AddInt16("_fillrule", (int16)FillRule());

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_BLENDING_BIT) != 0) {
		source_alpha alphaSourceMode;
		alpha_function alphaFunctionMode;
		GetBlendingMode(&alphaSourceMode, &alphaFunctionMode);

		ret = data->AddInt16("_blend", (int16)alphaSourceMode);
		if (ret == B_OK)
			ret = data->AddInt16("_blend", (int16)alphaFunctionMode);
	}

	if (ret == B_OK && (fState->archiving_flags & B_VIEW_DRAWING_MODE_BIT) != 0)
		ret = data->AddInt32("_dmod", DrawingMode());

	if (ret == B_OK)
		ret = fLayoutData->AddDataToArchive(data);

	if (ret == B_OK)
		ret = data->AddInt16("_show", fShowLevel);

	if (deep && ret == B_OK) {
		for (BView* child = fFirstChild; child != NULL && ret == B_OK;
			child = child->fNextSibling)
			ret = archiver.AddArchivable("_views", child, deep);

		if (ret == B_OK)
			ret = archiver.AddArchivable(kLayoutField, GetLayout(), deep);
	}

	return archiver.Finish(ret);
}


status_t
BView::AllUnarchived(const BMessage* from)
{
	BUnarchiver unarchiver(from);
	status_t err = B_OK;

	int32 count;
	from->GetInfo("_views", NULL, &count);

	for (int32 i = 0; err == B_OK && i < count; i++) {
		BView* child;
		err = unarchiver.FindObject<BView>("_views", i, child);
		if (err == B_OK)
			err = _AddChild(child, NULL) ? B_OK : B_ERROR;
	}

	if (err == B_OK) {
		BLayout*& layout = fLayoutData->fLayout;
		err = unarchiver.FindObject(kLayoutField, layout);
		if (err == B_OK && layout) {
			fFlags |= B_SUPPORTS_LAYOUT;
			fLayoutData->fLayout->SetOwner(this);
		}
	}

	return err;
}


status_t
BView::AllArchived(BMessage* into) const
{
	return BHandler::AllArchived(into);
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

	if (fToolTip != NULL)
		fToolTip->ReleaseReference();

	if (fVerScroller != NULL)
		fVerScroller->SetTarget((BView*)NULL);
	if (fHorScroller != NULL)
		fHorScroller->SetTarget((BView*)NULL);

	SetName(NULL);

	_RemoveCommArray();

	// Remove the linked list of previous states
	ViewState* state = fState->previous_state;
	while (state != NULL) {
		ViewState* stateToDelete = state;
		state = state->previous_state;
		delete stateToDelete;
	}

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


void
BView::_ConvertToScreen(BPoint* point, bool checkLock) const
{
	if (!fParent) {
		if (fOwner)
			fOwner->ConvertToScreen(point);

		return;
	}

	if (checkLock)
		_CheckOwnerLock();

	_ConvertToParent(point, false);
	fParent->_ConvertToScreen(point, false);
}


void
BView::ConvertToScreen(BPoint* point) const
{
	_ConvertToScreen(point, true);
}


BPoint
BView::ConvertToScreen(BPoint point) const
{
	ConvertToScreen(&point);

	return point;
}


void
BView::_ConvertFromScreen(BPoint* point, bool checkLock) const
{
	if (!fParent) {
		if (fOwner)
			fOwner->ConvertFromScreen(point);

		return;
	}

	if (checkLock)
		_CheckOwnerLock();

	_ConvertFromParent(point, false);
	fParent->_ConvertFromScreen(point, false);
}


void
BView::ConvertFromScreen(BPoint* point) const
{
	_ConvertFromScreen(point, true);
}


BPoint
BView::ConvertFromScreen(BPoint point) const
{
	ConvertFromScreen(&point);

	return point;
}


void
BView::ConvertToScreen(BRect* rect) const
{
	BPoint offset(0.0, 0.0);
	ConvertToScreen(&offset);
	rect->OffsetBy(offset);
}


BRect
BView::ConvertToScreen(BRect rect) const
{
	ConvertToScreen(&rect);

	return rect;
}


void
BView::ConvertFromScreen(BRect* rect) const
{
	BPoint offset(0.0, 0.0);
	ConvertFromScreen(&offset);
	rect->OffsetBy(offset);
}


BRect
BView::ConvertFromScreen(BRect rect) const
{
	ConvertFromScreen(&rect);

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
			if (fOwner->fPulseRunner == NULL)
				fOwner->SetPulseRate(fOwner->PulseRate());
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


bool
BView::IsPrinting() const
{
	return fIsPrinting;
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
BView::SetViewCursor(const BCursor* cursor, bool sync)
{
	if (cursor == NULL)
		fCursor = -1;

	if (cursor == NULL || fOwner == NULL)
		return;

	_CheckLock();

	// For now, we just use the cursor id and do not implement custom cursors at all.
	fCursor = cursor->fServerToken;
}

int32
BView::CursorID() const
{
	return fCursor;
}


void
BView::Flush() const
{
	if (fOwner)
		fOwner->Flush();
}


void
BView::Sync() const
{
	_CheckOwnerLock();
	if (fOwner)
		fOwner->Sync();
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

	if (Window())
		Window()->_KeyboardNavigation();
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
BView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	// Hook function
	//STRACE(("\tHOOK: BView(%s)::MouseMoved()\n", Name()));
}


void
BView::Pulse()
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::Pulse()\n", Name()));
}


void
BView::TargetedByScrollView(BScrollView* scroll_view)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::TargetedByScrollView()\n", Name()));
}


void
BView::WindowActivated(bool active)
{
	// Hook function
	STRACE(("\tHOOK: BView(%s)::WindowActivated()\n", Name()));
}


//	#pragma mark - Input Functions


void
BView::BeginRectTracking(BRect startRect, uint32 style)
{
	// if (_CheckOwnerLockAndSwitchCurrent()) {
	// 	fOwner->fLink->StartMessage(AS_VIEW_BEGIN_RECT_TRACK);
	// 	fOwner->fLink->Attach<BRect>(startRect);
	// 	fOwner->fLink->Attach<uint32>(style);
	// 	fOwner->fLink->Flush();
	// }
}


void
BView::EndRectTracking()
{
	// if (_CheckOwnerLockAndSwitchCurrent()) {
	// 	fOwner->fLink->StartMessage(AS_VIEW_END_RECT_TRACK);
	// 	fOwner->fLink->Flush();
	// }
}


void
BView::DragMessage(BMessage* message, BRect dragRect, BHandler* replyTo)
{
	if (!message)
		return;

	_CheckOwnerLock();

	// // calculate the offset
	// BPoint offset;
	// uint32 buttons;
	// BMessage* current = fOwner->CurrentMessage();
	// if (!current || current->FindPoint("be:view_where", &offset) != B_OK)
	// 	GetMouse(&offset, &buttons, false);
	// offset -= dragRect.LeftTop();

	// if (!dragRect.IsValid()) {
	// 	DragMessage(message, NULL, B_OP_BLEND, offset, replyTo);
	// 	return;
	// }

	// // TODO: that's not really what should happen - the app_server should take
	// // the chance *NOT* to need to drag a whole bitmap around but just a frame.

	// // create a drag bitmap for the rect
	// BBitmap* bitmap = new(std::nothrow) BBitmap(dragRect, B_RGBA32);
	// if (bitmap == NULL)
	// 	return;

	// uint32* bits = (uint32*)bitmap->Bits();
	// uint32 bytesPerRow = bitmap->BytesPerRow();
	// uint32 width = dragRect.IntegerWidth() + 1;
	// uint32 height = dragRect.IntegerHeight() + 1;
	// uint32 lastRow = (height - 1) * width;

	// memset(bits, 0x00, height * bytesPerRow);

	// // top
	// for (uint32 i = 0; i < width; i += 2)
	// 	bits[i] = 0xff000000;

	// // bottom
	// for (uint32 i = (height % 2 == 0 ? 1 : 0); i < width; i += 2)
	// 	bits[lastRow + i] = 0xff000000;

	// // left
	// for (uint32 i = 0; i < lastRow; i += width * 2)
	// 	bits[i] = 0xff000000;

	// // right
	// for (uint32 i = (width % 2 == 0 ? width : 0); i < lastRow; i += width * 2)
	// 	bits[width - 1 + i] = 0xff000000;

	// DragMessage(message, bitmap, B_OP_BLEND, offset, replyTo);
}


void
BView::DragMessage(BMessage* message, BBitmap* image, BPoint offset,
	BHandler* replyTo)
{
	DragMessage(message, image, B_OP_COPY, offset, replyTo);
}


void
BView::DragMessage(BMessage* message, BBitmap* image,
	drawing_mode dragMode, BPoint offset, BHandler* replyTo)
{
	if (message == NULL)
		return;

	// if (image == NULL) {
	// 	// TODO: workaround for drags without a bitmap - should not be necessary if
	// 	//	we move the rectangle dragging into the app_server
	// 	image = new(std::nothrow) BBitmap(BRect(0, 0, 0, 0), B_RGBA32);
	// 	if (image == NULL)
	// 		return;
	// }

	// if (replyTo == NULL)
	// 	replyTo = this;

	// if (replyTo->Looper() == NULL)
	// 	debugger("DragMessage: warning - the Handler needs a looper");

	// _CheckOwnerLock();

	// if (!message->HasInt32("buttons")) {
	// 	BMessage* msg = fOwner->CurrentMessage();
	// 	uint32 buttons;

	// 	if (msg == NULL
	// 		|| msg->FindInt32("buttons", (int32*)&buttons) != B_OK) {
	// 		BPoint point;
	// 		GetMouse(&point, &buttons, false);
	// 	}

	// 	message->AddInt32("buttons", buttons);
	// }

	// BMessage::Private privateMessage(message);
	// privateMessage.SetReply(BMessenger(replyTo, replyTo->Looper()));

	// int32 bufferSize = message->FlattenedSize();
	// char* buffer = new(std::nothrow) char[bufferSize];
	// if (buffer != NULL) {
	// 	message->Flatten(buffer, bufferSize);

	// 	fOwner->fLink->StartMessage(AS_VIEW_DRAG_IMAGE);
	// 	fOwner->fLink->Attach<int32>(image->_ServerToken());
	// 	fOwner->fLink->Attach<int32>((int32)dragMode);
	// 	fOwner->fLink->Attach<BPoint>(offset);
	// 	fOwner->fLink->Attach<int32>(bufferSize);
	// 	fOwner->fLink->Attach(buffer, bufferSize);

	// 	// we need to wait for the server
	// 	// to actually process this message
	// 	// before we can delete the bitmap
	// 	int32 code;
	// 	fOwner->fLink->FlushWithReply(code);

	// 	delete [] buffer;
	// } else {
	// 	fprintf(stderr, "BView::DragMessage() - no memory to flatten drag "
	// 		"message\n");
	// }

	// delete image;
}


void
BView::GetMouse(BPoint* _location, uint32* _buttons, bool checkMessageQueue)
{
	if (_location == NULL && _buttons == NULL)
		return;

	if (_location != NULL)
		_location->Set(fLastMousePosition.x, fLastMousePosition.y);
	if (_buttons != NULL)
		*_buttons = (fLastButtonState[B_PRIMARY_MOUSE_BUTTON] * B_PRIMARY_MOUSE_BUTTON)
			+ (fLastButtonState[B_SECONDARY_MOUSE_BUTTON] * B_SECONDARY_MOUSE_BUTTON)
			+ (fLastButtonState[B_TERTIARY_MOUSE_BUTTON] * B_TERTIARY_MOUSE_BUTTON);
}


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


BScrollBar*
BView::ScrollBar(orientation direction) const
{
	switch (direction) {
		case B_VERTICAL:
			return fVerScroller;

		case B_HORIZONTAL:
			return fHorScroller;

		default:
			return NULL;
	}
}


void
BView::ScrollBy(float deltaX, float deltaY)
{
	ScrollTo(BPoint(fBounds.left + deltaX, fBounds.top + deltaY));
}


void
BView::ScrollTo(BPoint where)
{
	// scrolling by fractional values is not supported
	where.x = roundf(where.x);
	where.y = roundf(where.y);

	// no reason to process this further if no scroll is intended.
	if (where.x == fBounds.left && where.y == fBounds.top)
		return;

	// make sure scrolling is within valid bounds
	if (fHorScroller) {
		float min, max;
		fHorScroller->GetRange(&min, &max);

		if (where.x < min)
			where.x = min;
		else if (where.x > max)
			where.x = max;
	}
	if (fVerScroller) {
		float min, max;
		fVerScroller->GetRange(&min, &max);

		if (where.y < min)
			where.y = min;
		else if (where.y > max)
			where.y = max;
	}

	_CheckLockAndSwitchCurrent();

	float xDiff = where.x - fBounds.left;
	float yDiff = where.y - fBounds.top;

	// we modify our bounds rectangle by deltaX/deltaY coord units hor/ver.
	fBounds.OffsetTo(where.x, where.y);

	// then set the new values of the scrollbars
	if (fHorScroller && xDiff != 0.0)
		fHorScroller->SetValue(fBounds.left);
	if (fVerScroller && yDiff != 0.0)
		fVerScroller->SetValue(fBounds.top);

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
	if (fOwner != NULL
		&& fOwner->CurrentMessage() != NULL
		&& fOwner->CurrentMessage()->what == B_MOUSE_DOWN) {
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
	_CheckOwnerLockAndSwitchCurrent();
	BPrivate::ViewState* state = new BPrivate::ViewState();
	*state = *fState;
	state->previous_state = fState;

	// initialize origin, scale and transform, new states start "clean".
	state->scale = 1.0f;
	state->origin.Set(0, 0);
	state->transform.Reset();
	// the app_server also reset alpha mask here

	fState = state;
}


void
BView::PopState()
{
	if (fState->previous_state == NULL) {
		printf("WARNING: BView::PopState() - no previous state to pop");
		return;
	}

	_CheckOwnerLockAndSwitchCurrent();

	ViewState* stateToDelete = fState;
	fState = fState->previous_state;
	delete stateToDelete;
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
BView::SetTransform(BAffineTransform transform)
{
	fState->transform = transform;
	fState->archiving_flags |= B_VIEW_TRANSFORM_BIT;
}


BAffineTransform
BView::Transform() const
{

	return fState->transform;
}


BAffineTransform
BView::TransformTo(coordinate_space basis) const
{
	if (basis == B_CURRENT_STATE_COORDINATES)
		return B_AFFINE_IDENTITY_TRANSFORM;

	BAffineTransform transform = fState->parent_composite_transform * Transform();
	float scale = fState->parent_composite_scale * Scale();
	transform.PreScaleBy(scale, scale);
	BPoint origin = Origin();
	origin.x *= fState->parent_composite_scale;
	origin.y *= fState->parent_composite_scale;
	origin += fState->parent_composite_origin;
	transform.TranslateBy(origin);

	if (basis == B_PREVIOUS_STATE_COORDINATES) {
		transform.TranslateBy(-fState->parent_composite_origin);
		transform.PreMultiplyInverse(fState->parent_composite_transform);
		transform.ScaleBy(1.0f / fState->parent_composite_scale);
		return transform;
	}

	if (basis == B_VIEW_COORDINATES)
		return transform;

	origin = B_ORIGIN;

	if (basis == B_PARENT_VIEW_COORDINATES || basis == B_PARENT_VIEW_DRAW_COORDINATES) {
		BView* parent = Parent();
		if (parent != NULL) {
			ConvertToParent(&origin);
			transform.TranslateBy(origin);
			if (basis == B_PARENT_VIEW_DRAW_COORDINATES)
				transform = transform.PreMultiplyInverse(parent->TransformTo(B_VIEW_COORDINATES));
			return transform;
		}
		basis = B_WINDOW_COORDINATES;
	}

	//ConvertToScreen(&origin);
	//if (basis == B_WINDOW_COORDINATES) {
	//	BWindow* window = Window();
	//	if (window != NULL)
	//		origin -= window->Frame().LeftTop();
	//}
	//transform.TranslateBy(origin);
	return transform;
}


void
BView::TranslateBy(double x, double y)
{
	// TODO
	fState->archiving_flags |= B_VIEW_TRANSFORM_BIT;
}


void
BView::ScaleBy(double x, double y)
{
	// TODO

	fState->archiving_flags |= B_VIEW_TRANSFORM_BIT;
}


void
BView::RotateBy(double angleRadians)
{
	// TODO

	fState->archiving_flags |= B_VIEW_TRANSFORM_BIT;
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
	return fState->line_join;
}


cap_mode
BView::LineCapMode() const
{
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

	fState->archiving_flags |= B_VIEW_FILL_RULE_BIT;
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
	fState->archiving_flags |= B_VIEW_DRAWING_MODE_BIT;
}


drawing_mode
BView::DrawingMode() const
{
	return fState->drawing_mode;
}


void
BView::SetBlendingMode(source_alpha sourceAlpha, alpha_function alphaFunction)
{
	if (sourceAlpha == fState->alpha_source_mode
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
	if (fState->which_high_color == which
		&& fState->which_high_color_tint == tint)
		return;

	fState->which_high_color = which;
	fState->which_high_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_HIGH_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_HIGH_COLOR_BIT;

		fState->high_color = tint_color(ui_color(which), tint);
	} else {
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
	if (fState->which_low_color == which
		&& fState->which_low_color_tint == tint)
		return;

	fState->which_low_color = which;
	fState->which_low_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_LOW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_LOW_COLOR_BIT;

		fState->low_color = tint_color(ui_color(which), tint);
	} else {
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
	if (view == NULL || (view->Window() != NULL && !view->LockLooper()))
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
	if (fState->which_view_color == which
		&& fState->which_view_color_tint == tint)
		return;

	fState->which_view_color = which;
	fState->which_view_color_tint = tint;

	if (which != B_NO_COLOR) {
		fState->archiving_flags |= B_VIEW_WHICH_VIEW_COLOR_BIT;
		fState->archiving_flags &= ~B_VIEW_VIEW_COLOR_BIT;

		fState->view_color = tint_color(ui_color(which), tint);
	} else {
		fState->archiving_flags &= ~B_VIEW_WHICH_VIEW_COLOR_BIT;
	}
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
		if (mask & B_FONT_FAMILY_AND_STYLE) {
			font_family family;
			font_style style;
			
			font->GetFamilyAndStyle(&family, &style);
			fState->font.SetFamilyAndStyle(family, style);
		}

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
	return fState->font.StringWidth(string, length);
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

	// FIXME: this may or may not want the intersection of this and the local clipping region
	// plus we need to intersect with all previous state clipping regions (see GetCombinedClippingRegion()).
	// Hardly ever used, so not worth the trouble for now.
	*region = fState->clipping_region;
}


void
BView::ConstrainClippingRegion(BRegion* region)
{
	// Null region means to reset clipping region to default
	if (!region) {
		fState->clipping_region = BRegion(Bounds());
		fState->clipping_region_used = false;
		return;
	}

	fState->clipping_region = *region;
	fState->clipping_region_used = true;
	fState->archiving_flags |= B_VIEW_CLIP_REGION_BIT;
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


void
BView::ClipToShape(BShape* shape)
{
	_ClipToShape(shape, false);
}


void
BView::ClipToInverseShape(BShape* shape)
{
	_ClipToShape(shape, true);
}


//	#pragma mark - Drawing Functions


void
BView::DrawBitmapAsync(const BBitmap* bitmap, BRect bitmapRect /* source */, BRect viewRect /* dest */,
	uint32 options)
{
	if (bitmap == NULL || fOwner == NULL
		|| !bitmapRect.IsValid() || !viewRect.IsValid())
		return;

	_CheckLockAndSwitchCurrent();

	int height = bitmap->Bounds().IntegerHeight() + 1;
	int width = bitmap->Bounds().IntegerWidth() + 1;
	cairo_format_t format = color_space_to_cairo_format(bitmap->ColorSpace());
	int stride = BPrivate::get_bytes_per_row(bitmap->ColorSpace(), width);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	
	// Place a bitmap image in the view within the designated destination rectangle.
	// The point and the destination rectangle are stated in the BView's coordinate system.

	// If a source rectangle is given, only that part of the bitmap image is drawn. Otherwise,
	// the entire bitmap is placed in the view. The source rectangle is stated in the internal
	// coordinates of the BBitmap object.

	// If the source image is bigger than the destination rectangle, it's scaled to fit.

	//viewRect.PrintToStream();
	//bitmapRect.PrintToStream();
	//printf("bits length: %ld\n", bitmap->BitsLength());
	//printf("format: %ld\n", bitmap->ColorSpace());

	cairo_surface_t *imageSurface = cairo_image_surface_create_for_data((unsigned char*)bitmap->Bits(), format, width, height, stride);

	double xScale = viewRect.Width() / bitmapRect.Width();
	double yScale = viewRect.Height() / bitmapRect.Height();
	cairo_translate(cr, viewRect.left - (viewRect.left * xScale), viewRect.top - (viewRect.top * yScale));
	cairo_scale(cr, xScale, yScale);

	cairo_set_source_surface(cr, imageSurface, viewRect.left - bitmapRect.left, viewRect.top - bitmapRect.top);

	cairo_rectangle(cr, viewRect.left - bitmapRect.left - 0.5, viewRect.top - bitmapRect.top - 0.5, bitmapRect.Width() + 1, bitmapRect.Height() + 1);
	cairo_fill(cr);
	cairo_surface_destroy(imageSurface);
#endif
}


void
BView::DrawBitmapAsync(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect)
{
	DrawBitmapAsync(bitmap, bitmapRect, viewRect, 0);
}


void
BView::DrawBitmapAsync(const BBitmap* bitmap, BRect viewRect)
{
	if (bitmap && fOwner) {
		DrawBitmapAsync(bitmap, bitmap->Bounds().OffsetToCopy(B_ORIGIN),
			viewRect, 0);
	}
}


void
BView::DrawBitmapAsync(const BBitmap* bitmap, BPoint where)
{
	if (bitmap == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	int height = bitmap->Bounds().IntegerHeight() + 1;
	int width = bitmap->Bounds().IntegerWidth() + 1;
	cairo_format_t format = color_space_to_cairo_format(bitmap->ColorSpace());
	int stride = BPrivate::get_bytes_per_row(bitmap->ColorSpace(), width);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_surface_t *imageSurface = cairo_image_surface_create_for_data((unsigned char*)bitmap->Bits(), format, width, height, stride);
	cairo_set_source_surface(cr, imageSurface, where.x, where.y);
	cairo_rectangle(cr, where.x, where.y, width, height);
	cairo_fill(cr);
	cairo_surface_destroy(imageSurface);
#endif
}


void
BView::DrawBitmapAsync(const BBitmap* bitmap)
{
	DrawBitmapAsync(bitmap, PenLocation());
}


void
BView::DrawBitmap(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect,
	uint32 options)
{
	if (fOwner) {
		DrawBitmapAsync(bitmap, bitmapRect, viewRect, options);
		Sync();
	}
}


void
BView::DrawBitmap(const BBitmap* bitmap, BRect bitmapRect, BRect viewRect)
{
	if (fOwner) {
		DrawBitmapAsync(bitmap, bitmapRect, viewRect, 0);
		Sync();
	}
}


void
BView::DrawBitmap(const BBitmap* bitmap, BRect viewRect)
{
	if (bitmap && fOwner) {
		DrawBitmap(bitmap, bitmap->Bounds().OffsetToCopy(B_ORIGIN), viewRect,
			0);
	}
}


void
BView::DrawBitmap(const BBitmap* bitmap, BPoint where)
{
	if (fOwner) {
		DrawBitmapAsync(bitmap, where);
		Sync();
	}
}


void
BView::DrawBitmap(const BBitmap* bitmap)
{
	DrawBitmap(bitmap, PenLocation());
}


void
BView::DrawTiledBitmapAsync(const BBitmap* bitmap, BRect viewRect,
	BPoint phase)
{
	if (bitmap == NULL || fOwner == NULL || !viewRect.IsValid())
		return;

	_CheckLockAndSwitchCurrent();

	// FIXME
	//ViewDrawBitmapInfo info;
	//info.bitmapToken = bitmap->_ServerToken();
	//info.options = B_TILE_BITMAP;
	//info.viewRect = viewRect;
	//info.bitmapRect = bitmap->Bounds().OffsetToCopy(phase);

	//fOwner->fLink->StartMessage(AS_VIEW_DRAW_BITMAP);
	//fOwner->fLink->Attach<ViewDrawBitmapInfo>(info);
}


void
BView::DrawTiledBitmap(const BBitmap* bitmap, BRect viewRect, BPoint phase)
{
	if (fOwner) {
		DrawTiledBitmapAsync(bitmap, viewRect, phase);
		Sync();
	}
}


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

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	/* Create a PangoLayout, set the font and draw the text */
	PangoLayout *layout = pango_cairo_create_layout(cr);

	PangoFontDescription *desc = (PangoFontDescription*)fState->font.GetPangoFontDescription();
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	if (fState->font.Flags() & B_DISABLE_ANTIALIASING) {
		PangoContext *pctx = pango_layout_get_context(layout);
		cairo_font_options_t *options = cairo_font_options_create();
		cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_NONE);
		pango_cairo_context_set_font_options(pctx, options);
		cairo_font_options_destroy(options);
	}

	font_height height;
	fState->font.GetHeight(&height);

	cairo_move_to(cr, location.x, location.y - height.ascent - 0.5);
	cairo_rotate(cr, fState->font.Rotation());	// FIXME - we need to transform this to get rotation around the center
	pango_layout_set_text(layout, string, length);
	pango_cairo_show_layout(cr, layout);

	/* free the layout object */
	g_object_unref(layout);
#endif
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

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	PangoFontDescription *desc = (PangoFontDescription*)fState->font.GetPangoFontDescription();

	// Create a PangoLayout, set the font and draw the text
	for (int32 i = 0; i < locationCount; i++) {
		// Surely some of this can be factored out of the loop
		PangoLayout *layout = pango_cairo_create_layout(cr);

		pango_layout_set_text(layout, string, length);
		pango_layout_set_font_description(layout, desc);

		cairo_move_to(cr, locations[i].x, locations[i].y - fState->font.Size()); // FIXME - Fudge factor
		pango_cairo_show_layout(cr, layout);

		/* free the layout object */
		g_object_unref (layout);
	}

	pango_font_description_free(desc);
#endif
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

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
				radius, 0, 2*M_PI);
	cairo_stroke(cr);
#endif
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

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
				radius, 0, 2*M_PI);
	cairo_fill(cr);
#endif
}


void
BView::FillEllipse(BRect rect, const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
		radius, 0, 2*M_PI);
	cairo_fill(cr);
#endif
}


void
BView::StrokeArc(BPoint center, float xRadius, float yRadius, float startAngle,
	float arcAngle, ::pattern pattern)
{
	StrokeArc(BRect(center.x - xRadius, center.y - yRadius, center.x + xRadius,
		center.y + yRadius), startAngle, arcAngle, pattern);
}


void
BView::StrokeArc(BRect rect, float startAngle, float arcAngle,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
		radius, startAngle, arcAngle);
	cairo_stroke(cr);
#endif
}


void
BView::FillArc(BPoint center,float xRadius, float yRadius, float startAngle,
	float arcAngle, ::pattern pattern)
{
	FillArc(BRect(center.x - xRadius, center.y - yRadius, center.x + xRadius,
		center.y + yRadius), startAngle, arcAngle, pattern);
}


void
BView::FillArc(BPoint center,float xRadius, float yRadius, float startAngle,
	float arcAngle, const BGradient& gradient)
{
	FillArc(BRect(center.x - xRadius, center.y - yRadius, center.x + xRadius,
		center.y + yRadius), startAngle, arcAngle, gradient);
}


void
BView::FillArc(BRect rect, float startAngle, float arcAngle,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);
#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
		radius, startAngle, arcAngle);
	cairo_fill(cr);
#endif
}


void
BView::FillArc(BRect rect, float startAngle, float arcAngle,
	const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);
	double radius = rect.Width() / 2.0;

	cairo_arc(cr, rect.left + radius, rect.top + radius,
		radius, startAngle, arcAngle);
	cairo_fill(cr);
#endif
}


void
BView::StrokeBezier(BPoint* controlPoints, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_move_to(cr, controlPoints[0].x, controlPoints[0].y);
	cairo_curve_to(cr, controlPoints[1].x, controlPoints[1].y,
		controlPoints[2].x, controlPoints[2].y, controlPoints[3].x,
		controlPoints[3].y);
	cairo_stroke(cr);
#endif
}


void
BView::FillBezier(BPoint* controlPoints, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_move_to(cr, controlPoints[0].x, controlPoints[0].y);
	cairo_curve_to(cr, controlPoints[1].x, controlPoints[1].y,
		controlPoints[2].x, controlPoints[2].y, controlPoints[3].x,
		controlPoints[3].y);
	cairo_fill(cr);
#endif
}


void
BView::FillBezier(BPoint* controlPoints, const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);

	cairo_move_to(cr, controlPoints[0].x, controlPoints[0].y);
	cairo_curve_to(cr, controlPoints[1].x, controlPoints[1].y,
		controlPoints[2].x, controlPoints[2].y, controlPoints[3].x,
		controlPoints[3].y);
	cairo_fill(cr);
#endif
}


void
BView::StrokeRect(BRect rect, ::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_rectangle(cr, rect.left, rect.top, rect.Width(), rect.Height());
	cairo_stroke(cr);
#endif
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

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_rectangle(cr, rect.left - 0.5, rect.top - 0.5, rect.Width() + 1, rect.Height() + 1);
	cairo_fill(cr);
#endif
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

	_CheckLockAndSwitchCurrent();

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);
	cairo_rectangle(cr, rect.left - 0.5, rect.top - 0.5, rect.Width() + 1, rect.Height() + 1);
    cairo_fill(cr);
#endif
}


void
BView::StrokeRoundRect(BRect rect, float xRadius, float yRadius,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
	_UpdatePattern(pattern);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	double x = rect.left;
	double y = rect.top;
	double w = rect.Width();
	double h = rect.Height();

#if 0
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
#else
    float ARC_TO_BEZIER = 0.55228475;
    if (xRadius > w - xRadius)
		xRadius = w / 2;

    if (yRadius > h - yRadius)
		yRadius = h / 2;

    // approximate (quite close) the arc using a bezier curve
    float c1 = ARC_TO_BEZIER * xRadius;
    float c2 = ARC_TO_BEZIER * yRadius;

    cairo_new_path(cr);
    cairo_move_to (cr,  x + xRadius, y);
    cairo_rel_line_to (cr,  w - 2 * xRadius, 0.0);
    cairo_rel_curve_to (cr,  c1, 0.0, xRadius, c2, xRadius, yRadius);
    cairo_rel_line_to (cr,  0, h - 2 * yRadius);
    cairo_rel_curve_to (cr,  0.0, c2, c1 - xRadius, yRadius, -xRadius, yRadius);
    cairo_rel_line_to (cr,  -w + 2 * xRadius, 0);
    cairo_rel_curve_to (cr,  -c1, 0, -xRadius, -c2, -xRadius, -yRadius);
    cairo_rel_line_to (cr, 0, -h + 2 * yRadius);
    cairo_rel_curve_to (cr, 0.0, -c2, xRadius - c1, -yRadius, xRadius, -yRadius);
    cairo_close_path (cr);
#endif

	cairo_stroke(cr);

#endif
}


void
BView::FillRoundRect(BRect rect, float xRadius, float yRadius,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	double x = rect.left;
	double y = rect.top;
	double w = rect.Width();
	double h = rect.Height();

#if 0
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
#else
    float ARC_TO_BEZIER = 0.55228475;
    if (xRadius > w - xRadius)
		xRadius = w / 2;

    if (yRadius > h - yRadius)
		yRadius = h / 2;

    // approximate (quite close) the arc using a bezier curve
    float c1 = ARC_TO_BEZIER * xRadius;
    float c2 = ARC_TO_BEZIER * yRadius;

    cairo_new_path(cr);
    cairo_move_to (cr,  x + xRadius, y);
    cairo_rel_line_to (cr,  w - 2 * xRadius, 0.0);
    cairo_rel_curve_to (cr,  c1, 0.0, xRadius, c2, xRadius, yRadius);
    cairo_rel_line_to (cr,  0, h - 2 * yRadius);
    cairo_rel_curve_to (cr,  0.0, c2, c1 - xRadius, yRadius, -xRadius, yRadius);
    cairo_rel_line_to (cr,  -w + 2 * xRadius, 0);
    cairo_rel_curve_to (cr,  -c1, 0, -xRadius, -c2, -xRadius, -yRadius);
    cairo_rel_line_to (cr, 0, -h + 2 * yRadius);
    cairo_rel_curve_to (cr, 0.0, -c2, xRadius - c1, -yRadius, xRadius, -yRadius);
    cairo_close_path (cr);
#endif

	cairo_fill(cr);
#endif
}


void
BView::FillRoundRect(BRect rect, float xRadius, float yRadius,
	const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

#if DRAW

	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);

	double x = rect.left;
	double y = rect.top;
	double w = rect.Width();
	double h = rect.Height();
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
#endif
}


void
BView::FillRegion(BRegion* region, ::pattern pattern)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	uint32 rects = region->CountRects();

	for (uint32 i = 0; i < rects; i++) {
		cairo_rectangle(cr, region->RectAt(i).left - 0.5,
			region->RectAt(i).top - 0.5,
			region->RectAt(i).Width() + 1,
			region->RectAt(i).Height() + 1);
	}
	cairo_fill(cr);
#endif
}


void
BView::FillRegion(BRegion* region, const BGradient& gradient)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);

	uint32 rects = region->CountRects();

	for (uint32 i = 0; i < rects; i++) {
		cairo_rectangle(cr, region->RectAt(i).left - 0.5,
			region->RectAt(i).top -0.5 ,
			region->RectAt(i).Width() + 1,
			region->RectAt(i).Height() + 1);
	}
	cairo_fill(cr);
#endif
}


void
BView::StrokeTriangle(BPoint point1, BPoint point2, BPoint point3, BRect bounds,
	::pattern pattern)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	_UpdatePattern(pattern);
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_move_to(cr, point1.x, point1.y);
	cairo_line_to(cr, point2.x, point2.y);
	cairo_line_to(cr, point3.x, point3.y);
	cairo_close_path(cr);

	cairo_stroke(cr);
#endif
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
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_move_to(cr, point1.x, point1.y);
	cairo_line_to(cr, point2.x, point2.y);
	cairo_line_to(cr, point3.x, point3.y);
	cairo_close_path(cr);
	cairo_fill(cr);
#endif
}


void
BView::FillTriangle(BPoint point1, BPoint point2, BPoint point3, BRect bounds,
	const BGradient& gradient)
{
	if (fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cr.AddGradient(gradient);

	cairo_move_to(cr, point1.x, point1.y);
	cairo_line_to(cr, point2.x, point2.y);
	cairo_line_to(cr, point3.x, point3.y);
	cairo_close_path(cr);
	cairo_fill(cr);
#endif
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
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	cairo_move_to(cr, start.x, start.y);
	cairo_line_to(cr, end.x, end.y);
	cairo_stroke(cr);
#endif
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
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	CairoShapeIterator it(cr.Context());
	it.Iterate(shape);
	cairo_stroke(cr);
#endif
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
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	CairoShapeIterator it(cr.Context());
	it.Iterate(shape);
	cairo_fill(cr);
#endif
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
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	CairoShapeIterator it(cr.Context());
	cr.AddGradient(gradient);
	it.Iterate(shape);
	cairo_fill(cr);
#endif
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

	_CheckLockAndSwitchCurrent();
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	for (uint32 i = 0; i < fCommArray->count; i++) {
        cairo_set_source_rgb(cr,
            rgb_to_cairo_color(fCommArray->array[i].color.red),
            rgb_to_cairo_color(fCommArray->array[i].color.green),
            rgb_to_cairo_color(fCommArray->array[i].color.blue));

        BPoint start = fCommArray->array[i].startPoint;
        BPoint end = fCommArray->array[i].endPoint;
        cairo_move_to(cr, start.x, start.y);
        cairo_line_to(cr, end.x, end.y);
	}
	cairo_stroke(cr);
#endif
	_RemoveCommArray();
}

void
BView::SetViewBitmap(const BBitmap* bitmap, BRect srcRect, BRect dstRect,
	uint32 followFlags, uint32 options)
{
	_SetViewBitmap(bitmap, srcRect, dstRect, followFlags, options);
}


void
BView::SetViewBitmap(const BBitmap* bitmap, uint32 followFlags, uint32 options)
{
	BRect rect;
 	if (bitmap)
		rect = bitmap->Bounds();

 	rect.OffsetTo(B_ORIGIN);

	_SetViewBitmap(bitmap, rect, rect, followFlags, options);
}


void
BView::ClearViewBitmap()
{
	_SetViewBitmap(NULL, BRect(), BRect(), 0, 0);
}


void
BView::CopyBits(BRect src, BRect dst)
{
	if (fOwner == NULL)
		return;

	if (!src.IsValid() || !dst.IsValid())
		return;

	_CheckLockAndSwitchCurrent();

#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);

	// The surface is owned by the context and should not be deleted/freed after use
	cairo_surface_t* src_surface = cairo_get_target(cr);

	cairo_set_source_surface(cr, src_surface, src.left, src.top);
	cairo_rectangle(cr, dst.left - 0.5, dst.top - 0.5, dst.Width() + 1, dst.Height() + 1);
	cairo_fill(cr);
#endif
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

	_CheckLockAndSwitchCurrent();

	if (fOwner) {
		widget_schedule_redraw(view_widget);
	}
}


void
BView::Invalidate(const BRegion* region)
{
	if (region == NULL || fOwner == NULL)
		return;

	_CheckLockAndSwitchCurrent();

	// TODO better
	Invalidate(region->Frame());
}


void
BView::Invalidate()
{
	Invalidate(Bounds());
}


void
BView::DelayedInvalidate(bigtime_t delay)
{
	DelayedInvalidate(delay, Bounds());
}


void
BView::DelayedInvalidate(bigtime_t delay, BRect invalRect)
{
	if (fOwner == NULL)
		return;

	invalRect.left = (int)invalRect.left;
	invalRect.top = (int)invalRect.top;
	invalRect.right = (int)invalRect.right;
	invalRect.bottom = (int)invalRect.bottom;
	if (!invalRect.IsValid())
		return;

	_CheckLockAndSwitchCurrent();

	Invalidate(invalRect);	// FIXME
}


void
BView::InvertRect(BRect rect)
{
#if DRAW
	CairoContext cr(view_widget, fState, &fLocalClipping, &fBounds);
	cairo_rectangle(cr, rect.left - 0.5, rect.top - 0.5, rect.Width() + 1, rect.Height() + 1);
	cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_fill(cr);
#endif
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
	_UpdateViewClippingRegion(false);

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
		_Detach();
	}

	BView* parent = fParent;
	if (!parent || !parent->_RemoveChildFromList(this))
		return false;

	//if (owner != NULL && !fTopLevelView) {
		// the top level view is deleted by the app_server automatically
		// owner->fLink->StartMessage(AS_VIEW_DELETE);
		// owner->fLink->Attach<int32>(_get_object_token_(this));
	//}

	parent->InvalidateLayout();
	parent->_UpdateViewClippingRegion(false);

	if (view_widget != NULL)
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


status_t
BView::GetSupportedSuites(BMessage* data)
{
	if (data == NULL)
		return B_BAD_VALUE;

	status_t status = data->AddString("suites", "suite/vnd.Be-view");
	BPropertyInfo propertyInfo(sViewPropInfo);
	if (status == B_OK)
		status = data->AddFlat("messages", &propertyInfo);
	if (status == B_OK)
		return BHandler::GetSupportedSuites(data);
	return status;
}


BHandler*
BView::ResolveSpecifier(BMessage* message, int32 index, BMessage* specifier,
	int32 what, const char* property)
{
	if (message->what == B_WINDOW_MOVE_BY
		|| message->what == B_WINDOW_MOVE_TO) {
		return this;
	}

	BPropertyInfo propertyInfo(sViewPropInfo);
	status_t err = B_BAD_SCRIPT_SYNTAX;
	BMessage replyMsg(B_REPLY);

	switch (propertyInfo.FindMatch(message, index, specifier, what, property)) {
		case 0:
		case 1:
		case 3:
			return this;

		case 2:
			if (fShelf) {
				message->PopSpecifier();
				return fShelf;
			}

			err = B_NAME_NOT_FOUND;
			replyMsg.AddString("message", "This window doesn't have a shelf");
			break;

		case 4:
		{
			if (!fFirstChild) {
				err = B_NAME_NOT_FOUND;
				replyMsg.AddString("message", "This window doesn't have "
					"children.");
				break;
			}
			BView* child = NULL;
			switch (what) {
				case B_INDEX_SPECIFIER:
				{
					int32 index;
					err = specifier->FindInt32("index", &index);
					if (err == B_OK)
						child = ChildAt(index);
					break;
				}
				case B_REVERSE_INDEX_SPECIFIER:
				{
					int32 rindex;
					err = specifier->FindInt32("index", &rindex);
					if (err == B_OK)
						child = ChildAt(CountChildren() - rindex);
					break;
				}
				case B_NAME_SPECIFIER:
				{
					const char* name;
					err = specifier->FindString("name", &name);
					if (err == B_OK)
						child = FindView(name);
					break;
				}
			}

			if (child != NULL) {
				message->PopSpecifier();
				return child;
			}

			if (err == B_OK)
				err = B_BAD_INDEX;

			replyMsg.AddString("message",
				"Cannot find view at/with specified index/name.");
			break;
		}

		default:
			return BHandler::ResolveSpecifier(message, index, specifier, what,
				property);
	}

	if (err < B_OK) {
		replyMsg.what = B_MESSAGE_NOT_UNDERSTOOD;

		if (err == B_BAD_SCRIPT_SYNTAX)
			replyMsg.AddString("message", "Didn't understand the specifier(s)");
		else
			replyMsg.AddString("message", strerror(err));
	}

	replyMsg.AddInt32("error", err);
	message->SendReply(&replyMsg);
	return NULL;
}


void
BView::MessageReceived(BMessage* message)
{
	if (!message->HasSpecifiers()) {
		switch (message->what) {
			case B_INVALIDATE:
			{
				BRect rect;
				if (message->FindRect("be:area", &rect) == B_OK)
					Invalidate(rect);
				else
					Invalidate();
				break;
			}

			case B_KEY_DOWN:
			{
				// TODO: cannot use "string" here if we support having different
				// font encoding per view (it's supposed to be converted by
				// BWindow::_HandleKeyDown() one day)
				const char* string;
				ssize_t bytes;
				if (message->FindData("bytes", B_STRING_TYPE,
						(const void**)&string, &bytes) == B_OK)
					KeyDown(string, bytes - 1);
				break;
			}

			case B_KEY_UP:
			{
				// TODO: same as above
				const char* string;
				ssize_t bytes;
				if (message->FindData("bytes", B_STRING_TYPE,
						(const void**)&string, &bytes) == B_OK)
					KeyUp(string, bytes - 1);
				break;
			}

			case B_VIEW_RESIZED:
				FrameResized(message->GetInt32("width", 0),
					message->GetInt32("height", 0));
				break;

			case B_VIEW_MOVED:
				FrameMoved(fParentOffset);
				break;

			case B_MOUSE_DOWN:
			{
				BPoint where;
				message->FindPoint("be:view_where", &where);
				printf("BView::B_MOUSE_DOWN at (%f, %f)\n", where.x, where.y);
				MouseDown(where);
				break;
			}

			case B_MOUSE_IDLE:
			{
				BPoint where;
				if (message->FindPoint("be:view_where", &where) != B_OK)
					break;

				// BToolTip* tip;
				// if (GetToolTipAt(where, &tip))
				// 	ShowToolTip(tip);
				// else
					BHandler::MessageReceived(message);
				break;
			}

			case B_MOUSE_MOVED:
			{
				uint32 eventOptions = fEventOptions | fMouseEventOptions;
				bool noHistory = eventOptions & B_NO_POINTER_HISTORY;
				bool dropIfLate = !(eventOptions & B_FULL_POINTER_HISTORY);

				bigtime_t eventTime;
				if (message->FindInt64("when", (int64*)&eventTime) < B_OK)
					eventTime = system_time();

				uint32 transit;
				message->FindInt32("be:transit", (int32*)&transit);
				// don't drop late messages with these important transit values
				if (transit == B_ENTERED_VIEW || transit == B_EXITED_VIEW)
					dropIfLate = false;

				// TODO: The dropping code may have the following problem: On
				// slower computers, 20ms may just be to abitious a delay.
				// There, we might constantly check the message queue for a
				// newer message, not find any, and still use the only but later
				// than 20ms message, which of course makes the whole thing
				// later than need be. An adaptive delay would be kind of neat,
				// but would probably use additional BWindow members to count
				// the successful versus fruitless queue searches and the delay
				// value itself or something similar.
				if (noHistory
					|| (dropIfLate && (system_time() - eventTime > 20000))) {
					// filter out older mouse moved messages in the queue
					BWindow* window = Window();
					window->_DequeueAll();
					BMessageQueue* queue = window->MessageQueue();
					queue->Lock();

					BMessage* moved;
					for (int32 i = 0; (moved = queue->FindMessage(i)) != NULL;
						 i++) {
						if (moved != message && moved->what == B_MOUSE_MOVED) {
							// there is a newer mouse moved message in the
							// queue, just ignore the current one, the newer one
							// will be handled here eventually
							queue->Unlock();
							return;
						}
					}
					queue->Unlock();
				}

				BPoint where;
				uint32 buttons;
				message->FindPoint("be:view_where", &where);
				message->FindInt32("buttons", (int32*)&buttons);

				if (transit == B_EXITED_VIEW || transit == B_OUTSIDE_VIEW)
					HideToolTip();

				BMessage* dragMessage = NULL;
				if (message->HasMessage("be:drag_message")) {
					dragMessage = new BMessage();
					if (message->FindMessage("be:drag_message", dragMessage)
						!= B_OK) {
						delete dragMessage;
						dragMessage = NULL;
					}
				}

				MouseMoved(where, transit, dragMessage);
				delete dragMessage;
				break;
			}

			case B_MOUSE_UP:
			{
				BPoint where;
				message->FindPoint("be:view_where", &where);
				fMouseEventOptions = 0;
				MouseUp(where);
				break;
			}

			case B_MOUSE_WHEEL_CHANGED:
			{
				BScrollBar* horizontal = ScrollBar(B_HORIZONTAL);
				BScrollBar* vertical = ScrollBar(B_VERTICAL);
				if (horizontal == NULL && vertical == NULL) {
					// Pass the message to the next handler
					BHandler::MessageReceived(message);
					break;
				}

				float deltaX = 0.0f;
				float deltaY = 0.0f;

				if (horizontal != NULL)
					message->FindFloat("be:wheel_delta_x", &deltaX);

				if (vertical != NULL)
					message->FindFloat("be:wheel_delta_y", &deltaY);

				if (deltaX == 0.0f && deltaY == 0.0f)
					break;

				if ((modifiers() & B_CONTROL_KEY) != 0)
					std::swap(horizontal, vertical);

				if (horizontal != NULL && deltaX != 0.0f)
					ScrollWithMouseWheelDelta(horizontal, deltaX);

				if (vertical != NULL && deltaY != 0.0f)
					ScrollWithMouseWheelDelta(vertical, deltaY);

				break;
			}

			// prevent message repeats
			case B_COLORS_UPDATED:
			case B_FONTS_UPDATED:
				break;

			case B_SCREEN_CHANGED:
			case B_WORKSPACE_ACTIVATED:
			case B_WORKSPACES_CHANGED:
			{
				BWindow* window = Window();
				if (window == NULL)
					break;

				// propagate message to child views
				int32 childCount = CountChildren();
				for (int32 i = 0; i < childCount; i++) {
					BView* view = ChildAt(i);
					if (view != NULL)
						window->PostMessage(message, view);
				}
				break;
			}

			default:
				BHandler::MessageReceived(message);
				break;
		}

		return;
	}

	// Scripting message

	BMessage replyMsg(B_REPLY);
	status_t err = B_BAD_SCRIPT_SYNTAX;
	int32 index;
	BMessage specifier;
	int32 what;
	const char* property;

	if (message->GetCurrentSpecifier(&index, &specifier, &what, &property)
			!= B_OK) {
		return BHandler::MessageReceived(message);
	}

	BPropertyInfo propertyInfo(sViewPropInfo);
	switch (propertyInfo.FindMatch(message, index, &specifier, what,
			property)) {
		case 0:
			if (message->what == B_GET_PROPERTY) {
				err = replyMsg.AddRect("result", Frame());
			} else if (message->what == B_SET_PROPERTY) {
				BRect newFrame;
				err = message->FindRect("data", &newFrame);
				if (err == B_OK) {
					MoveTo(newFrame.LeftTop());
					ResizeTo(newFrame.Width(), newFrame.Height());
				}
			}
			break;
		case 1:
			if (message->what == B_GET_PROPERTY) {
				err = replyMsg.AddBool("result", IsHidden());
			} else if (message->what == B_SET_PROPERTY) {
				bool newHiddenState;
				err = message->FindBool("data", &newHiddenState);
				if (err == B_OK) {
					if (newHiddenState == true)
						Hide();
					else
						Show();
				}
			}
			break;
		case 3:
			err = replyMsg.AddInt32("result", CountChildren());
			break;
		default:
			return BHandler::MessageReceived(message);
	}

	if (err != B_OK) {
		replyMsg.what = B_MESSAGE_NOT_UNDERSTOOD;

		if (err == B_BAD_SCRIPT_SYNTAX)
			replyMsg.AddString("message", "Didn't understand the specifier(s)");
		else
			replyMsg.AddString("message", strerror(err));

		replyMsg.AddInt32("error", err);
	}

	message->SendReply(&replyMsg);
}


status_t
BView::Perform(perform_code code, void* _data)
{
	switch (code) {
		case PERFORM_CODE_MIN_SIZE:
			((perform_data_min_size*)_data)->return_value
				= BView::MinSize();
			return B_OK;
		case PERFORM_CODE_MAX_SIZE:
			((perform_data_max_size*)_data)->return_value
				= BView::MaxSize();
			return B_OK;
		case PERFORM_CODE_PREFERRED_SIZE:
			((perform_data_preferred_size*)_data)->return_value
				= BView::PreferredSize();
			return B_OK;
		case PERFORM_CODE_LAYOUT_ALIGNMENT:
			((perform_data_layout_alignment*)_data)->return_value
				= BView::LayoutAlignment();
			return B_OK;
		case PERFORM_CODE_HAS_HEIGHT_FOR_WIDTH:
			((perform_data_has_height_for_width*)_data)->return_value
				= BView::HasHeightForWidth();
			return B_OK;
		case PERFORM_CODE_GET_HEIGHT_FOR_WIDTH:
		{
			perform_data_get_height_for_width* data
				= (perform_data_get_height_for_width*)_data;
			BView::GetHeightForWidth(data->width, &data->min, &data->max,
				&data->preferred);
			return B_OK;
		}
		case PERFORM_CODE_SET_LAYOUT:
		{
			perform_data_set_layout* data = (perform_data_set_layout*)_data;
			BView::SetLayout(data->layout);
			return B_OK;
		}
		case PERFORM_CODE_LAYOUT_INVALIDATED:
		{
			perform_data_layout_invalidated* data
				= (perform_data_layout_invalidated*)_data;
			BView::LayoutInvalidated(data->descendants);
			return B_OK;
		}
		case PERFORM_CODE_DO_LAYOUT:
		{
			BView::DoLayout();
			return B_OK;
		}
		case PERFORM_CODE_LAYOUT_CHANGED:
		{
			BView::LayoutChanged();
			return B_OK;
		}
		case PERFORM_CODE_GET_TOOL_TIP_AT:
		{
			perform_data_get_tool_tip_at* data
				= (perform_data_get_tool_tip_at*)_data;
			data->return_value
				= BView::GetToolTipAt(data->point, data->tool_tip);
			return B_OK;
		}
		case PERFORM_CODE_ALL_UNARCHIVED:
		{
			perform_data_all_unarchived* data =
				(perform_data_all_unarchived*)_data;

			data->return_value = BView::AllUnarchived(data->archive);
			return B_OK;
		}
		case PERFORM_CODE_ALL_ARCHIVED:
		{
			perform_data_all_archived* data =
				(perform_data_all_archived*)_data;

			data->return_value = BView::AllArchived(data->archive);
			return B_OK;
		}
	}

	return BHandler::Perform(code, _data);
}


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

	if (fTopLevelView
		&& fOwner != NULL)
		fOwner->PostMessage(B_LAYOUT_WINDOW);
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
BView::SetToolTip(const char* text)
{
	if (text == NULL || text[0] == '\0') {
		SetToolTip((BToolTip*)NULL);
		return;
	}

	if (BTextToolTip* tip = dynamic_cast<BTextToolTip*>(fToolTip))
		tip->SetText(text);
	else
		SetToolTip(new BTextToolTip(text));
}


void
BView::SetToolTip(BToolTip* tip)
{
	if (fToolTip == tip)
		return;
	else if (tip == NULL)
		HideToolTip();

	if (fToolTip != NULL)
		fToolTip->ReleaseReference();

	fToolTip = tip;

	if (fToolTip != NULL)
		fToolTip->AcquireReference();
}


BToolTip*
BView::ToolTip() const
{
	return fToolTip;
}


// ShowToolTip and HideToolTip are not really used in the current implementation
void
BView::ShowToolTip(BToolTip* tip)
{
	if (tip == NULL)
		return;

	BPoint where;
	GetMouse(&where, NULL, false);

	//BToolTipManager::Manager()->ShowTip(tip, ConvertToScreen(where), this);
}


void
BView::HideToolTip()
{
	//BToolTipManager::Manager()->HideTip();
}


bool
BView::GetToolTipAt(BPoint point, BToolTip** _tip)
{
	if (fToolTip != NULL) {
		*_tip = fToolTip;
		return true;
	}

	*_tip = NULL;
	return false;
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

	fVerScroller = NULL;
	fHorScroller = NULL;

	fIsPrinting = false;
	fAttached = false;

	fViewBitmap = NULL;

	fCursor = -1;

	// TODO: Since we cannot communicate failure, we don't use std::nothrow here
	// TODO: Maybe we could auto-delete those views on AddChild() instead?
	fState = new BPrivate::ViewState;

	fBounds = frame.OffsetToCopy(B_ORIGIN);
	fShelf = NULL;

	fEventMask = 0;
	fEventOptions = 0;
	fMouseEventOptions = 0;

	fLayoutData = new LayoutData;

	fToolTip = NULL;

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
		if (fOwner->fFocus == this)
			MakeFocus(false);

		if (fOwner->fLastMouseMovedView == this)
			fOwner->fLastMouseMovedView = NULL;

		fOwner->RemoveHandler(this);
		if (fShelf)
			fOwner->RemoveHandler(fShelf);
	}

	if (newOwner && newOwner != fOwner) {
		newOwner->AddHandler(this);
		if (fShelf)
			newOwner->AddHandler(fShelf);

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
	if (!rect.IsValid()) {
		printf("Warning: Invalid rect in BView::_ClipToRect()\n");

		if (!inverse) {
			fState->clipping_region = BRegion();
			fState->clipping_region_used = false;
		}
		return;
	}

	if (inverse) {
		// TODO: we should have a definition for a rect (or region)
		// with "infinite" area. For now, this region size should do...
		if (!fState->clipping_region_used)
			fState->clipping_region = BRegion(BRect(-(1 << 16), -(1 << 16), (1 << 16), (1 << 16)));
		fState->clipping_region.Exclude(rect);
	} else {
		if (!fState->clipping_region_used)
			fState->clipping_region = BRegion(rect);
		else {
			BRegion rectRegion(rect);
			fState->clipping_region.IntersectWith(&rectRegion);
		}
	}

	fState->clipping_region_used = true;
}


void
BView::_ClipToShape(BShape* shape, bool inverse)
{
	if (shape == NULL)
		return;

	shape_data* sd = (shape_data*)shape->fPrivateData;
	if (sd->opCount == 0 || sd->ptCount == 0)
		return;

	// TODO iterate the shape into the clip region
	// This is never used in the current source code, so we don't implement it for now
}


void BView::_UpdateViewClippingRegion(bool deep)
{
	// the clipping region starts with this view's bounds that lie within our parent's bounds
	BRect bounds = Parent() ? Bounds() & Parent()->Bounds() : Bounds();
	bounds.right -= 1;
	bounds.bottom -= 1;
	
	fLocalClipping.Set(bounds);

	if (BView* child = fFirstChild) {
		// if this view does not draw over children,
		// exclude all children from the clipping
		if ((fFlags & B_DRAW_ON_CHILDREN) == 0) {
			BRegion childrenRegion;

			for (; child; child = child->NextSibling()) {
				if (!child->IsHidden()
					&& (child->fFlags & B_TRANSPARENT_BACKGROUND) == 0) {
					childrenRegion.Include(child->Frame());
				}
			}

			fLocalClipping.Exclude(&childrenRegion);
		}
		// if the operation is "deep", make children rebuild their
		// clipping too
		if (deep) {
			for (child = fFirstChild; child; child = child->NextSibling())
				child->_UpdateViewClippingRegion(true);
		}
	}
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
	view_widget = window_add_subsurface(fOwner->window, this, SUBSURFACE_SYNCHRONIZED);

	if (fTopLevelView) {
		widget_set_allocation(view_widget, WAYLAND_TOPVIEW_H_SLOP, WAYLAND_TOPVIEW_V_SLOP, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	} else {
		// Position our Wayland widget based on the parent widget's position
		rectangle allocation;
		widget_get_allocation(fParent->view_widget, &allocation);
		widget_set_allocation(view_widget, fParentOffset.x + allocation.x, fParentOffset.y + allocation.y, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	}

	//printf("View %s Bounds: %f %f %f %f\n", Name(), Bounds().left, Bounds().top, Bounds().right, Bounds().bottom);
	/* We set the input region of the subsurface where the image is draw as
	 * NULL, as the input region of the parent surface is automatically set
	 * by the toytoolkit. But as the window that finds the widget in a
	 * certain (x, y) position looks for surfaces that are on top first, it
	 * will call the image_widget handlers for input related stuff. */
	set_empty_input_region(view_widget, window_get_display(fOwner->window));
	widget_set_redraw_handler(view_widget, view_redraw_handler);
	// widget_set_resize_handler(view_widget, view_resize_handler);
	widget_set_enter_handler(view_widget, view_pointer_enter_handler);
	widget_set_leave_handler(view_widget, view_pointer_leave_handler);
	widget_set_motion_handler(view_widget, view_pointer_motion_handler);
	widget_set_button_handler(view_widget, view_button_handler);
	widget_set_axis_handler(view_widget, view_axis_handler);

	// we create all its children, too

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
	if (fParent != NULL && fParent->view_widget != NULL) {
		rectangle allocation;
		widget_get_allocation(fParent->view_widget, &allocation);
		widget_set_allocation(view_widget, x + allocation.x, y + allocation.y, Bounds().IntegerWidth(), Bounds().IntegerHeight());
	}

	if (Window() != NULL && fFlags & B_FRAME_EVENTS) {
		BMessage moved(B_VIEW_MOVED);
		moved.AddInt64("when", system_time());
		moved.AddPoint("where", BPoint(x, y));

		BMessenger target(this);
		target.SendMessage(&moved);
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

	_UpdateViewClippingRegion(true);

	if (fFlags & B_FRAME_EVENTS) {
		BMessage resized(B_VIEW_RESIZED);
		resized.AddInt64("when", system_time());
		resized.AddInt32("width", fBounds.IntegerWidth());
		resized.AddInt32("height", fBounds.IntegerHeight());

		BMessenger target(this);
		target.SendMessage(&resized);
	}
}


/*!	Relayouts the view according to its resizing mode. */
void
BView::_ParentResizedBy(int32 x, int32 y)
{
	uint32 resizingMode = fFlags & _RESIZE_MASK_;
	BRect newFrame = Frame();

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
		// move view
		//printf("Moving %s to %f %f\n", Name(), newFrame.left, newFrame.top);
		_MoveTo((int32)roundf(newFrame.left), (int32)roundf(newFrame.top));
	}

	if (newFrame != Frame()) {
		// resize view
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
			if (fOwner->fPulseRunner == NULL)
				fOwner->SetPulseRate(fOwner->PulseRate());
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
BView::_ColorsUpdated(BMessage* message)
{
	if (fTopLevelView
		&& fLayoutData->fLayout != NULL) {
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetHighUIColor(B_PANEL_TEXT_COLOR);
	}

	rgb_color color;

	const char* colorName = ui_color_name(fState->which_view_color);
	if (colorName != NULL && message->FindColor(colorName, &color) == B_OK) {
		fState->view_color = tint_color(color, fState->which_view_color_tint);
	}

	colorName = ui_color_name(fState->which_low_color);
	if (colorName != NULL && message->FindColor(colorName, &color) == B_OK) {
		fState->low_color = tint_color(color, fState->which_low_color_tint);
	}

	colorName = ui_color_name(fState->which_high_color);
	if (colorName != NULL && message->FindColor(colorName, &color) == B_OK) {
		fState->high_color = tint_color(color, fState->which_high_color_tint);
	}

	MessageReceived(message);

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling)
		child->_ColorsUpdated(message);

	Invalidate();
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

		if (fOwner->CurrentFocus() == this) {
			MakeFocus(false);
			// MakeFocus() is virtual and might not be
			// passing through to the BView version,
			// but we need to make sure at this point
			// that we are not the focus view anymore.
			if (fOwner->CurrentFocus() == this)
				fOwner->_SetFocus(NULL, true);
		}

		if (fOwner->fDefaultButton == this)
			fOwner->SetDefaultButton(NULL);

		if (fOwner->fKeyMenuBar == this)
			fOwner->fKeyMenuBar = NULL;

		if (fOwner->fLastMouseMovedView == this)
			fOwner->fLastMouseMovedView = NULL;

		if (fOwner->fLastViewToken == _get_object_token_(this))
			fOwner->fLastViewToken = B_NULL_TOKEN;

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

	//ConvertFromScreen(&updateRect);

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
	else
	{
		// DEBUG: temporary blue bg to see where/if the view is actually drawn
		// cairo_t* cr = widget_cairo_create(view_widget);
		// cairo_set_source_rgba(cr, rgb_to_cairo_color(200),
		// 							rgb_to_cairo_color(255),
		// 							rgb_to_cairo_color(255), 1);
		// cairo_paint(cr);
		// cairo_destroy(cr);
	}

	// TODO: make states robust (the hook implementation could
	// mess things up if it uses non-matching Push- and PopState(),
	// we would not be guaranteed to still have the same state on
	// the stack after having called Draw())
	PushState();
	Draw(updateRect);
	PopState();
	Flush();
}


void
BView::_DrawAfterChildren(BRect updateRect)
{
	if (IsHidden(this) || !(Flags() & B_WILL_DRAW)
		|| !(Flags() & B_DRAW_ON_CHILDREN))
		return;

	// ConvertFromScreen(&updateRect);

	// TODO: make states robust (see above)
	PushState();
	DrawAfterChildren(updateRect);
	PopState();
	Flush();
}


void
BView::_FontsUpdated(BMessage* message)
{
	MessageReceived(message);

	for (BView* child = fFirstChild; child != NULL;
			child = child->fNextSibling) {
		child->_FontsUpdated(message);
	}
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


BShelf*
BView::_Shelf() const
{
	return fShelf;
}


void
BView::_SetShelf(BShelf* shelf)
{
	if (fShelf != NULL && fOwner != NULL)
		fOwner->RemoveHandler(fShelf);

	fShelf = shelf;

	if (fShelf != NULL && fOwner != NULL)
		fOwner->AddHandler(fShelf);
}


status_t
BView::_SetViewBitmap(const BBitmap* bitmap, BRect srcRect, BRect dstRect,
	uint32 followFlags, uint32 options)
{
	if (!_CheckOwnerLockAndSwitchCurrent())
		return B_ERROR;

	status_t status = B_ERROR;

	//int32 serverToken = bitmap ? bitmap->_ServerToken() : -1;

	//fOwner->fLink->StartMessage(AS_VIEW_SET_VIEW_BITMAP);
	//fOwner->fLink->Attach<int32>(serverToken);
	//fOwner->fLink->Attach<BRect>(srcRect);
	//fOwner->fLink->Attach<BRect>(dstRect);
	//fOwner->fLink->Attach<int32>(followFlags);
	//fOwner->fLink->Attach<int32>(options);


	return status;
}


bool
BView::_CheckOwnerLockAndSwitchCurrent() const
{
	//STRACE(("BView(%s)::_CheckOwnerLockAndSwitchCurrent()\n", Name()));

	if (fOwner == NULL) {
		debugger("View method requires owner and doesn't have one.");
		return false;
	}

	_CheckLockAndSwitchCurrent();

	return true;
}


bool
BView::_CheckOwnerLock() const
{
	if (fOwner) {
		fOwner->check_lock();
		return true;
	} else {
		debugger("View method requires owner and doesn't have one.");
		return false;
	}
}


void
BView::_CheckLockAndSwitchCurrent() const
{
	//STRACE(("BView(%s)::_CheckLockAndSwitchCurrent()\n", Name()));

	if (!fOwner)
		return;

	fOwner->check_lock();
}


void
BView::_CheckLock() const
{
	if (fOwner)
		fOwner->check_lock();
}


status_t
BView::ScrollWithMouseWheelDelta(BScrollBar* scrollBar, float delta)
{
	if (scrollBar == NULL || delta == 0.0f)
		return B_BAD_VALUE;

	float smallStep;
	float largeStep;
	scrollBar->GetSteps(&smallStep, &largeStep);

	// pressing the shift key scrolls faster (following the pseudo-standard set
	// by other desktop environments).
	if ((modifiers() & B_SHIFT_KEY) != 0)
		delta *= largeStep;
	else
		delta *= smallStep * 3;

	scrollBar->SetValue(scrollBar->Value() + delta);

	return B_OK;
}


void BView::_ReservedView13() {}
void BView::_ReservedView14() {}
void BView::_ReservedView15() {}
void BView::_ReservedView16() {}


BView::BView(const BView& other)
	:
	BHandler()
{
	// this is private and not functional, but exported
}


BView&
BView::operator=(const BView& other)
{
	// this is private and not functional, but exported
	return *this;
}


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
		"\tToken: %" B_PRId32 "\n"
		"\tFlags: %" B_PRId32 "\n"
		"\tView origin: (%f,%f)\n"
		"\tView Bounds rectangle: (%f,%f,%f,%f)\n"
		"\tShow level: %d\n"
		"\tTopView?: %s\n"
		// "\tBPicture: %s\n"
		"\tVertical Scrollbar %s\n"
		"\tHorizontal Scrollbar %s\n"
		"\tIs Printing?: %s\n"
		"\tShelf?: %s\n"
		"\tEventMask: %" B_PRId32 "\n"
		"\tEventOptions: %" B_PRId32 "\n",
	Name(),
	fParent ? fParent->Name() : "NULL",
	fFirstChild ? fFirstChild->Name() : "NULL",
	fNextSibling ? fNextSibling->Name() : "NULL",
	fPreviousSibling ? fPreviousSibling->Name() : "NULL",
	fOwner ? fOwner->Name() : "NULL",
	_get_object_token_(this),
	fFlags,
	fParentOffset.x, fParentOffset.y,
	fBounds.left, fBounds.top, fBounds.right, fBounds.bottom,
	fShowLevel,
	fTopLevelView ? "YES" : "NO",
	// fCurrentPicture? "YES" : "NULL",
	fVerScroller? "YES" : "NULL",
	fHorScroller? "YES" : "NULL",
	fIsPrinting? "YES" : "NO",
	fShelf? "YES" : "NO",
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

	fState->font.PrintToStream();

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
