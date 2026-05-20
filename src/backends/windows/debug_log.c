/*
 * Debug logging for Windows backend
 */
#ifdef _WIN32

#include "debug_log.h"
#include <stdbool.h>
#include <time.h>

static FILE* debug_file = NULL;
static int debug_initialized = 0;

// Change this to turn on Windows/Wine debug logging
static bool debug_enabled = false;

void debug_log_init(void) {
    if (debug_initialized)
        return;

    debug_initialized = 1;
    if (!debug_enabled)
        return;
    
    // Open log file in user's temp directory
    char logpath[MAX_PATH];
    GetTempPathA(MAX_PATH, logpath);
    strcat(logpath, "cosmoe_debug.log");
    
    debug_file = fopen(logpath, "w");
    if (debug_file) {
        fprintf(debug_file, "=== Cosmoe Windows Backend Debug Log ===\n");
        fflush(debug_file);
        
        // Also show a message box with log location
        char msg[512];
        snprintf(msg, sizeof(msg), "Debug logging enabled.\nLog file: %s", logpath);
        OutputDebugStringA(msg);
    }
}

void debug_log_close(void) {
    if (debug_file) {
        fprintf(debug_file, "=== End of log ===\n");
        fclose(debug_file);
        debug_file = NULL;
    }
    debug_initialized = 0;
	debug_enabled = 0;
}

void debug_log(const char* format, ...) {
    if (!debug_initialized) {
        debug_log_init();
    }
    if (!debug_enabled)
        return;
    
    va_list args;
    char buffer[1024];
    
    // Format the message
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Get timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    // Write to file
    if (debug_file) {
        fprintf(debug_file, "[%s] %s\n", timestamp, buffer);
        fflush(debug_file);
    }
    
    // Also write to stderr for immediate visibility
    fprintf(stderr, "[COSMOE:%s] %s\n", timestamp, buffer);
    fflush(stderr);
    
    // Send to OutputDebugString (visible in Wine with WINEDEBUG=+debugstr)
    char full_msg[1200];
    snprintf(full_msg, sizeof(full_msg), "[COSMOE:%s] %s", timestamp, buffer);
    OutputDebugStringA(full_msg);
}

void debug_log_error(const char* message) {
    if (!debug_initialized)
        debug_log_init();
    if (!debug_enabled)
        return;

    DWORD error = GetLastError();
    char error_msg[512];
    
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_msg,
        sizeof(error_msg),
        NULL
    );
    
    debug_log("%s - Error %lu: %s", message, error, error_msg);
}

#endif // _WIN32
