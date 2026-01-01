/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Shared DLL initialization for Windows
 * Include this file in any shared library to provide DllMain/DllEntryPoint
 */

#ifndef _DLL_INIT_H
#define _DLL_INIT_H

#ifdef _WIN32
#include <windows.h>

// Windows DLL entry point (MinGW expects DllMain or DllEntryPoint)
static BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)hinstDLL;
	(void)lpvReserved;
	
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			// DLL is being loaded into a process
			break;
		case DLL_PROCESS_DETACH:
			// DLL is being unloaded from a process
			break;
		case DLL_THREAD_ATTACH:
			// A thread is being created
			break;
		case DLL_THREAD_DETACH:
			// A thread is exiting cleanly
			break;
	}
	return TRUE;
}

// Alias for older MinGW linkers
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	return DllMain(hinstDLL, fdwReason, lpvReserved);
}
#endif

#endif // _DLL_INIT_H
