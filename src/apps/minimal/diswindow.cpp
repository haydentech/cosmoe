/*This is diswindow.cpp*/

#include <Application.h>
#include <Placeholder.h>

#include "diswindow.h"


DisWindow::DisWindow(BRect rect)
	: BWindow (rect, "Minimal Cosmoe App", B_TITLED_WINDOW, B_NOT_V_RESIZABLE) 
{														// make this stuff see AddChild Below
	BPlaceholder* place1 = new BPlaceholder(BRect(15, 15, 100, 55), "1", B_FOLLOW_ALL_SIDES);
	// BPlaceholder* place2 = new BPlaceholder(BRect(15, 57, 100, 107), "2", B_FOLLOW_NONE);
	// BPlaceholder* place3 = new BPlaceholder(BRect(102, 15, 250, 55), "3", B_FOLLOW_NONE);
	// BPlaceholder* place4 = new BPlaceholder(BRect(102, 57, 250, 107), "4", B_FOLLOW_NONE);
	AddChild(place1);
	// AddChild(place2);
	// AddChild(place3);
	// AddChild(place4);
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}
