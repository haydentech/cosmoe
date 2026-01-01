// Windows DLL entry point stub for test addons
// Test addons don't need any special initialization, so this is just a minimal stub

#ifdef _WIN32
#include <windows.h>

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    // Do nothing - test addons don't need initialization
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;
    return TRUE;
}

// Some mingw configurations expect DllEntryPoint instead of DllMain
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return DllMain(hinstDLL, fdwReason, lpvReserved);
}
#endif
