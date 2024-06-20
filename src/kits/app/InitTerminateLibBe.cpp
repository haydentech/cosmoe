/*
 * Copyright 2001-2009, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold (bonefish@users.sf.net)
 */

//!	Global library initialization/termination routines.


#include <stdio.h>
#include <stdlib.h>

// The c++ compiler in 3.4.1 and 3.5
// strips out these initialization functions due to gcc bug 16717.

#include <MessagePrivate.h>
#include <RosterPrivate.h>


// debugging
#define DBG(x) x
//#define DBG(x)
#define OUT	printf



// initialize_before
static void __attribute__ ((constructor))
initialize_before()
{
	DBG(OUT("initialize_before()\n"));

	BMessage::Private::StaticInit();
	BRoster::Private::InitBeRoster();

	//pthread_atfork(NULL, NULL, initialize_forked_child);
	DBG(OUT("initialize_before() done\n"));
}

// terminate_after
static void __attribute__ ((destructor))
terminate_after()
{
	DBG(OUT("terminate_after()\n"));

	BRoster::Private::DeleteBeRoster();
	BMessage::Private::StaticCleanup();
	BMessage::Private::StaticCacheCleanup();

	DBG(OUT("terminate_after() done\n"));
}

