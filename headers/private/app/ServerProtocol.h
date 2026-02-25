/*
 * Copyright 2001-2016, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Jérôme Duval, jerome.duval@free.fr
 *		Axel Dörfler, axeld@pinc-software.de
 *		Andrej Spielmann, <andrej.spielmann@seh.ox.ac.uk>
 *		Julian Harnath, <julian.harnath@rwth-aachen.de>
 */
#ifndef APP_SERVER_PROTOCOL_H
#define APP_SERVER_PROTOCOL_H


#include <SupportDefs.h>


#ifdef HAIKU_TARGET_PLATFORM_LIBBE_TEST
#	define SERVER_PORT_NAME "haiku-test:app_server"
#endif


#define AS_PROTOCOL_VERSION	1

enum {
	// NOTE: all defines have to start with "AS_" to let the "code_to_name"
	// utility work correctly

	// Application definitions
	AS_GET_DESKTOP,
	AS_REGISTER_INPUT_SERVER = 1,

	AS_GET_SCREEN_FRAME,

	AS_CREATE_WINDOW,
	AS_CREATE_OFFSCREEN_WINDOW,
	AS_DELETE_WINDOW,

	AS_SET_CURSOR,
	AS_SET_VIEW_CURSOR,

	AS_SHOW_CURSOR,
	AS_HIDE_CURSOR,
	AS_OBSCURE_CURSOR,
	AS_QUERY_CURSOR_HIDDEN,

	AS_CREATE_CURSOR,
	AS_CREATE_CURSOR_BITMAP,
	AS_DELETE_CURSOR,

	AS_BEGIN_RECT_TRACKING,
	AS_END_RECT_TRACKING,

	AS_GET_CURSOR_POSITION,
	AS_GET_CURSOR_BITMAP,

	// Window definitions
	AS_SHOW_OR_HIDE_WINDOW,
	AS_INTERNAL_HIDE_WINDOW,
	AS_MINIMIZE_WINDOW,
	AS_QUIT_WINDOW,
	AS_SEND_BEHIND,
	AS_SET_LOOK,
	AS_SET_FEEL,
	AS_SET_FLAGS,
	AS_DISABLE_UPDATES,
	AS_ENABLE_UPDATES,
	AS_BEGIN_UPDATE,
	AS_END_UPDATE,
	AS_NEEDS_UPDATE,
	AS_FORCE_UPDATE,
	AS_SET_WINDOW_TITLE,
	AS_GET_POSITION,
	AS_WINDOW_RESIZE,
	AS_WINDOW_MOVE,
	AS_SET_SIZE_LIMITS,
	AS_ACTIVATE_WINDOW,
	AS_IS_FRONT_WINDOW,
	AS_WINDOW_SHOW,
	AS_WINDOW_HIDE,

	AS_LAST_CODE
};

#define BEGIN_MESSAGE BPrivate::AppServerLink link; BPrivate::AppServerLink* fLink = &link;
#endif	// APP_SERVER_PROTOCOL_H
