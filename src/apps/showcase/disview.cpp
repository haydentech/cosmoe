
#ifndef DIS_VIEW_H	
#include "disview.h"	
#endif				

#include <Font.h>
#include <Region.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>

#include <cstdio>

#ifdef __HAIKU__
extern status_t GetAppIcon(const char* iconName, icon_size which, BBitmap* icon);
#endif

DisView::DisView(BRect aRect,
		 const char *name)
					: BView ( aRect,
							name,
							B_FOLLOW_NONE,
							B_WILL_DRAW)
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
#if !defined(__HAIKU__)
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
#else
	status_t err = GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (err != B_OK)
		printf("Could not load app icon in DisView: %d\n", err);
#endif
}


void DisView::Draw(BRect rect)
{
	rgb_color hcol = {208, 208, 158, 255};
	rgb_color lcol = {100, 150, 65, 255};
	rgb_color black = {0, 0, 0, 255};

    const int sideLength = 8;
    const int offset = 11;
	
	BRect r(1,1, 1 + sideLength, 1 + sideLength);

    ClipToInverseRect(r);
    SetHighColor(hcol);
    FillRect(Bounds());

    r.OffsetBy(0, offset);
	
	SetLowColor(lcol);
	FillRect(r, B_SOLID_LOW);
	
	SetHighColor(black);
	StrokeRect(r);
	
	r.OffsetBy(offset, 0);
	
	SetHighColor(black);
	StrokeRect(r);
	SetHighColor(lcol);
	FillRect(r.InsetByCopy(1, 1));
	
	r.OffsetBy(0, offset);
	
	SetHighColor(black);
	StrokeRect(r);
	SetHighColor(lcol);
	FillRect(r.InsetByCopy(2, 2));
	
	r.OffsetBy(-offset, 0);
	PushState();
	SetHighColor(black);
	ClipToRect(r);
	FillRect(Bounds());
	PopState();
	
	r.OffsetBy(0, offset);
	BRegion reg(r);
	FillRegion(&reg, B_SOLID_LOW);

	r.OffsetBy(offset, 0);
	BeginLineArray(4);
    AddLine(BPoint(r.left, r.top), BPoint(r.right, r.top), lcol);
    AddLine(BPoint(r.right, r.top), BPoint(r.right, r.bottom), lcol);
    AddLine(BPoint(r.right, r.bottom), BPoint(r.left, r.bottom), lcol);
    AddLine(BPoint(r.left, r.bottom), BPoint(r.left, r.top), lcol);
    EndLineArray();

    r.OffsetBy(0, offset);
    StrokeLine(r.LeftTop(), r.RightTop());

    r.OffsetBy(-offset, 0);
    StrokeLine(r.LeftTop(), r.RightTop());
	
	MovePenTo(21, 10);
	SetHighColor(lcol);
	DrawString("Draw Testing");
	StrokeLine(PenLocation(), PenLocation() + BPoint(5, 5));

	MovePenTo(21, 10 + offset);
	SetHighColor(black);
	SetDrawingMode(B_OP_OVER);
	DrawString("Draw Testing");
	StrokeLine(PenLocation(), PenLocation() + BPoint(5, 5));

	MovePenTo(21, 10 + (offset * 2));
	SetHighColor(lcol);
	SetDrawingMode(B_OP_BLEND);
	DrawString("Draw Testing");
	StrokeLine(PenLocation(), PenLocation() + BPoint(5, 5));

	MovePenTo(21, 10 + (offset * 3));
	SetHighColor(lcol);
	SetDrawingMode(B_OP_INVERT);
	DrawString("Draw Testing");
	StrokeLine(PenLocation(), PenLocation() + BPoint(5, 5));
	
	SetDrawingMode(B_OP_COPY);

	SetHighColor(black);

	r.OffsetBy(0, offset);

	PushState();

	// Line 1
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	MovePenTo(5, 50);
	DrawBitmap(fIcon);

	MovePenTo(40, 50);
	SetDrawingMode(B_OP_COPY);
	DrawBitmap(fIcon);

	MovePenTo(75, 50);
	SetDrawingMode(B_OP_ADD);
	DrawBitmap(fIcon);

	// Line 2
	MovePenTo(5, 90);
	SetDrawingMode(B_OP_BLEND);
	DrawBitmap(fIcon);

	MovePenTo(40, 90);
	SetDrawingMode(B_OP_SUBTRACT);
	DrawBitmap(fIcon);

	MovePenTo(75, 90);
	SetDrawingMode(B_OP_ERASE);
	DrawBitmap(fIcon);

	// Line 3
	MovePenTo(5, 130);
	SetDrawingMode(B_OP_INVERT);
	DrawBitmap(fIcon);

	MovePenTo(40, 130);
	SetDrawingMode(B_OP_MAX);
	DrawBitmap(fIcon);

	MovePenTo(75, 130);
	SetDrawingMode(B_OP_SELECT);
	DrawBitmap(fIcon);

	PopState();
	
	SetPenSize(2.0);
	
	BRect drawRect(120, 10, 140, 30);
	FillRect(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	FillRect(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	FillRect(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30, 0);

	SetDrawingMode(B_OP_BLEND);
	FillRect(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeRect(drawRect, B_SOLID_LOW);
	SetDrawingMode(B_OP_COPY);
	
	drawRect.Set(120, 40, 140, 60);
	SetPenSize(1.0);
	StrokeRect(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	StrokeRect(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	StrokeRect(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeRect(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(3.0);
	StrokeRect(drawRect, B_MIXED_COLORS);
	
	drawRect.Set(120, 70, 140, 90);
	SetPenSize(1.0);
	FillEllipse(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	FillEllipse(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	FillEllipse(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);

	SetDrawingMode(B_OP_BLEND);
	FillEllipse(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeEllipse(drawRect, B_SOLID_LOW);
	SetDrawingMode(B_OP_COPY);

	drawRect.Set(120, 100, 140, 120);
	SetPenSize(1.0);
	StrokeEllipse(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	StrokeEllipse(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	StrokeEllipse(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeEllipse(drawRect, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(3.0);
	StrokeEllipse(drawRect, B_MIXED_COLORS);

	drawRect.Set(120, 130, 140, 150);
	SetPenSize(1.0);
	FillRoundRect(drawRect, 5, 5, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	FillRoundRect(drawRect, 5, 5, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	FillRoundRect(drawRect, 5, 5, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);

	SetDrawingMode(B_OP_BLEND);
	FillRoundRect(drawRect, 5, 5, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeRoundRect(drawRect, 5, 5, B_SOLID_LOW);
	SetDrawingMode(B_OP_COPY);

	drawRect.Set(120, 160, 140, 180);
	SetPenSize(1.0);
	StrokeRoundRect(drawRect, 5, 5, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	StrokeRoundRect(drawRect, 5, 5, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	StrokeRoundRect(drawRect, 5, 5, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(2.0);
	StrokeRoundRect(drawRect, 5, 5, B_SOLID_LOW);
	drawRect.OffsetBy(30,0);
	SetPenSize(3.0);
	StrokeRoundRect(drawRect, 5, 5, B_MIXED_COLORS);
}
