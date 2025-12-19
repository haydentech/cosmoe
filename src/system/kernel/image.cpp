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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <image.h>
#include <dlfcn.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>      // For _dyld_image_count(), _dyld_get_image_name(), etc.
#include <mach-o/loader.h>    // For Mach-O header structures
#include <mach/mach.h>         // For vm_region APIs (if needed for segment info)
#endif


extern thread_id _main_thread_for_team(team_id);

thread_id load_image(int32 argc, const char **argv, const char **envp)
{
	// The goal is to return the launched application's main thread id.
	// The current app does NOT die, as with a straight exec.

	int pid;
  
	if ((pid = fork()) < 0)
	{
		return B_ERROR;
	}
	else if (pid == 0)
	{
		// Fork succeeded, we're in the parent process, and pid holds the pid of the child
		// Wait up to 2.5 seconds for the child process to get set up in the thread table
		// before we give up.
		thread_id main;
		int tries = 50;

		while (tries > 0 && ((main = _main_thread_for_team(pid)) < 0))
		{
			snooze(50000);
			tries--;
		}
		return main;
	}
	else
	{
		// We're in the child process
		execvpe(argv[0], (char* const*)argv, (char* const*)envp);
	}

	return B_ERROR;
}


image_id load_add_on(const char* path)
{
	void* hdll = dlopen(path, RTLD_LAZY);

	if (!hdll)
		printf("load_add_on(): dlopen('%s', RTLD_LAZY) failed: %s\n", path, dlerror());

	return hdll;
}


status_t unload_add_on(image_id imageID)
{
	void* hdll = (void*)imageID;
	return dlclose(hdll) ? B_ERROR : B_OK;
}


status_t get_image_symbol(image_id imid, const char* name, int32 sclass, void** pptr)
{
	void* hdll;
	const char* err = NULL;

	// Check if this is a special marker for the main executable (low bit set)
	if ((uintptr_t)imid & 0x1) {
		// Main executable - use RTLD_DEFAULT to search global scope
		hdll = RTLD_DEFAULT;
	} else {
		hdll = (void*)imid;
	}

	*pptr = dlsym(hdll, name);
	err = dlerror();
	if (err)
	{
		printf("get_image_symbol(): dlsym('%s') failed: %s\n", name, err);
		return B_BAD_IMAGE_ID;
	}

	return B_OK;
}


status_t
_get_image_info(image_id image, image_info *info, size_t size)
{
	if (!info || size != sizeof(image_info))
		return B_BAD_VALUE;
	
	if (!image)
		return B_BAD_IMAGE_ID;
	
	// Try to use dladdr to get information about the image
	// Note: image_id from load_add_on is a dlopen handle, but dladdr
	// needs an address within the loaded library, not the handle itself
	Dl_info dl_info;
	bool dladdr_success = (dladdr(image, &dl_info) != 0);
	
#ifdef __linux__
	// Linux-specific implementation using /proc/self/maps
	// Parse /proc/self/maps to get memory region information
	FILE* maps = fopen("/proc/self/maps", "r");
	if (!maps)
		return B_ERROR;
	
	char line[1024];
	void* text_start = NULL;
	void* text_end = NULL;
	void* data_start = NULL;
	void* data_end = NULL;
	char image_path[512] = {0};
	bool found_image = false;
	
	// If dladdr worked, use its filename; otherwise try to match the handle address
	const char* target_path = (dladdr_success && dl_info.dli_fname) ? dl_info.dli_fname : NULL;
	
	// Find memory regions for this image
	while (fgets(line, sizeof(line), maps)) {
		unsigned long start, end;
		char perms[5];
		unsigned long offset;
		char path[512] = {0};
		
		// Parse the maps line: address perms offset dev inode pathname
		int matched = sscanf(line, "%lx-%lx %4s %lx %*s %*s %511[^\n]",
		                     &start, &end, perms, &offset, path);
		
		if (matched >= 4) {
			// Check if this line is for our image
			bool is_our_image = false;
			
			if (target_path && matched == 5) {
				// We have a filename from dladdr, match it
				is_our_image = (strstr(path, target_path) != NULL);
			} else if (matched == 5) {
				// No dladdr info, check if the handle address falls in this range
				unsigned long handle_addr = (unsigned long)image;
				if (handle_addr >= start && handle_addr < end) {
					is_our_image = true;
					target_path = path;  // Remember this path for subsequent lines
				}
			}
			
			if (is_our_image) {
				found_image = true;
				
				// Save the image path from the first match
				if (image_path[0] == '\0' && matched == 5) {
					strncpy(image_path, path, sizeof(image_path) - 1);
				}
				
				// Executable segment (r-xp)
				if (perms[0] == 'r' && perms[2] == 'x') {
					if (!text_start || (void*)start < text_start) {
						text_start = (void*)start;
					}
					if (!text_end || (void*)end > text_end) {
						text_end = (void*)end;
					}
				}
				// Data segment (rw-p or r--p with data)
				else if (perms[0] == 'r' && (perms[1] == 'w' || offset > 0)) {
					if (!data_start || (void*)start < data_start) {
						data_start = (void*)start;
					}
					if (!data_end || (void*)end > data_end) {
						data_end = (void*)end;
					}
				}
			}
		}
	}
	
	fclose(maps);
	
	if (!found_image)
		return B_BAD_IMAGE_ID;
	
#else  // macOS and other platforms
	// macOS implementation: Use dladdr and parse Mach-O headers for detailed segment info
	if (!dladdr_success)
		return B_BAD_IMAGE_ID;
	
	void* text_start = NULL;
	void* text_end = NULL;
	void* data_start = NULL;
	void* data_end = NULL;
	char image_path[512] = {0};
	
	if (dl_info.dli_fname) {
		strncpy(image_path, dl_info.dli_fname, sizeof(image_path) - 1);
	}
	
#ifdef __APPLE__
	// Parse Mach-O header to get accurate segment information
	const struct mach_header* header = (const struct mach_header*)dl_info.dli_fbase;
	
	if (header) {
		// Get the slide (ASLR offset)
		intptr_t slide = 0;
		
		// Try to find this image in the dyld image list to get the slide
		uint32_t image_count = _dyld_image_count();
		for (uint32_t i = 0; i < image_count; i++) {
			if (_dyld_get_image_header(i) == (const struct mach_header*)header) {
				slide = _dyld_get_image_vmaddr_slide(i);
				break;
			}
		}
		
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
			// 32-bit Mach-O
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
	}
	
	// Fallback if parsing failed
	if (!text_start) {
		text_start = (void*)dl_info.dli_fbase;
	}
#else
	// For non-Apple platforms, use basic dladdr information
	text_start = (void*)dl_info.dli_fbase;
#endif
#endif
	
	// Fill in the image_info structure
	info->id = image;
	info->type = B_LIBRARY_IMAGE; // Assume library for loaded images
	info->sequence = 0;
	info->init_order = 0;
	info->init_routine = NULL;
	info->term_routine = NULL;
	info->device = 0;
	info->node = 0;
	
	if (image_path[0] != '\0') {
		strncpy(info->name, image_path, MAXPATHLEN - 1);
		info->name[MAXPATHLEN - 1] = '\0';
	} else if (dladdr_success && dl_info.dli_fname) {
		strncpy(info->name, dl_info.dli_fname, MAXPATHLEN - 1);
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
				
				// Try to get a dlopen handle for this library path
				void* handle = NULL;
				
				if (is_main_executable) {
					// For the main executable, we can't use dlopen.
					// We'll need to use the executable's own symbols via a workaround.
					// Try using NULL (RTLD_DEFAULT) which searches global scope
					handle = NULL;  // Will be handled specially in get_image_symbol
				} else {
					// First try RTLD_NOLOAD to get existing handle
					handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
					
					if (!handle) {
						// RTLD_NOLOAD failed, try using dladdr to find the right path
						Dl_info dl_info;
						if (dladdr(text_start, &dl_info) != 0 && dl_info.dli_fname) {
							handle = dlopen(dl_info.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
						}
					}
				}
				
				// Fill in the image_info structure
				// For main executable, use a special marker (text_start with low bit set)
				// so get_image_symbol knows to use RTLD_DEFAULT
				if (is_main_executable) {
					info->id = (image_id)((uintptr_t)text_start | 0x1);
				} else {
					info->id = handle ? (image_id)handle : (image_id)text_start;
				}
				
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
	
	// Try to get a dlopen handle for this library
	void* handle = NULL;
	if (is_main_executable) {
		// For main executable, use special marker (base address with low bit set)
		handle = (void*)((uintptr_t)base_address | 0x1);
	} else {
		// Try to get handle with RTLD_NOLOAD (don't load if not already loaded)
		handle = dlopen(image_name, RTLD_LAZY | RTLD_NOLOAD);
		if (!handle) {
			// Fallback to base address if we can't get handle
			handle = base_address;
		}
	}
	
	// Fill in the image_info structure
	info->id = (image_id)handle;
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
