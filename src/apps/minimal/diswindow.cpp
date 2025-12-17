/*This is diswindow.cpp*/

#include <Application.h>
#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>
#include <stdio.h>

#include "diswindow.h"


class RulerView : public BView {
	public:
								RulerView(BRect rect);
		virtual					~RulerView();
	
		virtual void			Draw(BRect updateRect);
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
	: BView(frame, "ruler", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
	SetViewColor(240, 240, 240);
}

RulerView::~RulerView()
{
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
}