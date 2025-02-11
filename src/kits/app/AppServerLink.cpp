/*
 * Copyright 2001-2005, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Erik Jaesler (erik@cgsoftware.com)
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include <Application.h>
#include <Locker.h>

#include <ApplicationPrivate.h>
#include <AppServerLink.h>


/**	AppServerLink provides proxied access to the application's
 *	connection with the app_server.
 *	It has BAutolock semantics:
 *	creating one locks the app_server connection; destroying one
 *	unlocks the connection.
 */


static BLocker sLock("AppServerLink_sLock");


namespace BPrivate {

AppServerLink::AppServerLink(void)
{
}


AppServerLink::~AppServerLink()
{
}

}	// namespace BPrivate
