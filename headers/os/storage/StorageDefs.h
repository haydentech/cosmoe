/*
 * Copyright 2002-2006, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DEF_STORAGE_H
#define _DEF_STORAGE_H


#include <fcntl.h>
#include <sys/param.h>
#include <limits.h>

/* Provide fallbacks for platform headers that may not define these */
#ifndef NAME_MAX
#ifdef _POSIX_NAME_MAX
#define NAME_MAX _POSIX_NAME_MAX
#else
#define NAME_MAX 255
#endif
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#else
/* Windows traditional MAX_PATH is 260; use a conservative default */
#define MAXPATHLEN 260
#endif
#endif

#ifndef SYMLINK_MAX
#define SYMLINK_MAX 40
#endif


/* Limits */
#define B_DEV_NAME_LENGTH		128
#define B_FILE_NAME_LENGTH		NAME_MAX
#define B_PATH_NAME_LENGTH		MAXPATHLEN
#define B_ATTR_NAME_LENGTH		(B_FILE_NAME_LENGTH - 1)
#define B_MIME_TYPE_LENGTH		(B_ATTR_NAME_LENGTH - 15)
#define B_MAX_SYMLINKS			SYMLINK_MAX

/* Open Modes */
#define B_READ_ONLY			O_RDONLY	/* read only */
#define B_WRITE_ONLY		O_WRONLY	/* write only */
#define B_READ_WRITE		O_RDWR		/* read and write */

#define B_FAIL_IF_EXISTS	O_EXCL		/* exclusive create */
#define B_CREATE_FILE		O_CREAT		/* create the file */
#define B_ERASE_FILE		O_TRUNC		/* erase the file's data */
#define B_OPEN_AT_END		O_APPEND	/* point to the end of the data */

/* Node Flavors */
enum node_flavor {
	B_FILE_NODE			= 0x01,
	B_SYMLINK_NODE		= 0x02,
	B_DIRECTORY_NODE	= 0x04,
	B_ANY_NODE			= 0x07
};

#ifndef O_RWMASK
#define O_RWMASK O_ACCMODE
#endif

#ifndef O_NOFOLLOW
// Windows doesn't have O_NOFOLLOW, define it as 0 (no-op)
#define O_NOFOLLOW 0
#endif

#ifndef O_NOTRAVERSE
#define O_NOTRAVERSE O_NOFOLLOW
#endif

#ifndef S_IUMSK
#ifdef _WIN32
// Windows doesn't have ALLPERMS, define all permission bits manually
#define S_IUMSK 0777
#else
#define S_IUMSK ALLPERMS
#endif
#endif

#ifdef _WIN32
// Windows compatibility for POSIX functions
#include <sys/stat.h>
#define lstat stat
#ifndef S_ISLNK
#define S_ISLNK(m) (0)
#endif

#ifndef ALLPERMS
#define ALLPERMS 0777
#endif
#endif

#endif /* _DEF_STORAGE_H */
