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
	// On Haiku/BeOS, debugger() prints a message and enters the debugger
	// For macOS, we'll just print the message to stderr without trapping
	if (message != NULL) {
		fprintf(stderr, "DEBUGGER: %s\n", message);
		fflush(stderr);
	} else {
		fprintf(stderr, "DEBUGGER: (no message)\n");
		fflush(stderr);
	}
	// Note: On a real system, this would enter the debugger
	// For now, we just print the message and continue
}

// Simple implementation of debug_printf() for macOS  
void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
