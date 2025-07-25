#include "disapp.h"
#include "diswindow.h"
#include "disview.h"


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.gui-torture")
{
	DisWindow *window1;
	DisWindow *window2;
	DisWindow *window3;

	BRect rect(30, 100, 300, 125);

	window1 = new DisWindow(rect, "1");
	window2 = new DisWindow(rect, "2");
	window3 = new DisWindow(rect, "3");

	window1->Show();
	window2->Show();
	window3->Show();
}
