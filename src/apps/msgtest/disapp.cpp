#include "disapp.h"
#include "diswindow.h"

#include <Alert.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe-msgtest")
{
	DisWindow *window;
	BRect rect;

	rect.Set(30, 100, 440, 140);
	window = new DisWindow(rect);
	window->Populate();
	window->Show();
}
