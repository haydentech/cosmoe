/*
 * Copyright 2002-2008, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>
#include <Debug.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



bool _rtDebugFlag = true;


void
debugger(const char *message)
{
	debug_printf("%" B_PRId32 ": DEBUGGER: %s\n", find_thread(NULL), message);
}



// TODO: Remove. Temporary debug helper.
// (accidently these are more or less the same as _sPrintf())
int
debug_printf(const char *format, ...)
{
	int count;

	va_list list;
	va_start(list, format);

	count = debug_vprintf(format, list);

	va_end(list);
	return count;
}


int
debug_vprintf(const char *format, va_list args)
{
	char buffer[1024];
	int count = vsnprintf(buffer, sizeof(buffer), format, args);

	return count;
}

