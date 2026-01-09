//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//----------------------------------------------------------------------
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
#include <errno.h>
#include <string.h>
#include <stdio.h>

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

//------------------------------------------------------------------------------
// File Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::open(const char *path, OpenFlags flags, int &result)
{
	if (path == NULL || flags & O_CREAT) {
		result = -1;
		return B_BAD_VALUE;
	}

	result = ::_open(path, flags);
	return (result == -1) ? convertErrno(errno) : B_OK;
}

status_t
BPrivate::Storage::open(const char *path, OpenFlags flags,
						 int &result, bool fallBackToReadOnly)
{
	status_t error = open(path, flags, result);
	if ((error == B_READ_ONLY_DEVICE || error == B_PERMISSION_DENIED)
		&& fallBackToReadOnly && ((flags & O_RWMASK) == O_RDWR)) {
		flags = (flags & ~O_RWMASK) | O_RDONLY;
		error = open(path, flags, result);
	}
	return error;
}

status_t
BPrivate::Storage::open(const char *path, OpenFlags flags,
				  CreationFlags creationFlags, int &result)
{
	if (path == NULL) {
		result = -1;
		return B_BAD_VALUE;
	}

	result = ::_open(path, flags | O_CREAT, creationFlags);
	return (result == -1) ? convertErrno(errno) : B_OK;
}

status_t
BPrivate::Storage::open(const char *path, OpenFlags flags,
				  CreationFlags creationFlags, int &result,
				  bool fallBackToReadOnly)
{
	status_t error = open(path, flags, creationFlags, result);
	if ((error == B_READ_ONLY_DEVICE || error == B_PERMISSION_DENIED)
		&& fallBackToReadOnly && ((flags & O_RWMASK) == O_RDWR)) {
		flags = (flags & ~O_RWMASK) | O_RDONLY;
		error = open(path, flags, creationFlags, result);
	}

	return error;
}

status_t
BPrivate::Storage::close(int file)
{
	return (::_close(file) == -1) ? errno : B_OK;
}

ssize_t
BPrivate::Storage::read(int fd, void *buf, size_t len)
{
	ssize_t result = (buf == NULL ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
		result = ::_read(fd, buf, len);
		if (result == -1)
			result = errno;
	}
	return result;
}

ssize_t
BPrivate::Storage::read(int fd, void *buf, off_t pos, size_t len)
{
	ssize_t result = (buf == NULL || pos < 0 ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
		off_t oldPos = ::_lseeki64(fd, 0, SEEK_CUR);
		if (oldPos == -1)
			return errno;
		
		if (::_lseeki64(fd, pos, SEEK_SET) == -1)
			return errno;
		
		result = ::_read(fd, buf, len);
		if (result == -1)
			result = errno;
		
		::_lseeki64(fd, oldPos, SEEK_SET);
	}
	return result;
}

ssize_t
BPrivate::Storage::write(int fd, const void *buf, size_t len)
{
	ssize_t result = (buf == NULL ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
		result = ::_write(fd, buf, len);
		if (result == -1)
			result = errno;
	}
	return result;
}

ssize_t
BPrivate::Storage::write(int fd, const void *buf, off_t pos, size_t len)
{
	ssize_t result = (buf == NULL || pos < 0 ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
		off_t oldPos = ::_lseeki64(fd, 0, SEEK_CUR);
		if (oldPos == -1)
			return errno;
		
		if (::_lseeki64(fd, pos, SEEK_SET) == -1)
			return errno;
		
		result = ::_write(fd, buf, len);
		if (result == -1)
			result = errno;
		
		::_lseeki64(fd, oldPos, SEEK_SET);
	}
	return result;
}

off_t
BPrivate::Storage::seek(int fd, off_t pos, BPrivate::Storage::SeekMode mode)
{
	off_t result = ::_lseeki64(fd, pos, mode);
	if (result == -1)
		result = errno;
	return result;
}

off_t
BPrivate::Storage::get_position(int fd)
{
	off_t result = ::_lseeki64(fd, 0, SEEK_CUR);
	if (result == -1)
		result = errno;
	return result;
}

int
BPrivate::Storage::dup(int file)
{
	return ::_dup(file);
}

status_t
BPrivate::Storage::dup(int file, int& result)
{
	status_t error = B_OK;
	if (file == -1)
		result = -1;
	else {
		result = dup(file);
		if (result == -1)
			error = errno;
	}
	return error;
}

status_t
BPrivate::Storage::sync(int file)
{
	// Windows doesn't have fsync(), use _commit() instead
	return (::_commit(file) == -1) ? errno : B_OK;
}

status_t
BPrivate::Storage::lock(int file, OpenFlags mode, FileLock *lock)
{
	if (lock == NULL)
		return B_BAD_VALUE;

	HANDLE hFile = (HANDLE)_get_osfhandle(file);
	if (hFile == INVALID_HANDLE_VALUE)
		return B_ERROR;

	OVERLAPPED overlapped = {0};
	DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;
	
	if (mode == B_READ_ONLY)
		flags = 0; // Shared lock
	else
		flags |= LOCKFILE_EXCLUSIVE_LOCK; // Exclusive lock

	if (!LockFileEx(hFile, flags, 0, MAXDWORD, MAXDWORD, &overlapped))
		return B_ERROR;

	return B_OK;
}

status_t
BPrivate::Storage::unlock(int file, FileLock *lock)
{
	if (lock == NULL)
		return B_BAD_VALUE;

	HANDLE hFile = (HANDLE)_get_osfhandle(file);
	if (hFile == INVALID_HANDLE_VALUE)
		return B_ERROR;

	OVERLAPPED overlapped = {0};
	if (!UnlockFileEx(hFile, 0, MAXDWORD, MAXDWORD, &overlapped))
		return B_ERROR;

	return B_OK;
}

status_t
BPrivate::Storage::get_stat(const char *path, Stat *s)
{
	if (path == NULL || s == NULL)
		return B_BAD_VALUE;
		
	struct _stat64 winStat;
	if (::_stat64(path, &winStat) == -1)
		return errno;
	
	// Convert _stat64 to stat
	s->st_dev = winStat.st_dev;
	s->st_ino = winStat.st_ino;
	s->st_mode = winStat.st_mode;
	s->st_nlink = winStat.st_nlink;
	s->st_uid = winStat.st_uid;
	s->st_gid = winStat.st_gid;
	s->st_rdev = winStat.st_rdev;
	s->st_size = winStat.st_size;
	s->st_atime = winStat.st_atime;
	s->st_mtime = winStat.st_mtime;
	s->st_ctime = winStat.st_ctime;
	
	return B_OK;
}

status_t
BPrivate::Storage::get_stat(int file, Stat *s)
{
	if (s == NULL)
		return B_BAD_VALUE;
		
	struct _stat64 winStat;
	if (::_fstat64(file, &winStat) == -1)
		return errno;
	
	// Convert _stat64 to stat
	s->st_dev = winStat.st_dev;
	s->st_ino = winStat.st_ino;
	s->st_mode = winStat.st_mode;
	s->st_nlink = winStat.st_nlink;
	s->st_uid = winStat.st_uid;
	s->st_gid = winStat.st_gid;
	s->st_rdev = winStat.st_rdev;
	s->st_size = winStat.st_size;
	s->st_atime = winStat.st_atime;
	s->st_mtime = winStat.st_mtime;
	s->st_ctime = winStat.st_ctime;
	
	return B_OK;
}

status_t
BPrivate::Storage::get_stat(entry_ref &ref, Stat *result)
{
	char path[B_PATH_NAME_LENGTH];
	status_t status;
	
	status = BPrivate::Storage::entry_ref_to_path(&ref, path, B_PATH_NAME_LENGTH);
	return (status != B_OK) ? status : BPrivate::Storage::get_stat(path, result);
}

status_t
BPrivate::Storage::set_stat(int file, Stat &s, StatMember what)
{
	int result;
	
	switch (what) {
		case WSTAT_MODE:
			result = ::_chmod(NULL, s.st_mode); // Need path, not fd
			return B_ERROR; // Not supported via fd on Windows
			break;

		case WSTAT_UID:
		case WSTAT_GID:
			// Windows doesn't have Unix-style ownership
			return B_ERROR;
			
		case WSTAT_SIZE:
			result = ::_chsize_s(file, s.st_size);
			return result < 0 ? errno : B_OK;
			
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

status_t
BPrivate::Storage::set_stat(const char *file, Stat &s, StatMember what)
{
	int result;
	
	switch (what) {
		case WSTAT_MODE:
			result = ::_chmod(file, s.st_mode);
			break;
			
		case WSTAT_UID:
		case WSTAT_GID:
			// Windows doesn't have Unix-style ownership
			return B_ERROR;

		case WSTAT_SIZE:
		{
			int fd = ::_open(file, O_RDWR);
			if (fd != -1) {
				result = ::_chsize_s(fd, s.st_size);
				::_close(fd);
			} else
				result = -1;
			break;
		}
			
		case WSTAT_ATIME:
		case WSTAT_MTIME:
		{
			struct __utimbuf64 {
				__time64_t actime;
				__time64_t modtime;
			} buffer;
			
			// Get current times first
			Stat oldStat;
			if (get_stat(file, &oldStat) != B_OK) {
				result = -1;
				break;
			}
			
			buffer.actime = (what == WSTAT_ATIME) ? s.st_atime : oldStat.st_atime;
			buffer.modtime = (what == WSTAT_MTIME) ? s.st_mtime : oldStat.st_mtime;
		// _utime64 doesn't exist in MinGW, use _utime with the file path
		// This is a limitation - we'd need to get the path from the fd
		return B_ERROR;		break;
	}
	
	case WSTAT_CRTIME:
		// Windows has creation time but it's harder to set
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
		if (*dir = ::opendir(path)) {
			// On Windows, we open the directory path as a file to get an fd
			result = ::_open(path, _O_RDONLY);
			if (result == -1) {
				::closedir(*dir);
				*dir = NULL;
			}
		}
	} else {
		DIR* tempdir;
		if (tempdir = ::opendir(path)) {
			result = ::_open(path, _O_RDONLY);
			closedir(tempdir);
		}
	}
	
	return (result < 0) ? B_ENTRY_NOT_FOUND : B_OK;
}

status_t
BPrivate::Storage::create_dir(const char *path, mode_t mode)
{
	status_t error = (path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		if (::_mkdir(path) == -1)
			error = errno;
	}
	return error;
}

status_t
BPrivate::Storage::create_dir(const char *path, int &result, mode_t mode)
{
	status_t error = create_dir(path, mode);
	if (error == B_OK)
		error = open_dir(path, result, NULL);
	return error;
}

status_t
BPrivate::Storage::read_dir(int dir, DIR** dirDir, DirEntry *buffer, size_t length, int32 count)
{
	// On Windows, DIR* is managed separately from fd
	// We rely on the caller maintaining the DIR* pointer
	if (*dirDir == NULL || buffer == NULL)
		return B_BAD_VALUE;
	
	int32 result = 0;
	if (count > 0) {
		errno = 0;
		if (dirent *entry = readdir(*dirDir)) {
			size_t entryLen = (char*)(entry->d_name) + strlen(entry->d_name) + 1 - (char*)entry;
			if (length >= entryLen) {
				memcpy(buffer, entry, entryLen);
				result = 1;
			} else {
				result = B_BAD_VALUE;
			}
		}
	}
	return result;
}

status_t
BPrivate::Storage::rewind_dir(DIR* dir)
{
	if (dir != NULL) {
		::rewinddir(dir);
		return B_OK;
	}
	return B_BAD_VALUE;
}

status_t
BPrivate::Storage::find_dir(int dir, DIR** dirDir, const char *name,
					  DirEntry *result, size_t length)
{
	if (dir < 0 || name == NULL || result == NULL || *dirDir == NULL)
		return B_BAD_VALUE;
	
	status_t status = BPrivate::Storage::rewind_dir(*dirDir);
	if (status == B_OK) {
		while (BPrivate::Storage::read_dir(dir, dirDir, result, length, 1) == 1) {
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
	return BPrivate::Storage::dup(dir, result);
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
	}
	return error;
}

status_t
BPrivate::Storage::create_link(const char *path, const char *linkToPath, int &result)
{
	status_t error = create_link(path, linkToPath);
	if (error == B_OK)
		error = open(path, O_RDWR, result);
	return error;
}

ssize_t
BPrivate::Storage::read_link(const char *path, char *result, size_t size)
{
	if (result == NULL)
		return B_BAD_VALUE;
	
	// Open the symbolic link without following it
	HANDLE hFile = CreateFileA(path, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	
	if (hFile == INVALID_HANDLE_VALUE) {
		if (size > 0)
			result[0] = 0;
		return B_ERROR;
	}
	
	// Get the target path
	WCHAR wPath[MAX_PATH];
	DWORD len = GetFinalPathNameByHandleW(hFile, wPath, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	CloseHandle(hFile);
	
	if (len == 0 || len >= MAX_PATH) {
		if (size > 0)
			result[0] = 0;
		return B_ERROR;
	}
	
	// Convert back to UTF-8
	int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, NULL, 0, NULL, NULL);
	if (bytesNeeded > (int)size) {
		// Buffer too small, but return actual length
		WideCharToMultiByte(CP_UTF8, 0, wPath, -1, result, size, NULL, NULL);
		if (size > 0)
			result[size - 1] = 0;
	} else {
		WideCharToMultiByte(CP_UTF8, 0, wPath, -1, result, bytesNeeded, NULL, NULL);
	}
	
	int actualLen = strlen(result);
	return actualLen;
}

ssize_t
BPrivate::Storage::read_link(int fd, char *result, size_t size)
{
	ssize_t error = (result ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		HANDLE hFile = (HANDLE)_get_osfhandle(fd);
		if (hFile == INVALID_HANDLE_VALUE)
			return B_ERROR;
		
		WCHAR wPath[MAX_PATH];
		DWORD len = GetFinalPathNameByHandleW(hFile, wPath, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		
		if (len == 0 || len >= MAX_PATH)
			return B_ERROR;
		
		int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, NULL, 0, NULL, NULL);
		if (bytesNeeded > (int)size) {
			WideCharToMultiByte(CP_UTF8, 0, wPath, -1, result, size, NULL, NULL);
			if (size > 0)
				result[size - 1] = 0;
		} else {
			WideCharToMultiByte(CP_UTF8, 0, wPath, -1, result, bytesNeeded, NULL, NULL);
		}
		
		return strlen(result);
	}
	return error;
}

//------------------------------------------------------------------------------
// Miscellaneous Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::dir_to_self_entry_ref(int dir, entry_ref *result)
{
	return B_ERROR;
}

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
BPrivate::Storage::rename(const char *oldPath, const char *newPath)
{
	if (oldPath == NULL || newPath == NULL)
		return B_BAD_VALUE;
	
	return (::rename(oldPath, newPath) == -1) ? errno : B_OK;
}

status_t
BPrivate::Storage::remove(const char *path)
{
	if (path == NULL)
		return B_BAD_VALUE;
	
	return (::remove(path) == -1) ? errno : B_OK;
}

bool
BPrivate::Storage::is_same_fs_object(int fd1, int fd2)
{
	struct _stat64 stat1, stat2;
	if ((::_fstat64(fd1, &stat1) < 0) || (::_fstat64(fd2, &stat2) < 0))
		return false;

	return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
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
BPrivate::Storage::get_canonical_path(const char *path, char *&result)
{
	if (!path)
		return B_BAD_VALUE;
	
	result = new(std::nothrow) char[B_PATH_NAME_LENGTH];
	if (!result)
		return B_NO_MEMORY;
	
	status_t error = get_canonical_path(path, result, B_PATH_NAME_LENGTH);
	if (error != B_OK) {
		delete[] result;
		result = NULL;
	}
	
	return error;
}

status_t
BPrivate::Storage::get_canonical_dir_path(const char *path, char *result, size_t size)
{
	// For Windows, just use get_canonical_path - we don't need to open the directory
	return get_canonical_path(path, result, size);
}

status_t
BPrivate::Storage::get_canonical_dir_path(const char *path, char *&result)
{
	// For Windows, just use get_canonical_path - we don't need to open the directory
	return get_canonical_path(path, result);
}
