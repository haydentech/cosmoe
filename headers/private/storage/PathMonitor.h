/*
 * Copyright 2007-2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PATH_MONITOR_H
#define _PATH_MONITOR_H


#include <NodeMonitor.h>


// Monitoring a path always implies B_WATCH_NAME for the path itself. I.e. even
// if only B_WATCH_STAT is specified, B_ENTRY_{CREATED,MOVED,REMOVED}
// notifications are sent when the respective entry is created/moved/removed.

// additional flags (combined with those in NodeMonitor.h)
#define B_WATCH_RECURSIVELY			0x0100
	// Watch not only the entry specified by the path, but also recursively all
	// descendents. A recursive B_WATCH_DIRECTORY is implied, i.e.
	// B_ENTRY_{CREATED,MOVED,REMOVED} notifications will be sent for any entry
	// change below the given path, unless explicitly suppressed by
	// B_WATCH_{FILES,DIRECTORIES}_ONLY.
#define	B_WATCH_FILES_ONLY			0x0200
	// A notification will only be sent when the node it concerns is not a
	// directory. No effect in non-recursive mode.
#define	B_WATCH_DIRECTORIES_ONLY	0x0400
	// A notification will only be sent when the node it concerns is a
	// directory. No effect in non-recursive mode.

#define B_PATH_MONITOR		'_PMN'


namespace BPrivate {


class BPathMonitor {
public:
	static	status_t			StartWatching(const char* path, uint32 flags,
									BMessenger target);

	static	status_t			StopWatching(const char* path,
									BMessenger target);
	static	status_t			StopWatching(BMessenger target);

private:
								BPathMonitor();
								~BPathMonitor();

	static	status_t			_InitLockerIfNeeded();
	static	status_t			_InitLooperIfNeeded();
};

}	// namespace BPrivate

#endif	// _PATH_MONITOR_H
