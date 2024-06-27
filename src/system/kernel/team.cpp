/*
 * Copyright 2008-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2010, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


/*!	Team functions */


#include <OS.h>
#include <unistd.h>

/*

typedef struct {
	team_id			team;
	int32			thread_count;
	int32			image_count;
	int32			area_count;
	thread_id		debugger_nub_thread;
	port_id			debugger_nub_port;
	int32			argc;
	char			args[64];
	uid_t			uid;
	gid_t			gid;
} team_info;

#define B_CURRENT_TEAM	0
#define B_SYSTEM_TEAM	1

*/

status_t _get_next_team_info(int32 *cookie, team_info *info, size_t size)
{
	return B_ERROR;
}

team_id
team_get_current_team_id(void)
{
	return getpid();
}

