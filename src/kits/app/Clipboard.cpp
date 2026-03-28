/*
 * Copyright 2001-2009, Haiku Inc.
 * Copyright 2025, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Gabe Yoder (gyoder@stny.rr.com)
 *		Bill Hayden (hayden@haydentech.com)
 */


#include <Clipboard.h>

#include <Application.h>
#include <CosmoeBackendAPI.h>
#include <OS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BClipboard sClipboard(NULL);
BClipboard *be_clipboard = &sClipboard;

using namespace BPrivate;

static const bigtime_t kClipboardRefreshInterval = 2000000; // 2 seconds


BClipboard::BClipboard(const char *name, bool transient)
	:
	fLock("clipboard")
{
	if (name != NULL)
		fName = strdup(name);
	else
		fName = strdup("system");

	fData = NULL;  // Delay allocation until first use
	fCount = 0;
	fLastDownloadTime = 0;
	fHaveCachedData = false;
}


BClipboard::~BClipboard()
{
	free(fName);
	delete fData;
}


const char *
BClipboard::Name() const
{
	return (const char *)fName;
}


/*!	\brief Returns the (locally cached) number of commits to the clipboard.

	The returned value is the number of successful Commit() invocations for
	the clipboard represented by this object, either invoked on this object
	or another (even from another application).

	\return The number of commits to the clipboard.
*/
uint32
BClipboard::LocalCount() const
{
	return fCount;
}


/*!	\brief Returns the number of commits to the clipboard.

	The returned value is the number of successful Commit() invocations for
	the clipboard represented by this object, either invoked on this object
	or another (even from another application). This method retrieves the
	value directly from the system service managing the clipboards, so it is
	more expensive, but more up-to-date than LocalCount(), which returns a
	locally cached value.

	\return The number of commits to the clipboard.
*/
uint32
BClipboard::SystemCount() const
{
	// In our serverless implementation, SystemCount() is the same as LocalCount()
	return fCount;
}


status_t
BClipboard::StartWatching(BMessenger target)
{
	// Clipboard watching is not yet supported in the backend
	// implementation as we don't have a clipboard server to send notifications
	(void)target;
	return B_NOT_SUPPORTED;
}


status_t
BClipboard::StopWatching(BMessenger target)
{
	// Clipboard watching is not yet supported in the backend
	// implementation as we don't have a clipboard server to send notifications
	(void)target;
	return B_NOT_SUPPORTED;
}


bool
BClipboard::Lock()
{
	// Will this work correctly if clipboard is deleted while still waiting on
	// fLock.Lock() ?
	bool locked = fLock.Lock();

	if (locked)
		(void)_DownloadFromSystem(false);

	return locked;
}


void
BClipboard::Unlock()
{
	fLock.Unlock();
}


bool
BClipboard::IsLocked() const
{
	return fLock.IsLocked();
}


status_t
BClipboard::Clear()
{
	if (!_AssertLocked())
		return B_NOT_ALLOWED;

	_EnsureDataAllocated();
	return fData->MakeEmpty();
}


status_t
BClipboard::Commit()
{
	return Commit(false);
}


status_t
BClipboard::Commit(bool failIfChanged)
{
	if (!_AssertLocked())
		return B_NOT_ALLOWED;

	// Note: failIfChanged is not supported in the direct X11 backend
	// implementation as we don't have a clipboard server tracking versions
	(void)failIfChanged;

	return _UploadToSystem();
}


status_t
BClipboard::Revert()
{
	if (!_AssertLocked())
		return B_NOT_ALLOWED;

	_EnsureDataAllocated();
	status_t status = fData->MakeEmpty();

	//TODO get the data here
	if (status == B_OK)
		status = _DownloadFromSystem();

	return status;
}


BMessenger
BClipboard::DataSource() const
{
	return fDataSource;
}


BMessage *
BClipboard::Data() const
{
	if (!_AssertLocked())
		return NULL;

	const_cast<BClipboard*>(this)->_EnsureDataAllocated();
	return fData;
}


//	#pragma mark - Private methods


BClipboard::BClipboard(const BClipboard &)
{
	// This is private, and I don't use it, so I'm not going to implement it
}


BClipboard & BClipboard::operator=(const BClipboard &)
{
	// This is private, and I don't use it, so I'm not going to implement it
	return *this;
}


void BClipboard::_ReservedClipboard1() {}
void BClipboard::_ReservedClipboard2() {}
void BClipboard::_ReservedClipboard3() {}


bool
BClipboard::_AssertLocked() const
{
	// This function is for jumping to the debugger if not locked
	if (!fLock.IsLocked()) {
		debugger("The clipboard must be locked before proceeding.");
		return false;
	}
	return true;
}


void
BClipboard::_EnsureDataAllocated()
{
	if (fData == NULL)
		fData = new BMessage();
}


status_t
BClipboard::_DownloadFromSystem(bool force)
{
	_EnsureDataAllocated();

	bigtime_t now = system_time();
	if (!force && fHaveCachedData
		&& now - fLastDownloadTime < kClipboardRefreshInterval)
		return B_OK;
	
	// Get clipboard text from backend
	if (!be_app)
		return B_ERROR;
		
	cosmoe_display_t display = be_app->Display();
	if (!display)
		return B_ERROR;

	size_t length = 0;
	char* text = cosmoe_display_get_clipboard_text(display, &length);
	if (!text) {
		fData->MakeEmpty();
		fLastDownloadTime = now;
		fHaveCachedData = true;
		return B_OK;  // Empty clipboard is not an error
	}

	// Clear current data
	fData->MakeEmpty();

	// Add text data to the BMessage
	status_t status = fData->AddData("text/plain", B_MIME_TYPE, text, length);
	free(text);

	if (status == B_OK) {
		fLastDownloadTime = now;
		fHaveCachedData = true;
	}

	return status;
}


status_t
BClipboard::_UploadToSystem()
{
	_EnsureDataAllocated();
	
	// Get text from the BMessage
	const char* text = NULL;
	ssize_t length = 0;
	
	if (fData->FindData("text/plain", B_MIME_TYPE, (const void**)&text, &length) != B_OK) {
		// No text data in clipboard
		return B_OK;
	}

	// Upload to backend
	if (!be_app)
		return B_ERROR;
		
	cosmoe_display_t display = be_app->Display();
	if (!display)
		return B_ERROR;

	int result = cosmoe_display_set_clipboard_text(display, text, length);
	if (result != 0)
		return B_ERROR;

	// Increment commit count
	fCount++;
	fLastDownloadTime = system_time();
	fHaveCachedData = true;

	return B_OK;
}
