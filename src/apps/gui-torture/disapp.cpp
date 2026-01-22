#include "disapp.h"
#include "diswindow.h"
#include "disview.h"


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe.gui-torture")
{
	DisWindow *window1;
	DisWindow *window2;
	DisWindow *window3;
	DisWindow *window4;
	DisWindow *window5;

	BRect rect(30, 100, 300, 200);

	window1 = new DisWindow(rect, "1");
	window2 = new DisWindow(rect, "2");
	window3 = new DisWindow(rect, "3");
	window4 = new DisWindow(rect, "4");
	window5 = new DisWindow(rect, "5");

	window1->Show();
	window2->Show();
	window3->Show();
	window4->Show();
	window5->Show();
}
