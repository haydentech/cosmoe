/*
 * Copyright 2003-2009 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stefano Ceccherini <stefano.ceccherini@gmail.com>
 *		Carwyn Jones <turok2@currantbun.com>
 */


#include <DirectWindow.h>

#include <algorithm>
#include <stdlib.h>
#include <string.h>

#include <Application.h>
#include <Screen.h>

#include <AppServerLink.h>
#include <ApplicationPrivate.h>
#include <ServerProtocol.h>

#include <cairo.h>


static const uint32 kDirectWindowInfoSize = sizeof(direct_buffer_info)
	+ 15 * sizeof(clipping_rect);


//	#pragma mark -


BDirectWindow::BDirectWindow(BRect frame, const char* title, window_type type,
		uint32 flags, uint32 workspace)
	:
	BWindow(frame, title, type, flags, workspace)
{
	_InitData();
}


BDirectWindow::BDirectWindow(BRect frame, const char* title, window_look look,
		window_feel feel, uint32 flags, uint32 workspace)
	:
	BWindow(frame, title, look, feel, flags, workspace)
{
	_InitData();
}


BDirectWindow::~BDirectWindow()
{
	_DisposeData();
}


//	#pragma mark - BWindow API implementation


BArchivable*
BDirectWindow::Instantiate(BMessage* data)
{
	return NULL;
}


status_t
BDirectWindow::Archive(BMessage* data, bool deep) const
{
	return inherited::Archive(data, deep);
}


void
BDirectWindow::Quit()
{
	inherited::Quit();
}


void
BDirectWindow::DispatchMessage(BMessage* message, BHandler* handler)
{
	inherited::DispatchMessage(message, handler);
}


void
BDirectWindow::MessageReceived(BMessage* message)
{
	inherited::MessageReceived(message);
}


void
BDirectWindow::FrameMoved(BPoint newPosition)
{
	inherited::FrameMoved(newPosition);
	if (fConnectionEnable)
		_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_MODIFY
			| B_BUFFER_MOVED | B_CLIPPING_MODIFIED));
}


void
BDirectWindow::WorkspacesChanged(uint32 oldWorkspaces, uint32 newWorkspaces)
{
	inherited::WorkspacesChanged(oldWorkspaces, newWorkspaces);
}


void
BDirectWindow::WorkspaceActivated(int32 index, bool state)
{
	inherited::WorkspaceActivated(index, state);
}


void
BDirectWindow::FrameResized(float newWidth, float newHeight)
{
	inherited::FrameResized(newWidth, newHeight);
	if (fConnectionEnable)
		_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_MODIFY
			| B_BUFFER_RESIZED | B_CLIPPING_MODIFIED));
}


void
BDirectWindow::Minimize(bool minimize)
{
	inherited::Minimize(minimize);
}


void
BDirectWindow::Zoom(BPoint recPosition, float recWidth, float recHeight)
{
	inherited::Zoom(recPosition, recWidth, recHeight);
}


void
BDirectWindow::ScreenChanged(BRect screenFrame, color_space depth)
{
	inherited::ScreenChanged(screenFrame, depth);
	if (fConnectionEnable)
		_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_MODIFY
			| B_BUFFER_RESET | B_CLIPPING_MODIFIED));
}


void
BDirectWindow::MenusBeginning()
{
	inherited::MenusBeginning();
}


void
BDirectWindow::MenusEnded()
{
	inherited::MenusEnded();
}


void
BDirectWindow::WindowActivated(bool state)
{
	inherited::WindowActivated(state);
}


void
BDirectWindow::Show()
{
	inherited::Show();
	if (!fConnectionEnable)
		_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_START
			| B_CLIPPING_MODIFIED | B_BUFFER_RESET));
}


void
BDirectWindow::Hide()
{
	if (fConnectionEnable)
		_NotifyDirectConnection(B_DIRECT_STOP);
	inherited::Hide();
}


BHandler*
BDirectWindow::ResolveSpecifier(BMessage* message, int32 index,
	BMessage* specifier, int32 what, const char* property)
{
	return inherited::ResolveSpecifier(message, index, specifier, what,
		property);
}


status_t
BDirectWindow::GetSupportedSuites(BMessage* data)
{
	return inherited::GetSupportedSuites(data);
}


status_t
BDirectWindow::Perform(perform_code d, void* arg)
{
	return inherited::Perform(d, arg);
}


void
BDirectWindow::task_looper()
{
	inherited::task_looper();
}


BMessage*
BDirectWindow::ConvertToMessage(void* raw, int32 code)
{
	return inherited::ConvertToMessage(raw, code);
}


//	#pragma mark - BDirectWindow specific API


void
BDirectWindow::DirectConnected(direct_buffer_info* info)
{
}


status_t
BDirectWindow::GetClippingRegion(BRegion* region, BPoint* origin) const
{
	if (region == NULL)
		return B_BAD_VALUE;

	if (!_LockDirect())
		return B_ERROR;

	if (!fInDirectConnect || fBufferDesc == NULL) {
		_UnlockDirect();
		return B_ERROR;
	}

	int32 originX = origin != NULL ? (int32)origin->x : 0;
	int32 originY = origin != NULL ? (int32)origin->y : 0;

	region->MakeEmpty();
	uint32 clipCount = std::min(fBufferDesc->clip_list_count,
		(uint32)((fInfoAreaSize - sizeof(direct_buffer_info))
			/ sizeof(clipping_rect) + 1));
	for (uint32 index = 0; index < clipCount; index++)
		region->Include(fBufferDesc->clip_list[index]);
	region->OffsetBy(-originX, -originY);

	_UnlockDirect();
	return B_OK;
}


status_t
BDirectWindow::SetFullScreen(bool enable)
{
	if (fIsFullScreen == enable)
		return B_OK;

	if (!Lock())
		return B_ERROR;

	if (enable) {
		BScreen screen(this);
		BRect frame = screen.Frame();
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
	}

	fIsFullScreen = enable;
	_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_MODIFY
		| B_BUFFER_MOVED | B_BUFFER_RESIZED | B_CLIPPING_MODIFIED));
	Unlock();
	return B_OK;
}


bool
BDirectWindow::IsFullScreen() const
{
	return fIsFullScreen;
}


/*static*/ bool
BDirectWindow::SupportsWindowMode(screen_id id)
{
	return true;
}


//	#pragma mark - Private methods


/*static*/ int32
BDirectWindow::_daemon_thread(void* arg)
{
	return static_cast<BDirectWindow*>(arg)->_DirectDaemon();
}


int32
BDirectWindow::_DirectDaemon()
{
	while (!fDaemonKiller) {
		bool requestUpdate = false;

		_LockDirect();
		if (fBackingSurface != NULL) {
			cairo_surface_mark_dirty(fBackingSurface);
			fBackingSurfaceValid = true;
			fBackingSurfaceDirtyRegion.Include(Bounds());
			requestUpdate = !fUpdatesDisabled && be_app != NULL
				&& be_app->Display() != NULL && fWindowToken != B_NULL_TOKEN;
		}
		_UnlockDirect();

		if (requestUpdate) {
			BEGIN_MESSAGE
			fLink->StartMessage(AS_FORCE_UPDATE);
			fLink->Attach<int32_t>(fWindowToken);
			fLink->Attach<BRect>(BRect(0, 0, -1, -1));
			fLink->Flush();
		}

		snooze(16000);
	}

	return B_OK;
}


bool
BDirectWindow::_LockDirect() const
{
	BDirectWindow* casted = const_cast<BDirectWindow*>(this);
	pthread_mutex_lock(&casted->fBackingSurfaceLock);
	return true;
}


void
BDirectWindow::_UnlockDirect() const
{
	BDirectWindow* casted = const_cast<BDirectWindow*>(this);
	pthread_mutex_unlock(&casted->fBackingSurfaceLock);
}


void
BDirectWindow::_InitData()
{
	fDaemonKiller = false;
	fConnectionEnable = false;
	fIsFullScreen = false;
	fInDirectConnect = false;
	fDirectLock = 0;
	fDirectSem = -1;
	fDirectLockCount = 0;
	fDirectLockOwner = -1;
	fDirectLockStack = NULL;
	fDisableSem = -1;
	fDisableSemAck = -1;
	fInitStatus = 0;
	fInfoAreaSize = kDirectWindowInfoSize;
	fClonedClippingArea = -1;
	fSourceClippingArea = -1;
	fDirectDaemonId = -1;
	fBufferDesc = (direct_buffer_info*)calloc(1, fInfoAreaSize);
}


void
BDirectWindow::_DisposeData()
{
	if (fConnectionEnable)
		_NotifyDirectConnection(B_DIRECT_STOP);

	free(fBufferDesc);
	fBufferDesc = NULL;
}


void
BDirectWindow::_BackingSurfaceWillChange()
{
	if (fConnectionEnable)
		_NotifyDirectConnection(B_DIRECT_STOP);
}


void
BDirectWindow::_BackingSurfaceDidChange()
{
	if (!IsHidden())
		_NotifyDirectConnection((direct_buffer_state)(B_DIRECT_START
			| B_CLIPPING_MODIFIED | B_BUFFER_RESET));
}


void
BDirectWindow::_FillDirectBufferInfo(direct_buffer_state state)
{
	memset(fBufferDesc, 0, fInfoAreaSize);

	fBufferDesc->buffer_state = state;
	fBufferDesc->driver_state = (direct_driver_state)0;
	fBufferDesc->bits = fBackingSurface != NULL
		? cairo_image_surface_get_data(fBackingSurface) : NULL;
	fBufferDesc->pci_bits = NULL;
	fBufferDesc->bytes_per_row = fBackingSurface != NULL
		? cairo_image_surface_get_stride(fBackingSurface) : 0;
	fBufferDesc->bits_per_pixel = 32;
	fBufferDesc->pixel_format = B_RGBA32;
	fBufferDesc->layout = B_BUFFER_NONINTERLEAVED;
	fBufferDesc->orientation = B_BUFFER_TOP_TO_BOTTOM;

	int32 width = 0;
	int32 height = 0;
	if (fBackingSurface != NULL) {
		width = cairo_image_surface_get_width(fBackingSurface);
		height = cairo_image_surface_get_height(fBackingSurface);
	}

	clipping_rect bounds;
	bounds.left = 0;
	bounds.top = 0;
	bounds.right = std::max(0, width - 1);
	bounds.bottom = std::max(0, height - 1);

	fBufferDesc->clip_list_count = width > 0 && height > 0 ? 1 : 0;
	fBufferDesc->window_bounds = bounds;
	fBufferDesc->clip_bounds = bounds;
	fBufferDesc->clip_list[0] = bounds;
}


void
BDirectWindow::_NotifyDirectConnection(direct_buffer_state state)
{
	if (fBufferDesc == NULL || !_LockDirect())
		return;

	thread_id threadToWait = -1;

	if (fBackingSurface != NULL)
		cairo_surface_flush(fBackingSurface);

	_FillDirectBufferInfo(state);

	fInDirectConnect = true;
	DirectConnected(fBufferDesc);
	fInDirectConnect = false;

	if (fBackingSurface != NULL) {
		cairo_surface_mark_dirty(fBackingSurface);
		fBackingSurfaceValid = true;
		fBackingSurfaceDirtyRegion.Include(Bounds());
	}

	if ((state & B_DIRECT_MODE_MASK) == B_DIRECT_START)
		fConnectionEnable = true;
	else if ((state & B_DIRECT_MODE_MASK) == B_DIRECT_STOP) {
		fConnectionEnable = false;
		fDaemonKiller = true;
		threadToWait = fDirectDaemonId;
		fDirectDaemonId = -1;
	}

	if ((state & B_DIRECT_MODE_MASK) == B_DIRECT_START
		&& fDirectDaemonId < 0) {
		fDaemonKiller = false;
		fDirectDaemonId = spawn_thread(_daemon_thread, "direct refresh",
			B_DISPLAY_PRIORITY, this);
		if (fDirectDaemonId >= 0) {
			if (resume_thread(fDirectDaemonId) != B_OK)
				fDirectDaemonId = -1;
		}
	}

	_UnlockDirect();

	if (threadToWait >= 0 && threadToWait != find_thread(NULL)) {
		status_t result;
		wait_for_thread(threadToWait, &result);
	}

	if (!fUpdatesDisabled && be_app != NULL && be_app->Display() != NULL
		&& fWindowToken != B_NULL_TOKEN) {
		BEGIN_MESSAGE
		fLink->StartMessage(AS_FORCE_UPDATE);
		fLink->Attach<int32_t>(fWindowToken);
		fLink->Attach<BRect>(BRect(0, 0, -1, -1));
		fLink->Flush();
	}
}


void BDirectWindow::_ReservedDirectWindow1() {}
void BDirectWindow::_ReservedDirectWindow2() {}
void BDirectWindow::_ReservedDirectWindow3() {}
void BDirectWindow::_ReservedDirectWindow4() {}
