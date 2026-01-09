/*
 * DLL entry point for libbe.dll on Windows
 * 
 * IMPORTANT NOTE: DllMain is NOT being called by MinGW-w64's loader!
 * Despite being properly defined and compiled into the DLL, the Windows
 * loader never invokes it. This appears to be a MinGW-w64 CRT issue where
 * their DllMainCRTStartup doesn't chain to user-provided DllMain functions.
 * 
 * SOLUTION: Programs using libbe.dll on Windows MUST explicitly call
 * initialization functions at startup. BApplication does this automatically,
 * but standalone programs (tests, utilities) need to call it manually.
 * 
 * This code is left in place for future investigation and potential fixes.
 */

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>

// Forward declare initialization functions from InitTerminateLibBe.cpp  
extern "C" void __libbe_initialize_before();
extern "C" void __libbe_terminate_after();

extern "C" {

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)hinstDLL;
	(void)lpvReserved;
	
	// This code is never executed - see note above
	FILE* test = fopen("C:\\dllmain_was_called.txt", "a");
	if (test) {
		fprintf(test, "DllMain called: reason=%lu\n", fdwReason);
		fflush(test);
		fclose(test);
	}
	
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			__libbe_initialize_before();
			break;
			
		case DLL_PROCESS_DETACH:
			__libbe_terminate_after();
			break;
	}
	
	return TRUE;
}

} // extern "C"

#endif // _WIN32
