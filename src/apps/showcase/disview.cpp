
#ifndef DIS_VIEW_H	
#include "disview.h"	
#endif				

#include <Font.h>
#include <Region.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>

#ifdef __HAIKU__
extern status_t GetAppIcon(const char* iconName, icon_size which, BBitmap* icon);
#endif

DisView::DisView(BRect aRect,
		 const char *name)
					: BView ( aRect,
							name,
							B_FOLLOW_LEFT_RIGHT,
							B_WILL_DRAW)
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
#if !defined(__HAIKU__)
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
#else
	status_t err = GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (err != B_OK)
		printf("Could not load app icon in DisView: %ld\n", err);
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
	
	MovePenTo(21,21);
	SetHighColor(black);

	DrawString("Draw Testing");

	StrokeLine(PenLocation(), PenLocation() + BPoint(5, 5));

	r.OffsetBy(0, offset);

	PushState();
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	DrawBitmap(fIcon);
	PopState();
	
	//SetLineWidth(2.0);
	
	BRect drawRect(120, 10, 140, 30);
	FillRect(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	FillRect(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	FillRect(drawRect, B_SOLID_LOW);
	
	drawRect.Set(120, 40, 140, 60);
	StrokeRect(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	StrokeRect(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	StrokeRect(drawRect, B_SOLID_LOW);
	
	drawRect.Set(120, 70, 140, 90);
	FillEllipse(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	FillEllipse(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	FillEllipse(drawRect, B_SOLID_LOW);

	drawRect.Set(120, 70, 140, 90);
	StrokeEllipse(drawRect, B_SOLID_HIGH);
	drawRect.OffsetBy(30, 0);
	StrokeEllipse(drawRect, B_MIXED_COLORS);
	drawRect.OffsetBy(30,0);
	StrokeEllipse(drawRect, B_SOLID_LOW);
}
