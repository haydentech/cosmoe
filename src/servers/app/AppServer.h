/*
 * Copyright 2001-2015, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef	APP_SERVER_H
#define	APP_SERVER_H


#include <Application.h>
#include <List.h>
#include <Locker.h>
#include <ObjectList.h>
#include <OS.h>
#include <String.h>
#include <Window.h>

#include "MessageLooper.h"
#include "ServerConfig.h"

class ServerApp;
class BitmapManager;
class Desktop;


class AppServer : public MessageLooper  {
public:
								AppServer(status_t* status);
	virtual						~AppServer();

		void			RunLooper();
		virtual port_id	MessagePort() const { return fMessagePort; }

private:
		virtual void	_DispatchMessage(int32 code, BPrivate::LinkReceiver& link);

			Desktop*			_CreateDesktop(uid_t userID,
									const char* targetScreen);
	virtual	Desktop*			_FindDesktop(uid_t userID,
									const char* targetScreen);

			void				_LaunchInputServer();

private:
		port_id			fMessagePort;

			BObjectList<Desktop> fDesktops;
			BLocker				fDesktopLock;
};


extern BitmapManager *gBitmapManager;
extern port_id gAppServerPort;


#endif	/* APP_SERVER_H */
