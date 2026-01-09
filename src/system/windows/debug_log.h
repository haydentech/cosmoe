/*
 * Debug logging for Windows backend
 * Logs to both file and OutputDebugString for Wine debugging
 */
#ifndef _WINDOWS_DEBUG_LOG_H
#define _WINDOWS_DEBUG_LOG_H

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize debug logging (creates log file)
void debug_log_init(void);

// Close debug logging
void debug_log_close(void);

// Log a debug message (printf-style)
void debug_log(const char* format, ...);

// Log with Windows error code
void debug_log_error(const char* message);

#ifdef __cplusplus
}
#endif

#else
// Non-Windows: just use printf
#define debug_log_init()
#define debug_log_close()
#define debug_log printf
#define debug_log_error(msg) fprintf(stderr, "%s\n", msg)
#endif

#endif // _WINDOWS_DEBUG_LOG_H
