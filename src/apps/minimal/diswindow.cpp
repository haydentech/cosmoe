/*This is diswindow.cpp*/

#include <Application.h>
#include <Placeholder.h>
#include <TextControl.h>
#include <IconUtils.h>
#include <ControlLook.h>
#include <Bitmap.h>
#include <stdio.h>

#include "diswindow.h"

BBitmap* fIcon;

DisWindow::DisWindow(BRect rect)
	: BWindow (rect, "Minimal Cosmoe App", B_TITLED_WINDOW, B_NOT_V_RESIZABLE) 
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (fIcon == NULL) {
		fprintf(stderr, "Failed to load icon\n");
	}

	
	BPlaceholder* place1 = new BPlaceholder(BRect(15, 15, 100, 555), "1", B_FOLLOW_ALL_SIDES);
	// BPlaceholder* place2 = new BPlaceholder(BRect(15, 57, 100, 107), "2", B_FOLLOW_NONE);
	// BPlaceholder* place3 = new BPlaceholder(BRect(102, 15, 250, 55), "3", B_FOLLOW_NONE);
	// BPlaceholder* place4 = new BPlaceholder(BRect(102, 57, 250, 107), "4", B_FOLLOW_NONE);
	place1->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_Y);
	AddChild(place1);
	// AddChild(place2);
	// AddChild(place3);
	// AddChild(place4);

	// BTextControl* aTextControl = new BTextControl(BRect(10, 35, 180, 70), "a text control",
	// 									 "Type here:",
	// 									 "Some sample text", NULL, B_FOLLOW_LEFT_RIGHT);
	//AddChild(aTextControl);
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}
