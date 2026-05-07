#include "WindowFeelApp.h"

#include "WindowFeelWindow.h"


WindowFeelApp::WindowFeelApp()
	:	BApplication("application/x-vnd.Cosmoe-WindowFeel")
{
	WindowFeelWindow* window = new WindowFeelWindow();
	window->Show();
}