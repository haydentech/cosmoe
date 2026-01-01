/*
 * DLL entry point for libbe.dll on Windows
 * Minimal stub - no initialization needed for now
 */

#ifdef _WIN32
#include <windows.h>

extern "C" {

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)hinstDLL;
	(void)lpvReserved;
	
	// No initialization needed currently
	return TRUE;
}

} // extern "C"

#endif // _WIN32
