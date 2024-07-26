/*
 * Copyright 2001-2016, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Axel Dörfler, axeld@pinc-software.de
 *		Stephan Aßmus <superstippi@gmx.de>
 * 		Christian Packmann
 */


#include "AppServer.h"

#include <AutoDeleter.h>
#include <PortLink.h>

#include "BitmapManager.h"
#include "Desktop.h"
#include "GlobalFontManager.h"
#include "InputManager.h"
#include "ScreenManager.h"
#include "ServerProtocol.h"


//#define DEBUG_SERVER
#ifdef DEBUG_SERVER
#	include <stdio.h>
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif


// Globals
port_id gAppServerPort;
BTokenSpace gTokenSpace;
uint32 gAppServerSIMDFlags = 0;


/*!	\brief Constructor

	This loads the default fonts, allocates all the major global variables,
	spawns the main housekeeping threads, loads user preferences for the UI
	and decorator, and allocates various locks.
*/
AppServer::AppServer(status_t* status)
	:
	MessageLooper("app_server"),
	fMessagePort(-1),
	fDesktopLock("AppServerDesktopLock")
{
	fMessagePort = create_port(DEFAULT_MONITOR_PORT_SIZE, SERVER_PORT_NAME);
	if (fMessagePort < B_OK)
		debugger("app_server could not create message port");

	fLink.SetReceiverPort(fMessagePort);


	gInputManager = new InputManager();

	// Create the font server and scan the proper directories.
	gFontManager = new GlobalFontManager;
	if (gFontManager->InitCheck() != B_OK)
		debugger("font manager could not be initialized!");

	gFontManager->Run();

	gScreenManager = new ScreenManager();
	gScreenManager->Run();

	// Create the bitmap allocator. Object declared in BitmapManager.cpp
	gBitmapManager = new BitmapManager();

#if 0
	// This is not presently needed, as app_server is launched from the login session.
#ifndef HAIKU_TARGET_PLATFORM_LIBBE_TEST
	// TODO: check the attached displays, and launch login session for them
	BMessage data;
	data.AddString("name", "app_server");
	data.AddInt32("session", 0);
	BLaunchRoster().Target("login", data);
#endif
#endif
	if (status != NULL)
		*status = B_OK;
}


/*!	\brief Destructor
	Reached only when the server is asked to shut down in Test mode.
*/
AppServer::~AppServer()
{
	delete gBitmapManager;

	gScreenManager->Lock();
	gScreenManager->Quit();

	gFontManager->Lock();
	gFontManager->Quit();
}


void
AppServer::RunLooper()
{
	rename_thread(find_thread(NULL), "picasso");
	_message_thread((void*)this);
}


/*!	\brief Message handling function for all messages sent to the app_server
	\param code ID of the message sent
	\param buffer Attachment buffer for the message.

*/
void
AppServer::_DispatchMessage(int32 code, BPrivate::LinkReceiver& msg)
{
	switch (code) {
		case AS_GET_DESKTOP:
		{
			Desktop* desktop = NULL;

			port_id replyPort;
			msg.Read<port_id>(&replyPort);

			int32 userID;
			msg.Read<int32>(&userID);

			char* targetScreen = NULL;
			msg.ReadString(&targetScreen);
			if (targetScreen != NULL && strlen(targetScreen) == 0) {
				free(targetScreen);
				targetScreen = NULL;
			}

			int32 version;
			if (msg.Read<int32>(&version) < B_OK
				|| version != AS_PROTOCOL_VERSION) {
				fprintf(stderr, "Application for user %" B_PRId32 " with port "
					"%" B_PRId32 " does not support the current server "
					"protocol.\n", userID, replyPort);
			} else {
				desktop = _FindDesktop(userID, targetScreen);
				if (desktop == NULL) {
					// we need to create a new desktop object for this user
					// TODO: test if the user exists on the system
					// TODO: maybe have a separate AS_START_DESKTOP_SESSION for
					// authorizing the user
					desktop = _CreateDesktop(userID, targetScreen ? targetScreen : "SCREEN");
				}
			}

			free(targetScreen);

			BPrivate::LinkSender reply(replyPort);
			if (desktop != NULL) {
				reply.StartMessage(B_OK);
				reply.Attach<port_id>(desktop->MessagePort());
			} else
				reply.StartMessage(B_ERROR);

			reply.Flush();
			break;
		}

#if TEST_MODE
		case B_QUIT_REQUESTED:
		{
			// We've been asked to quit, so (for now) broadcast to all
			// desktops to quit. This situation will occur only when the server
			// is compiled as a regular Be application.

			fQuitting = true;

			while (fDesktops.CountItems() > 0) {
				Desktop *desktop = fDesktops.RemoveItemAt(0);

				thread_id thread = desktop->Thread();
				desktop->PostMessage(B_QUIT_REQUESTED);

				// we just wait for the desktop to kill itself
				status_t status;
				wait_for_thread(thread, &status);
			}

			delete this;

			// we are now clear to exit
			exit(0);
			break;
		}
#endif

		default:
			STRACE(("Server::MainLoop received unexpected code %" B_PRId32 " "
				"(offset %" B_PRId32 ")\n", code, code - SERVER_TRUE));
			break;
	}
}


/*!	\brief Creates a desktop object for an authorized user
*/
Desktop*
AppServer::_CreateDesktop(uid_t userID, const char* targetScreen)
{
	BAutolock locker(fDesktopLock);
	ObjectDeleter<Desktop> desktop;
	try {
		desktop.SetTo(new Desktop(userID, targetScreen));

		status_t status = desktop->Init();
		if (status == B_OK)
			status = desktop->Run();
		if (status == B_OK && !fDesktops.AddItem(desktop.Get()))
			status = B_NO_MEMORY;

		if (status != B_OK) {
			fprintf(stderr, "Cannot initialize Desktop object: %s\n",
				strerror(status));
			return NULL;
		}
	} catch (...) {
		// there is obviously no memory left
		return NULL;
	}

	return desktop.Detach();
}


/*!	\brief Finds the desktop object that belongs to a certain user
*/
Desktop*
AppServer::_FindDesktop(uid_t userID, const char* targetScreen)
{
	BAutolock locker(fDesktopLock);

	for (int32 i = 0; i < fDesktops.CountItems(); i++) {
		Desktop* desktop = fDesktops.ItemAt(i);

		if (desktop->UserID() == userID) {
			//&& ((desktop->TargetScreen() == NULL && targetScreen == NULL)
			//	|| (desktop->TargetScreen() != NULL && targetScreen != NULL
			//		&& strcmp(desktop->TargetScreen(), targetScreen) == 0))) {
			return desktop;
		}
	}

	return NULL;
}


//	#pragma mark -


int
main(int argc, char** argv)
{
	if (find_port(SERVER_PORT_NAME) >= B_OK)
		return -1;

	status_t status;
	AppServer* server = new AppServer(&status);
	if (status == B_OK)
		server->RunLooper();

	return status == B_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
