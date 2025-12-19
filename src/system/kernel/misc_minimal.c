/*
 * Minimal misc.c implementation for macOS
 * Contains only the functions needed for cosmoe without the conflicting ones
 */

#include <SupportDefs.h>
#include <OS.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>

// Simple implementation of system_time() for macOS
bigtime_t system_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (bigtime_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

// Simple implementation of debugger() for macOS
void debugger(const char *message)
{
	// On macOS, we can just call the builtin debugger trap
	__builtin_debugtrap();
}

// Simple implementation of debug_printf() for macOS  
void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
