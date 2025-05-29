#include "disapp.h"
#include "diswindow.h"

#include <Alert.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe-Guido")
{
	DisWindow *window;
	BRect rect;

	rect.Set(30, 100, 640, 400);
	window = new DisWindow(rect);
	window->Populate();
	window->Show();
}
