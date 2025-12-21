/*This is diswindow.cpp*/

#include <Application.h>
#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>
#include <stdio.h>
#include <List.h>

#include "diswindow.h"


class RulerView : public BView {
	public:
								RulerView(BRect rect);
		virtual					~RulerView();
	
		virtual void			AttachedToWindow();
		virtual void			Draw(BRect updateRect);
		virtual void			MouseDown(BPoint where);
		virtual void			MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);

		virtual void			KeyDown(const char* bytes, int32 numBytes);

	private:

		BList					fClickPoints;
		BPoint					fMousePos;
		bool					fMouseInView;
};

DisWindow::DisWindow(BRect rect)
	: BWindow (rect, "Minimal Cosmoe App", B_TITLED_WINDOW, 0) 
{
	RulerView* aRulerView = new RulerView(Bounds());
	AddChild(aRulerView);
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


// RulerView - draws a horizontal pixel ruler
RulerView::RulerView(BRect frame)
	: BView(frame, "ruler", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE)
{
	SetViewColor(240, 240, 240);
}

RulerView::~RulerView()
{
	// Clean up allocated BPoint objects
	for (int32 i = 0; i < fClickPoints.CountItems(); i++) {
		BPoint* pt = (BPoint*)fClickPoints.ItemAt(i);
		delete pt;
	}
	fClickPoints.MakeEmpty();
}

void
RulerView::AttachedToWindow()
{
	BView::AttachedToWindow();
	MakeFocus(true);  // Grab keyboard focus after attached to window
}

void RulerView::Draw(BRect updateRect)
{
	BView::Draw(updateRect);
	
	SetHighColor(0, 0, 0);
	SetLowColor(ViewColor());
	
	BRect bounds = Bounds();
	float width = bounds.Width();
	
	// Draw tick marks every 2 pixels
	for (float x = 0; x <= width; x += 2) {
		int tickNumber = (int)(x / 2);
		float tickHeight = 5;  // Default height
		
		// Every 25th tick (every 50 pixels) is triple height
		if (tickNumber % 25 == 0) {
			tickHeight = 15;
		}
		// Every 5th tick (every 10 pixels) is double height
		else if (tickNumber % 5 == 0) {
			tickHeight = 10;
		}
		
		// Draw the tick mark from bottom up
		StrokeLine(BPoint(x, bounds.bottom), 
					BPoint(x, bounds.bottom - tickHeight));
		
		// Draw label above every 50th tick (every 100 pixels)
		if (tickNumber % 50 == 0 && tickNumber > 0) {
			int pixelValue = tickNumber * 2;  // Convert to actual pixels
			char label[32];
			snprintf(label, sizeof(label), "%d pixels", pixelValue);
			
			// Center the text above the tick
			float stringWidth = StringWidth(label);
			DrawString(label, BPoint(x - stringWidth / 2, 10));
		}


	}

	// Draw red dot at 1,1 to verify origin accuracy
	SetHighColor(200, 0, 0);
	StrokeLine(BPoint(0, 0), BPoint(0, 0));
	
	// Draw the view dimensions at the center
	char sizeLabel[64];
	snprintf(sizeLabel, sizeof(sizeLabel), "%.0f x %.0f pixels", 
			 bounds.Width(), bounds.Height());
	float labelWidth = StringWidth(sizeLabel);
	float centerX = bounds.Width() / 2;
	float centerY = bounds.Height() / 2;
	
	// Draw with a slight background for readability
	SetHighColor(0, 0, 0);
	font_height fh;
	GetFontHeight(&fh);
	float textHeight = fh.ascent + fh.descent;
	FillRoundRect(BRect(centerX - labelWidth / 2 - 2, centerY - textHeight / 2 - 2,
				   centerX + labelWidth / 2 + 2, centerY + textHeight / 2), 4, 4);
	
	SetHighColor(205, 205, 145);
	DrawString(sizeLabel, BPoint(centerX - labelWidth / 2, centerY - 4 + fh.ascent / 2));

	for (int32 i = 0; i < fClickPoints.CountItems(); i++) {
		BPoint* pt = (BPoint*)fClickPoints.ItemAt(i);
		if (pt) {
			SetHighColor(255, 0, 0);
			//StrokeLine(BPoint(bounds.left, pt->y), BPoint(bounds.right, pt->y));
			StrokeLine(BPoint(pt->x, bounds.top), BPoint(pt->x, bounds.bottom));
		}
	}

	if (fMouseInView) {
		// Draw vertical line at mouse X position
		SetHighColor(0, 0, 255);
		StrokeLine(BPoint(fMousePos.x, bounds.top), BPoint(fMousePos.x, bounds.bottom));
	}	
}

void RulerView::MouseDown(BPoint where)
{
	printf("RulerView MouseDown at (%.1f, %.1f)\n", where.x, where.y);
	
	fClickPoints.AddItem(new BPoint(where));
	Invalidate();
}

void
RulerView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (code == B_ENTERED_VIEW) {
		fMouseInView = true;
	} else if (code == B_EXITED_VIEW) {
		fMouseInView = false;
	}
	
	fMousePos = where;
	Invalidate();  // Redraw with updated mouse position
}

void
RulerView::KeyDown(const char* bytes, int32 numBytes)
{
	int32 count = fClickPoints.CountItems();
	printf("RulerView KeyDown: clearing %d click points\n", count);
	
	// Delete all BPoint objects before clearing the list
	for (int32 i = 0; i < count; i++) {
		BPoint* pt = (BPoint*)fClickPoints.ItemAt(i);
		delete pt;
	}
	fClickPoints.MakeEmpty();
	Invalidate();
}