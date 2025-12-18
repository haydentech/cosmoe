// MessageRunnerTestHelpers.cpp

#include <stdio.h>

#include <Autolock.h>
#include <MessageRunner.h>

#include "MessageRunnerTestHelpers.h"

enum {
	JITTER	= 10000,
};

// constructor
MessageRunnerTestHandler::MessageRunnerTestHandler()
	: BHandler("message runner test handler"),
	  fReplyCount()
{
}

// destructor
MessageRunnerTestHandler::~MessageRunnerTestHandler()
{
}

// MessageReceived
void
MessageRunnerTestHandler::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case MSG_REPLY:
			fReplyCount++;
			break;
	}
}


///////////////////////////
// MessageRunnerTestLooper

struct MessageRunnerTestLooper::MessageInfo {
	bigtime_t	time;
};

// constructor
MessageRunnerTestLooper::MessageRunnerTestLooper()
	: BLooper(),
	  fMessageInfos()
{
}

// destructor
MessageRunnerTestLooper::~MessageRunnerTestLooper()
{
	for (int32 i = 0; MessageInfo *info = MessageInfoAt(i); i++)
		delete info;
}

// MessageReceived
void
MessageRunnerTestLooper::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case MSG_RUNNER_MESSAGE:
		{
			MessageInfo *info = new MessageInfo;
			info->time = system_time();
			fMessageInfos.AddItem(info);
			message->SendReply(MSG_REPLY);
			break;
		}
	}
}

// CheckMessages
bool
MessageRunnerTestLooper::CheckMessages(bigtime_t startTime, bigtime_t interval,
									   int32 count)
{
	return CheckMessages(0, startTime, interval, count);
}

// CheckMessages
bool
MessageRunnerTestLooper::CheckMessages(int32 skip, bigtime_t startTime,
									   bigtime_t interval, int32 count)
{
	BAutolock _lock(this);
	bool result = (fMessageInfos.CountItems() == count + skip);
if (!result) {
printf("message counts don't match: %d vs. %d\n", fMessageInfos.CountItems(),
count + skip);
}
	for (int32 i = 0; result && i < count; i++) {
		MessageInfo *info = MessageInfoAt(i + skip);
		bigtime_t expectedTime = startTime + (i + 1) * interval;
		result = (expectedTime - JITTER < info->time
				  && info->time < expectedTime + JITTER);
if (!result)
printf("message out of time: %ld vs. %ld\n", info->time, expectedTime);
	}
	return result;
}

// MessageInfoAt
MessageRunnerTestLooper::MessageInfo*
MessageRunnerTestLooper::MessageInfoAt(int32 index) const
{
	return static_cast<MessageInfo*>(fMessageInfos.ItemAt(index));
}


////////////////////////
// MessageRunnerTestApp

// constructor
MessageRunnerTestApp::MessageRunnerTestApp(const char *signature)
	: BApplication(signature),
	  fThread(-1),
	  fReadySem(-1),
	  fReplyCount(0),
	  fLooper(NULL),
	  fHandler(NULL)
{
	// create a semaphore to signal when app is ready
	fReadySem = create_sem(0, "app ready sem");
	
	// create a looper
	fLooper = new MessageRunnerTestLooper;
	fLooper->Run();
	// create a handler
	fHandler = new MessageRunnerTestHandler;
	AddHandler(fHandler);
	// start out message loop in a different thread
	Unlock();
	fThread = spawn_thread(_ThreadEntry, "message runner app thread",
						   B_NORMAL_PRIORITY, this);
	resume_thread(fThread);
	
	// Wait for app thread to be ready
	acquire_sem(fReadySem);
}

// destructor
MessageRunnerTestApp::~MessageRunnerTestApp()
{
	printf("MessageRunnerTestApp destructor: starting\n");
	fflush(stdout);
	
	// quit the looper
	fLooper->Lock();
	fLooper->Quit();
	
	printf("MessageRunnerTestApp destructor: about to lock for quit\n");
	fflush(stdout);
	
	// shut down our message loop
	// Need to lock to post the message, then unlock so it can be processed
	Lock();
	PostMessage(B_QUIT_REQUESTED);
	Unlock();
	
	printf("MessageRunnerTestApp destructor: posted quit, waiting for thread\n");
	fflush(stdout);
	
	// wait for the thread to exit
	int32 dummy;
	wait_for_thread(fThread, &dummy);
	
	printf("MessageRunnerTestApp destructor: thread exited\n");
	fflush(stdout);
	
	// clean up semaphore
	delete_sem(fReadySem);
	
	// delete the handler (commented out as it may have already been deleted)
	// RemoveHandler(fHandler);
	// delete fHandler;
	
	printf("MessageRunnerTestApp destructor: done\n");
	fflush(stdout);
}

// MessageReceived
void
MessageRunnerTestApp::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case MSG_REPLY:
			fReplyCount++;
			printf("MessageRunnerTestApp: got reply count=%d\n", fReplyCount);
			break;
	}
}

// QuitRequested
bool
MessageRunnerTestApp::QuitRequested()
{
	return true;
}

// _ThreadEntry
int32
MessageRunnerTestApp::_ThreadEntry(void *data)
{
	MessageRunnerTestApp *app = static_cast<MessageRunnerTestApp*>(data);
	app->Lock();
	// Signal that we're ready
	release_sem(app->fReadySem);
	app->Run();
	return 0;
}

