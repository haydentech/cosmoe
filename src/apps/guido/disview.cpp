
#ifndef DIS_VIEW_H	
#include "disview.h"	
#endif				

#include <Font.h>
#include <Region.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>

DisView::DisView(BRect aRect,
		 const char *name)
					: BView ( aRect,
							name,
							B_FOLLOW_TOP_BOTTOM,
							B_WILL_DRAW)
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
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

	SetDrawingMode(B_OP_OVER);
	DrawBitmap(fIcon);
	SetDrawingMode(B_OP_COPY);
}
