// SMTarget.cpp

#include <stdio.h>

#include <OS.h>
#include <TestUtils.h>
#include <cppunit/TestAssert.h>

#include <MessengerPrivate.h>

#include "SMTarget.h"
#include "SMLooper.h"
#include "SMRemoteTargetApp.h"


using namespace std;


// SMTarget

// constructor
SMTarget::SMTarget()
{
}

// destructor
SMTarget::~SMTarget()
{
}

// Init
void
SMTarget::Init(bigtime_t unblockTime, bigtime_t replyDelay)
{
}

// Handler
BHandler *
SMTarget::Handler()
{
	return NULL;
}

// Messenger
BMessenger
SMTarget::Messenger()
{
	return BMessenger();
}

// DeliverySuccess
bool
SMTarget::DeliverySuccess()
{
	return false;
}


// LocalSMTarget

// constructor
LocalSMTarget::LocalSMTarget(bool preferred)
			 : SMTarget(),
			   fHandler(NULL),
			   fLooper(NULL)
{
	// create looper and handler
	fLooper = new SMLooper;
	fLooper->Run();
	if (!preferred) {
		fHandler = new SMHandler;
		CHK(fLooper->Lock());
		fLooper->AddHandler(fHandler);
		fLooper->Unlock();
	}
}

// destructor
LocalSMTarget::~LocalSMTarget()
{
	if (fLooper) {
		fLooper->Lock();
		if (fHandler) {
			fLooper->RemoveHandler(fHandler);
			delete fHandler;
		}
		fLooper->Quit();
	}
}

// Init
void
LocalSMTarget::Init(bigtime_t unblockTime, bigtime_t replyDelay)
{
	fLooper->SetReplyDelay(replyDelay);
	fLooper->BlockUntil(unblockTime);
}

// Handler
BHandler *
LocalSMTarget::Handler()
{
	return fHandler;
}

// Messenger
BMessenger
LocalSMTarget::Messenger()
{
	return BMessenger(fHandler, fLooper);
}

// DeliverySuccess
bool
LocalSMTarget::DeliverySuccess()
{
	return fLooper->DeliverySuccess();
}


// RemoteSMTarget

// constructor
RemoteSMTarget::RemoteSMTarget(bool preferred)
			  : SMTarget(),
				fLocalTarget(NULL),
				fLocalPort(-1),
				fRemotePort(-1),
				fTarget()
{
	fLocalTarget = new LocalSMTarget(preferred);
	fTarget = fLocalTarget->Messenger();

	BMessenger::Private targetPrivate(fTarget);
	team_id fakeRemoteTeam = team_get_current_team_id() + 1;
	targetPrivate.SetTo(fakeRemoteTeam, targetPrivate.Port(),
		targetPrivate.Token());
}

// destructor
RemoteSMTarget::~RemoteSMTarget()
{
	delete fLocalTarget;
}

// Init
void
RemoteSMTarget::Init(bigtime_t unblockTime, bigtime_t replyDelay)
{
	fLocalTarget->Init(unblockTime, replyDelay);
}

// Messenger
BMessenger
RemoteSMTarget::Messenger()
{
	return fTarget;
}

// DeliverySuccess
bool
RemoteSMTarget::DeliverySuccess()
{
	return fLocalTarget->DeliverySuccess();
}

// _SendRequest
status_t
RemoteSMTarget::_SendRequest(int32 code, const void *buffer, size_t size)
{
	return write_port(fRemotePort, code, buffer, size);
}

// _GetReply
status_t
RemoteSMTarget::_GetReply(int32 expectedCode, void *buffer, size_t size)
{
	status_t error = B_OK;
	int32 code;
	ssize_t readSize = read_port(fLocalPort, &code, buffer, size);
	if (readSize < 0)
		error = readSize;
	else if ((uint32)readSize != size)
		error = B_ERROR;
	else if (code != expectedCode)
		error = B_ERROR;
	return error;
}

// ID
int32 RemoteSMTarget::fID = 0;


