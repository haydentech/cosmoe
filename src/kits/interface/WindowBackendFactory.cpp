/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 */

#include "WindowBackend.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace BPrivate {

WindowBackendFactory* WindowBackendFactory::sInstance = NULL;

// Backend plugin entry point function type
typedef WindowBackend* (*backend_create_func_t)();


WindowBackendFactory::WindowBackendFactory()
	:
	fCurrentBackend(NULL),
	fPreferredType(BACKEND_AUTO),
	fBackendLibHandle(NULL)
{
}


WindowBackendFactory::~WindowBackendFactory()
{
	ReleaseBackend();
}


WindowBackendFactory*
WindowBackendFactory::Instance()
{
	if (sInstance == NULL)
		sInstance = new WindowBackendFactory();
	return sInstance;
}


void
WindowBackendFactory::SetPreferredBackend(backend_type type)
{
	fPreferredType = type;
	
	// If we already have a backend loaded and it's different, release it
	if (fCurrentBackend != NULL && fCurrentBackend->GetType() != type) {
		ReleaseBackend();
	}
}


backend_type
WindowBackendFactory::DetectBackend()
{
	// Check environment variable first
	const char* backendEnv = getenv("COSMOE_BACKEND");
	if (backendEnv != NULL) {
		if (strcasecmp(backendEnv, "wayland") == 0)
			return BACKEND_WAYLAND;
		else if (strcasecmp(backendEnv, "x11") == 0)
			return BACKEND_X11;
	}

	// Auto-detect based on environment
	// Try Wayland first (modern default)
	if (getenv("WAYLAND_DISPLAY") != NULL) {
		printf("WindowBackendFactory: Detected Wayland environment\n");
		return BACKEND_WAYLAND;
	}

	// Fall back to X11
	if (getenv("DISPLAY") != NULL) {
		printf("WindowBackendFactory: Detected X11 environment\n");
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
	printf("WindowBackendFactory: No windowing environment detected, defaulting to Wayland\n");
	return BACKEND_WAYLAND;
}


bool
WindowBackendFactory::IsBackendAvailable(backend_type type)
{
	const char* libName = NULL;

	switch (type) {
		case BACKEND_WAYLAND:
			libName = "libcosmoe-wayland.so";
			break;
		case BACKEND_X11:
			libName = "libcosmoe-x11.so";
			break;
		default:
			return false;
	}

	// Try to load the library temporarily to see if it exists
	void* handle = dlopen(libName, RTLD_LAZY | RTLD_LOCAL);
	if (handle != NULL) {
		dlclose(handle);
		return true;
	}

	return false;
}


WindowBackend*
WindowBackendFactory::LoadBackend(backend_type type)
{
	const char* libName = NULL;
	const char* createFuncName = "CreateWindowBackend";

	switch (type) {
		case BACKEND_WAYLAND:
			libName = "libcosmoe-wayland.so";
			break;
		case BACKEND_X11:
			libName = "libcosmoe-x11.so";
			break;
		default:
			fprintf(stderr, "WindowBackendFactory: Unknown backend type %d\n", type);
			return NULL;
	}

	// Load the backend library
	fBackendLibHandle = dlopen(libName, RTLD_NOW | RTLD_LOCAL);
	if (fBackendLibHandle == NULL) {
		fprintf(stderr, "WindowBackendFactory: Failed to load %s: %s\n",
			libName, dlerror());
		return NULL;
	}

	// Get the creation function
	backend_create_func_t createFunc = (backend_create_func_t)
		dlsym(fBackendLibHandle, createFuncName);
	
	if (createFunc == NULL) {
		fprintf(stderr, "WindowBackendFactory: Failed to find %s in %s: %s\n",
			createFuncName, libName, dlerror());
		dlclose(fBackendLibHandle);
		fBackendLibHandle = NULL;
		return NULL;
	}

	// Create the backend instance
	WindowBackend* backend = createFunc();
	if (backend == NULL) {
		fprintf(stderr, "WindowBackendFactory: Backend creation function returned NULL\n");
		dlclose(fBackendLibHandle);
		fBackendLibHandle = NULL;
		return NULL;
	}

	printf("WindowBackendFactory: Successfully loaded %s backend\n", backend->GetName());
	return backend;
}


WindowBackend*
WindowBackendFactory::GetBackend(backend_type type)
{
	// If we already have a backend, return it
	if (fCurrentBackend != NULL) {
		// Check if the type matches what's requested
		if (type != BACKEND_AUTO && fCurrentBackend->GetType() != type) {
			fprintf(stderr, "WindowBackendFactory: Warning - backend type mismatch "
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
	fCurrentBackend = LoadBackend(targetType);
	if (fCurrentBackend != NULL)
		return fCurrentBackend;

	// If loading failed and we're auto-detecting, try alternatives
	if (type == BACKEND_AUTO) {
		fprintf(stderr, "WindowBackendFactory: Primary backend failed, trying alternatives\n");
		
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
	}

	fprintf(stderr, "WindowBackendFactory: Failed to load any backend\n");
	return NULL;
}


void
WindowBackendFactory::ReleaseBackend()
{
	if (fCurrentBackend != NULL) {
		delete fCurrentBackend;
		fCurrentBackend = NULL;
	}

	if (fBackendLibHandle != NULL) {
		dlclose(fBackendLibHandle);
		fBackendLibHandle = NULL;
	}
}

} // namespace BPrivate
