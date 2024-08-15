/*
 * Copyright 2024, Bill Hayden, hayden@haydentech.com
 * Distributed under the terms of the MIT License.
 */


/*!	Team functions */


#include <OS.h>
#include <AppMisc.h>

#include <unistd.h>
#include <string.h>

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
extern int32 _thread_count_for_team(team_id);
extern team_id	_get_next_team(team_id);


status_t _get_team_info(team_id id, team_info *info, size_t size)
{
	if (size != sizeof(team_info))
		return B_ERROR;
		
	if (info == NULL)
		return B_ERROR;

	info->team = id;
	info->thread_count = _thread_count_for_team(id);

	char buffer[B_PATH_NAME_LENGTH];
	if (BPrivate::get_app_path(id, buffer) == B_OK)
		strlcpy(info->args, buffer, 64);
	else
		strcpy(info->args, "unknown");
	
	return B_OK;
}


status_t _get_next_team_info(int32 *cookie, team_info *info, size_t size)
{
	team_id id = *cookie;

	if ((id = _get_next_team(id)) >= 0)
	{
		*cookie = id;
		return _get_team_info(id, info, size);
	}

	return B_NO_MORE_TEAMS;
}


team_id
team_get_current_team_id(void)
{
	return getpid();
}

