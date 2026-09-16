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
//	File Name:		image.cpp
//	Authors:		Bill Hayden (hayden@haydentech.com)
//------------------------------------------------------------------------------

// On macOS, avoid thread_info collision with mach headers
#ifdef __APPLE__
#define COSMOE_NO_THREAD_INFO
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mutex>
#include <string>
#include <unordered_map>

// Platform-specific system headers
#ifdef _WIN32
	#include <process.h>  // For getpid() on Windows
#else
	#include <unistd.h>   // For getpid() on POSIX systems
#endif

#include <image.h>

// Platform-specific library loading
#ifdef _WIN32
	#include <windows.h>
	#include <psapi.h>     // For EnumProcessModules
	#include <tlhelp32.h>  // For CreateToolhelp32Snapshot
	#define IMG_HANDLE HMODULE
	#define IMG_OPEN(path, flags) LoadLibraryA(path)
	#define IMG_CLOSE(handle) (FreeLibrary((HMODULE)handle) ? 0 : -1)
	#define IMG_SYMBOL(handle, name) GetProcAddress((HMODULE)handle, name)
	#define IMG_ERROR() "Windows LoadLibrary error"
	#define IMG_DEFAULT ((HMODULE)NULL)  // NULL handle searches loaded modules
	#define IMG_RTLD_LAZY 0
	#define IMG_RTLD_NOLOAD 0
#else
	#include <dlfcn.h>
	#ifdef __linux__
		#include <link.h>
	#endif
	#define IMG_HANDLE void*
	#define IMG_OPEN(path, flags) dlopen(path, flags)
	#define IMG_CLOSE(handle) dlclose(handle)
	#define IMG_SYMBOL(handle, name) dlsym(handle, name)
	#define IMG_ERROR() dlerror()
	#define IMG_DEFAULT RTLD_DEFAULT
	#define IMG_RTLD_LAZY RTLD_LAZY
	#define IMG_RTLD_NOLOAD RTLD_NOLOAD
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>      // For _dyld_image_count(), _dyld_get_image_name(), etc.
#include <mach-o/loader.h>    // For Mach-O header structures
#include <mach/mach.h>         // For vm_region APIs (if needed for segment info)
#endif


extern thread_id _main_thread_for_team(team_id);


namespace {

struct ImageRecord {
	IMG_HANDLE	handle;
	std::string	path;
	void*		baseAddress;
	bool		ownsHandle;
	bool		isMainExecutable;
};

struct ImageRegistry {
	std::mutex						lock;
	std::unordered_map<image_id, ImageRecord>	images;
	image_id						nextImageID = 1;
};


ImageRegistry&
image_registry()
{
	// Image APIs can be called from another translation unit's static
	// initializer. A function-local static avoids initialization-order issues.
	static ImageRegistry registry;
	return registry;
}


image_id
register_image(IMG_HANDLE handle, const char* path, void* baseAddress,
	bool ownsHandle, bool isMainExecutable)
{
	ImageRegistry& registry = image_registry();
	std::lock_guard<std::mutex> locker(registry.lock);

	// Do not reuse IDs: an ID retained after unload must never refer to a
	// subsequently loaded image.
	if (registry.nextImageID <= 0 || registry.nextImageID == INT32_MAX)
		return B_ERROR;

	const image_id id = registry.nextImageID++;
	registry.images.emplace(id, ImageRecord { handle, path != NULL ? path : "",
		baseAddress, ownsHandle, isMainExecutable });
	return id;
}


image_id
register_discovered_image(IMG_HANDLE handle, const char* path, void* baseAddress,
	bool isMainExecutable)
{
	ImageRegistry& registry = image_registry();
	std::lock_guard<std::mutex> locker(registry.lock);
	for (const auto& entry : registry.images) {
		const ImageRecord& image = entry.second;
		if (image.isMainExecutable == isMainExecutable
			&& image.baseAddress == baseAddress && image.path == path) {
			return entry.first;
		}
	}

	if (registry.nextImageID <= 0 || registry.nextImageID == INT32_MAX)
		return B_ERROR;

	const image_id id = registry.nextImageID++;
	registry.images.emplace(id, ImageRecord { handle, path != NULL ? path : "",
		baseAddress, false, isMainExecutable });
	return id;
}


bool
get_image_record(image_id id, ImageRecord& record)
{
	ImageRegistry& registry = image_registry();
	std::lock_guard<std::mutex> locker(registry.lock);
	auto found = registry.images.find(id);
	if (found == registry.images.end())
		return false;
	record = found->second;
	return true;
}

} // namespace

thread_id load_image(int32 argc, const char **argv, const char **envp)
{
	// The goal is to return the launched application's main thread id.
	// The current app does NOT die, as with a straight exec.

	int pid;

#ifdef _WIN32
	/* On Windows, use spawn to create the child process without waiting.
	 * _spawnve with _P_NOWAIT returns the process id on success. We then
	 * poll the thread table similarly to POSIX above.
	 */
	pid = _spawnve(_P_NOWAIT, argv[0], (char* const*)argv, (char* const*)envp);
	if (pid < 0)
		return B_ERROR;

	thread_id main;
	int tries = 50;

	while (tries > 0 && ((main = _main_thread_for_team(pid)) < 0)) {
		snooze(50000);
		tries--;
	}

	return main;
#else
	if ((pid = fork()) < 0)
	{
		return B_ERROR;
	}
	else if (pid == 0)
	{
		/* Child: exec the new process. */
#ifdef __APPLE__
		execve(argv[0], (char* const*)argv, (char* const*)envp);
#else
		execvpe(argv[0], (char* const*)argv, (char* const*)envp);
#endif
		/* If exec fails */
		return B_ERROR;
	}
	else
	{
		/* Parent: wait for main thread registration */
		thread_id main;
		int tries = 50;

		while (tries > 0 && ((main = _main_thread_for_team(pid)) < 0))
		{
			snooze(50000);
			tries--;
		}
		return main;
	}
#endif
}


image_id load_add_on(const char* path)
{
	if (path == NULL)
		return B_BAD_VALUE;

	IMG_HANDLE hdll = IMG_OPEN(path, IMG_RTLD_LAZY);

	if (!hdll)
		fprintf(stderr, "load_add_on(): Failed to load '%s': %s\n", path, IMG_ERROR());

	if (!hdll)
		return B_BAD_IMAGE_ID;

	image_id id = register_image(hdll, path, NULL, true, false);
	if (id < 0) {
		IMG_CLOSE(hdll);
		return id;
	}
	return id;
}


status_t unload_add_on(image_id imageID)
{
	ImageRegistry& registry = image_registry();
	std::lock_guard<std::mutex> locker(registry.lock);
	auto found = registry.images.find(imageID);
	if (found == registry.images.end() || !found->second.ownsHandle)
		return B_BAD_IMAGE_ID;

	if (IMG_CLOSE(found->second.handle) != 0)
		return B_ERROR;

	registry.images.erase(found);
	return B_OK;
}


status_t get_image_symbol(image_id imid, const char* name, int32 sclass, void** pptr)
{
	if (pptr == NULL || name == NULL)
		return B_BAD_VALUE;

	const char* err = NULL;
	const bool verboseLookupErrors
		= getenv("COSMOE_DEBUG_SYMBOL_LOOKUP") != NULL;
	(void)sclass;

	ImageRegistry& registry = image_registry();
	std::lock_guard<std::mutex> locker(registry.lock);
	auto found = registry.images.find(imid);
	if (found == registry.images.end())
		return B_BAD_IMAGE_ID;

	const ImageRecord& image = found->second;
	IMG_HANDLE hdll = image.handle;
	bool closeAfterLookup = false;

#ifndef _WIN32
	if (image.isMainExecutable) {
		hdll = IMG_DEFAULT;
	} else if (!image.ownsHandle) {
		hdll = IMG_OPEN(image.path.c_str(), IMG_RTLD_LAZY | IMG_RTLD_NOLOAD);
		if (hdll == NULL)
			return B_BAD_IMAGE_ID;
		closeAfterLookup = true;
	}
#endif

	// POSIX specifies that dlerror() must be cleared before dlsym().
#ifndef _WIN32
	IMG_ERROR();
#endif
	*pptr = (void*)IMG_SYMBOL(hdll, name);
#ifdef _WIN32
	if (*pptr == NULL) {
		if (verboseLookupErrors) {
			fprintf(stderr,
				"get_image_symbol(): Failed to find symbol '%s' (error %lu)\n",
				name, GetLastError());
		}
		return B_BAD_IMAGE_ID;
	}

	if (*pptr != NULL && !image.isMainExecutable) {
		HMODULE symbolModule = NULL;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
				| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)*pptr, &symbolModule)
			|| symbolModule != hdll) {
			*pptr = NULL;
			if (verboseLookupErrors) {
				fprintf(stderr,
					"get_image_symbol(): Symbol '%s' resolved outside requested image\n",
					name);
			}
			return B_BAD_IMAGE_ID;
		}
	}
#else
	err = IMG_ERROR();
#endif
	if (closeAfterLookup)
		IMG_CLOSE(hdll);
	if (err)
	{
		if (verboseLookupErrors) {
			fprintf(stderr, "get_image_symbol(): Failed to find symbol '%s': %s\n",
				name, err);
		}
		return B_BAD_IMAGE_ID;
	}

	return B_OK;
}


status_t
_get_image_info(image_id image, image_info *info, size_t size)
{
	if (!info || size != sizeof(image_info))
		return B_BAD_VALUE;

	ImageRecord record;
	if (!get_image_record(image, record))
		return B_BAD_IMAGE_ID;

#ifdef _WIN32
	HMODULE hdll = (HMODULE)record.handle;
	if (!hdll)
		return B_BAD_IMAGE_ID;

	MODULEINFO modInfo = {0};
	if (!GetModuleInformation(GetCurrentProcess(), hdll, &modInfo, sizeof(modInfo))) {
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
				| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)record.baseAddress, &hdll)) {
			return B_BAD_IMAGE_ID;
		}

		if (!GetModuleInformation(GetCurrentProcess(), hdll, &modInfo,
				sizeof(modInfo))) {
			return B_BAD_IMAGE_ID;
		}
	}

	uint8* imageBase = (uint8*)modInfo.lpBaseOfDll;
	void* text_start = NULL;
	void* text_end = NULL;
	void* data_start = NULL;
	void* data_end = NULL;
	char image_path[512] = {0};
	GetModuleFileNameA(hdll, image_path, sizeof(image_path));
	char exe_path[512] = {0};
	bool is_main_executable = GetModuleFileNameA(NULL, exe_path,
		sizeof(exe_path)) > 0 && _stricmp(image_path, exe_path) == 0;

	IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)imageBase;
	if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
		IMAGE_NT_HEADERS* ntHeaders
			= (IMAGE_NT_HEADERS*)(imageBase + dosHeader->e_lfanew);
		if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
			IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
			for (unsigned i = 0; i < ntHeaders->FileHeader.NumberOfSections;
					i++, section++) {
				if (section->Misc.VirtualSize == 0)
					continue;

				uint8* sectionStart = imageBase + section->VirtualAddress;
				uint8* sectionEnd = sectionStart + section->Misc.VirtualSize;

				if (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
					if (!text_start || sectionStart < (uint8*)text_start)
						text_start = sectionStart;
					if (!text_end || sectionEnd > (uint8*)text_end)
						text_end = sectionEnd;
				}

				if (section->Characteristics & IMAGE_SCN_MEM_WRITE) {
					if (!data_start || sectionStart < (uint8*)data_start)
						data_start = sectionStart;
					if (!data_end || sectionEnd > (uint8*)data_end)
						data_end = sectionEnd;
				}
			}
		}
	}

	if (!text_start || !text_end) {
		text_start = modInfo.lpBaseOfDll;
		text_end = (void*)(imageBase + modInfo.SizeOfImage);
	}
	if (!data_start || !data_end) {
		data_start = text_start;
		data_end = text_end;
	}

#else
	void* address = record.baseAddress;
	if (address == NULL && record.isMainExecutable)
		address = (void*)&_get_image_info;

#ifdef __linux__
	if (address == NULL && record.handle != NULL) {
		struct link_map* linkMap = NULL;
		if (dlinfo(record.handle, RTLD_DI_LINKMAP, &linkMap) == 0
			&& linkMap != NULL) {
			address = (void*)linkMap->l_addr;
		}
	}
#endif
	if (address == NULL)
		return B_BAD_IMAGE_ID;

	Dl_info dl_info;
	if (dladdr(address, &dl_info) == 0)
		return B_BAD_IMAGE_ID;

	void* text_start = dl_info.dli_fbase;
	void* text_end = NULL;
	void* data_start = text_start;
	void* data_end = NULL;
	char image_path[512] = {0};
	if (dl_info.dli_fname)
		strncpy(image_path, dl_info.dli_fname, sizeof(image_path) - 1);
	bool is_main_executable = false;

#ifdef __linux__
	char maps_path[64];
	snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", getpid());
	FILE* maps = fopen(maps_path, "r");
	if (maps) {
		char line[1024];
		char resolved_path[512] = {0};
		uintptr_t image_base = (uintptr_t)dl_info.dli_fbase;

		while (fgets(line, sizeof(line), maps)) {
			unsigned long start, end;
			char perms[5];
			unsigned long offset;
			char path[512] = {0};

			int matched = sscanf(line, "%lx-%lx %4s %lx %*s %*s %511[^\n]",
				&start, &end, perms, &offset, path);
			if (matched == 5 && path[0] == '/'
					&& image_base >= start && image_base < end) {
				strncpy(resolved_path, path, sizeof(resolved_path) - 1);
				break;
			}
		}

		if (resolved_path[0] != '\0') {
			rewind(maps);
			text_start = NULL;
			text_end = NULL;
			data_start = NULL;
			data_end = NULL;

			while (fgets(line, sizeof(line), maps)) {
				unsigned long seg_start, seg_end;
				char seg_perms[5];
				unsigned long seg_offset;
				char seg_path[512] = {0};

				int matched = sscanf(line, "%lx-%lx %4s %lx %*s %*s %511[^\n]",
					&seg_start, &seg_end, seg_perms, &seg_offset, seg_path);
				if (matched != 5 || strcmp(seg_path, resolved_path) != 0)
					continue;

				if (seg_perms[0] == 'r' && seg_perms[2] == 'x') {
					if (!text_start || (void*)seg_start < text_start)
						text_start = (void*)seg_start;
					if (!text_end || (void*)seg_end > text_end)
						text_end = (void*)seg_end;
				} else if (seg_perms[0] == 'r'
					&& (seg_perms[1] == 'w' || seg_offset > 0)) {
					if (!data_start || (void*)seg_start < data_start)
						data_start = (void*)seg_start;
					if (!data_end || (void*)seg_end > data_end)
						data_end = (void*)seg_end;
				}
			}

			strncpy(image_path, resolved_path, sizeof(image_path) - 1);

			char exe_path[512];
			ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
			if (len != -1) {
				exe_path[len] = '\0';
				is_main_executable = strcmp(resolved_path, exe_path) == 0;
			}
		}

		fclose(maps);
	}
#endif
#endif

	/* Fill in the image_info structure with best-effort data */
	info->id = image;
	info->type = is_main_executable ? B_APP_IMAGE : B_LIBRARY_IMAGE;
	info->sequence = 0;
	info->init_order = 0;
	info->init_routine = NULL;
	info->term_routine = NULL;
	info->device = 0;
	info->node = 0;

	if (image_path[0] != '\0') {
		strncpy(info->name, image_path, MAXPATHLEN - 1);
		info->name[MAXPATHLEN - 1] = '\0';
	} else {
		info->name[0] = '\0';
	}

	info->text = text_start;
	info->data = data_start;
	info->text_size = text_end ? (int32)((char*)text_end - (char*)text_start) : 0;
	info->data_size = data_end ? (int32)((char*)data_end - (char*)data_start) : 0;

	return B_OK;
}


status_t
_get_next_image_info(team_id team, int32 *cookie, image_info *info, size_t size)
{
	if (!cookie || !info || size != sizeof(image_info))
		return B_BAD_VALUE;
	
#ifdef __linux__
	// Linux-specific implementation using /proc/<pid>/maps
	// Parse /proc/<pid>/maps to enumerate all loaded images
	char maps_path[64];
	snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", 
	         (team == B_CURRENT_TEAM) ? getpid() : team);
	
	FILE* maps = fopen(maps_path, "r");
	if (!maps)
		return B_BAD_TEAM_ID;
	
	char line[1024];
	int32 current_index = 0;
	char last_image_path[512] = {0};
	
	// Scan through /proc/maps looking for executable segments
	while (fgets(line, sizeof(line), maps)) {
		unsigned long start, end;
		char perms[5];
		unsigned long offset;
		char path[512] = {0};
		
		// Parse the maps line: address perms offset dev inode pathname
		int matched = sscanf(line, "%lx-%lx %4s %lx %*s %*s %511[^\n]",
		                     &start, &end, perms, &offset, path);
		
		// Look for executable segments with a pathname
		if (matched == 5 && perms[0] == 'r' && perms[2] == 'x' && path[0] == '/') {
			// Skip if this is the same image we just processed
			if (strcmp(path, last_image_path) == 0)
				continue;
			
			// Check if this is the image index we're looking for
			if (current_index == *cookie) {
				strncpy(last_image_path, path, sizeof(last_image_path) - 1);
				
				// Found the requested image - now gather all its segments
				fclose(maps);
				maps = fopen(maps_path, "r");
				if (!maps)
					return B_ERROR;
				
				void* text_start = NULL;
				void* text_end = NULL;
				void* data_start = NULL;
				void* data_end = NULL;
				bool found_segments = false;
				
				// Re-scan to find all segments for this image
				while (fgets(line, sizeof(line), maps)) {
					unsigned long seg_start, seg_end;
					char seg_perms[5];
					unsigned long seg_offset;
					char seg_path[512] = {0};
					
					matched = sscanf(line, "%lx-%lx %4s %lx %*s %*s %511[^\n]",
					                 &seg_start, &seg_end, seg_perms, &seg_offset, seg_path);
					
					if (matched == 5 && strcmp(seg_path, path) == 0) {
						found_segments = true;
						
						// Executable segment (r-xp)
						if (seg_perms[0] == 'r' && seg_perms[2] == 'x') {
							if (!text_start || (void*)seg_start < text_start)
								text_start = (void*)seg_start;
							if (!text_end || (void*)seg_end > text_end)
								text_end = (void*)seg_end;
						}
						// Data segment (rw-p or r--p)
						else if (seg_perms[0] == 'r' && (seg_perms[1] == 'w' || seg_offset > 0)) {
							if (!data_start || (void*)seg_start < data_start)
								data_start = (void*)seg_start;
							if (!data_end || (void*)seg_end > data_end)
								data_end = (void*)seg_end;
						}
					}
				}
				
				fclose(maps);
				
				if (!found_segments)
					return B_ERROR;
				
				// Determine if this is the app image or a library
				char exe_path[512];
				char exe_buffer[64];
				snprintf(exe_buffer, sizeof(exe_buffer), "/proc/%d/exe", 
				         (team == B_CURRENT_TEAM) ? getpid() : team);
				ssize_t len = readlink(exe_buffer, exe_path, sizeof(exe_path) - 1);
				bool is_main_executable = false;
				
				if (len != -1) {
					exe_path[len] = '\0';
					is_main_executable = (strcmp(path, exe_path) == 0);
				}
				
				// The table owns no reference for enumerated images. Acquiring an
				// RTLD_NOLOAD handle here would add a loader reference that callers
				// of get_next_image_info() have no obligation to release.
				info->id = register_discovered_image(NULL, path, text_start,
					is_main_executable);
				if (info->id < 0)
					return info->id;
				
				info->type = is_main_executable ? B_APP_IMAGE : B_LIBRARY_IMAGE;
				
				info->sequence = current_index;
				info->init_order = 0;
				info->init_routine = NULL;
				info->term_routine = NULL;
				info->device = 0;
				info->node = 0;
				
				strncpy(info->name, path, MAXPATHLEN - 1);
				info->name[MAXPATHLEN - 1] = '\0';
				
				info->text = text_start;
				info->data = data_start;
				info->text_size = text_end ? (int32)((char*)text_end - (char*)text_start) : 0;
				info->data_size = data_end ? (int32)((char*)data_end - (char*)data_start) : 0;
				
				// Increment cookie for next iteration
				*cookie += 1;
				
				return B_OK;
			}
			
			// Not the one we're looking for yet, keep counting
			strncpy(last_image_path, path, sizeof(last_image_path) - 1);
			current_index++;
		}
	}
	
	fclose(maps);
	
	// No more images
	return B_ENTRY_NOT_FOUND;
	
#elif defined(_WIN32)
	// Windows implementation: Use EnumProcessModules to enumerate loaded modules
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
	                               FALSE, 
	                               (team == B_CURRENT_TEAM) ? _getpid() : team);
	if (!hProcess)
		return B_BAD_TEAM_ID;
	
	// Get list of all modules
	HMODULE hModules[1024];
	DWORD cbNeeded;
	
	if (!EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
		CloseHandle(hProcess);
		return B_ERROR;
	}
	
	DWORD moduleCount = cbNeeded / sizeof(HMODULE);
	
	// Check if the requested index is valid
	if (*cookie < 0 || (DWORD)*cookie >= moduleCount) {
		CloseHandle(hProcess);
		return B_ENTRY_NOT_FOUND;
	}
	
	// Get the module at the requested index
	HMODULE hModule = hModules[*cookie];
	
	// Get module information
	MODULEINFO modInfo;
	if (!GetModuleInformation(hProcess, hModule, &modInfo, sizeof(modInfo))) {
		CloseHandle(hProcess);
		return B_ERROR;
	}
	
	// Get module filename
	char module_path[MAX_PATH];
	if (GetModuleFileNameExA(hProcess, hModule, module_path, sizeof(module_path)) == 0) {
		CloseHandle(hProcess);
		return B_ERROR;
	}
	
	// Determine if this is the main executable
	char exe_path[MAX_PATH];
	bool is_main_executable = false;
	if (GetModuleFileNameExA(hProcess, NULL, exe_path, sizeof(exe_path)) > 0) {
		is_main_executable = (_stricmp(module_path, exe_path) == 0);
	}
	
	// Windows PE format doesn't separate text/data as cleanly as ELF
	// Treat the entire module as a single region
	void* text_start = modInfo.lpBaseOfDll;
	void* text_end = (char*)modInfo.lpBaseOfDll + modInfo.SizeOfImage;
	void* data_start = text_start;
	void* data_end = text_end;
	
	// Fill in the image_info structure
	info->id = register_discovered_image(hModule, module_path, text_start,
		is_main_executable);
	if (info->id < 0) {
		CloseHandle(hProcess);
		return info->id;
	}
	info->type = is_main_executable ? B_APP_IMAGE : B_LIBRARY_IMAGE;
	info->sequence = *cookie;
	info->init_order = 0;
	info->init_routine = NULL;
	info->term_routine = NULL;
	info->device = 0;
	info->node = 0;
	
	strncpy(info->name, module_path, MAXPATHLEN - 1);
	info->name[MAXPATHLEN - 1] = '\0';
	
	info->text = text_start;
	info->data = data_start;
	info->text_size = (int32)((char*)text_end - (char*)text_start);
	info->data_size = (int32)((char*)data_end - (char*)data_start);
	
	CloseHandle(hProcess);
	
	// Increment cookie for next iteration
	*cookie += 1;
	
	return B_OK;
	
#else  // macOS and other platforms
	// macOS implementation: Use dyld APIs to enumerate loaded images
	
#ifdef __APPLE__
	// Get the total number of loaded images
	uint32_t image_count = _dyld_image_count();
	
	// Check if the requested index is valid
	if (*cookie < 0 || (uint32_t)*cookie >= image_count) {
		return B_ENTRY_NOT_FOUND;
	}
	
	// Get the image name and header for this index
	const char* image_name = _dyld_get_image_name(*cookie);
	const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(*cookie);
	
	if (!image_name || !header) {
		return B_ERROR;
	}
	
	// Determine image type: main executable is always at index 0
	bool is_main_executable = (*cookie == 0);
	
	// Get base address (slide + header address)
	intptr_t slide = _dyld_get_image_vmaddr_slide(*cookie);
	void* base_address = (void*)((uintptr_t)header + slide);
	
	// Parse Mach-O header to find segment information
	void* text_start = NULL;
	void* text_end = NULL;
	void* data_start = NULL;
	void* data_end = NULL;
	
	// Determine if this is 64-bit or 32-bit Mach-O
	bool is_64bit = (header->magic == MH_MAGIC_64 || header->magic == MH_CIGAM_64);
	
	if (is_64bit) {
		const struct mach_header_64* header64 = (const struct mach_header_64*)header;
		const struct load_command* cmd = (const struct load_command*)(header64 + 1);
		
		for (uint32_t i = 0; i < header64->ncmds; i++) {
			if (cmd->cmd == LC_SEGMENT_64) {
				const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
				
				// __TEXT segment (executable code)
				if (strcmp(seg->segname, "__TEXT") == 0) {
					text_start = (void*)(seg->vmaddr + slide);
					text_end = (void*)(seg->vmaddr + seg->vmsize + slide);
				}
				// __DATA segment (initialized data)
				else if (strcmp(seg->segname, "__DATA") == 0 || strcmp(seg->segname, "__DATA_CONST") == 0) {
					if (!data_start) {
						data_start = (void*)(seg->vmaddr + slide);
					}
					data_end = (void*)(seg->vmaddr + seg->vmsize + slide);
				}
			}
			cmd = (const struct load_command*)((char*)cmd + cmd->cmdsize);
		}
	} else {
		// 32-bit Mach-O (less common on modern macOS, but included for completeness)
		const struct load_command* cmd = (const struct load_command*)(header + 1);
		
		for (uint32_t i = 0; i < header->ncmds; i++) {
			if (cmd->cmd == LC_SEGMENT) {
				const struct segment_command* seg = (const struct segment_command*)cmd;
				
				// __TEXT segment (executable code)
				if (strcmp(seg->segname, "__TEXT") == 0) {
					text_start = (void*)(seg->vmaddr + slide);
					text_end = (void*)(seg->vmaddr + seg->vmsize + slide);
				}
				// __DATA segment (initialized data)
				else if (strcmp(seg->segname, "__DATA") == 0) {
					if (!data_start) {
						data_start = (void*)(seg->vmaddr + slide);
					}
					data_end = (void*)(seg->vmaddr + seg->vmsize + slide);
				}
			}
			cmd = (const struct load_command*)((char*)cmd + cmd->cmdsize);
		}
	}
	
	// Fill in the image_info structure
	info->id = register_discovered_image(NULL, image_name, base_address,
		is_main_executable);
	if (info->id < 0)
		return info->id;
	info->type = is_main_executable ? B_APP_IMAGE : B_LIBRARY_IMAGE;
	info->sequence = *cookie;
	info->init_order = 0;
	info->init_routine = NULL;
	info->term_routine = NULL;
	info->device = 0;
	info->node = 0;
	
	strncpy(info->name, image_name, MAXPATHLEN - 1);
	info->name[MAXPATHLEN - 1] = '\0';
	
	info->text = text_start;
	info->data = data_start;
	info->text_size = text_end && text_start ? (int32)((char*)text_end - (char*)text_start) : 0;
	info->data_size = data_end && data_start ? (int32)((char*)data_end - (char*)data_start) : 0;
	
	// Increment cookie for next iteration
	*cookie += 1;
	
	return B_OK;
	
#else
	// Other platforms: not supported
	return B_NOT_SUPPORTED;
#endif
#endif
}
