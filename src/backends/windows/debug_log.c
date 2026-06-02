/*
 * Debug logging for Windows backend
 */
#ifdef _WIN32

#include "debug_log.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE* debug_file = NULL;
static int debug_initialized = 0;
static char debug_log_path[MAX_PATH];

// Change this to turn on Windows/Wine debug logging
static bool debug_enabled = false;

static void
open_debug_log_file(void)
{
    DWORD path_length = GetCurrentDirectoryA(sizeof(debug_log_path), debug_log_path);
    if (path_length > 0 && path_length < sizeof(debug_log_path) - 1) {
        strcat(debug_log_path, "\\cosmoe_debug.log");
        debug_file = fopen(debug_log_path, "w");
        if (debug_file)
            return;
    }

    GetTempPathA(sizeof(debug_log_path), debug_log_path);
    strcat(debug_log_path, "cosmoe_debug.log");
    debug_file = fopen(debug_log_path, "w");
}

void debug_log_init(void) {
    if (debug_initialized)
        return;

    debug_initialized = 1;
    const char* env = getenv("COSMOE_DEBUG_LOG");
    if (env && env[0] != '\0' && strcmp(env, "0") != 0)
        debug_enabled = true;

    // Allow enabling logs for Explorer launches by dropping a marker file
    // named "cosmoe_debug.on" next to the executable (working directory).
    if (!debug_enabled) {
        DWORD attrs = GetFileAttributesA("cosmoe_debug.on");
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            debug_enabled = true;
    }

    if (!debug_enabled)
        return;
    
    // Prefer a log next to the process working directory for easy inspection.
    open_debug_log_file();
    if (debug_file) {
        fprintf(debug_file, "=== Cosmoe Windows Backend Debug Log ===\n");
        fprintf(debug_file, "Log file: %s\n", debug_log_path);
        fflush(debug_file);
        
        // Also show a message box with log location
        char msg[512];
        snprintf(msg, sizeof(msg), "Debug logging enabled.\nLog file: %s", debug_log_path);
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
