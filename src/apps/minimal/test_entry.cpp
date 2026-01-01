/*
 * Simple test to verify Windows GUI subsystem entry point works
 */

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MessageBoxA(NULL, "WinMain called! The executable is working.", "Success", MB_OK);
	return 0;
}
