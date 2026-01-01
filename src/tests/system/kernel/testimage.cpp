//------------------------------------------------------------------------------
//	Copyright (c) 2004-2025, Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		testimage.cpp
//	Authors:		Bill Hayden (hayden@haydentech.com)
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#else
#include <windows.h>
// Windows equivalents for dlfcn.h
#define RTLD_LAZY 0
#define dlopen(name, flags) (void*)LoadLibraryA(name)
#define dlsym(handle, name) (void*)GetProcAddress((HMODULE)handle, name)
#define dlclose(handle) FreeLibrary((HMODULE)handle)
static inline const char* dlerror(void) { 
    static char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, buf, sizeof(buf), NULL);
    return buf;
}
#endif

#include <OS.h>
#include <image.h>

const char* get_image_type_name(int type) {
	switch (type) {
		case B_APP_IMAGE: return "APP";
		case B_LIBRARY_IMAGE: return "LIBRARY";
		default: return "UNKNOWN";
	}
}

const char* get_basename(const char* path) {
	const char* last_slash = strrchr(path, '/');
	return last_slash ? last_slash + 1 : path;
}

void test_get_image_info()
{
	printf("\n=== Testing _get_image_info ===\n");
	
	// Load a test library
	void* libm = dlopen("libm.so.6", RTLD_LAZY);
	if (!libm) {
		printf("FAIL: Could not load libm.so.6: %s\n", dlerror());
		return;
	}
	
	// Test 1: Get info using a function address (proper way)
	printf("\nTest 1: _get_image_info with function address\n");
	void* cos_addr = dlsym(libm, "cos");
	if (cos_addr) {
		image_info info;
		status_t result = _get_image_info((image_id)cos_addr, &info, sizeof(info));
		
		if (result == B_OK) {
			printf("  PASS: Got image info for libm\n");
			printf("    Name: %s\n", get_basename(info.name));
			printf("    Text: %p, size: %d KB\n", info.text, info.text_size / 1024);
			printf("    Data: %p, size: %d KB\n", info.data, info.data_size / 1024);
			
			// Verify the data looks reasonable
			if (info.text_size > 0 && info.text_size < 10 * 1024 * 1024) {
				printf("  PASS: Text size is reasonable\n");
			} else {
				printf("  FAIL: Text size looks wrong: %d\n", info.text_size);
			}
		} else {
			printf("  FAIL: _get_image_info returned %d\n", result);
		}
	}
	
	// Test 2: Error handling - NULL info pointer
	printf("\nTest 2: Error handling - NULL info pointer\n");
	status_t result = _get_image_info((image_id)cos_addr, NULL, sizeof(image_info));
	if (result == B_BAD_VALUE) {
		printf("  PASS: NULL info returns B_BAD_VALUE\n");
	} else {
		printf("  FAIL: Expected B_BAD_VALUE, got %d\n", result);
	}
	
	// Test 3: Error handling - NULL image_id
	printf("\nTest 3: Error handling - NULL image_id\n");
	image_info info;
	result = _get_image_info(NULL, &info, sizeof(info));
	if (result == B_BAD_IMAGE_ID) {
		printf("  PASS: NULL image_id returns B_BAD_IMAGE_ID\n");
	} else {
		printf("  FAIL: Expected B_BAD_IMAGE_ID, got %d\n", result);
	}
	
	// Test 4: Error handling - wrong size
	printf("\nTest 4: Error handling - wrong size\n");
	result = _get_image_info((image_id)cos_addr, &info, sizeof(info) - 1);
	if (result == B_BAD_VALUE) {
		printf("  PASS: Wrong size returns B_BAD_VALUE\n");
	} else {
		printf("  FAIL: Expected B_BAD_VALUE, got %d\n", result);
	}
	
	dlclose(libm);
}

void test_get_next_image_info()
{
	printf("\n=== Testing _get_next_image_info ===\n");
	
	// Load some libraries to make the test interesting
	printf("\nLoading test libraries...\n");
	void* libm = dlopen("libm.so.6", RTLD_LAZY);
	void* libpthread = dlopen("libpthread.so.0", RTLD_LAZY);
	void* libdl = dlopen("libdl.so.2", RTLD_LAZY);
	
	printf("\nTest 1: Enumerate all loaded images\n");
	printf("%-4s %-10s %-40s %12s %12s\n", "Seq", "Type", "Name", "Text Size", "Data Size");
	printf("================================================================================\n");
	
	int32 cookie = 0;
	image_info info;
	status_t result;
	int count = 0;
	bool found_app = false;
	bool found_library = false;
	
	while ((result = _get_next_image_info(B_CURRENT_TEAM, &cookie, &info, sizeof(info))) == B_OK) {
		printf("%-4d %-10s %-40s %10d KB %10d KB\n",
		       info.sequence,
		       get_image_type_name(info.type),
		       get_basename(info.name),
		       info.text_size / 1024,
		       info.data_size / 1024);
		
		if (info.type == B_APP_IMAGE) found_app = true;
		if (info.type == B_LIBRARY_IMAGE) found_library = true;
		
		count++;
		
		// Safety check - don't iterate forever
		if (count > 100) {
			printf("WARNING: stopped after 100 images\n");
			break;
		}
	}
	
	printf("================================================================================\n");
	printf("Total: %d images loaded\n", count);
	
	if (result == B_ENTRY_NOT_FOUND) {
		printf("  PASS: Iteration ended with B_ENTRY_NOT_FOUND\n");
	} else {
		printf("  FAIL: Iteration ended with unexpected status %d\n", result);
	}
	
	if (count > 0) {
		printf("  PASS: Found %d images\n", count);
	} else {
		printf("  FAIL: No images found\n");
	}
	
	if (found_app) {
		printf("  PASS: Found at least one B_APP_IMAGE\n");
	} else {
		printf("  FAIL: No B_APP_IMAGE found\n");
	}
	
	if (found_library) {
		printf("  PASS: Found at least one B_LIBRARY_IMAGE\n");
	} else {
		printf("  FAIL: No B_LIBRARY_IMAGE found\n");
	}
	
	// Test 2: Error handling - NULL cookie
	printf("\nTest 2: Error handling - NULL cookie\n");
	result = _get_next_image_info(B_CURRENT_TEAM, NULL, &info, sizeof(info));
	if (result == B_BAD_VALUE) {
		printf("  PASS: NULL cookie returns B_BAD_VALUE\n");
	} else {
		printf("  FAIL: Expected B_BAD_VALUE, got %d\n", result);
	}
	
	// Test 3: Error handling - NULL info
	printf("\nTest 3: Error handling - NULL info\n");
	cookie = 0;
	result = _get_next_image_info(B_CURRENT_TEAM, &cookie, NULL, sizeof(info));
	if (result == B_BAD_VALUE) {
		printf("  PASS: NULL info returns B_BAD_VALUE\n");
	} else {
		printf("  FAIL: Expected B_BAD_VALUE, got %d\n", result);
	}
	
	// Test 4: Error handling - invalid team
	printf("\nTest 4: Error handling - invalid team\n");
	cookie = 0;
	result = _get_next_image_info(99999, &cookie, &info, sizeof(info));
	if (result == B_BAD_TEAM_ID) {
		printf("  PASS: Invalid team returns B_BAD_TEAM_ID\n");
	} else {
		printf("  FAIL: Expected B_BAD_TEAM_ID, got %d\n", result);
	}
	
	// Test 5: Verify sequence numbers are sequential
	printf("\nTest 5: Verify sequence numbers are sequential\n");
	cookie = 0;
	int32 expected_seq = 0;
	bool sequences_valid = true;
	
	while ((result = _get_next_image_info(B_CURRENT_TEAM, &cookie, &info, sizeof(info))) == B_OK) {
		if (info.sequence != expected_seq) {
			printf("  FAIL: Expected sequence %d, got %d\n", expected_seq, info.sequence);
			sequences_valid = false;
			break;
		}
		expected_seq++;
		
		if (expected_seq > 100) break;  // Safety
	}
	
	if (sequences_valid) {
		printf("  PASS: All sequence numbers are sequential\n");
	}
	
	// Cleanup
	if (libm) dlclose(libm);
	if (libpthread) dlclose(libpthread);
	if (libdl) dlclose(libdl);
}

int main()
{
	printf("==============================================\n");
	printf("  Image API Tests\n");
	printf("==============================================\n");
	
	test_get_image_info();
	test_get_next_image_info();
	
	printf("\n==============================================\n");
	printf("  All Image Tests Complete\n");
	printf("==============================================\n");
	
	return 0;
}
