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

// Cosmoe note: The c++ compiler in 3.4.1 and 3.5
// strips out these initialization functions due to gcc bug 16717.

#include <AppMisc.h>
#include <LooperList.h>
#include <MessagePrivate.h>
#include <RosterPrivate.h>
#include <TokenSpace.h>
#include <OS.h>


extern void __initialize_locale_kit();


// debugging
#define DBG(x) x
//#define DBG(x)
#define OUT	printf


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
static void __attribute__ ((constructor))
initialize_before()
{
	DBG(OUT("initialize_before()\n"));

	BMessage::Private::StaticInit();
	BRoster::Private::InitBeRoster();
	if (_register_main_thread() != B_OK)
		printf("Could not register main thread\n");

	pthread_atfork(NULL, NULL, initialize_forked_child);

	DBG(OUT("initialize_before() done\n"));
}

// terminate_after
static void __attribute__ ((destructor))
terminate_after()
{
	DBG(OUT("terminate_after()\n"));

	BRoster::Private::DeleteBeRoster();
	BMessage::Private::StaticCleanup();

	if (geteuid() == 0)
		BMessage::Private::StaticCacheCleanup();

	DBG(OUT("terminate_after() done\n"));
}

