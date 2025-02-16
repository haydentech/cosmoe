/*
 * Copyright 2001-2015 Haiku, Inc. All rights reserved
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm, bpmagic@columbus.rr.com
 *		Axel Dörfler, axeld@pinc-software.de
 *		Erik Jaesler, erik@cgsoftware.com
 *		Ingo Weinhold, bonefish@@users.sf.net
 */


// BLooper class spawns a thread that runs a message loop.


#include <Looper.h>

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <Autolock.h>
// #include <Message.h>
// #include <MessageFilter.h>
// #include <MessageQueue.h>
// #include <Messenger.h>
// #include <PropertyInfo.h>

// #include <AppMisc.h>
// #include <AutoLocker.h>
// #include <DirectMessageTarget.h>
// #include <LooperList.h>
// #include <MessagePrivate.h>
// #include <TokenSpace.h>


// debugging
//#define DBG(x) x
#define DBG(x)	;
#define PRINT(x)	DBG({ printf("[%6" B_PRId32 "] ", find_thread(NULL)); printf x; })

/*
#include <Autolock.h>
#include <Locker.h>
static BLocker sDebugPrintLocker("BLooper debug print");
#define PRINT(x)	DBG({						\
	BAutolock _(sDebugPrintLocker);				\
	debug_printf("[%6ld] ", find_thread(NULL));	\
	debug_printf x;								\
})
*/


#define FILTER_LIST_BLOCK_SIZE	5
#define DATA_BLOCK_SIZE			5


// using BPrivate::gDefaultTokens;
// using BPrivate::gLooperList;
// using BPrivate::BLooperList;

//port_id _get_looper_port_(const BLooper* looper);

enum {
	BLOOPER_PROCESS_INTERNALLY = 0,
	BLOOPER_HANDLER_BY_INDEX
};

// static property_info sLooperPropInfo[] = {
// 	{
// 		"Handler",
// 			{},
// 			{B_INDEX_SPECIFIER, B_REVERSE_INDEX_SPECIFIER},
// 			NULL, BLOOPER_HANDLER_BY_INDEX,
// 			{},
// 			{},
// 			{}
// 	},
// 	{
// 		"Handlers",
// 			{B_GET_PROPERTY},
// 			{B_DIRECT_SPECIFIER},
// 			NULL, BLOOPER_PROCESS_INTERNALLY,
// 			{B_MESSENGER_TYPE},
// 			{},
// 			{}
// 	},
// 	{
// 		"Handler",
// 			{B_COUNT_PROPERTIES},
// 			{B_DIRECT_SPECIFIER},
// 			NULL, BLOOPER_PROCESS_INTERNALLY,
// 			{B_INT32_TYPE},
// 			{},
// 			{}
// 	},

// 	{ 0 }
// };

struct _loop_data_ {
	BLooper*	looper;
	thread_id	thread;
};


//	#pragma mark -


BLooper::BLooper(const char* name, int32 priority, int32 portCapacity)
	:
	BHandler(name)
{
	_InitData(name, priority, -1, portCapacity);
}


BLooper::~BLooper()
{
	// if (fRunCalled && !fTerminating) {
	// 	debugger("You can't call delete on a BLooper object "
	// 		"once it is running.");
	// }

	// Lock();

	// // In case the looper thread calls Quit() fLastMessage is not deleted.
	// if (fLastMessage) {
	// 	delete fLastMessage;
	// 	fLastMessage = NULL;
	// }

	// // Close the message port and read and reply to the remaining messages.
	// if (fMsgPort >= 0 && fOwnsPort)
	// 	close_port(fMsgPort);

	// // Clear the queue so our call to IsMessageWaiting() below doesn't give
	// // us bogus info
	// fDirectTarget->Close();

	// BMessage* message;
	// while ((message = fDirectTarget->Queue()->NextMessage()) != NULL) {
	// 	delete message;
	// 		// msg will automagically post generic reply
	// }

	// if (fOwnsPort) {
	// 	do {
	// 		delete ReadMessageFromPort(0);
	// 			// msg will automagically post generic reply
	// 	} while (IsMessageWaiting());

	// 	delete_port(fMsgPort);
	// }
	// fDirectTarget->Release();

	// // Clean up our filters
	// SetCommonFilterList(NULL);

	// AutoLocker<BLooperList> ListLock(gLooperList);
	// RemoveHandler(this);

	// // Remove all the "child" handlers
	// int32 count = fHandlers.CountItems();
	// for (int32 i = 0; i < count; i++) {
	// 	BHandler* handler = (BHandler*)fHandlers.ItemAtFast(i);
	// 	handler->SetNextHandler(NULL);
	// 	handler->SetLooper(NULL);
	// }
	// fHandlers.MakeEmpty();

	// Unlock();
	// gLooperList.RemoveLooper(this);
	// delete_sem(fLockSem);
}


void
BLooper::AddHandler(BHandler* handler)
{
	if (handler == NULL)
		return;

	AssertLocked();

	if (handler->Looper() == NULL) {
		fHandlers.AddItem(handler);
		handler->SetLooper(this);
		if (handler != this)	// avoid a cycle
			handler->SetNextHandler(this);
	}
}


bool
BLooper::RemoveHandler(BHandler* handler)
{
	if (handler == NULL)
		return false;

	AssertLocked();

	if (handler->Looper() == this && fHandlers.RemoveItem(handler)) {
		if (handler == fPreferred)
			fPreferred = NULL;

		handler->SetNextHandler(NULL);
		handler->SetLooper(NULL);
		return true;
	}

	return false;
}


int32
BLooper::CountHandlers() const
{
	AssertLocked();

	return fHandlers.CountItems();
}


BHandler*
BLooper::HandlerAt(int32 index) const
{
	AssertLocked();

	return (BHandler*)fHandlers.ItemAt(index);
}


int32
BLooper::IndexOf(BHandler* handler) const
{
	AssertLocked();

	return fHandlers.IndexOf(handler);
}


BHandler*
BLooper::PreferredHandler() const
{
	return fPreferred;
}


void
BLooper::SetPreferredHandler(BHandler* handler)
{
	if (handler && handler->Looper() == this && IndexOf(handler) >= 0) {
		fPreferred = handler;
	} else {
		fPreferred = NULL;
	}
}


thread_id
BLooper::Run()
{
	// AssertLocked();

	// if (fRunCalled) {
	// 	// Not allowed to call Run() more than once
	// 	debugger("can't call BLooper::Run twice!");
	// 	return fThread;
	// }

	// fThread = spawn_thread(_task0_, Name(), fInitPriority, this);
	// if (fThread < B_OK)
	// 	return fThread;

	// if (fMsgPort < B_OK)
	// 	return fMsgPort;

	// fRunCalled = true;
	// Unlock();

	// status_t err = resume_thread(fThread);
	// if (err < B_OK)
	// 	return err;

	// return fThread;
	return 1;
}


void
BLooper::Loop()
{
	// AssertLocked();

	// if (fRunCalled) {
	// 	// Not allowed to call Loop() or Run() more than once
	// 	debugger("can't call BLooper::Loop twice!");
	// 	return;
	// }

	// fThread = find_thread(NULL);
	fRunCalled = true;

	// task_looper();
}


void
BLooper::Quit()
{
	PRINT(("BLooper::Quit()\n"));

	// if (!IsLocked()) {
	// 	printf("ERROR - you must Lock a looper before calling Quit(), "
	// 		"team=%" B_PRId32 ", looper=%s\n", Team(),
	// 		Name() ? Name() : "unnamed");
	// }

	// // Try to lock
	// if (!Lock()) {
	// 	// We're toast already
	// 	return;
	// }

	// PRINT(("  is locked\n"));

	// if (!fRunCalled) {
	// 	PRINT(("  Run() has not been called yet\n"));
	// 	fTerminating = true;
	// 	delete this;
	// } else if (find_thread(NULL) == fThread) {
	// 	PRINT(("  We are the looper thread\n"));
	// 	fTerminating = true;
	// 	delete this;
	// 	exit_thread(0);
	// } else {
	// 	PRINT(("  Run() has already been called and we are not the looper thread\n"));

	// 	// As with sem in _Lock(), we need to cache this here in case the looper
	// 	// disappears before we get to the wait_for_thread() below
	// 	thread_id thread = Thread();

	// 	// We need to unlock here. Otherwise the looper thread can't
	// 	// dispatch the _QUIT_ message we're going to post.
	// 	UnlockFully();

	// 	// As per the BeBook, if we've been called by a thread other than
	// 	// our own, the rest of the message queue has to get processed.  So
	// 	// we put this in the queue, and when it shows up, we'll call Quit()
	// 	// from our own thread.
	// 	// QuitRequested() will not be called in this case.
	// 	PostMessage(_QUIT_);

	// 	// We have to wait until the looper is done processing any remaining
	// 	// messages.
	// 	status_t status;
	// 	while (wait_for_thread(thread, &status) == B_INTERRUPTED)
	// 		;
	// }

	PRINT(("BLooper::Quit() done\n"));
}


bool
BLooper::QuitRequested()
{
	return true;
}


bool
BLooper::Lock()
{
	// Defer to global _Lock(); see notes there
	return true;
}


void
BLooper::Unlock()
{
PRINT(("BLooper::Unlock()\n"));

PRINT(("BLooper::Unlock() done\n"));
}


bool
BLooper::IsLocked() const
{
	return true;
}

void BLooper::_ReservedLooper1() {}
void BLooper::_ReservedLooper2() {}
void BLooper::_ReservedLooper3() {}
void BLooper::_ReservedLooper4() {}
void BLooper::_ReservedLooper5() {}
void BLooper::_ReservedLooper6() {}



BLooper::BLooper(int32 priority, port_id port, const char* name)
{
	_InitData(name, priority, port, B_LOOPER_PORT_DEFAULT_CAPACITY);
}

bool
BLooper::AssertLocked() const
{
	if (!IsLocked()) {
		debugger("looper must be locked before proceeding\n");
		return false;
	}

	return true;
}

void
BLooper::check_lock()
{
}


void
BLooper::_InitData(const char* name, int32 priority, port_id port,
	int32 portCapacity)
{
	//gLooperList.AddLooper(this);
		// this will also lock this looper

	AddHandler(this);
}

