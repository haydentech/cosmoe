/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Shared DLL initialization for Windows
 * 
 * NOTE: This header is currently unused because MinGW-w64's DllMainCRTStartup
 * does not call user-provided DllMain functions. Each DLL (libbe, libtracker)
 * defines its own DllMain in a .cpp file, but these are not being invoked.
 * 
 * Instead, Windows programs must explicitly call initialization functions like
 * _register_main_thread() at startup.
 */

#ifndef _DLL_INIT_H
#define _DLL_INIT_H

#ifdef _WIN32
// Placeholder for future DLL initialization improvements
#endif

#endif // _DLL_INIT_H
