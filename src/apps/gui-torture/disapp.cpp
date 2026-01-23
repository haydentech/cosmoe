#include "disapp.h"
#include "diswindow.h"
#include "disview.h"
#include <cstdio>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.Cosmoe.gui-torture")
{
	// Change this to create more or fewer windows
	const int numWindows = 5;
	
	BRect rect(30, 100, 300, 200);

	for (int i = 1; i <= numWindows; i++) {
		char title[32];
		snprintf(title, sizeof(title), "%d", i);
		
		DisWindow *window = new DisWindow(rect, title);
		window->Show();
	}
}
