/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"

#include <iostream>
#include <stdio.h>
#include <String.h>


#include <Button.h>
#include <StringView.h>
#include <TextControl.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Slider.h>
#include <ScrollBar.h>

#include <ScrollView.h>



// Debug subclass of BStringView that overrides MouseDown
class BStringViewDebug : public BStringView
{
public:
	BStringViewDebug(BRect frame, const char* name, const char* text, uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW)
		: BStringView(frame, name, text, resizingMode, flags)
	{
	}

	virtual void MouseDown(BPoint where)
	{
		printf("BStringViewDebug::MouseDown at (%.1f, %.1f), Bounds=(%f,%f,%f,%f)\n", 
			where.x, where.y, Bounds().left, Bounds().top, Bounds().right, Bounds().bottom);
		BMessage* msg = Window()->CurrentMessage();
		if (msg) {
			printf("Current message:\n");
			msg->PrintToStream();
		}
		BStringView::MouseDown(where);
	}
};



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "msgtest", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE)
{
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


void DisWindow::Populate()
{
	// Scrolling tab content
	BStringView* coordStringView = new BStringView(BRect(10, 10, 400, 35), "coord_view", 
													"Click in the scrolling area below", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	Lock();
	AddChild(coordStringView);
	Unlock();

	BStringViewDebug* scrollString = new BStringViewDebug(BRect(10, 45, 155, 64), "scrolling string view", "Use the horizontal scrollbar below to scroll this very long and wordy string of text.", B_FOLLOW_LEFT_RIGHT);
	BScrollBar* horizScroll = new BScrollBar(BRect(10, 65, 155, 65 + B_H_SCROLL_BAR_HEIGHT), "horizontal scrollbar", scrollString, 0, 170, B_HORIZONTAL);
	Lock();
	AddChild(scrollString);
	AddChild(horizScroll);
	Unlock();
}



void DisWindow::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		default:
			BWindow::MessageReceived(message);
			break;
	}
}
