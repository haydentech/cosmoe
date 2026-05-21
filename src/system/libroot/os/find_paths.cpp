/*
 * Copyright 2015, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2013, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include <FindDirectory.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>

#include <fs_attr.h>

#include <AutoDeleter.h>
#include <StringList.h>
#include <Path.h>
#include <syscalls.h>



static bool
is_user_directory(directory_which which)
{
	return which >= B_USER_DIRECTORY && which < B_APPS_DIRECTORY;
}


static bool
is_system_directory(directory_which which)
{
	return which >= B_SYSTEM_DIRECTORY && which < B_USER_DIRECTORY;
}


static status_t
add_resolved_path(directory_which which, const char* subPath, uint32 flags,
	BStringList& paths)
{
	if ((flags & B_FIND_PATHS_SYSTEM_ONLY) != 0 && is_user_directory(which))
		return B_OK;
	if ((flags & B_FIND_PATHS_USER_ONLY) != 0 && is_system_directory(which))
		return B_OK;

	BPath path;
	status_t error = find_directory(which, &path,
		(flags & B_FIND_PATH_CREATE_DIRECTORY) != 0);
	if (error != B_OK)
		return error;

	if (subPath != NULL && subPath[0] != '\0') {
		error = path.Append(subPath);
		if (error != B_OK)
			return error;
	}

	if ((flags & B_FIND_PATH_EXISTING_ONLY) != 0) {
		BEntry entry(path.Path());
		if (entry.InitCheck() != B_OK || !entry.Exists())
			return B_ENTRY_NOT_FOUND;
	}

	if (!paths.HasString(path.Path()) && !paths.Add(path.Path()))
		return B_NO_MEMORY;

	return B_OK;
}


static status_t
add_resolved_paths(const directory_which* directories, size_t count,
	const char* subPath, uint32 flags, BStringList& paths)
{
	status_t error = B_ENTRY_NOT_FOUND;
	bool addedPath = false;

	for (size_t i = 0; i < count; i++) {
		status_t result = add_resolved_path(directories[i], subPath, flags,
			paths);
		if (result == B_OK) {
			addedPath = true;
			continue;
		}

		if (error == B_ENTRY_NOT_FOUND)
			error = result;
	}

	return addedPath ? B_OK : error;
}


status_t
find_paths_list_etc(const char* architecture,
	path_base_directory baseDirectory, const char* subPath,
	uint32 flags, BStringList& paths)
{
	(void)architecture;

	switch (baseDirectory) {
		case B_FIND_PATH_ADD_ONS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_ADDONS_DIRECTORY,
				B_USER_ADDONS_DIRECTORY,
				B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY,
				B_SYSTEM_ADDONS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_APPS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_APPS_DIRECTORY,
				B_SYSTEM_APPS_DIRECTORY,
				B_APPS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_BIN_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_BIN_DIRECTORY,
				B_SYSTEM_NONPACKAGED_BIN_DIRECTORY,
				B_SYSTEM_BIN_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_BOOT_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_BOOT_DIRECTORY,
				B_SYSTEM_BOOT_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_CACHE_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_CACHE_DIRECTORY,
				B_SYSTEM_CACHE_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_DATA_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_DATA_DIRECTORY,
				B_USER_DATA_DIRECTORY,
				B_SYSTEM_NONPACKAGED_DATA_DIRECTORY,
				B_SYSTEM_DATA_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_DEVELOP_DIRECTORY:
		{
			const directory_which directories[] = {
				// B_USER_NONPACKAGED_DEVELOP_DIRECTORY,
				// B_USER_DEVELOP_DIRECTORY,
				// B_SYSTEM_NONPACKAGED_DEVELOP_DIRECTORY,
				B_SYSTEM_DEVELOP_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_DOCUMENTATION_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_DOCUMENTATION_DIRECTORY,
				B_SYSTEM_NONPACKAGED_DOCUMENTATION_DIRECTORY,
				B_SYSTEM_DOCUMENTATION_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_ETC_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_ETC_DIRECTORY,
				B_SYSTEM_ETC_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_FONTS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_FONTS_DIRECTORY,
				B_USER_FONTS_DIRECTORY,
				B_SYSTEM_NONPACKAGED_FONTS_DIRECTORY,
				B_SYSTEM_FONTS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_HEADERS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_HEADERS_DIRECTORY,
				B_USER_HEADERS_DIRECTORY,
				B_SYSTEM_NONPACKAGED_HEADERS_DIRECTORY,
				B_SYSTEM_HEADERS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_LIB_DIRECTORY:
		case B_FIND_PATH_DEVELOP_LIB_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_LIB_DIRECTORY,
				B_USER_LIB_DIRECTORY,
				B_SYSTEM_NONPACKAGED_LIB_DIRECTORY,
				B_SYSTEM_LIB_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_LOG_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_LOG_DIRECTORY,
				B_SYSTEM_LOG_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_MEDIA_NODES_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_MEDIA_NODES_DIRECTORY,
				B_USER_MEDIA_NODES_DIRECTORY,
				B_SYSTEM_NONPACKAGED_MEDIA_NODES_DIRECTORY,
				B_SYSTEM_MEDIA_NODES_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_PACKAGES_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_PACKAGES_DIRECTORY,
				B_SYSTEM_PACKAGES_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_PREFERENCES_DIRECTORY:
		case B_FIND_PATH_SETTINGS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_SETTINGS_DIRECTORY,
				B_SYSTEM_SETTINGS_DIRECTORY,
				B_PREFERENCES_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_SERVERS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_SERVERS_DIRECTORY,
				B_SYSTEM_SERVERS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_SOUNDS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_SOUNDS_DIRECTORY,
				B_USER_SOUNDS_DIRECTORY,
				B_SYSTEM_NONPACKAGED_SOUNDS_DIRECTORY,
				B_SYSTEM_SOUNDS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_SPOOL_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_SPOOL_DIRECTORY,
				B_SYSTEM_SPOOL_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_TRANSLATORS_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_NONPACKAGED_TRANSLATORS_DIRECTORY,
				B_USER_TRANSLATORS_DIRECTORY,
				B_SYSTEM_NONPACKAGED_TRANSLATORS_DIRECTORY,
				B_SYSTEM_TRANSLATORS_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		case B_FIND_PATH_VAR_DIRECTORY:
		{
			const directory_which directories[] = {
				B_USER_VAR_DIRECTORY,
				B_SYSTEM_VAR_DIRECTORY,
			};
			return add_resolved_paths(directories,
				sizeof(directories) / sizeof(directories[0]), subPath, flags,
				paths);
		}

		default:
			return B_BAD_VALUE;
	}
}


static status_t
copy_string_list_to_array(const BStringList& paths, char*** _paths,
	size_t* _pathCount)
{
	if (_paths == NULL || _pathCount == NULL)
		return B_BAD_VALUE;

	*_paths = NULL;
	*_pathCount = 0;

	int32 count = paths.CountStrings();
	if (count <= 0)
		return B_OK;

	size_t bytes = count * sizeof(char*);
	for (int32 i = 0; i < count; i++)
		bytes += paths.StringAt(i).Length() + 1;

	char** result = (char**)malloc(bytes);
	if (result == NULL)
		return B_NO_MEMORY;

	char* stringData = (char*)(result + count);
	for (int32 i = 0; i < count; i++) {
		BString path = paths.StringAt(i);
		result[i] = stringData;
		size_t length = path.Length() + 1;
		memcpy(stringData, path.String(), length);
		stringData += length;
	}

	*_paths = result;
	*_pathCount = count;
	return B_OK;
}


static status_t
copy_first_path_to_buffer(const BStringList& paths, char* pathBuffer,
	size_t bufferSize)
{
	if (pathBuffer == NULL || bufferSize == 0)
		return B_BAD_VALUE;

	if (paths.IsEmpty())
		return B_ENTRY_NOT_FOUND;

	BString path = paths.StringAt(0);
	if ((size_t)path.Length() + 1 > bufferSize)
		return B_BUFFER_OVERFLOW;

	memcpy(pathBuffer, path.String(), path.Length() + 1);
	return B_OK;
}


status_t
find_path(const void* codePointer, path_base_directory baseDirectory,
	const char* subPath, char* pathBuffer, size_t bufferSize)
{
	return find_path_etc(codePointer, NULL, NULL, baseDirectory, subPath, 0,
		pathBuffer, bufferSize);
}


status_t
find_path_etc(const void* codePointer, const char* dependency,
	const char* architecture, path_base_directory baseDirectory,
	const char* subPath, uint32 flags, char* pathBuffer, size_t bufferSize)
{
	(void)codePointer;
	(void)dependency;

	BStringList paths;
	status_t error = find_paths_list_etc(architecture, baseDirectory, subPath,
		flags, paths);
	if (error != B_OK)
		return error;

	return copy_first_path_to_buffer(paths, pathBuffer, bufferSize);
}


status_t
find_path_for_path(const char* path, path_base_directory baseDirectory,
	const char* subPath, char* pathBuffer, size_t bufferSize)
{
	return find_path_for_path_etc(path, NULL, NULL, baseDirectory, subPath, 0,
		pathBuffer, bufferSize);
}


status_t
find_path_for_path_etc(const char* path, const char* dependency,
	const char* architecture, path_base_directory baseDirectory,
	const char* subPath, uint32 flags, char* pathBuffer, size_t bufferSize)
{
	(void)path;
	(void)dependency;
	return find_path_etc(NULL, NULL, architecture, baseDirectory, subPath,
		flags, pathBuffer, bufferSize);
}


status_t
find_paths(path_base_directory baseDirectory, const char* subPath,
	char*** _paths, size_t* _pathCount)
{
	return find_paths_etc(NULL, baseDirectory, subPath, 0, _paths,
		_pathCount);
}


status_t
find_paths_etc(const char* architecture, path_base_directory baseDirectory,
	const char* subPath, uint32 flags, char*** _paths, size_t* _pathCount)
{
	BStringList paths;
	status_t error = find_paths_list_etc(architecture, baseDirectory, subPath,
		flags, paths);
	if (error != B_OK) {
		if (_paths != NULL)
			*_paths = NULL;
		if (_pathCount != NULL)
			*_pathCount = 0;
		return error;
	}

	return copy_string_list_to_array(paths, _paths, _pathCount);
}

