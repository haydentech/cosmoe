/*
 * Simplest possible BApplication test
 */

#include <windows.h>
#include <stdio.h>

// Just forward declare BApplication to link against it
class BApplication {
public:
	BApplication(const char* signature);
	virtual ~BApplication();
	void Run();
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MessageBoxA(NULL, "About to create BApplication", "Debug", MB_OK);
	
	BApplication app("application/x-vnd.test");
	
	MessageBoxA(NULL, "BApplication created successfully!", "Success", MB_OK);
	
	return 0;
}
