#include "diswindow.h"
#include "disapp.h"

#ifdef _WIN32
#include <windows.h>

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int s)
{
	MessageBoxA(NULL, "WinMain reached!", "Test", MB_OK | MB_TOPMOST);
	try {
		DisApplication app;
		app.Run();
	} catch(...) {
		MessageBoxA(NULL, "Exception!", "Error", MB_OK | MB_ICONERROR);
	}
	return 0;
}

#else
#include <stdio.h>
int main() {
	DisApplication app;
	app.Run();
	return 0;
}
#endif
