/*
 * Copyright 2001-2012, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Adrian Oanca <adioanca@cotty.iren.ro>
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Stefano Ceccherini (burton666@libero.it)
 *		Axel Dörfler, axeld@pinc-software.de
 *		Jérôme Duval, jerome.duval@free.fr
 *		Andrej Spielmann, <andrej.spielmann@seh.ox.ac.uk>
 *		Philippe Saint-Pierre, stpere@gmail.com
 *		Wim van der Meer, <WPJvanderMeer@gmail.com>
 */


/*!	\class ServerApp ServerApp.h
	\brief Counterpart to BApplication within the app_server
*/


#include "ServerApp.h"

#include <new>
#include <stdio.h>
#include <string.h>
#include <AppDefs.h>
#include <Autolock.h>
#include <Debug.h>
#include <List.h>
#include <ScrollBar.h>
#include <Shape.h>
#include <String.h>
#include <PortLink.h>

#include <FontPrivate.h>
#include <MessengerPrivate.h>
#include <ServerProtocol.h>

#include "AppServer.h"
#include "BitmapManager.h"
#include "CursorManager.h"
#include "CursorSet.h"
#include "Desktop.h"
#include "DecorManager.h"
#include "DrawingEngine.h"
#include "EventStream.h"
#include "FontManager.h"
#include "HWInterface.h"
#include "WorkspacesView.h"
#include "Screen.h"
#include "ServerBitmap.h"
#include "ServerConfig.h"
#include "ServerCursor.h"
#include "ServerPicture.h"
#include "ServerTokenSpace.h"
#include "ServerWindow.h"
#include "SystemPalette.h"
#include "Window.h"
#include "DrawState.h"


//#define DEBUG_SERVERAPP
#ifdef DEBUG_SERVERAPP
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

//#define DEBUG_SERVERAPP_FONT
#ifdef DEBUG_SERVERAPP_FONT
#	define FTRACE(x) printf x
#else
#	define FTRACE(x) ;
#endif

using std::nothrow;

static const uint32 kMsgUpdateShowAllDraggers = '_adg';
static const uint32 kMsgAppQuit = 'appQ';


ServerApp::ServerApp(Desktop* desktop, port_id clientReplyPort,
		port_id clientLooperPort, team_id clientTeam,
		int32 clientToken, const char* signature)
	:
	MessageLooper("application"),

	fMessagePort(-1),
	fClientReplyPort(clientReplyPort),
	fDesktop(desktop),
	fSignature(signature),
	fClientTeam(clientTeam),
	fWindowListLock("window list"),
	fTemporaryDisplayModeChange(0),
	fMapLocker("server app maps"),
	fAppCursor(NULL),
	fViewCursor(NULL),
	fCursorHideLevel(0),
	fIsActive(false),
	fMemoryAllocator(this)
{
	if (fSignature == "")
		fSignature = "application/no-signature";

	char name[B_OS_NAME_LENGTH];
	snprintf(name, sizeof(name), "a<%" B_PRId32 ":%s", clientTeam,
		SignatureLeaf());

	fMessagePort = create_port(DEFAULT_MONITOR_PORT_SIZE, name);
	if (fMessagePort < B_OK)
		return;

	fLink.SetSenderPort(fClientReplyPort);
	fLink.SetReceiverPort(fMessagePort);

	// we let the application own the port, so that we get aware when it's gone
	if (set_port_owner(fMessagePort, clientTeam) < B_OK) {
		delete_port(fMessagePort);
		fMessagePort = -1;
		return;
	}

	BMessenger::Private(fHandlerMessenger).SetTo(fClientTeam,
		clientLooperPort, clientToken);

	fInitialWorkspace = desktop->CurrentWorkspace();
		// TODO: this should probably be retrieved when the app is loaded!

	// record the current system wide fonts..
	desktop->LockSingleWindow();
	DesktopSettings settings(desktop);
	settings.GetDefaultPlainFont(fPlainFont);
	settings.GetDefaultBoldFont(fBoldFont);
	settings.GetDefaultFixedFont(fFixedFont);
	desktop->UnlockSingleWindow();

	STRACE(("ServerApp %s:\n", Signature()));
	STRACE(("\tBApp port: %" B_PRId32 "\n", fClientReplyPort));
	STRACE(("\tReceiver port: %" B_PRId32 "\n", fMessagePort));
}


ServerApp::~ServerApp()
{
	STRACE(("*ServerApp %s:~ServerApp()\n", Signature()));

	if (!fQuitting)
		debugger("ServerApp: destructor called after Run()!\n");

	// quit all server windows

	fWindowListLock.Lock();
	for (int32 i = fWindowList.CountItems(); i-- > 0;) {
		ServerWindow* window = fWindowList.ItemAt(i);
		window->Quit();
	}
	fWindowListLock.Unlock();

	// wait for the windows to quit
	snooze(20000);

	fWindowListLock.Lock();
	for (int32 i = fWindowList.CountItems(); i-- > 0;) {
		ServerWindow* window = fWindowList.ItemAt(i);

		// A window could have been removed in the mean time
		// (if those 20 milli seconds from above weren't enough)
		if (window == NULL)
			continue;

		sem_id deathSemaphore = window->DeathSemaphore();
		fWindowListLock.Unlock();

		// wait 3 seconds for our window to quit - that's quite a long
		// time, but killing it might have desastrous effects
		if (MessageLooper::WaitForQuit(deathSemaphore, 3000000) != B_OK) {
			// This really shouldn't happen, as it shows we're buggy
			debugger("ServerWindow doesn't respond!\n");
		}
		fWindowListLock.Lock();
	}

	fMapLocker.Lock();

	while (!fBitmapMap.empty()) {
		ServerBitmap* bitmap = fBitmapMap.begin()->second;

		fBitmapMap.erase(fBitmapMap.begin());
		bitmap->ReleaseReference();
	}

	for (int32 i = fPictureList.CountItems(); i-- > 0;) {
		delete (ServerPicture*)fPictureList.ItemAtFast(i);
	}

	fDesktop->GetCursorManager().DeleteCursors(fClientTeam);

	STRACE(("ServerApp %s::~ServerApp(): Exiting\n", Signature()));
}


/*!	\brief Checks if the application was initialized correctly
*/
status_t
ServerApp::InitCheck()
{
	if (fMessagePort < B_OK)
		return fMessagePort;

	if (fClientReplyPort < B_OK)
		return fClientReplyPort;

	if (fWindowListLock.Sem() < B_OK)
		return fWindowListLock.Sem();

	return B_OK;
}


/*!	\brief This quits the application and deletes it. You're not supposed
		to call its destructor directly.

	At the point you're calling this method, the application should already
	be removed from the application list.
*/
void
ServerApp::Quit(sem_id shutdownSemaphore)
{
	if (fThread < B_OK) {
		delete this;
		return;
	}

	// execute application deletion in the message looper thread

	fQuitting = true;
	PostMessage(kMsgAppQuit);

	send_data(fThread, 'QUIT', &shutdownSemaphore, sizeof(sem_id));
}


/*!	\brief Sets the ServerApp's active status
	\param value The new status of the ServerApp.

	This changes an internal flag and also sets the current cursor to the one
	specified by the application
*/
void
ServerApp::Activate(bool value)
{
	if (fIsActive == value)
		return;

	fIsActive = value;

	if (fIsActive) {
		if (_HasWindowUnderMouse()) {
			// Set the cursor to the application cursor, if any
			fDesktop->SetCursor(CurrentCursor());
		}
	}
}


/*!	\brief Send a message to the ServerApp's BApplication
	\param message The message to send
*/
void
ServerApp::SendMessageToClient(BMessage* message) const
{
	status_t status = fHandlerMessenger.SendMessage(message, (BHandler*)NULL,
		100000);
	if (status != B_OK) {
		printf("app %s send to client failed: %s\n", Signature(), strerror(status));
	}
}


void
ServerApp::SetCurrentCursor(ServerCursor* cursor)
{
	if (fViewCursor == cursor)
		return;

	if (fViewCursor)
		fViewCursor->ReleaseReference();

	fViewCursor = cursor;

	if (fViewCursor)
		fViewCursor->AcquireReference();

	fDesktop->SetCursor(CurrentCursor());
}


ServerCursor*
ServerApp::CurrentCursor() const
{
	if (fViewCursor != NULL)
		return fViewCursor;

	return fAppCursor;
}


bool
ServerApp::AddWindow(ServerWindow* window)
{
	BAutolock locker(fWindowListLock);

	return fWindowList.AddItem(window);
}


void
ServerApp::RemoveWindow(ServerWindow* window)
{
	BAutolock locker(fWindowListLock);

	fWindowList.RemoveItem(window);
}


bool
ServerApp::InWorkspace(int32 index) const
{
	BAutolock locker(fWindowListLock);

	// we could cache this, but then we'd have to recompute the cached
	// value everytime a window has closed or changed workspaces

	// TODO: support initial application workspace!

	for (int32 i = fWindowList.CountItems(); i-- > 0;) {
		ServerWindow* serverWindow = fWindowList.ItemAt(i);

		const Window* window = serverWindow->Window();
		if (window == NULL || window->IsOffscreenWindow())
			continue;

		// only normal and unhidden windows count

		if (window->IsNormal() && !window->IsHidden()
			&& window->InWorkspace(index))
			return true;
	}

	return false;
}


uint32
ServerApp::Workspaces() const
{
	uint32 workspaces = 0;

	BAutolock locker(fWindowListLock);

	// we could cache this, but then we'd have to recompute the cached
	// value everytime a window has closed or changed workspaces

	for (int32 i = fWindowList.CountItems(); i-- > 0;) {
		ServerWindow* serverWindow = fWindowList.ItemAt(i);

		const Window* window = serverWindow->Window();
		if (window == NULL || window->IsOffscreenWindow())
			continue;

		// only normal and unhidden windows count

		if (window->IsNormal() && !window->IsHidden())
			workspaces |= window->Workspaces();
	}

	// TODO: add initial application workspace!
	return workspaces;
}


/*!	\brief Acquires a reference of the desired bitmap, if available.
	\param token ID token of the bitmap to find
	\return The bitmap having that ID or NULL if not found
*/
ServerBitmap*
ServerApp::GetBitmap(int32 token) const
{
	if (token < 1)
		return NULL;

	BAutolock _(fMapLocker);

	ServerBitmap* bitmap = _FindBitmap(token);
	if (bitmap == NULL)
		return NULL;

	bitmap->AcquireReference();

	return bitmap;
}

bool
ServerApp::BitmapAdded(ServerBitmap* bitmap)
{
	BAutolock _(fMapLocker);

	try {
		fBitmapMap.insert(std::make_pair(bitmap->Token(), bitmap));
	} catch (std::bad_alloc& exception) {
		return false;
		}

	return true;
}

void
ServerApp::BitmapRemoved(ServerBitmap* bitmap)
			{
	BAutolock _(fMapLocker);
	fBitmapMap.erase(bitmap->Token());
}


// #pragma mark - private methods


void
ServerApp::_GetLooperName(char* name, size_t length)
{
	snprintf(name, length, "a:%" B_PRId32 ":%s", ClientTeam(), SignatureLeaf());
}


/*!	\brief Handler function for BApplication API messages
	\param code Identifier code for the message. Equivalent to BMessage::what
	\param buffer Any attachments

	Note that the buffer's exact format is determined by the particular message.
	All attachments are placed in the buffer via a PortLink, so it will be a
	matter of casting and incrementing an index variable to access them.
*/
void
ServerApp::_DispatchMessage(int32 code, BPrivate::LinkReceiver& link)
{
	switch (code) {
		case AS_CREATE_WINDOW:
		case AS_CREATE_OFFSCREEN_WINDOW:
		{
			STRACE(("ServerApp %s: AS_CREATE_WINDOW\n", Signature()));
			port_id clientReplyPort = -1;
			status_t status = _CreateWindow(code, link, clientReplyPort);

			// if sucessful, ServerWindow::Run() will already have replied
			if (status < B_OK) {
				STRACE(("ServerApp %s: AS_CREATE_WINDOW failed\n", Signature()));
				// window creation failed, we need to notify the client
				BPrivate::LinkSender reply(clientReplyPort);
				reply.StartMessage(status);
				reply.Flush();
			}
			break;
		}

		case AS_GET_WINDOW_LIST:
		{
			team_id team;
			if (link.Read<team_id>(&team) == B_OK)
				fDesktop->WriteWindowList(team, fLink.Sender());
			break;
		}

		case AS_GET_WINDOW_INFO:
		{
			int32 serverToken;
			if (link.Read<int32>(&serverToken) == B_OK)
				fDesktop->WriteWindowInfo(serverToken, fLink.Sender());
			break;
		}

		case AS_MINIMIZE_TEAM:
		{
			team_id team;
			if (link.Read<team_id>(&team) == B_OK)
				fDesktop->MinimizeApplication(team);
			break;
		}

		case AS_BRING_TEAM_TO_FRONT:
		{
			team_id team;
			if (link.Read<team_id>(&team) == B_OK)
				fDesktop->BringApplicationToFront(team);
			break;
		}

		case AS_WINDOW_ACTION:
		{
			int32 token;
			int32 action;

			link.Read<int32>(&token);
			if (link.Read<int32>(&action) != B_OK)
				break;

			fDesktop->WindowAction(token, action);
			break;
		}

		// Decorator commands

		case AS_SET_DECORATOR:
		{
			// Attached Data:
			// path to decorator add-on

			int32 index;
			link.Read<int32>(&index);
			if (gDecorManager.SetDecorator(index))
				fDesktop->BroadcastToAllApps(AS_UPDATE_DECORATOR);
			break;
		}
		case AS_COUNT_DECORATORS:
		{
			fLink.StartMessage(B_OK);
			fLink.Attach<int32>(gDecorManager.CountDecorators());
			fLink.Flush();
			break;
		}
		case AS_GET_DECORATOR:
		{
			fLink.StartMessage(B_OK);
			fLink.Attach<int32>(gDecorManager.GetDecorator());
			fLink.Flush();
			break;
		}
		case AS_GET_DECORATOR_NAME:
		{
			int32 index;
			link.Read<int32>(&index);

			BString str(gDecorManager.GetDecoratorName(index));
			if (str.CountChars() > 0) {
				fLink.StartMessage(B_OK);
				fLink.AttachString(str.String());
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}
		case AS_CREATE_BITMAP:
		{
			STRACE(("ServerApp %s: Received BBitmap creation request\n",
				Signature()));
			// Allocate a bitmap for an application

			// Attached Data:
			// 1) BRect bounds
			// 2) color_space space
			// 3) int32 bitmap_flags
			// 4) int32 bytes_per_row
			// 5) int32 screen_id

			// Reply Data:
			//	1) int32 server token
			//	2) area_id id of the area in which the bitmap data resides
			//	3) int32 area pointer offset used to calculate fBasePtr

			// First, let's attempt to allocate the bitmap
			ServerBitmap* bitmap = NULL;
			uint8 allocationFlags = kAllocator;

			BRect frame;
			color_space colorSpace;
			uint32 flags;
			int32 bytesPerRow;
			int32 screenID;

			link.Read<BRect>(&frame);
			link.Read<color_space>(&colorSpace);
			link.Read<uint32>(&flags);
			link.Read<int32>(&bytesPerRow);
			if (link.Read<int32>(&screenID) == B_OK) {
				// TODO: choose the right HWInterface with regards to the
				// screenID
				bitmap = gBitmapManager->CreateBitmap(&fMemoryAllocator,
					*fDesktop->HWInterface(), frame, colorSpace, flags,
					bytesPerRow, screenID, &allocationFlags);
			}

			STRACE(("ServerApp %s: Create Bitmap (%.1fx%.1f)\n",
				Signature(), frame.Width() + 1, frame.Height() + 1));

			if (bitmap != NULL && bitmap->SetOwner(this)) {

				fLink.StartMessage(B_OK);
				fLink.Attach<int32>(bitmap->Token());
				fLink.Attach<uint8>(allocationFlags);

				fLink.Attach<area_id>(
					fMemoryAllocator.Area(bitmap->AllocationCookie()));
				fLink.Attach<int32>(
					fMemoryAllocator.AreaOffset(bitmap->AllocationCookie()));

				if ((allocationFlags & kFramebuffer) != 0)
					fLink.Attach<int32>(bitmap->BytesPerRow());
			} else {
				if (bitmap != NULL)
					bitmap->ReleaseReference();

				fLink.StartMessage(B_NO_MEMORY);
			}

			fLink.Flush();
			break;
		}

		case AS_DELETE_BITMAP:
		{
			STRACE(("ServerApp %s: received BBitmap delete request\n",
				Signature()));

			// Attached Data:
			// 1) int32 token
			int32 token;
			link.Read<int32>(&token);

			fMapLocker.Lock();

			ServerBitmap* bitmap = _FindBitmap(token);
			if (bitmap != NULL) {
				STRACE(("ServerApp %s: Deleting Bitmap %ld\n", Signature(),
					token));

				bitmap->ReleaseClientReference();
			}

			fMapLocker.Unlock();
			break;
		}

		case AS_CREATE_PICTURE:
		{
			// TODO: Maybe rename this to AS_UPLOAD_PICTURE ?
			STRACE(("ServerApp %s: Create Picture\n", Signature()));
			break;
		}

		case AS_DELETE_PICTURE:
		{
			STRACE(("ServerApp %s: Delete Picture\n", Signature()));
			break;
		}

		case AS_CLONE_PICTURE:
		{
			break;
		}

		case AS_DOWNLOAD_PICTURE:
		{
			STRACE(("ServerApp %s: Download Picture\n", Signature()));
			
			break;
		}

		case AS_CURRENT_WORKSPACE:
			STRACE(("ServerApp %s: get current workspace\n", Signature()));

			if (fDesktop->LockSingleWindow()) {
				fLink.StartMessage(B_OK);
				fLink.Attach<int32>(fDesktop->CurrentWorkspace());
				fDesktop->UnlockSingleWindow();
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;

		case AS_ACTIVATE_WORKSPACE:
		{
			STRACE(("ServerApp %s: activate workspace\n", Signature()));

			// TODO: See above
			int32 index;
			link.Read<int32>(&index);

			fDesktop->SetWorkspace(index);
			break;
		}

		case AS_SET_WORKSPACE_LAYOUT:
		{
			int32 newColumns;
			int32 newRows;
			if (link.Read<int32>(&newColumns) == B_OK
				&& link.Read<int32>(&newRows) == B_OK)
				fDesktop->SetWorkspacesLayout(newColumns, newRows);
			break;
		}

		case AS_GET_WORKSPACE_LAYOUT:
		{
			if (fDesktop->LockSingleWindow()) {
				DesktopSettings settings(fDesktop);

				fLink.StartMessage(B_OK);
				fLink.Attach<int32>(settings.WorkspacesColumns());
				fLink.Attach<int32>(settings.WorkspacesRows());

				fDesktop->UnlockSingleWindow();
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		case AS_IDLE_TIME:
			STRACE(("ServerApp %s: idle time\n", Signature()));

			fLink.StartMessage(B_OK);
			fLink.Attach<bigtime_t>(fDesktop->EventDispatcher().IdleTime());
			fLink.Flush();
			break;

		case AS_SHOW_CURSOR:
		{
			STRACE(("ServerApp %s: Show Cursor\n", Signature()));
			fCursorHideLevel--;
			if (fCursorHideLevel < 0)
				fCursorHideLevel = 0;
			fDesktop->GetDrawingEngine()->ShowCursor();
			break;
		}

		case AS_HIDE_CURSOR:
		{
			STRACE(("ServerApp %s: Hide Cursor\n", Signature()));
			fCursorHideLevel++;
			fDesktop->GetDrawingEngine()->HideCursor();
			break;
		}

		case AS_OBSCURE_CURSOR:
		{
			STRACE(("ServerApp %s: Obscure Cursor\n", Signature()));
			fDesktop->GetDrawingEngine()->ObscureCursor();
			break;
		}

		case AS_QUERY_CURSOR_HIDDEN:
		{
			STRACE(("ServerApp %s: Received IsCursorHidden request\n",
				Signature()));

			fLink.StartMessage(fCursorHideLevel > 0 ? B_OK : B_ERROR);
			fLink.Flush();
			break;
		}

		case AS_SET_CURSOR:
		{
			STRACE(("ServerApp %s: SetCursor\n", Signature()));

			// Attached data:
			// 1) bool flag to send a reply
			// 2) int32 token ID of the cursor to set
			// 3) port_id port to receive a reply. Only exists if the sync flag
			//    is true.
			bool sync;
			int32 token;

			link.Read<bool>(&sync);
			if (link.Read<int32>(&token) != B_OK)
				break;

			if (!fDesktop->GetCursorManager().Lock())
				break;

			ServerCursor* oldCursor = fAppCursor;
			fAppCursor = fDesktop->GetCursorManager().FindCursor(token);
			if (fAppCursor != NULL)
				fAppCursor->AcquireReference();

			if (_HasWindowUnderMouse())
				fDesktop->SetCursor(CurrentCursor());

			if (oldCursor != NULL)
				oldCursor->ReleaseReference();

			fDesktop->GetCursorManager().Unlock();

			if (sync) {
				// The application is expecting a reply
				fLink.StartMessage(B_OK);
				fLink.Flush();
			}
			break;
		}

		case AS_SET_VIEW_CURSOR:
		{
			STRACE(("ServerApp %s: AS_SET_VIEW_CURSOR:\n", Signature()));

			ViewSetViewCursorInfo info;
			if (link.Read<ViewSetViewCursorInfo>(&info) != B_OK)
				break;

			if (fDesktop->GetCursorManager().Lock()) {
				ServerCursor* cursor = fDesktop->GetCursorManager().FindCursor(
					info.cursorToken);
				// If we found a cursor, make sure it doesn't go away. If we
				// get a NULL cursor, it probably means we are supposed to use
				// the system default cursor.
				if (cursor != NULL)
					cursor->AcquireReference();

				fDesktop->GetCursorManager().Unlock();

				// We need to acquire the write lock here, since we cannot
				// afford that the window thread to which the view belongs
				// is running and messing with that same view.
				fDesktop->LockAllWindows();

				// Find the corresponding view by the given token. It's ok
				// if this view does not exist anymore, since it may have
				// already be deleted in the window thread before this
				// message got here.
				View* view;
				if (fViewTokens.GetToken(info.viewToken, B_HANDLER_TOKEN,
					(void**)&view) == B_OK) {
					// Set the cursor on the view.
					view->SetCursor(cursor);

					// The cursor might need to be updated now.
					Window* window = view->Window();
					if (window != NULL && window->IsFocus()) {
						if (fDesktop->ViewUnderMouse(window) == view->Token())
							SetCurrentCursor(cursor);
					}
				}

				fDesktop->UnlockAllWindows();

				// Release the temporary reference.
				if (cursor != NULL)
					cursor->ReleaseReference();
			}

			if (info.sync) {
				// sync the client (it can now delete the cursor)
				fLink.StartMessage(B_OK);
				fLink.Flush();
			}
			break;
		}

		case AS_CREATE_CURSOR:
		{
			STRACE(("ServerApp %s: Create Cursor\n", Signature()));

			// Attached data:
			// 1) 68 bytes of fAppCursor data
			// 2) port_id reply port

			status_t status = B_ERROR;
			uint8 cursorData[68];
			ServerCursor* cursor = NULL;

//			if (link.Read(cursorData, sizeof(cursorData)) >= B_OK) {
//				cursor = new (nothrow) ServerCursor(cursorData);
//				if (cursor == NULL)
//					status = B_NO_MEMORY;
//			}
//
//			if (cursor != NULL) {
//				cursor->SetOwningTeam(fClientTeam);
//				fDesktop->GetCursorManager().AddCursor(cursor);
//
//				// Synchronous message - BApplication is waiting on the cursor's ID
//				fLink.StartMessage(B_OK);
//				fLink.Attach<int32>(cursor->Token());
//			} else
//				fLink.StartMessage(status);

			if (link.Read(cursorData, sizeof(cursorData)) >= B_OK) {
				cursor = fDesktop->GetCursorManager().CreateCursor(fClientTeam,
					cursorData);
				if (cursor == NULL)
					status = B_NO_MEMORY;
			}

			if (cursor != NULL) {
				// Synchronous message - BApplication is waiting on the
				// cursor's ID
				fLink.StartMessage(B_OK);
				fLink.Attach<int32>(cursor->Token());
			} else
				fLink.StartMessage(status);

			fLink.Flush();
			break;
		}

		case AS_REFERENCE_CURSOR:
		{
			STRACE(("ServerApp %s: Reference BCursor\n", Signature()));

			// Attached data:
			// 1) int32 token ID of the cursor to reference

			int32 token;
			if (link.Read<int32>(&token) != B_OK)
				break;

			if (!fDesktop->GetCursorManager().Lock())
				break;

			ServerCursor* cursor
				= fDesktop->GetCursorManager().FindCursor(token);
			if (cursor != NULL)
				cursor->AcquireReference();

			fDesktop->GetCursorManager().Unlock();

			break;
		}

		case AS_DELETE_CURSOR:
		{
			STRACE(("ServerApp %s: Delete BCursor\n", Signature()));

			// Attached data:
			// 1) int32 token ID of the cursor to delete

			int32 token;
			if (link.Read<int32>(&token) != B_OK)
				break;

			if (!fDesktop->GetCursorManager().Lock())
				break;

			ServerCursor* cursor
				= fDesktop->GetCursorManager().FindCursor(token);
			if (cursor != NULL)
				cursor->ReleaseReference();

			fDesktop->GetCursorManager().Unlock();

			break;
		}

		case AS_GET_SCROLLBAR_INFO:
		{
			STRACE(("ServerApp %s: Get ScrollBar info\n", Signature()));

			if (fDesktop->LockSingleWindow()) {
				scroll_bar_info info;
				DesktopSettings settings(fDesktop);
				settings.GetScrollBarInfo(info);

				fLink.StartMessage(B_OK);
				fLink.Attach<scroll_bar_info>(info);
				fDesktop->UnlockSingleWindow();
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		case AS_SET_SCROLLBAR_INFO:
		{
			STRACE(("ServerApp %s: Set ScrollBar info\n", Signature()));

			// Attached Data:
			// 1) scroll_bar_info scroll bar info structure

			scroll_bar_info info;
			if (link.Read<scroll_bar_info>(&info) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetScrollBarInfo(info);
			}

			fLink.StartMessage(B_OK);
			fLink.Flush();
			break;
		}

		case AS_GET_MENU_INFO:
		{
			STRACE(("ServerApp %s: Get menu info\n", Signature()));
			if (fDesktop->LockSingleWindow()) {
				menu_info info;
				DesktopSettings settings(fDesktop);
				settings.GetMenuInfo(info);

				fLink.StartMessage(B_OK);
				fLink.Attach<menu_info>(info);

				fDesktop->UnlockSingleWindow();
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		case AS_SET_MENU_INFO:
		{
			STRACE(("ServerApp %s: Set menu info\n", Signature()));
			menu_info info;
			if (link.Read<menu_info>(&info) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetMenuInfo(info);
					// TODO: SetMenuInfo() should do some validity check, so
					//	that the answer we're giving can actually be useful
			}

			fLink.StartMessage(B_OK);
			fLink.Flush();
			break;
		}

		case AS_SET_MOUSE_MODE:
		{
			STRACE(("ServerApp %s: Set Mouse Focus mode\n",
				Signature()));

			// Attached Data:
			// 1) enum mode_mouse mouse focus mode

			mode_mouse mouseMode;
			if (link.Read<mode_mouse>(&mouseMode) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetMouseMode(mouseMode);
			}
			break;
		}

		case AS_GET_MOUSE_MODE:
		{
			STRACE(("ServerApp %s: Get Mouse Focus mode\n",
				Signature()));

			if (fDesktop->LockSingleWindow()) {
				DesktopSettings settings(fDesktop);

				fLink.StartMessage(B_OK);
				fLink.Attach<mode_mouse>(settings.MouseMode());

				fDesktop->UnlockSingleWindow();
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		/* font messages */

		case AS_SET_SYSTEM_FONT:
		{
			FTRACE(("ServerApp %s: AS_SET_SYSTEM_FONT\n", Signature()));
			// gets:
			//	1) string - font type ("plain", ...)
			//	2) string - family
			//	3) string - style
			//	4) float - size

			char type[B_OS_NAME_LENGTH];
			font_family familyName;
			font_style styleName;
			float size;

			if (link.ReadString(type, sizeof(type)) == B_OK
				&& link.ReadString(familyName, sizeof(familyName)) == B_OK
				&& link.ReadString(styleName, sizeof(styleName)) == B_OK
				&& link.Read<float>(&size) == B_OK) {
				gFontManager->Lock();

				FontStyle* style
					= gFontManager->GetStyle(familyName, styleName);
				if (style != NULL) {
					ServerFont font(*style, size);
					gFontManager->Unlock();
						// We must not have locked the font manager when
						// locking the desktop (through LockedDesktopSettings
						// below)

					LockedDesktopSettings settings(fDesktop);

					// TODO: Should we also update our internal copies now?
					if (!strcmp(type, "plain"))
						settings.SetDefaultPlainFont(font);
					else if (!strcmp(type, "bold"))
						settings.SetDefaultBoldFont(font);
					else if (!strcmp(type, "fixed"))
						settings.SetDefaultFixedFont(font);
				} else
					gFontManager->Unlock();
			}
			break;
		}

		case AS_GET_SYSTEM_DEFAULT_FONT:
		{
			// input:
			//	1) string - font type ("plain", ...)

			ServerFont font;

			char type[B_OS_NAME_LENGTH];
			status_t status = link.ReadString(type, sizeof(type));
			if (status == B_OK) {
				if (!strcmp(type, "plain")) {
					font = *gFontManager->DefaultPlainFont();
				} else if (!strcmp(type, "bold")) {
					font = *gFontManager->DefaultBoldFont();
				} else if (!strcmp(type, "fixed")) {
					font = *gFontManager->DefaultFixedFont();
				} else
					status = B_BAD_VALUE;
			}

			if (status == B_OK) {
				// returns:
				//	1) string - family
				//	2) string - style
				//	3) float - size

				fLink.StartMessage(B_OK);
				fLink.AttachString(font.Family());
				fLink.AttachString(font.Style());
				fLink.Attach<float>(font.Size());
			} else
				fLink.StartMessage(status);

			fLink.Flush();
			break;
		}

		case AS_GET_SYSTEM_FONTS:
		{
			FTRACE(("ServerApp %s: AS_GET_SYSTEM_FONTS\n", Signature()));
			// Returns:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - size in points
			// 4) uint16 - face flags
			// 5) uint32 - font flags

			if (!fDesktop->LockSingleWindow()) {
				fLink.StartMessage(B_OK);
				fLink.Flush();
				break;
			}

			// The client is requesting the system fonts, this
			// could happend either at application start up, or
			// because the client is resyncing with the global
			// fonts. So we record the current system wide fonts
			// into our own copies at this point.
			DesktopSettings settings(fDesktop);

			settings.GetDefaultPlainFont(fPlainFont);
			settings.GetDefaultBoldFont(fBoldFont);
			settings.GetDefaultFixedFont(fFixedFont);

			fLink.StartMessage(B_OK);

			for (int32 i = 0; i < 3; i++) {
				ServerFont* font = NULL;
				switch (i) {
					case 0:
						font = &fPlainFont;
						fLink.AttachString("plain");
						break;

					case 1:
						font = &fBoldFont;
						fLink.AttachString("bold");
						break;

					case 2:
						font = &fFixedFont;
						fLink.AttachString("fixed");
						break;
				}

				fLink.Attach<uint16>(font->FamilyID());
				fLink.Attach<uint16>(font->StyleID());
				fLink.Attach<float>(font->Size());
				fLink.Attach<uint16>(font->Face());
				fLink.Attach<uint32>(font->Flags());
			}

			fDesktop->UnlockSingleWindow();
			fLink.Flush();
			break;
		}

		case AS_GET_FONT_LIST_REVISION:
		{
			STRACE(("ServerApp %s: AS_GET_FONT_LIST_REVISION\n", Signature()));

			fLink.StartMessage(B_OK);
			fLink.Attach<int32>(
				gFontManager->CheckRevision(fDesktop->UserID()));
			fLink.Flush();
			break;
		}

		case AS_GET_FAMILY_AND_STYLES:
		{
			FTRACE(("ServerApp %s: AS_GET_FAMILY_AND_STYLES\n", Signature()));

			// Attached Data:
			// 1) int32 the index of the font family to get

			// Returns:
			// 1) string - name of family
			// 2) uint32 - flags of font family (B_IS_FIXED || B_HAS_TUNED_FONT)
			// 3) count of styles in that family
			// For each style:
			//		1) string - name of style
			//		2) uint16 - face of style
			//		3) uint32 - flags of style

			int32 index;
			link.Read<int32>(&index);

			gFontManager->Lock();

			FontFamily* family = gFontManager->FamilyAt(index);
			if (family) {
				fLink.StartMessage(B_OK);
				fLink.AttachString(family->Name());
				fLink.Attach<uint32>(family->Flags());

				int32 count = family->CountStyles();
				fLink.Attach<int32>(count);

				for (int32 i = 0; i < count; i++) {
					FontStyle* style = family->StyleAt(i);

					fLink.AttachString(style->Name());
					fLink.Attach<uint16>(style->Face());
					fLink.Attach<uint32>(style->Flags());
				}
			} else
				fLink.StartMessage(B_BAD_VALUE);

			gFontManager->Unlock();
			fLink.Flush();
			break;
		}

		case AS_GET_FAMILY_AND_STYLE:
		{
			FTRACE(("ServerApp %s: AS_GET_FAMILY_AND_STYLE\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID

			// Returns:
			// 1) font_family The name of the font family
			// 2) font_style - name of the style

			uint16 familyID, styleID;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);

			gFontManager->Lock();

			FontStyle *fontStyle = gFontManager->GetStyle(familyID, styleID);
			if (fontStyle != NULL) {
				fLink.StartMessage(B_OK);
				fLink.AttachString(fontStyle->Family()->Name());
				fLink.AttachString(fontStyle->Name());
			} else
				fLink.StartMessage(B_BAD_VALUE);

			fLink.Flush();
			gFontManager->Unlock();
			break;
		}

		case AS_GET_FAMILY_AND_STYLE_IDS:
		{
			FTRACE(("ServerApp %s: AS_GET_FAMILY_AND_STYLE_IDS\n",
				Signature()));

			// Attached Data:
			// 1) font_family - name of font family to use
			// 2) font_style - name of style in family
			// 3) family ID - only used if 1) is empty
			// 4) style ID - only used if 2) is empty
			// 5) face - the font's current face

			// Returns:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) uint16 - face

			font_family family;
			font_style style;
			uint16 familyID, styleID;
			uint16 face;
			if (link.ReadString(family, sizeof(font_family)) == B_OK
				&& link.ReadString(style, sizeof(font_style)) == B_OK
				&& link.Read<uint16>(&familyID) == B_OK
				&& link.Read<uint16>(&styleID) == B_OK
				&& link.Read<uint16>(&face) == B_OK) {
				// get the font and return IDs and face
				gFontManager->Lock();

				FontStyle *fontStyle = gFontManager->GetStyle(family, style,
					familyID, styleID, face);

				if (fontStyle != NULL) {
					fLink.StartMessage(B_OK);
					fLink.Attach<uint16>(fontStyle->Family()->ID());
					fLink.Attach<uint16>(fontStyle->ID());

					// we try to keep the font face close to what we got
					face = fontStyle->PreservedFace(face);

					fLink.Attach<uint16>(face);
				} else
					fLink.StartMessage(B_NAME_NOT_FOUND);

				gFontManager->Unlock();
			} else
				fLink.StartMessage(B_BAD_VALUE);

			fLink.Flush();
			break;
		}

		case AS_GET_FONT_FILE_FORMAT:
		{
			FTRACE(("ServerApp %s: AS_GET_FONT_FILE_FORMAT\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID

			// Returns:
			// 1) uint16 font_file_format of font

			int32 familyID, styleID;
			link.Read<int32>(&familyID);
			link.Read<int32>(&styleID);

			gFontManager->Lock();

			FontStyle *fontStyle = gFontManager->GetStyle(familyID, styleID);
			if (fontStyle) {
				fLink.StartMessage(B_OK);
				fLink.Attach<uint16>((uint16)fontStyle->FileFormat());
			} else
				fLink.StartMessage(B_BAD_VALUE);

			gFontManager->Unlock();
			fLink.Flush();
			break;
		}

		case AS_GET_STRING_WIDTHS:
		{
			FTRACE(("ServerApp %s: AS_GET_STRING_WIDTHS\n", Signature()));

			// Attached Data:
			// 1) uint16 ID of family
			// 2) uint16 ID of style
			// 3) float point size of font
			// 4) uint8 spacing to use
			// 5) int32 numStrings
			// 6) int32 string length to measure (numStrings times)
			// 7) string String to measure (numStrings times)

			// Returns:
			// 1) float - width of the string in pixels (numStrings times)

			uint16 family, style;
			float size;
			uint8 spacing;

			link.Read<uint16>(&family);
			link.Read<uint16>(&style);
			link.Read<float>(&size);
			link.Read<uint8>(&spacing);
			int32 numStrings;
			if (link.Read<int32>(&numStrings) != B_OK) {
				// this results in a B_BAD_VALUE return
				numStrings = 0;
				size = 0.0f;
			}

			// TODO: don't use the stack for this - numStrings could be large
			float widthArray[numStrings];
			int32 lengthArray[numStrings];
			char *stringArray[numStrings];
			for (int32 i = 0; i < numStrings; i++) {
				// This version of ReadString allocates the strings, we free
				// them below
				link.ReadString(&stringArray[i], (size_t *)&lengthArray[i]);
			}

			ServerFont font;

			if (font.SetFamilyAndStyle(family, style) == B_OK && size > 0) {
				font.SetSize(size);
				font.SetSpacing(spacing);

				for (int32 i = 0; i < numStrings; i++) {
					if (!stringArray[i] || lengthArray[i] <= 0)
						widthArray[i] = 0.0;
					else {
						widthArray[i] = font.StringWidth(stringArray[i],
							lengthArray[i]);
					}
				}

				fLink.StartMessage(B_OK);
				fLink.Attach(widthArray, sizeof(widthArray));
			} else
				fLink.StartMessage(B_BAD_VALUE);

			fLink.Flush();

			for (int32 i = 0; i < numStrings; i++)
				free(stringArray[i]);
			break;
		}

		case AS_GET_FONT_BOUNDING_BOX:
		{
			FTRACE(("ServerApp %s: AS_GET_BOUNDING_BOX unimplemented\n",
				Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID

			// Returns:
			// 1) BRect - box holding entire font

			// ToDo: implement me!
			fLink.StartMessage(B_ERROR);
			fLink.Flush();
			break;
		}

		case AS_GET_TUNED_COUNT:
		{
			FTRACE(("ServerApp %s: AS_GET_TUNED_COUNT\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID

			// Returns:
			// 1) int32 - number of font strikes available

			uint16 familyID, styleID;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);

			gFontManager->Lock();

			FontStyle *fontStyle = gFontManager->GetStyle(familyID, styleID);
			if (fontStyle != NULL) {
				fLink.StartMessage(B_OK);
				fLink.Attach<int32>(fontStyle->TunedCount());
			} else
				fLink.StartMessage(B_BAD_VALUE);

			gFontManager->Unlock();
			fLink.Flush();
			break;
		}

		case AS_GET_TUNED_INFO:
		{
			FTRACE(("ServerApp %s: AS_GET_TUNED_INFO unimplmemented\n",
				Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) uint32 - index of the particular font strike

			// Returns:
			// 1) tuned_font_info - info on the strike specified
			// ToDo: implement me!

			fLink.StartMessage(B_ERROR);
			fLink.Flush();
			break;
		}

		case AS_GET_EXTRA_FONT_FLAGS:
		{
			FTRACE(("ServerApp %s: AS_GET_EXTRA_FONT_FLAGS\n",
				Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID

			// Returns:
			// 1) uint32 - extra font flags

			uint16 familyID, styleID;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);

			gFontManager->Lock();

			FontStyle *fontStyle = gFontManager->GetStyle(familyID, styleID);
			if (fontStyle != NULL) {
				fLink.StartMessage(B_OK);
				fLink.Attach<uint32>(fontStyle->Flags());
			} else
				fLink.StartMessage(B_BAD_VALUE);

			gFontManager->Unlock();
			fLink.Flush();
			break;
		}

		case AS_GET_FONT_HEIGHT:
		{
			FTRACE(("ServerApp %s: AS_GET_FONT_HEIGHT\n", Signature()));

			// Attached Data:
			// 1) uint16 family ID
			// 2) uint16 style ID
			// 3) float size

			uint16 familyID, styleID;
			float size;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&size);

			gFontManager->Lock();

			FontStyle *fontStyle = gFontManager->GetStyle(familyID, styleID);
			if (fontStyle != NULL) {
				font_height height;
				fontStyle->GetHeight(size, height);

				fLink.StartMessage(B_OK);
				fLink.Attach<font_height>(height);
			} else
				fLink.StartMessage(B_BAD_VALUE);

			gFontManager->Unlock();
			fLink.Flush();
			break;
		}

		case AS_GET_GLYPH_SHAPES:
		{
			FTRACE(("ServerApp %s: AS_GET_GLYPH_SHAPES\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - point size
			// 4) float - shear
			// 5) float - rotation
			// 6) float - false bold width
			// 6) uint32 - flags
			// 7) int32 - numChars
			// 8) int32 - numBytes
			// 8) char - chars (bytesInBuffer times)

			// Returns:
			// 1) BShape - glyph shape
			// numChars times

			uint16 familyID, styleID;
			uint32 flags;
			float size, shear, rotation, falseBoldWidth;

			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&size);
			link.Read<float>(&shear);
			link.Read<float>(&rotation);
			link.Read<float>(&falseBoldWidth);
			link.Read<uint32>(&flags);

			int32 numChars, numBytes;
			link.Read<int32>(&numChars);
			link.Read<int32>(&numBytes);

			// TODO: proper error checking
			char* charArray = new (nothrow) char[numBytes];
			link.Read(charArray, numBytes);

			ServerFont font;
			status_t status = font.SetFamilyAndStyle(familyID, styleID);
			if (status == B_OK) {
				font.SetSize(size);
				font.SetShear(shear);
				font.SetRotation(rotation);
				font.SetFalseBoldWidth(falseBoldWidth);
				font.SetFlags(flags);

				// TODO: proper error checking
				BShape** shapes = new (nothrow) BShape*[numChars];
				status = font.GetGlyphShapes(charArray, numChars, shapes);
				if (status == B_OK) {
					fLink.StartMessage(B_OK);
					for (int32 i = 0; i < numChars; i++) {
						fLink.AttachShape(*shapes[i]);
						delete shapes[i];
					}
				} else
					fLink.StartMessage(status);

				delete[] shapes;
			} else
				fLink.StartMessage(status);

			delete[] charArray;
			fLink.Flush();
			break;
		}

		case AS_GET_HAS_GLYPHS:
		{
			FTRACE(("ServerApp %s: AS_GET_HAS_GLYPHS\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) int32 - numChars
			// 4) int32 - numBytes
			// 5) char - the char buffer with size numBytes

			uint16 familyID, styleID;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);

			int32 numChars, numBytes;
			link.Read<int32>(&numChars);
			link.Read<int32>(&numBytes);
			// TODO: proper error checking
			char* charArray = new (nothrow) char[numBytes];
			link.Read(charArray, numBytes);

			ServerFont font;
			status_t status = font.SetFamilyAndStyle(familyID, styleID);
			if (status == B_OK) {
				bool hasArray[numChars];
				status = font.GetHasGlyphs(charArray, numBytes, hasArray);
				if (status == B_OK) {
					fLink.StartMessage(B_OK);
					fLink.Attach(hasArray, sizeof(hasArray));
				} else
					fLink.StartMessage(status);
			} else
				fLink.StartMessage(status);

			delete[] charArray;
			fLink.Flush();
			break;
		}

		case AS_GET_EDGES:
		{
			FTRACE(("ServerApp %s: AS_GET_EDGES\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) int32 - numChars
			// 4) int32 - numBytes
			// 5) char - the char buffer with size numBytes

			uint16 familyID, styleID;
			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);

			int32 numChars;
			link.Read<int32>(&numChars);

			uint32 numBytes;
			link.Read<uint32>(&numBytes);
			// TODO: proper error checking
			char* charArray = new (nothrow) char[numBytes];
			link.Read(charArray, numBytes);

			ServerFont font;
			status_t status = font.SetFamilyAndStyle(familyID, styleID);
			if (status == B_OK) {
				edge_info edgeArray[numChars];
				status = font.GetEdges(charArray, numBytes, edgeArray);
				if (status == B_OK) {
					fLink.StartMessage(B_OK);
					fLink.Attach(edgeArray, sizeof(edgeArray));
				} else
					fLink.StartMessage(status);
			} else
				fLink.StartMessage(status);

			delete[] charArray;
			fLink.Flush();
			break;
		}

		case AS_GET_ESCAPEMENTS:
		{
			FTRACE(("ServerApp %s: AS_GET_ESCAPEMENTS\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - point size
			// 4) uint8 - spacing
			// 5) float - rotation
			// 6) uint32 - flags
			// 7) int32 - numChars
			// 8) char - char     -\       both
			// 9) BPoint - offset -/ (numChars times)

			// Returns:
			// 1) BPoint - escapement
			// numChars times

			uint16 familyID, styleID;
			uint32 flags;
			float size, rotation;
			uint8 spacing;

			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&size);
			link.Read<uint8>(&spacing);
			link.Read<float>(&rotation);
			link.Read<uint32>(&flags);

			escapement_delta delta;
			link.Read<float>(&delta.nonspace);
			link.Read<float>(&delta.space);

			bool wantsOffsets;
			link.Read<bool>(&wantsOffsets);

			int32 numChars;
			link.Read<int32>(&numChars);

			uint32 numBytes;
			link.Read<uint32>(&numBytes);

			char* charArray = new(std::nothrow) char[numBytes];
			BPoint* escapements = new(std::nothrow) BPoint[numChars];
			BPoint* offsets = NULL;
			if (wantsOffsets)
				offsets = new(std::nothrow) BPoint[numChars];

			if (charArray == NULL || escapements == NULL
				|| (offsets == NULL && wantsOffsets)) {
				delete[] charArray;
				delete[] escapements;
				delete[] offsets;

				fLink.StartMessage(B_NO_MEMORY);
				fLink.Flush();
				break;
			}

			link.Read(charArray, numBytes);

			ServerFont font;
			status_t status = font.SetFamilyAndStyle(familyID, styleID);
			if (status == B_OK) {
				font.SetSize(size);
				font.SetSpacing(spacing);
				font.SetRotation(rotation);
				font.SetFlags(flags);

				status = font.GetEscapements(charArray, numBytes, numChars,
					delta, escapements, offsets);

				if (status == B_OK) {
					fLink.StartMessage(B_OK);
					for (int32 i = 0; i < numChars; i++)
						fLink.Attach<BPoint>(escapements[i]);

					if (offsets) {
						for (int32 i = 0; i < numChars; i++)
							fLink.Attach<BPoint>(offsets[i]);
					}
				} else
					fLink.StartMessage(status);
			} else
				fLink.StartMessage(status);

			delete[] charArray;
			delete[] escapements;
			delete[] offsets;
			fLink.Flush();
			break;
		}

		case AS_GET_ESCAPEMENTS_AS_FLOATS:
		{
			FTRACE(("ServerApp %s: AS_GET_ESCAPEMENTS_AS_FLOATS\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - point size
			// 4) uint8 - spacing
			// 5) float - rotation
			// 6) uint32 - flags
			// 7) float - additional "nonspace" delta
			// 8) float - additional "space" delta
			// 9) int32 - numChars
			// 10) int32 - numBytes
			// 11) char - the char buffer with size numBytes

			// Returns:
			// 1) float - escapement buffer with numChar entries

			uint16 familyID, styleID;
			uint32 flags;
			float size, rotation;
			uint8 spacing;

			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&size);
			link.Read<uint8>(&spacing);
			link.Read<float>(&rotation);
			link.Read<uint32>(&flags);

			escapement_delta delta;
			link.Read<float>(&delta.nonspace);
			link.Read<float>(&delta.space);

			int32 numChars;
			link.Read<int32>(&numChars);

			uint32 numBytes;
			link.Read<uint32>(&numBytes);

			char* charArray = new (nothrow) char[numBytes];
			float* escapements = new (nothrow) float[numChars];
			if (charArray == NULL || escapements == NULL) {
				delete[] charArray;
				delete[] escapements;
				fLink.StartMessage(B_NO_MEMORY);
				fLink.Flush();
				break;
			}

			link.Read(charArray, numBytes);

			// figure out escapements

			ServerFont font;
			status_t status = font.SetFamilyAndStyle(familyID, styleID);
			if (status == B_OK) {
				font.SetSize(size);
				font.SetSpacing(spacing);
				font.SetRotation(rotation);
				font.SetFlags(flags);

				status = font.GetEscapements(charArray, numBytes, numChars,
					delta, escapements);

				if (status == B_OK) {
					fLink.StartMessage(B_OK);
					fLink.Attach(escapements, numChars * sizeof(float));
				}
			}

			delete[] charArray;
			delete[] escapements;

			if (status != B_OK)
				fLink.StartMessage(status);

			fLink.Flush();
			break;
		}

		case AS_GET_BOUNDINGBOXES_CHARS:
		case AS_GET_BOUNDINGBOXES_STRING:
		{
			FTRACE(("ServerApp %s: AS_GET_BOUNDINGBOXES_CHARS\n", Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - point size
			// 4) float - rotation
			// 5) float - shear
			// 6) float - false bold width
			// 7) uint8 - spacing
			// 8) uint32 - flags
			// 9) font_metric_mode - mode
			// 10) bool - string escapement
			// 11) escapement_delta - additional delta
			// 12) int32 - numChars
			// 13) int32 - numBytes
			// 14) char - the char buffer with size numBytes

			// Returns:
			// 1) BRect - rects with numChar entries

			uint16 familyID, styleID;
			uint32 flags;
			float size, rotation, shear, falseBoldWidth;
			uint8 spacing;
			font_metric_mode mode;
			bool stringEscapement;

			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&size);
			link.Read<float>(&rotation);
			link.Read<float>(&shear);
			link.Read<float>(&falseBoldWidth);
			link.Read<uint8>(&spacing);
			link.Read<uint32>(&flags);
			link.Read<font_metric_mode>(&mode);
			link.Read<bool>(&stringEscapement);

			escapement_delta delta;
			link.Read<escapement_delta>(&delta);

			int32 numChars;
			link.Read<int32>(&numChars);

			uint32 numBytes;
			link.Read<uint32>(&numBytes);

			bool success = false;

			char* charArray = new(std::nothrow) char[numBytes];
			BRect* rectArray = new(std::nothrow) BRect[numChars];
			if (charArray != NULL && rectArray != NULL) {
				link.Read(charArray, numBytes);

				// figure out escapements

				ServerFont font;
				if (font.SetFamilyAndStyle(familyID, styleID) == B_OK) {
					font.SetSize(size);
					font.SetRotation(rotation);
					font.SetShear(shear);
					font.SetFalseBoldWidth(falseBoldWidth);
					font.SetSpacing(spacing);
					font.SetFlags(flags);

					// TODO: implement for real
					if (font.GetBoundingBoxes(charArray, numBytes,
								rectArray, stringEscapement, mode, delta,
								code == AS_GET_BOUNDINGBOXES_STRING) == B_OK) {
						fLink.StartMessage(B_OK);
						fLink.Attach(rectArray, sizeof(rectArray));
						success = true;
					}
				}
			}

			if (!success)
				fLink.StartMessage(B_ERROR);

			fLink.Flush();

			delete[] charArray;
			delete[] rectArray;
			break;
		}

		case AS_GET_BOUNDINGBOXES_STRINGS:
		{
			FTRACE(("ServerApp %s: AS_GET_BOUNDINGBOXES_STRINGS\n",
				Signature()));

			// Attached Data:
			// 1) uint16 - family ID
			// 2) uint16 - style ID
			// 3) float - point size
			// 4) float - rotation
			// 5) float - shear
			// 6) float - false bold width
			// 7) uint8 - spacing
			// 8) uint32 - flags
			// 9) font_metric_mode - mode
			// 10) int32 numStrings
			// 11) escapement_delta - additional delta (numStrings times)
			// 12) int32 string length to measure (numStrings times)
			// 13) string - string (numStrings times)

			// Returns:
			// 1) BRect - rects with numStrings entries

			uint16 familyID, styleID;
			uint32 flags;
			float ptsize, rotation, shear, falseBoldWidth;
			uint8 spacing;
			font_metric_mode mode;

			link.Read<uint16>(&familyID);
			link.Read<uint16>(&styleID);
			link.Read<float>(&ptsize);
			link.Read<float>(&rotation);
			link.Read<float>(&shear);
			link.Read<float>(&falseBoldWidth);
			link.Read<uint8>(&spacing);
			link.Read<uint32>(&flags);
			link.Read<font_metric_mode>(&mode);

			int32 numStrings;
			link.Read<int32>(&numStrings);

			escapement_delta deltaArray[numStrings];
			char* stringArray[numStrings];
			int32 lengthArray[numStrings];
			for(int32 i = 0; i < numStrings; i++) {
				// This version of ReadString allocates the strings, we free
				// them below
				// TODO: this does not work on 64-bit (size_t != int32)
				link.ReadString(&stringArray[i], (size_t*)&lengthArray[i]);
				link.Read<escapement_delta>(&deltaArray[i]);
			}

			// TODO: don't do this on the heap! (at least check the size before)
			BRect rectArray[numStrings];

			ServerFont font;
			bool success = false;
			if (font.SetFamilyAndStyle(familyID, styleID) == B_OK) {
				font.SetSize(ptsize);
				font.SetRotation(rotation);
				font.SetShear(shear);
				font.SetFalseBoldWidth(falseBoldWidth);
				font.SetSpacing(spacing);
				font.SetFlags(flags);

				if (font.GetBoundingBoxesForStrings(stringArray, lengthArray,
					numStrings, rectArray, mode, deltaArray) == B_OK) {
					fLink.StartMessage(B_OK);
					fLink.Attach(rectArray, sizeof(rectArray));
					success = true;
				}
			}

			for (int32 i = 0; i < numStrings; i++)
				free(stringArray[i]);

			if (!success)
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		// Screen commands

		case AS_VALID_SCREEN_ID:
		{
			// Attached data
			// 1) int32 screen

			int32 id;
			if (link.Read<int32>(&id) == B_OK
				&& id == B_MAIN_SCREEN_ID.id)
				fLink.StartMessage(B_OK);
			else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		case AS_GET_NEXT_SCREEN_ID:
		{
			// Attached data
			// 1) int32 screen

			int32 id;
			link.Read<int32>(&id);

			// TODO: for now, just say we're the last one
			fLink.StartMessage(B_ENTRY_NOT_FOUND);
			fLink.Flush();
			break;
		}

		case AS_SCREEN_GET_COLORMAP:
		{
			STRACE(("ServerApp %s: AS_SCREEN_GET_COLORMAP\n", Signature()));

			int32 id;
			link.Read<int32>(&id);

			const color_map* colorMap = SystemColorMap();
			if (colorMap != NULL) {
				fLink.StartMessage(B_OK);
				fLink.Attach<color_map>(*colorMap);
			} else
				fLink.StartMessage(B_ERROR);

			fLink.Flush();
			break;
		}

		case AS_GET_DESKTOP_COLOR:
		{
			STRACE(("ServerApp %s: get desktop color\n", Signature()));

			uint32 index;
			link.Read<uint32>(&index);

			fLink.StartMessage(B_OK);
			fDesktop->Lock();

			// we're nice to our children (and also take the default case
			// into account which asks for the current workspace)
			if (index >= (uint32)kMaxWorkspaces)
				index = fDesktop->CurrentWorkspace();

			Workspace workspace(*fDesktop, index);
			fLink.Attach<rgb_color>(workspace.Color());

			fDesktop->Unlock();
			fLink.Flush();
			break;
		}

		case AS_SET_DESKTOP_COLOR:
		{
			STRACE(("ServerApp %s: set desktop color\n", Signature()));

			rgb_color color;
			uint32 index;
			bool makeDefault;

			link.Read<rgb_color>(&color);
			link.Read<uint32>(&index);
			if (link.Read<bool>(&makeDefault) != B_OK)
				break;

			fDesktop->Lock();

			// we're nice to our children (and also take the default case
			// into account which asks for the current workspace)
			if (index >= (uint32)kMaxWorkspaces)
				index = fDesktop->CurrentWorkspace();

			Workspace workspace(*fDesktop, index);
			workspace.SetColor(color, makeDefault);

			fDesktop->Unlock();
			break;
		}

		case AS_SET_UI_COLOR:
		{
			STRACE(("ServerApp %s: Set UI Color\n", Signature()));

			// Attached Data:
			// 1) color_which which
			// 2) rgb_color color

			color_which which;
			rgb_color color;

			link.Read<color_which>(&which);
			if (link.Read<rgb_color>(&color) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetUIColor(which, color);
			}
			break;
		}
		// Hinting and aliasing

		case AS_SET_SUBPIXEL_ANTIALIASING:
		{
			bool subpix;
			if (link.Read<bool>(&subpix) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetSubpixelAntialiasing(subpix);
			}
			fDesktop->Redraw();
			break;
		}

		case AS_GET_SUBPIXEL_ANTIALIASING:
		{
			DesktopSettings settings(fDesktop);
			fLink.StartMessage(B_OK);
			fLink.Attach<bool>(settings.SubpixelAntialiasing());
			fLink.Flush();
			break;
		}

		case AS_SET_HINTING:
		{
			uint8 hinting;
			if (link.Read<uint8>(&hinting) == B_OK && hinting < 3) {
				LockedDesktopSettings settings(fDesktop);
				if (hinting != settings.Hinting()) {
					settings.SetHinting(hinting);
					fDesktop->Redraw();
				}
			}
			break;
		}

		case AS_GET_HINTING:
		{
			DesktopSettings settings(fDesktop);
			fLink.StartMessage(B_OK);
			fLink.Attach<uint8>(settings.Hinting());
			fLink.Flush();
			break;
		}

		case AS_SET_SUBPIXEL_AVERAGE_WEIGHT:
		{
			uint8 averageWeight;
			if (link.Read<uint8>(&averageWeight) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetSubpixelAverageWeight(averageWeight);
			}
			fDesktop->Redraw();
			break;
		}

		case AS_GET_SUBPIXEL_AVERAGE_WEIGHT:
		{
			DesktopSettings settings(fDesktop);
			fLink.StartMessage(B_OK);
			fLink.Attach<uint8>(settings.SubpixelAverageWeight());
			fLink.Flush();
			break;
		}

		case AS_SET_SUBPIXEL_ORDERING:
		{
			bool subpixelOrdering;
			if (link.Read<bool>(&subpixelOrdering) == B_OK) {
				LockedDesktopSettings settings(fDesktop);
				settings.SetSubpixelOrderingRegular(subpixelOrdering);
			}
			fDesktop->Redraw();
			break;
		}

		case AS_GET_SUBPIXEL_ORDERING:
		{
			DesktopSettings settings(fDesktop);
			fLink.StartMessage(B_OK);
			fLink.Attach<bool>(settings.IsSubpixelOrderingRegular());
			fLink.Flush();
			break;
		}

		default:
			printf("ServerApp %s received unhandled message code %" B_PRId32
				"\n", Signature(), code);

			if (link.NeedsReply()) {
				// the client is now blocking and waiting for a reply!
				fLink.StartMessage(B_ERROR);
				fLink.Flush();
			} else
				puts("message doesn't need a reply!");
			break;
	}
}


/*!	\brief The thread function ServerApps use to monitor messages
*/
void
ServerApp::_MessageLooper()
{
	// Message-dispatching loop for the ServerApp

	// First let's tell the client how to talk with us.
	fLink.StartMessage(B_OK);
	fLink.Attach<port_id>(fMessagePort);
	fLink.Attach<area_id>(fDesktop->SharedReadOnlyArea());
	fLink.Flush();

	BPrivate::LinkReceiver &receiver = fLink.Receiver();

	int32 code;
	status_t err = B_OK;

	while (!fQuitting) {
		STRACE(("info: ServerApp::_MessageLooper() listening on port %" B_PRId32
			".\n", fMessagePort));

		err = receiver.GetNextMessage(code, B_INFINITE_TIMEOUT);
		if (err != B_OK || code == B_QUIT_REQUESTED) {
			STRACE(("ServerApp: application seems to be gone...\n"));

			// Tell desktop to quit us
			BPrivate::LinkSender link(fDesktop->MessagePort());
			link.StartMessage(AS_DELETE_APP);
			link.Attach<thread_id>(Thread());
			link.Flush();
			break;
		}

		switch (code) {
			case kMsgAppQuit:
				// we receive this from our destructor on quit
				fQuitting = true;
				break;

			case AS_QUIT_APP:
			{
				// This message is received only when the app_server is asked
				// to shut down in test/debug mode. Of course, if we are testing
				// while using AccelerantDriver, we do NOT want to shut down
				// client applications. The server can be quit in this fashion
				// through the driver's interface, such as closing the
				// ViewDriver's window.

				STRACE(("ServerApp %s:Server shutdown notification received\n",
					Signature()));

				// If we are using the real, accelerated version of the
				// DrawingEngine, we do NOT want the user to be able shut down
				// the server. The results would NOT be pretty
#if TEST_MODE
				BMessage pleaseQuit(B_QUIT_REQUESTED);
				SendMessageToClient(&pleaseQuit);
#endif
				break;
			}

			default:
				STRACE(("ServerApp %s: Got a Message to dispatch\n",
					Signature()));
				_DispatchMessage(code, receiver);
				break;
		}
	}

	// Quit() will send us a message; we're handling the exiting procedure
	thread_id sender;
	sem_id shutdownSemaphore;
	receive_data(&sender, &shutdownSemaphore, sizeof(sem_id));

	delete this;

	if (shutdownSemaphore >= B_OK)
		release_sem(shutdownSemaphore);
}


status_t
ServerApp::_CreateWindow(int32 code, BPrivate::LinkReceiver& link,
	port_id& clientReplyPort)
{
	// Attached data:
	// 1) int32 bitmap token (only for AS_CREATE_OFFSCREEN_WINDOW)
	// 2) BRect window frame
	// 3) uint32 window look
	// 4) uint32 window feel
	// 5) uint32 window flags
	// 6) uint32 workspace index
	// 7) int32 BHandler token of the window
	// 8) port_id window's reply port
	// 9) port_id window's looper port
	// 10) const char * title

	BRect frame;
	int32 bitmapToken;
	uint32 look;
	uint32 feel;
	uint32 flags;
	uint32 workspaces;
	int32 token;
	port_id looperPort;
	char* title;

	if (code == AS_CREATE_OFFSCREEN_WINDOW)
		link.Read<int32>(&bitmapToken);

	link.Read<BRect>(&frame);
	link.Read<uint32>(&look);
	link.Read<uint32>(&feel);
	link.Read<uint32>(&flags);
	link.Read<uint32>(&workspaces);
	link.Read<int32>(&token);
	link.Read<port_id>(&clientReplyPort);
	link.Read<port_id>(&looperPort);
	if (link.ReadString(&title) != B_OK)
		return B_ERROR;

	if (!frame.IsValid()) {
		// make sure we pass a valid rectangle to ServerWindow
		frame.right = frame.left + 1;
		frame.bottom = frame.top + 1;
	}

	status_t status = B_NO_MEMORY;
	ServerWindow *window = NULL;

	if (code == AS_CREATE_OFFSCREEN_WINDOW) {
	} else {
		window = new (nothrow) ServerWindow(title, this, clientReplyPort,
			looperPort, token);
		STRACE(("\nServerApp %s: New Window %s (%g:%g, %g:%g)\n",
			Signature(), title, frame.left, frame.top,
			frame.right, frame.bottom));
	}

	free(title);

	// NOTE: the reply to the client is handled in ServerWindow::Run()
	if (window != NULL) {
		status = window->Init(frame, (window_look)look, (window_feel)feel,
			flags, workspaces);
		if (status == B_OK && !window->Run()) {
			fprintf(stderr, "ServerApp::_CreateWindow() - failed to run the window thread\n");
			status = B_ERROR;
		}

		if (status != B_OK)
			delete window;
	}

	return status;
}


bool
ServerApp::_HasWindowUnderMouse()
{
	BAutolock locker(fWindowListLock);

	for (int32 i = fWindowList.CountItems(); i-- > 0;) {
		ServerWindow* serverWindow = fWindowList.ItemAt(i);

		if (fDesktop->ViewUnderMouse(serverWindow->Window()) != B_NULL_TOKEN)
			return true;
	}

	return false;
}


ServerBitmap*
ServerApp::_FindBitmap(int32 token) const
{
	//ASSERT(fMapLock.IsLocked());

	BitmapMap::const_iterator iterator = fBitmapMap.find(token);
	if (iterator == fBitmapMap.end())
		return NULL;

	return iterator->second;
}


ServerPicture *
ServerApp::CreatePicture(const ServerPicture *original)
{
	ServerPicture *picture;
	if (original != NULL)
		picture = new (nothrow) ServerPicture(*original);
	else
		picture = new (nothrow) ServerPicture();

	if (picture != NULL)
		fPictureList.AddItem(picture);
	
	return picture;
}


ServerPicture *
ServerApp::_FindPicture(const int32 &token) const
{
	// TODO: we need to make sure the picture is ours?!
	ServerPicture* picture;
	if (gTokenSpace.GetToken(token, kPictureToken, (void**)&picture) == B_OK)
		return picture;

	return NULL;
}


bool
ServerApp::DeletePicture(const int32 &token)
{
	ServerPicture *picture = _FindPicture(token);
	if (picture == NULL)
		return false;
	
	if (!fPictureList.RemoveItem(picture))
		return false;
	
	delete picture;

	return true;
}


