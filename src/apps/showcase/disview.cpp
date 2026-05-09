
#ifndef DIS_VIEW_H	
#include "disview.h"	
#endif				

#include <Font.h>
#include <Region.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>
#include <Shape.h>

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
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(64)), 0, B_RGBA32);
#if !defined(__HAIKU__)
	BIconUtils::GetAppIcon("leaf_icon", (icon_size)64, fIcon);
#else
	status_t err = GetAppIcon("leaf_icon", (icon_size)64, fIcon);
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

	// DrawBitmap and FillRect with all 11 blending modes
	drawing_mode modes[] = {B_OP_COPY, B_OP_OVER, B_OP_ERASE,
							B_OP_INVERT, B_OP_ADD, B_OP_SUBTRACT,
							B_OP_BLEND, B_OP_MIN, B_OP_MAX,
							B_OP_SELECT, B_OP_ALPHA};
	const int modeCount = sizeof(modes) / sizeof(modes[0]);
	const int x_offset = 42;
	const int y_offset = 42;
	
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	SetHighColor(255, 0, 0, 120);

	for (int i = 0; i < 18; i++) {
		SetDrawingMode(i < modeCount ? modes[i] : B_OP_COPY);
		float drawX = 5.0f + (x_offset * (i % 3));
		float drawY = 60.0f + (y_offset * (i / 3));
		BRect fillRect(5.0f + (x_offset * (i % 3)), 52.0f + (y_offset * (i / 3)),
						37.0f + (x_offset * (i % 3)), 58.0f + (y_offset * (i / 3)));
		BRect iconRect(drawX, drawY, drawX + 31.0f, drawY + 31.0f);

		if (i < 11) {
			DrawBitmap(fIcon, iconRect);
			FillRect(fillRect, B_SOLID_HIGH);
		} else if (i < 17) {
			// Ellipse radius testing
			SetHighColor(0, 0, 0, 255);
			StrokeRect(iconRect, B_SOLID_HIGH);
			BRect ellipseRect(iconRect);
			ellipseRect.InsetBy(1.0f + ((i - 11) * 0.5f), 1.0f + ((i - 11) * 0.5f));
			if (i < 14)
				FillEllipse(ellipseRect, B_SOLID_HIGH);
			else
				StrokeEllipse(ellipseRect, B_SOLID_HIGH);
		} else {
			iconRect.bottom = drawY + 15.0f;
			FillEllipse(iconRect, B_SOLID_HIGH);
			iconRect.OffsetBy(0, 17.0f);
			StrokeEllipse(iconRect, B_SOLID_HIGH);
		}
	}

	PopState();
	
	SetPenSize(2.0);
	BRect drawRect(135, 5, 155, 25);

	// FillRect
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
	drawRect.OffsetBy(-120, 30);
	
	// StrokeRect
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
	drawRect.OffsetBy(-120, 30);
	
	// FillEllipse
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
	drawRect.OffsetBy(-120, 30);

	// StrokeEllipse
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
	drawRect.OffsetBy(-120, 30);

	// FillRoundRect
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
	drawRect.OffsetBy(-120, 30);

	// StrokeRoundRect
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

	SetPenSize(1.0);
	drawRect.Set(140, 240, 150, 250);
	StrokeRect(drawRect, B_SOLID_HIGH);

	// StrokeShape
	BPoint tri1, tri2, tri3;
	float hInset = drawRect.Width() / 3;
	float vInset = drawRect.Height() / 3;
	drawRect.InsetBy(hInset, vInset);

	tri1.Set(drawRect.left + 1, drawRect.bottom + 1);
	tri2.Set(drawRect.left + 1 + drawRect.Width() / 1.33,
		(drawRect.top + drawRect.bottom + 1) / 2);
	tri3.Set(drawRect.left + 1, drawRect.top);
			
	BShape arrowShape;
	arrowShape.MoveTo(tri1);
	arrowShape.LineTo(tri2);
	arrowShape.LineTo(tri3);

	SetPenSize(3.0);
	MovePenTo(BPoint(0,0));
	StrokeShape(&arrowShape);

	// CopyBits: same size destination
	const int w = 32;
	const int h = 32;
	drawRect.Set(135, 180, 135 + w, 180 + h);
	CopyBits(BRect(5, 60, 5 + w, 60 + h), drawRect);

	// CopyBits: stretch destination horizontally
	drawRect.OffsetBy(36,0);
	drawRect.right += 10;
	CopyBits(BRect(5, 60, 5 + w, 60 + h), drawRect);

	// CopyBits: stretch destination past the view bounds to test clipping
	drawRect.OffsetBy(46,0);
	drawRect.right += 50;
	drawRect.bottom += 60;
	CopyBits(BRect(5, 60, 5 + w, 60 + h), drawRect);
}
