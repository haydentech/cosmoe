/*
 * Copyright 2001-2011, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold (bonefish@users.sf.net)
 */

//!	Global library initialization/termination routines.


#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#ifdef OUT
#undef OUT
#endif
#endif

// Cosmoe note: The c++ compiler in 3.4.1 and 3.5
// strips out these initialization functions due to gcc bug 16717.

#include <AppMisc.h>
#include <LooperList.h>
#include <MessagePrivate.h>
//#include <RosterPrivate.h>
#include <TokenSpace.h>
#include <OS.h>


extern void __initialize_locale_kit();


// debugging
//#define DBG(x) x
#define DBG(x)
#define OUT	printf


#ifdef _WIN32
static bool
running_under_wine()
{
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL)
		return false;

	return GetProcAddress(ntdll, "wine_get_version") != NULL;
}


static void
configure_wine_pangocairo_backend()
{
	if (!running_under_wine())
		return;

	const char* backend = getenv("PANGOCAIRO_BACKEND");
	if (backend != NULL && backend[0] != '\0')
		return;

	_putenv("PANGOCAIRO_BACKEND=fontconfig");
}
#endif


static void
initialize_forked_child()
{
	DBG(OUT("initialize_forked_child()\n"));

	BMessage::Private::StaticReInitForkedChild();
	BPrivate::gLooperList.InitAfterFork();
	BPrivate::gDefaultTokens.InitAfterFork();
	BPrivate::init_team_after_fork();
	if (_register_main_thread() != B_OK)
		printf("Could not register main thread\n");

	DBG(OUT("initialize_forked_child() done\n"));
}


// initialize_before
// On non-Windows: called automatically via constructor attribute
// On Windows: called explicitly from DllMain (see libbe_dllmain.cpp)
#ifdef _WIN32
extern "C" void __libbe_initialize_before()
#else
static void __attribute__ ((constructor))
initialize_before()
#endif
{
	DBG(OUT("initialize_before()\n"));

#ifdef _WIN32
	configure_wine_pangocairo_backend();
#endif

	BMessage::Private::StaticInit();
	//BRoster::Private::InitBeRoster();
	if (_register_main_thread() != B_OK)
		printf("Could not register main thread\n");

#ifndef _WIN32
	pthread_atfork(NULL, NULL, initialize_forked_child);
#endif

	DBG(OUT("initialize_before() done\n"));
}

// terminate_after
// On non-Windows: called automatically via destructor attribute
// On Windows: called explicitly from DllMain (see libbe_dllmain.cpp)
#ifdef _WIN32
extern "C" void __libbe_terminate_after()
#else
static void __attribute__ ((destructor))
terminate_after()
#endif
{
	DBG(OUT("terminate_after()\n"));

	//BRoster::Private::DeleteBeRoster();
	
    // Don't clean up the message cache on exit - it causes crashes
    // when other static objects (like BClipboard) are destroyed after
    // this and try to delete BMessages that reference the cache.
    // The OS will clean up the memory anyway.
    // BMessage::Private::StaticCleanup();
    // BMessage::Private::StaticCacheCleanup();

	DBG(OUT("terminate_after() done\n"));
}

