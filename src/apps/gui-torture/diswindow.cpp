#include <Application.h>

#include "diswindow.h"
#include "disview.h"


int DisWindow::fCount = 0;


DisWindow::DisWindow(BRect rect, const char *name)
	: BWindow (rect, name, B_TITLED_WINDOW, B_NOT_V_RESIZABLE) 
{
	fCount++;
	SetPulseRate(50000);
	DisView* aView = new DisView(BRect(10, 10, 200, 100), "disview", "1");
	AddChild(aView);
}

bool DisWindow::QuitRequested()
{
	if (--fCount <= 0)
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
	}

	return true;
}

