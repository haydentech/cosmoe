/*
 * Copyright 2004-2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYSTEM_SYSCALLS_H
#define _SYSTEM_SYSCALLS_H


#include <DiskDeviceDefs.h>
#include <image.h>
#include <OS.h>

#include <signal.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif


// Cosmoe shims
#define _kern_cpu_enabled(x) 1
#define _kern_set_cpu_enabled(x,y)
#define suggest_thread_priority(x) B_NORMAL_PRIORITY


#endif	/* _SYSTEM_SYSCALLS_H */
