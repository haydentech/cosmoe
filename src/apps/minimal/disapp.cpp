#include "disapp.h"
#include "diswindow.h"
#include "disview.h"


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.minimal")
{
	DisWindow *window;
	BRect rect(30, 100, 330, 160);

	window = new DisWindow(rect);

	window->Show();
}
