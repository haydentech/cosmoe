/*This is diswindow.cpp*/

#include <Application.h>
#include <Placeholder.h>
#include <Button.h>
#include <TextControl.h>

#include "diswindow.h"


DisWindow::DisWindow(BRect rect)
	: BWindow (rect, "Minimal Cosmoe App", B_TITLED_WINDOW, B_NOT_V_RESIZABLE) 
{														// make this stuff see AddChild Below
	// Add a button which brings up a BAlert
	// BButton* anAlertButton = new BButton(BRect(25, 50, 155, 70), "Button 4", "Show Alert", new BMessage(B_PULSE), B_FOLLOW_LEFT_RIGHT);
	// AddChild(anAlertButton);
	
	//BPlaceholder* place1 = new BPlaceholder(BRect(15, 15, 100, 55), "1", B_FOLLOW_ALL_SIDES);
	// BPlaceholder* place2 = new BPlaceholder(BRect(15, 57, 100, 107), "2", B_FOLLOW_NONE);
	// BPlaceholder* place3 = new BPlaceholder(BRect(102, 15, 250, 55), "3", B_FOLLOW_NONE);
	// BPlaceholder* place4 = new BPlaceholder(BRect(102, 57, 250, 107), "4", B_FOLLOW_NONE);
	//AddChild(place1);
	// AddChild(place2);
	// AddChild(place3);
	// AddChild(place4);

	BTextControl* aTextControl = new BTextControl(BRect(10, 35, 180, 70), "a text control",
										 "Type here:",
										 "Some sample text", NULL, B_FOLLOW_LEFT_RIGHT);
	AddChild(aTextControl);
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}
