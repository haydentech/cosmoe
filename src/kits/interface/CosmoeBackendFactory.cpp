/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 */

#include "CosmoeBackend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific library loading
#ifdef _WIN32
	#include <windows.h>
	#define LIB_HANDLE HMODULE
	#define LIB_OPEN(name) LoadLibraryA(name)
	#define LIB_CLOSE(handle) FreeLibrary((HMODULE)handle)
	#define LIB_SYMBOL(handle, name) GetProcAddress((HMODULE)handle, name)
	#define LIB_ERROR() "Windows LoadLibrary error"
	#define LIB_EXT ".dll"
	
	// Helper function to show error messages on Windows GUI apps
	static void ShowWindowsError(const char* message) {
		MessageBoxA(NULL, message, "Cosmoe Backend Error", MB_OK | MB_ICONERROR);
		fprintf(stderr, "%s\n", message);
	}
	
	static void ShowWindowsErrorF(const char* format, ...) {
		char buffer[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		ShowWindowsError(buffer);
	}
#else
	#include <dlfcn.h>
	#define LIB_HANDLE void*
	#define LIB_OPEN(name) dlopen(name, RTLD_NOW | RTLD_LOCAL)
	#define LIB_CLOSE(handle) dlclose(handle)
	#define LIB_SYMBOL(handle, name) dlsym(handle, name)
	#define LIB_ERROR() dlerror()
	#ifdef __APPLE__
		#define LIB_EXT ".dylib"
	#else
		#define LIB_EXT ".so"
	#endif
#endif

namespace BPrivate {

CosmoeBackendFactory* CosmoeBackendFactory::sInstance = NULL;

// Backend plugin entry point function type
typedef CosmoeBackend* (*backend_create_func_t)();


CosmoeBackendFactory::CosmoeBackendFactory()
	:
	fCurrentBackend(NULL),
	fPreferredType(BACKEND_AUTO),
	fBackendLibHandle(NULL)
{
}


CosmoeBackendFactory::~CosmoeBackendFactory()
{
	ReleaseBackend();
}


CosmoeBackendFactory*
CosmoeBackendFactory::Instance()
{
	if (sInstance == NULL)
		sInstance = new CosmoeBackendFactory();
	return sInstance;
}


void
CosmoeBackendFactory::SetPreferredBackend(backend_type type)
{
	fPreferredType = type;
	
	// If we already have a backend loaded and it's different, release it
	if (fCurrentBackend != NULL && fCurrentBackend->GetType() != type) {
		ReleaseBackend();
	}
}


backend_type
CosmoeBackendFactory::DetectBackend()
{
	// Check environment variable first
	const char* backendEnv = getenv("COSMOE_BACKEND");
	if (backendEnv != NULL) {
		if (strcasecmp(backendEnv, "wayland") == 0)
			return BACKEND_WAYLAND;
		else if (strcasecmp(backendEnv, "x11") == 0)
			return BACKEND_X11;
		else if (strcasecmp(backendEnv, "cocoa") == 0)
			return BACKEND_COCOA;
		else if (strcasecmp(backendEnv, "windows") == 0 || strcasecmp(backendEnv, "win32") == 0)
			return BACKEND_WINDOWS;
	}

#ifdef _WIN32
	// On Windows, always use Windows backend
	return BACKEND_WINDOWS;
#elif defined(__APPLE__)
	// On macOS, always use Cocoa backend
	return BACKEND_COCOA;
#endif

	// Auto-detect based on environment (Linux)
	// Try Wayland first (modern default)
	if (getenv("WAYLAND_DISPLAY") != NULL) {
		printf("CosmoeBackendFactory: Detected Wayland environment\n");
		return BACKEND_WAYLAND;
	}

	// Fall back to X11
	if (getenv("DISPLAY") != NULL) {
		printf("CosmoeBackendFactory: Detected X11 environment\n");
		return BACKEND_X11;
	}

	// Check XDG_SESSION_TYPE as last resort
	const char* sessionType = getenv("XDG_SESSION_TYPE");
	if (sessionType != NULL) {
		if (strcmp(sessionType, "wayland") == 0)
			return BACKEND_WAYLAND;
		else if (strcmp(sessionType, "x11") == 0)
			return BACKEND_X11;
	}

	// Default to Wayland if nothing else works
	printf("CosmoeBackendFactory: No windowing environment detected, defaulting to Wayland\n");
	return BACKEND_WAYLAND;
}


bool
CosmoeBackendFactory::IsBackendAvailable(backend_type type)
{
	const char* libName = NULL;

	switch (type) {
		case BACKEND_WAYLAND:
			libName = "libcosmoe-wayland";
			break;
		case BACKEND_X11:
			libName = "libcosmoe-x11";
			break;
		case BACKEND_COCOA:
			libName = "libcosmoe-cocoa";
			break;
		case BACKEND_WINDOWS:
			libName = "libcosmoe-windows";
			break;
		default:
			return false;
	}

	// Build full library name with platform-specific extension
	char fullLibName[256];
	snprintf(fullLibName, sizeof(fullLibName), "%s%s", libName, LIB_EXT);

	// Try to load the library temporarily to see if it exists
	LIB_HANDLE handle = LIB_OPEN(fullLibName);
	if (handle != NULL) {
		LIB_CLOSE(handle);
		return true;
	}

	return false;
}


CosmoeBackend*
CosmoeBackendFactory::LoadBackend(backend_type type)
{
	const char* libName = NULL;
	const char* createFuncName = "CreateCosmoeBackend";

	switch (type) {
		case BACKEND_WAYLAND:
			libName = "libcosmoe-wayland";
			break;
		case BACKEND_X11:
			libName = "libcosmoe-x11";
			break;
		case BACKEND_COCOA:
			libName = "libcosmoe-cocoa";
			break;
		case BACKEND_WINDOWS:
			libName = "libcosmoe-windows";
			break;
		default:
			fprintf(stderr, "CosmoeBackendFactory: Unknown backend type %d\n", type);
			return NULL;
	}

	// Build full library name with platform-specific extension
	char fullLibName[256];
	snprintf(fullLibName, sizeof(fullLibName), "%s%s", libName, LIB_EXT);

	// Load the backend library
	fBackendLibHandle = LIB_OPEN(fullLibName);
	if (fBackendLibHandle == NULL) {
#ifdef _WIN32
		char errorMsg[512];
		snprintf(errorMsg, sizeof(errorMsg), 
			"Failed to load %s\n\nError code: %lu\n\nMake sure the DLL is in the same directory as the application.",
			fullLibName, GetLastError());
		ShowWindowsError(errorMsg);
#else
		fprintf(stderr, "CosmoeBackendFactory: Failed to load %s: %s\n",
			fullLibName, LIB_ERROR());
#endif
		return NULL;
	}

	// Get the creation function
	backend_create_func_t createFunc = (backend_create_func_t)
		LIB_SYMBOL(fBackendLibHandle, createFuncName);
	
	if (createFunc == NULL) {
#ifdef _WIN32
		char errorMsg[512];
		snprintf(errorMsg, sizeof(errorMsg), 
			"Failed to find function '%s' in %s\n\nThe DLL may be corrupted or incompatible.",
			createFuncName, fullLibName);
		ShowWindowsError(errorMsg);
#else
		fprintf(stderr, "CosmoeBackendFactory: Failed to find %s in %s: %s\n",
			createFuncName, libName, LIB_ERROR());
#endif
		LIB_CLOSE(fBackendLibHandle);
		fBackendLibHandle = NULL;
		return NULL;
	}

	// Create the backend instance
	CosmoeBackend* backend = createFunc();
	if (backend == NULL) {
#ifdef _WIN32
		ShowWindowsError("Backend creation function returned NULL\n\nThe backend failed to initialize.");
#else
		fprintf(stderr, "CosmoeBackendFactory: Backend creation function returned NULL\n");
#endif
		LIB_CLOSE(fBackendLibHandle);
		fBackendLibHandle = NULL;
		return NULL;
	}

	printf("CosmoeBackendFactory: Successfully loaded %s backend\n", backend->GetName());
	return backend;
}


CosmoeBackend*
CosmoeBackendFactory::GetBackend(backend_type type)
{
	// If we already have a backend, return it
	if (fCurrentBackend != NULL) {
		// Check if the type matches what's requested
		if (type != BACKEND_AUTO && fCurrentBackend->GetType() != type) {
			fprintf(stderr, "CosmoeBackendFactory: Warning - backend type mismatch "
				"(have %d, want %d)\n", fCurrentBackend->GetType(), type);
		}
		return fCurrentBackend;
	}

	// Determine which backend to load
	backend_type targetType = type;
	if (targetType == BACKEND_AUTO) {
		// Use preferred type if set, otherwise auto-detect
		targetType = (fPreferredType != BACKEND_AUTO) 
			? fPreferredType 
			: DetectBackend();
	}

	// Try to load the requested backend
	fprintf(stderr, "CosmoeBackendFactory: Attempting to load backend type %d\n", targetType);
	fCurrentBackend = LoadBackend(targetType);
	if (fCurrentBackend != NULL)
		return fCurrentBackend;

	// If loading failed and we're auto-detecting, try alternatives
	if (type == BACKEND_AUTO) {
		fprintf(stderr, "CosmoeBackendFactory: Primary backend (type %d) failed, trying alternatives\n", targetType);
		
#ifdef __APPLE__
		// On macOS, try Cocoa if we didn't already
		if (targetType != BACKEND_COCOA) {
			fprintf(stderr, "CosmoeBackendFactory: Trying Cocoa backend as fallback\n");
			fCurrentBackend = LoadBackend(BACKEND_COCOA);
			if (fCurrentBackend != NULL)
				return fCurrentBackend;
		} else {
			fprintf(stderr, "CosmoeBackendFactory: No alternatives on macOS (Cocoa already failed)\n");
		}
#else
		// Try Wayland if we didn't already
		if (targetType != BACKEND_WAYLAND) {
			fCurrentBackend = LoadBackend(BACKEND_WAYLAND);
			if (fCurrentBackend != NULL)
				return fCurrentBackend;
		}

		// Try X11 if we didn't already
		if (targetType != BACKEND_X11) {
			fCurrentBackend = LoadBackend(BACKEND_X11);
			if (fCurrentBackend != NULL)
				return fCurrentBackend;
		}
#endif
	}

	fprintf(stderr, "CosmoeBackendFactory: Failed to load any backend.\n");
#ifdef _WIN32
	ShowWindowsError("Failed to load any backend.\n\n"
		"Make sure libcosmoe-windows.dll and its dependencies are available.");
#elif defined(__APPLE__)
	fprintf(stderr, "  Hint: The Cocoa backend library may not be installed or have missing symbols.\n");
	fprintf(stderr, "  Try: sudo ninja -C builddir install\n");
	fprintf(stderr, "  Or set: DYLD_LIBRARY_PATH=builddir/src/kits:builddir/src/system/cocoa\n");
#endif
	return NULL;
}


void
CosmoeBackendFactory::ReleaseBackend()
{
	if (fCurrentBackend != NULL) {
		delete fCurrentBackend;
		fCurrentBackend = NULL;
	}

	if (fBackendLibHandle != NULL) {
		LIB_CLOSE(fBackendLibHandle);
		fBackendLibHandle = NULL;
	}
}

} // namespace BPrivate
