#include "disapp.h"
#include "diswindow.h"

#include <Alert.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe-Guido"),
	  fWindow(NULL)
{
	BRect rect;

	rect.Set(30, 100, 640, 480);
	fWindow = new DisWindow(rect);
	fWindow->Populate();
	fWindow->Show();

	SetPulseRate(500000);
}


void DisApplication::Pulse()
{
	if (fWindow && fWindow->Lock()) {
		fWindow->IncrementBar();
		fWindow->Unlock();
	}
}

