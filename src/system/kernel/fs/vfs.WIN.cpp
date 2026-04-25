/*
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 */
/*!
	\file kernel_interface.WIN.cpp
	Windows-specific implementation of kernel interface functions using Win32 APIs
*/

#include "kernel_interface.h"
#include "storage_support.h"

#include <OS.h>
#include <Entry.h>
#include <fs_info.h>
#include <os/add-ons/file_system/fsproto.h>

#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <storage_support.h>
#include <syscalls.h>

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

static status_t convertErrno(int result)
{
	status_t error;

	switch (result) {
		case EACCES:
			error = B_PERMISSION_DENIED;
			break;
		case EEXIST:
			error = B_FILE_EXISTS;
			break;
		case ENOENT:
			error = B_ENTRY_NOT_FOUND;
			break;
		default:
			error = B_ERROR;
			break;
	}

	return error;
}

static int
open_directory_fd(const char* path)
{
	if (path == NULL)
		return -1;

	char normalized[B_PATH_NAME_LENGTH];
	if (strlen(path) >= sizeof(normalized))
		return -1;

	strcpy(normalized, path);
	for (size_t i = 0; normalized[i] != '\0'; i++) {
		if (normalized[i] == '/')
			normalized[i] = '\\';
	}

	HANDLE hDirectory = CreateFileA(normalized,
		FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);

	if (hDirectory == INVALID_HANDLE_VALUE)
		return -1;

	int fd = _open_osfhandle((intptr_t)hDirectory, _O_RDONLY);
	if (fd == -1)
		CloseHandle(hDirectory);

	return fd;
}


static status_t
resolve_path_for_fd(int fd, const char* path, char* resolved, size_t resolvedSize)
{
	if (path == NULL || resolved == NULL)
		return B_BAD_VALUE;

	if (BPrivate::Storage::is_absolute_path(path) || fd < 0) {
		if (strlen(path) >= resolvedSize)
			return B_NAME_TOO_LONG;
		strcpy(resolved, path);
		return B_OK;
	}

	char basePath[B_PATH_NAME_LENGTH];
	status_t error = BPrivate::Storage::dir_to_path(fd, basePath, sizeof(basePath));
	if (error != B_OK)
		return error;

	size_t baseLen = strlen(basePath);
	bool needSlash = baseLen > 0 && basePath[baseLen - 1] != '/' && basePath[baseLen - 1] != '\\';
	size_t totalLen = baseLen + (needSlash ? 1 : 0) + strlen(path);
	if (totalLen >= resolvedSize)
		return B_NAME_TOO_LONG;

	strcpy(resolved, basePath);
	if (needSlash)
		strcat(resolved, "/");
	strcat(resolved, path);
	return B_OK;
}

//------------------------------------------------------------------------------
// File Functions
//------------------------------------------------------------------------------

status_t
_kern_open(int fd, const char *path, uint32 flags, uint32 creationFlags)
{
	int result;

	if (path == NULL) {
		if (fd < 0) {
			result = -1;
			return B_BAD_VALUE;
		}

		result = ::_dup(fd);
		return (result == -1) ? convertErrno(errno) : B_OK;
	}

	char resolvedPath[B_PATH_NAME_LENGTH];
	status_t error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
	if (error != B_OK) {
		result = -1;
		return error;
	}

	result = ::_open(resolvedPath, flags, creationFlags);
	return (result < 0) ? convertErrno(errno) : result;
}


status_t
_kern_fsync(int file, bool dataOnly)
{
	(void)dataOnly;
	// Windows doesn't have fsync(), use _commit() instead
	return (::_commit(file) == -1) ? errno : B_OK;
}

status_t
_kern_lock_node(int file /*, OpenFlags mode, FileLock *lock*/)
{
	HANDLE hFile = (HANDLE)_get_osfhandle(file);
	if (hFile == INVALID_HANDLE_VALUE)
		return B_ERROR;

	OVERLAPPED overlapped = {0};
	DWORD flags = LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK;

	if (!LockFileEx(hFile, flags, 0, MAXDWORD, MAXDWORD, &overlapped))
		return B_ERROR;

	return B_OK;
}

status_t
_kern_unlock_node(int file)
{
	HANDLE hFile = (HANDLE)_get_osfhandle(file);
	if (hFile == INVALID_HANDLE_VALUE)
		return B_ERROR;

	OVERLAPPED overlapped = {0};
	if (!UnlockFileEx(hFile, 0, MAXDWORD, MAXDWORD, &overlapped))
		return B_ERROR;

	return B_OK;
}

status_t
_kern_read_stat(int fd, const char* path, bool traverseLink, struct stat *stat, size_t statSize)
{
	(void)statSize;
	(void)traverseLink;

	if (stat == NULL)
		return B_BAD_VALUE;

	if (path != NULL) {
		char resolvedPath[B_PATH_NAME_LENGTH];
		status_t error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
		if (error != B_OK)
			return error;

		return (::stat(resolvedPath, stat) == -1) ? errno : B_OK;
	}

	if (fd >= 0)
		return (::fstat(fd, stat) == -1) ? errno : B_OK;

	return B_BAD_VALUE;
}

status_t
_kern_write_stat(int fd, const char* path, bool traverseLeafLink,
	const struct stat* stat, size_t statSize, int statMask)
{
	(void)traverseLeafLink;
	(void)statSize;

	if (stat == NULL)
		return B_BAD_VALUE;

	const struct stat& s = *stat;
	int result;
	char resolvedPath[B_PATH_NAME_LENGTH];
	status_t error;

	if (path != NULL) {
		error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
		if (error != B_OK)
			return error;

		switch (statMask) {
			case WSTAT_MODE:
				result = ::_chmod(resolvedPath, s.st_mode);
				break;

			case WSTAT_UID:
			case WSTAT_GID:
				return B_ERROR;

			case WSTAT_SIZE:
			{
				int localFD = ::_open(resolvedPath, _O_WRONLY);
				if (localFD == -1)
					result = -1;
				else {
					result = ::_chsize_s(localFD, s.st_size);
					::_close(localFD);
					if (result != 0)
						errno = result;
				}
				break;
			}

			case WSTAT_ATIME:
			case WSTAT_MTIME:
			{
				struct __stat64 oldStat;
				result = ::_stat64(resolvedPath, &oldStat);
				if (result < 0)
					break;

				struct __utimbuf64 buffer;
				buffer.actime = (statMask == WSTAT_ATIME) ? s.st_atime : oldStat.st_atime;
				buffer.modtime = (statMask == WSTAT_MTIME) ? s.st_mtime : oldStat.st_mtime;
				result = ::_utime64(resolvedPath, &buffer);
				break;
			}

			case WSTAT_CRTIME:
			default:
				return B_BAD_VALUE;
		}

		return (result != 0) ? errno : B_OK;
	}

	if (fd < 0)
		return B_BAD_VALUE;

	switch (statMask) {
		case WSTAT_MODE:
			// Need path, not fd on Windows.
			return B_ERROR;
			break;

		case WSTAT_UID:
		case WSTAT_GID:
			// Windows doesn't have Unix-style ownership
			return B_ERROR;
			
		case WSTAT_SIZE:
			result = ::_chsize_s(fd, s.st_size);
			if (result != 0)
				errno = result;
			return result != 0 ? errno : B_OK;
			
		case WSTAT_ATIME:
		case WSTAT_MTIME:
		case WSTAT_CRTIME:
			// Would need futimes() equivalent or path
			return B_ERROR;
			
		default:
			return B_BAD_VALUE;	
	}

	return (result == -1) ? errno : B_OK;
}

//------------------------------------------------------------------------------// Directory Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::open_dir(const char *path, int &result, DIR** dir)
{
	result = -1;
	if (dir) {
		if ((*dir = ::opendir(path)) != NULL) {
			// Open a real directory handle and wrap it as a CRT fd.
			result = open_directory_fd(path);
			if (result == -1) {
				::closedir(*dir);
				*dir = NULL;
			}
		}
	} else {
		DIR* tempdir;
		if ((tempdir = ::opendir(path)) != NULL) {
			result = open_directory_fd(path);
			closedir(tempdir);
		}
	}
	
	return (result < 0) ? B_ENTRY_NOT_FOUND : B_OK;
}


status_t
_kern_create_dir(int fd, const char *path, mode_t mode)
{
	status_t error = (path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		char resolvedPath[B_PATH_NAME_LENGTH];
		error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
		if (error != B_OK)
			return error;

		if (::_mkdir(resolvedPath) == -1)
			error = convertErrno(errno);
	}
	return error;
}


status_t
BPrivate::Storage::find_dir(int dir, DIR** dirDir, const char *name,
					  dirent *result, size_t length)
{
	if (dir < 0 || name == NULL || result == NULL || *dirDir == NULL)
		return B_BAD_VALUE;
	
	status_t status = _kern_rewind_dir(*dirDir);
	if (status == B_OK) {
		while (_kern_read_dir(dir, dirDir, result, length, 1) == 1) {
			if (strcmp(result->d_name, name) == 0)
				return B_OK;
		}
		status = B_ENTRY_NOT_FOUND;
	}
	
	return status;
}

status_t
BPrivate::Storage::find_dir(int dir, DIR** dirDir, const char *name, entry_ref *result)
{
	// Not fully implemented - would need proper entry_ref support
	return B_ERROR;
}

status_t
BPrivate::Storage::dup_dir(int dir, int &result)
{
	status_t error = B_OK;
	if (dir == -1)
		result = -1;
	else {
		result = _kern_dup(dir);
		if (result == -1)
			error = errno;
	}
	return error;
}

status_t
BPrivate::Storage::close_dir(int dir)
{
	// On Windows, the DIR* and fd are separate
	// Close the fd; caller should close DIR* separately
	return (::_close(dir) == -1) ? errno : B_OK;
}

//------------------------------------------------------------------------------
// SymLink functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::create_link(const char *path, const char *linkToPath)
{
	status_t error = (path && linkToPath ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		// Convert to wide strings
		int pathLen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
		int linkLen = MultiByteToWideChar(CP_UTF8, 0, linkToPath, -1, NULL, 0);
		
		if (pathLen > 0 && linkLen > 0) {
			WCHAR* wPath = new WCHAR[pathLen];
			WCHAR* wLink = new WCHAR[linkLen];
			
			MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, pathLen);
			MultiByteToWideChar(CP_UTF8, 0, linkToPath, -1, wLink, linkLen);
			
			// Determine if target is a directory
			DWORD attrs = GetFileAttributesW(wLink);
			DWORD flags = (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) 
				? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
			
			if (!CreateSymbolicLinkW(wPath, wLink, flags))
				error = B_ERROR;
			
			delete[] wPath;
			delete[] wLink;
		} else {
			error = B_ERROR;
		}

		if (error == B_OK)
			error = _kern_open(-1, path, O_RDWR, (_S_IREAD | _S_IWRITE) & ~__gUmask);
	}
	return error;
}


status_t
_kern_read_link(int fd, const char *path, char *buffer, size_t *_bufferSize)
{
	if (buffer == NULL || _bufferSize == NULL)
		return B_BAD_VALUE;

	size_t requestedSize = *_bufferSize;
	if (path == NULL && fd < 0)
		return B_BAD_VALUE;

	HANDLE hFile = INVALID_HANDLE_VALUE;
	bool closeHandle = false;

	if (path != NULL) {
		// Match Haiku semantics: absolute paths ignore fd, relative paths use
		// fd when valid, otherwise they are interpreted relative to CWD.
		char resolvedPath[B_PATH_NAME_LENGTH];
		status_t error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
		if (error != B_OK)
			return error;

		hFile = CreateFileA(resolvedPath, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			if (requestedSize > 0)
				buffer[0] = '\0';
			return B_ERROR;
		}
		closeHandle = true;
	} else {
		hFile = (HANDLE)_get_osfhandle(fd);
		if (hFile == INVALID_HANDLE_VALUE) {
			if (requestedSize > 0)
				buffer[0] = '\0';
			return B_BAD_VALUE;
		}
	}

	WCHAR wPath[MAX_PATH];
	DWORD len = GetFinalPathNameByHandleW(hFile, wPath, MAX_PATH,
		FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

	if (closeHandle)
		CloseHandle(hFile);

	if (len == 0 || len >= MAX_PATH) {
		if (requestedSize > 0)
			buffer[0] = '\0';
		return B_ERROR;
	}

	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, NULL, 0, NULL, NULL);
	if (bytesNeeded <= 0) {
		if (requestedSize > 0)
			buffer[0] = '\0';
		return B_ERROR;
	}

	size_t actualLen = (size_t)(bytesNeeded - 1);
	*_bufferSize = actualLen;

	if (requestedSize > 0) {
		if (actualLen < requestedSize) {
			WideCharToMultiByte(CP_UTF8, 0, wPath, -1, buffer, (int)requestedSize, NULL, NULL);
		} else {
			WideCharToMultiByte(CP_UTF8, 0, wPath, -1, buffer, (int)requestedSize, NULL, NULL);
			buffer[requestedSize - 1] = '\0';
		}
	}

	return B_OK;
}

//------------------------------------------------------------------------------
// Miscellaneous Functions
//------------------------------------------------------------------------------


status_t
BPrivate::Storage::dir_to_path(int dir, char *result, size_t size)
{
	if (dir < 0 || result == NULL)
		return B_BAD_VALUE;

	HANDLE hFile = (HANDLE)_get_osfhandle(dir);
	if (hFile == INVALID_HANDLE_VALUE)
		return B_ERROR;

	WCHAR wPath[MAX_PATH];
	DWORD len = GetFinalPathNameByHandleW(hFile, wPath, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	
	if (len == 0 || len >= MAX_PATH)
		return B_ERROR;
	
	// Convert to UTF-8
	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, NULL, 0, NULL, NULL);
	if (bytesNeeded > (int)size)
		return B_BAD_VALUE;
	
	WideCharToMultiByte(CP_UTF8, 0, wPath, -1, result, bytesNeeded, NULL, NULL);
	
	// Remove \\?\ prefix if present
	if (strncmp(result, "\\\\?\\", 4) == 0) {
		memmove(result, result + 4, strlen(result + 4) + 1);
	}
	
	return B_OK;
}

status_t
_kern_rename(int oldFD, const char* oldPath, int newFD, const char* newPath)
{
	if (oldPath == NULL || newPath == NULL)
		return B_BAD_VALUE;

	char resolvedOldPath[B_PATH_NAME_LENGTH];
	char resolvedNewPath[B_PATH_NAME_LENGTH];
	status_t error = resolve_path_for_fd(oldFD, oldPath, resolvedOldPath, sizeof(resolvedOldPath));
	if (error != B_OK)
		return error;
	error = resolve_path_for_fd(newFD, newPath, resolvedNewPath, sizeof(resolvedNewPath));
	if (error != B_OK)
		return error;
	
	return (::rename(resolvedOldPath, resolvedNewPath) == -1) ? errno : B_OK;
}

status_t
_kern_unlink(int fd, const char *path)
{
	if (path == NULL)
		return B_BAD_VALUE;

	char resolvedPath[B_PATH_NAME_LENGTH];
	status_t error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
	if (error != B_OK)
		return error;
	
	if (::remove(resolvedPath) == 0)
		return B_OK;

	if (::_rmdir(resolvedPath) == 0)
		return B_OK;

	return errno;
}

status_t
_kern_remove_dir(int fd, const char *path)
{
	if (path == NULL)
		return B_BAD_VALUE;

	char resolvedPath[B_PATH_NAME_LENGTH];
	status_t error = resolve_path_for_fd(fd, path, resolvedPath, sizeof(resolvedPath));
	if (error != B_OK)
		return error;

	return (::_rmdir(resolvedPath) == -1) ? errno : B_OK;
}


// Windows-specific path canonicalization that doesn't require opening files
status_t
BPrivate::Storage::get_canonical_path(const char *path, char *result, size_t size)
{
	if (!path || !result)
		return B_BAD_VALUE;

	// Use GetFullPathName to canonicalize the path
	char buffer[MAX_PATH];
	DWORD len = GetFullPathNameA(path, MAX_PATH, buffer, NULL);
	
	if (len == 0 || len >= MAX_PATH)
		return B_BAD_VALUE;
	
	if (len >= size)
		return B_NAME_TOO_LONG;
	
	// Convert backslashes to forward slashes for consistency
	for (DWORD i = 0; i < len; i++) {
		if (buffer[i] == '\\')
			buffer[i] = '/';
	}
	
	strcpy(result, buffer);
	return B_OK;
}


status_t
BPrivate::Storage::get_canonical_dir_path(const char *path, char *result, size_t size)
{
	// For Windows, just use get_canonical_path - we don't need to open the directory
	return get_canonical_path(path, result, size);
}

