#include "disapp.h"
#include "diswindow.h"

#include <Alert.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe-Showcase"),
	  fWindow(NULL)
{
	BRect rect;

	rect.Set(30, 80, 770, 480);
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

