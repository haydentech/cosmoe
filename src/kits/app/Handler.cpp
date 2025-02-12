/*
 * Copyright 2001-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Erik Jaesler, erik@cgsoftware.com
 */



#include <Handler.h>
#include <Looper.h>

#include <algorithm>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>


static const char* kArchiveNameField = "_name";

static const uint32 kMsgStartObserving = '_OBS';
static const uint32 kMsgStopObserving = '_OBP';
static const char* kObserveTarget = "be:observe_target";


BHandler::BHandler(const char* name)
	:
	fName(NULL)
{
	_InitData(name);
}


BHandler::~BHandler()
{
	free(fName);
}


BLooper*
BHandler::Looper() const
{
	return fLooper;
}


void
BHandler::SetName(const char* name)
{
	if (fName != NULL) {
		free(fName);
		fName = NULL;
	}

	if (name != NULL)
		fName = strdup(name);
}


const char*
BHandler::Name() const
{
	return fName;
}


void
BHandler::SetNextHandler(BHandler* handler)
{
	if (fLooper == NULL) {
		debugger("handler must belong to looper before setting NextHandler");
		return;
	}

	if (!fLooper->IsLocked()) {
		debugger("The handler's looper must be locked before setting NextHandler");
		return;
	}

	if (handler != NULL && fLooper != handler->Looper()) {
		debugger("The handler and its NextHandler must have the same looper");
		return;
	}

	fNextHandler = handler;
}


BHandler*
BHandler::NextHandler() const
{
	return fNextHandler;
}


bool
BHandler::LockLooper()
{
	BLooper* looper = fLooper;
	// Locking the looper also makes sure that the looper is valid
	if (looper != NULL && looper->Lock()) {
		// Have we locked the right looper? That's as far as the
		// "pseudo-atomic" operation mentioned in the BeBook.
		if (fLooper == looper)
			return true;

		// we locked the wrong looper, bail out
		looper->Unlock();
	}

	return false;
}


void
BHandler::UnlockLooper()
{
	fLooper->Unlock();
}


void
BHandler::_InitData(const char* name)
{
	SetName(name);

	fLooper = NULL;
	fNextHandler = NULL;
}

void
BHandler::SetLooper(BLooper* looper)
{
	fLooper = looper;
}


void BHandler::_ReservedHandler2() {}
void BHandler::_ReservedHandler3() {}
void BHandler::_ReservedHandler4() {}


//	#pragma mark -

