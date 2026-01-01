/*
 * DLL entry point for libtracker.dll on Windows
 * Used for debugging
 */

#ifdef _WIN32
#include <windows.h>

extern "C" {

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			MessageBoxA(NULL, "libtracker.dll DLL_PROCESS_ATTACH", "Debug", MB_OK);
			break;
		case DLL_PROCESS_DETACH:
			MessageBoxA(NULL, "libtracker.dll DLL_PROCESS_DETACH", "Debug", MB_OK);
			break;
	}
	return TRUE;
}

} // extern "C"

#endif // _WIN32
