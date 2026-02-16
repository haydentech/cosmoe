#include "disapp.h"
#include "diswindow.h"

#include <Alert.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe-Columns"),
	  fWindow(NULL)
{
	BRect rect;

	rect.Set(30, 80, 640, 480);
	fWindow = new DisWindow(rect);
	fWindow->Populate();
	fWindow->Show();
}

