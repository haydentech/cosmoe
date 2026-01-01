/*
 * Minimal test that links against libbe.dll
 */

#include <windows.h>
#include <stdio.h>

// Forward declare a simple Be API function
extern "C" {
	extern int be_app;  // Just reference a simple exported variable
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MessageBoxA(NULL, "test_libbe: WinMain called!", "Success", MB_OK);
	
	// Try to access a Be API symbol
	char msg[100];
	snprintf(msg, sizeof(msg), "be_app pointer: %p", (void*)&be_app);
	MessageBoxA(NULL, msg, "Symbol Access", MB_OK);
	
	return 0;
}
