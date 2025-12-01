/*
 * Copyright 2025 Bill Hayden.
 * Copyright 2001-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold, ingo_weinhold@gmx.de
 *		Bill Hayden, hayden@haydentech.com
 */


#include <MessageRunner.h>

#include <Application.h>
#include <AppMisc.h>
#include <List.h>

#include <errno.h>



using namespace BPrivate;

typedef struct RunnerData {
	int32 token;
	BMessenger target;
	BMessage* message;
	bigtime_t interval;
	int32 count;
	BMessenger replyTo;
	pthread_t thread;
	bool detach;
	pthread_mutex_t mutex;
	volatile bool shouldStop;
} RunnerData;


int32 nextToken = 1;
BList messageRunners;
BLocker messageRunnersLock("message runners");

void destroy_runner(int32 token)
{
	if (token >= 0) {
		messageRunnersLock.Lock();

		for (int32 i = 0; i < messageRunners.CountItems(); i++) {
			RunnerData* runner = (RunnerData*)messageRunners.ItemAt(i);
			if (runner->token == token) {
				messageRunners.RemoveItem(i);

				BMessage* message = runner->message;

				if (!runner->detach) {
					// Signal thread to stop
					pthread_mutex_lock(&runner->mutex);
					runner->shouldStop = true;
					pthread_mutex_unlock(&runner->mutex);
					
					pthread_cancel(runner->thread);
					pthread_join(runner->thread, NULL);
					pthread_mutex_destroy(&runner->mutex);
					free(runner);
				}
				
				if (message != NULL)
					delete message;
				
				break;
			}
		}
	
		messageRunnersLock.Unlock();
	}
}


/*!	\brief Creates and initializes a new BMessageRunner.

	The target for replies to the delivered message(s) is \c be_app_messenger.

	The success of the initialization can (and should) be asked for via
	InitCheck(). This object will not take ownership of the \a message, you
	may freely change or delete it after creation.

	\note As soon as the last message has been sent, the message runner
		  becomes unusable. InitCheck() will still return \c B_OK, but
		  SetInterval(), SetCount() and GetInfo() will fail.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
*/
BMessageRunner::BMessageRunner(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count)
	:
	fToken(-1)
{
	_InitData(target, message, interval, count, be_app_messenger);
}


/*!	\brief Creates and initializes a new BMessageRunner.

	The target for replies to the delivered message(s) is \c be_app_messenger.

	The success of the initialization can (and should) be asked for via
	InitCheck(). This object will not take ownership of the \a message, you
	may freely change or delete it after creation.

	\note As soon as the last message has been sent, the message runner
		  becomes unusable. InitCheck() will still return \c B_OK, but
		  SetInterval(), SetCount() and GetInfo() will fail.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
*/
BMessageRunner::BMessageRunner(BMessenger target, const BMessage& message,
	bigtime_t interval, int32 count)
	:
	fToken(-1)
{
	_InitData(target, &message, interval, count, be_app_messenger);
}


/*!	\brief Creates and initializes a new BMessageRunner.

	This constructor version additionally allows to specify the target for
	replies to the delivered message(s).

	The success of the initialization can (and should) be asked for via
	InitCheck(). This object will not take ownership of the \a message, you
	may freely change or delete it after creation.

	\note As soon as the last message has been sent, the message runner
		  becomes unusable. InitCheck() will still return \c B_OK, but
		  SetInterval(), SetCount() and GetInfo() will fail.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
	\param replyTo Target replies to the delivered message(s) shall be sent to.
*/
BMessageRunner::BMessageRunner(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count, BMessenger replyTo)
	:
	fToken(-1)
{
	_InitData(target, message, interval, count, replyTo);
}


/*!	\brief Creates and initializes a new BMessageRunner.

	This constructor version additionally allows to specify the target for
	replies to the delivered message(s).

	The success of the initialization can (and should) be asked for via
	InitCheck(). This object will not take ownership of the \a message, you
	may freely change or delete it after creation.

	\note As soon as the last message has been sent, the message runner
		  becomes unusable. InitCheck() will still return \c B_OK, but
		  SetInterval(), SetCount() and GetInfo() will fail.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
	\param replyTo Target replies to the delivered message(s) shall be sent to.
*/
BMessageRunner::BMessageRunner(BMessenger target, const BMessage& message,
	bigtime_t interval, int32 count, BMessenger replyTo)
	:
	fToken(-1)
{
	_InitData(target, &message, interval, count, replyTo);
}


/*!	\brief Frees all resources associated with the object.
*/
BMessageRunner::~BMessageRunner()
{
	destroy_runner(fToken);
}


/*!	\brief Returns the status of the initialization.

	\note As soon as the last message has been sent, the message runner
		  becomes unusable. InitCheck() will still return \c B_OK, but
		  SetInterval(), SetCount() and GetInfo() will fail.

	\return \c B_OK, if the object is properly initialized, an error code
			otherwise.
*/
status_t
BMessageRunner::InitCheck() const
{
	return fToken >= 0 ? B_OK : fToken;
}


/*!	\brief Sets the interval of time between messages.
	\param interval The new interval in microseconds.
	\return
	- \c B_OK: Everything went fine.
	- \c B_NO_INIT: The message runner is not properly initialized.
	- \c B_BAD_VALUE: \a interval is \c 0 or negative, or the message runner
	  has already sent all messages to be sent and has become unusable.
*/
status_t
BMessageRunner::SetInterval(bigtime_t interval)
{
	return _SetParams(true, interval, false, 0);
}


/*!	\brief Sets the number of times message shall be sent.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
	- \c B_BAD_VALUE: The message runner has already sent all messages to be
	  sent and has become unusable.
	\return
	- \c B_OK: Everything went fine.
	- \c B_NO_INIT: The message runner is not properly initialized.
*/
status_t
BMessageRunner::SetCount(int32 count)
{
	return _SetParams(false, 0, true, count);
}


/*!	\brief Returns the time interval between two messages and the number of
		   times the message has still to be sent.

	Both parameters (\a interval and \a count) may be \c NULL.

	\param interval Pointer to a pre-allocated bigtime_t variable to be set
		   to the time interval. May be \c NULL.
	\param count Pointer to a pre-allocated int32 variable to be set
		   to the number of times the message has still to be sent.
		   May be \c NULL.
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: The message runner is not longer valid. All the
	  messages that had to be sent have already been sent.
*/
status_t
BMessageRunner::GetInfo(bigtime_t* interval, int32* count) const
{
	bool found = false;

	if (fToken < 0)
		return B_BAD_VALUE;

	messageRunnersLock.Lock();

	for (int32 i = 0; i < messageRunners.CountItems(); i++) {
		RunnerData* runner = (RunnerData*)messageRunners.ItemAt(i);
		if (runner->token == fToken) {
			pthread_mutex_lock(&runner->mutex);
			if (interval)
				*interval = runner->interval;

			if (count)
				*count = runner->count;
			pthread_mutex_unlock(&runner->mutex);

			found = true;
			break;
		}
	}

	messageRunnersLock.Unlock();

	if (count && *count == 0)
		return B_BAD_VALUE;

	return found ? B_OK : B_BAD_VALUE;
}


/*!	\brief Creates and initializes a detached BMessageRunner.

	You cannot alter the runner after the creation, and it will be deleted
	automatically once it is done.
	The target for replies to the delivered message(s) is \c be_app_messenger.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
*/
/*static*/ status_t
BMessageRunner::StartSending(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count)
{
	int32 token = _RegisterRunner(target, message, interval, count, true,
		be_app_messenger);

	return token >= B_OK ? B_OK : token;
}


/*!	\brief Creates and initializes a detached BMessageRunner.

	You cannot alter the runner after the creation, and it will be deleted
	automatically once it is done.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
		   between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
		   A value less than \c 0 for an unlimited number of repetitions.
	\param replyTo Target replies to the delivered message(s) shall be sent to.
*/
/*static*/ status_t
BMessageRunner::StartSending(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count, BMessenger replyTo)
{
	int32 token = _RegisterRunner(target, message, interval, count, true,
		replyTo);

	return token >= B_OK ? B_OK : token;
}


// FBC
void BMessageRunner::_ReservedMessageRunner1() {}
void BMessageRunner::_ReservedMessageRunner2() {}
void BMessageRunner::_ReservedMessageRunner3() {}
void BMessageRunner::_ReservedMessageRunner4() {}
void BMessageRunner::_ReservedMessageRunner5() {}
void BMessageRunner::_ReservedMessageRunner6() {}


#ifdef __HAIKU_BEOS_COMPATIBLE
//! Privatized copy constructor to prevent usage.
BMessageRunner::BMessageRunner(const BMessageRunner &)
	:
	fToken(-1)
{
}


//! Privatized assignment operator to prevent usage.
BMessageRunner&
BMessageRunner::operator=(const BMessageRunner&)
{
	return* this;
}
#endif


/*!	Initializes the BMessageRunner.

	The success of the initialization can (and should) be asked for via
	InitCheck().

	\note As soon as the last message has been sent, the message runner
	      becomes unusable. InitCheck() will still return \c B_OK, but
	      SetInterval(), SetCount() and GetInfo() will fail.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
	       between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
	       A value less than \c 0 for an unlimited number of repetitions.
	\param replyTo Target replies to the delivered message(s) shall be sent to.
*/
void
BMessageRunner::_InitData(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count, BMessenger replyTo)
{
	fToken = _RegisterRunner(target, message, interval, count, false, replyTo);
}


void* MessageRunnerLoop(void *data)
{
	int32 *runnerToken = (int32 *)data;
	RunnerData* runner = NULL;

	messageRunnersLock.Lock();

	// Use the token passed in data to find our runner
	for (int32 i = 0; i < messageRunners.CountItems(); i++) {
		runner = (RunnerData*)messageRunners.ItemAt(i);

		if (runner == NULL)
			continue;

		if (runner->token == *runnerToken) {
			break;
		}
	}

	messageRunnersLock.Unlock();

	// Bad token?  We never found the desired runner
	if (runner == NULL) {
		printf("----- message runner loop: could not find runner for token %d\n",
			*runnerToken);
		return NULL;
	}

	for (;;) {
		// Read interval under mutex protection to avoid race conditions
		pthread_mutex_lock(&runner->mutex);
		bigtime_t interval = runner->interval;
		bool shouldStop = runner->shouldStop;
		pthread_mutex_unlock(&runner->mutex);

		if (shouldStop)
			break;

		// Use nanosleep instead of deprecated usleep, and make it interruptible
		if (interval > 0) {
			struct timespec ts;
			ts.tv_sec = interval / 1000000;
			ts.tv_nsec = (interval % 1000000) * 1000;
			
			// nanosleep can be interrupted by signals (like pthread_cancel)
			while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
				// Check if we should stop after interruption
				pthread_mutex_lock(&runner->mutex);
				shouldStop = runner->shouldStop;
				pthread_mutex_unlock(&runner->mutex);
				if (shouldStop)
					break;
			}
		}

		if (shouldStop)
			break;

		status_t err = runner->target.SendMessage(runner->message, runner->replyTo);
		if (err != B_OK) {
			printf("----- message runner failed to send message %c%c%c%c, err = %d\n",
				runner->message->what >> 24,
				(runner->message->what >> 16) & 0xFF,
				(runner->message->what >> 8) & 0xFF,
				runner->message->what & 0xFF, err);
		}
		
		pthread_mutex_lock(&runner->mutex);
		if (runner->count > 0) {
			runner->count--;
			if (runner->count == 0) {
				if (runner->detach) {
					pthread_mutex_unlock(&runner->mutex);
					free(runner);
					runner = NULL;
				} else {
					pthread_mutex_unlock(&runner->mutex);
				}
				break;
			}
		}
		pthread_mutex_unlock(&runner->mutex);
	}

	// If we are detached, we must clean up after ourselves
	if (runner && runner->detach) {
		destroy_runner(*runnerToken);
	}

	return NULL;
}


/*!	Registers the BMessageRunner.

	\param target Target of the message(s).
	\param message The message to be sent to the target.
	\param interval Period of time before the first message is sent and
	       between messages (if more than one shall be sent) in microseconds.
	\param count Specifies how many times the message shall be sent.
	       A value less than \c 0 for an unlimited number of repetitions.
	\param replyTo Target replies to the delivered message(s) shall be sent to.

	\return The token the message runner is registered with, or the error code
	        while trying to register it.
*/
/*static*/ int32
BMessageRunner::_RegisterRunner(BMessenger target, const BMessage* message,
	bigtime_t interval, int32 count, bool detach, BMessenger replyTo)
{
	if (message == NULL || count == 0 || (count < 0 && detach))
		return B_BAD_VALUE;

	RunnerData* runner = (RunnerData*)malloc(sizeof(RunnerData));
	if (runner == NULL)
		return B_NO_MEMORY;

	runner->target = target;
	runner->message = new BMessage(*message);
	if (runner->message == NULL) {
		free(runner);
		return B_NO_MEMORY;
	}
	runner->interval = interval;
	runner->count = count;
	runner->detach = detach;
	runner->replyTo = replyTo;
	runner->token = -1;
	runner->shouldStop = false;
	pthread_mutex_init(&runner->mutex, NULL);

	if (pthread_create(&runner->thread, NULL, MessageRunnerLoop, runner) == 0) {
		messageRunnersLock.Lock();
		runner->token = nextToken++;
		messageRunners.AddItem(runner);
		messageRunnersLock.Unlock();
		return runner->token;
	}

	// pthread_create failed - clean up properly
	pthread_mutex_destroy(&runner->mutex);
	delete runner->message;
	free(runner);
	return B_ERROR;
}


/*!	Sets the message runner's interval and count parameters.

	The parameters \a resetInterval and \a resetCount specify whether
	the interval or the count parameter respectively shall be reset.

	At least one parameter must be set, otherwise the methods returns
	\c B_BAD_VALUE.

	\param resetInterval \c true, if the interval shall be reset, \c false
	       otherwise -- then \a interval is ignored.
	\param interval The new interval in microseconds.
	\param resetCount \c true, if the count shall be reset, \c false
	       otherwise -- then \a count is ignored.
	\param count Specifies how many times the message shall be sent.
	       A value less than \c 0 for an unlimited number of repetitions.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE The message runner is not longer valid. All the
	        messages that had to be sent have already been sent. Or both
	        \a resetInterval and \a resetCount are \c false.
*/
status_t
BMessageRunner::_SetParams(bool resetInterval, bigtime_t interval,
	bool resetCount, int32 count)
{
	bool found = false;

	if ((!resetInterval && !resetCount) || fToken < 0)
		return B_BAD_VALUE;

	messageRunnersLock.Lock();

	for (int32 i = 0; i < messageRunners.CountItems(); i++) {
		RunnerData* runner = (RunnerData*)messageRunners.ItemAt(i);
		if (runner == NULL)
			continue;
		if (runner->token == fToken) {
			pthread_mutex_lock(&runner->mutex);
			if (resetInterval)
				runner->interval = interval;

			if (resetCount && runner->count != 0)
				runner->count = count;

			found = runner->count != 0;
			pthread_mutex_unlock(&runner->mutex);
			break;
		}
	}

	messageRunnersLock.Unlock();
	return found ? B_OK : B_BAD_VALUE;
}
