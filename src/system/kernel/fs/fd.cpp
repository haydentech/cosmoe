/*
 * Copyright 2009-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2018, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 */


//! Operations on file descriptors

#include <syscalls.h>
#include <fs_info.h>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#include <kernel_interface.h>

#if defined(_WIN32)
#include <windows.h>
#endif

extern DIR* opendirfd(int fd);

namespace {

std::mutex sSymlinkFdLock;
std::unordered_map<int, std::string> sSymlinkFdPaths;

} // namespace


status_t
BPrivate::Storage::register_symlink_fd_path(int fd, const char* path)
{
	if (fd < 0 || path == NULL)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sSymlinkFdLock);
	sSymlinkFdPaths[fd] = path;
	return B_OK;
}


void
BPrivate::Storage::inherit_symlink_fd_path(int fromFD, int toFD)
{
	if (fromFD < 0 || toFD < 0)
		return;

	std::lock_guard<std::mutex> guard(sSymlinkFdLock);
	auto it = sSymlinkFdPaths.find(fromFD);
	if (it != sSymlinkFdPaths.end())
		sSymlinkFdPaths[toFD] = it->second;
}


status_t
BPrivate::Storage::get_symlink_fd_path(int fd, char* buffer, size_t size)
{
	if (fd < 0 || buffer == NULL || size == 0)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sSymlinkFdLock);
	auto it = sSymlinkFdPaths.find(fd);
	if (it == sSymlinkFdPaths.end())
		return B_ENTRY_NOT_FOUND;

	const std::string& path = it->second;
	if (path.size() >= size)
		return B_NAME_TOO_LONG;

	memcpy(buffer, path.c_str(), path.size() + 1);
	return B_OK;
}


void
BPrivate::Storage::unregister_symlink_fd_path(int fd)
{
	if (fd < 0)
		return;

	std::lock_guard<std::mutex> guard(sSymlinkFdLock);
	sSymlinkFdPaths.erase(fd);
}


//	#pragma mark - Kernel calls


/*! \param fd the file descriptor
	\param pos file position from which to be read
	\param buffer the buffer to be read into
	\param bufferSize the number of bytes to be read
	\return the number of bytes actually read or an error code
*/
ssize_t
_kern_read(int fd, off_t pos, void *buffer, size_t bufferSize)
{
	ssize_t result = (buffer == NULL ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
#if defined(_WIN32)
		// Use native Windows API to avoid _read() issues with partial reads
		HANDLE hFile = (HANDLE)_get_osfhandle(fd);
		if (hFile == INVALID_HANDLE_VALUE)
			return B_ERROR;

		if (pos >= 0) {
			LARGE_INTEGER offset;
			offset.QuadPart = pos;
			if (!SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN))
				return B_ERROR;
		}

		DWORD bytesRead = 0;
		if (!ReadFile(hFile, buffer, (DWORD)bufferSize, &bytesRead, NULL))
			return B_ERROR;

		result = bytesRead;
#else
		if (pos >= 0)
			result = ::read_pos(fd, pos, buffer, bufferSize);
		else
			result = ::read(fd, buffer, bufferSize);
		
		if (result == -1)
			result = -1;
#endif
	}
	return result;
}


/*! \param fd the file descriptor
	\param buf the buffer containing the data to be written
	\param len the number of bytes to be written
	\return the number of bytes actually written or an error code
*/
ssize_t
_kern_write(int fd, off_t pos, const void *buffer, size_t length)
{
	ssize_t result = (buffer == NULL ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
#if defined(_WIN32)
		if (pos >= 0 && ::_lseeki64(fd, pos, SEEK_SET) == -1)
			return errno;

		result = ::_write(fd, buffer, length);
#else
		if (pos >= 0)
			result = ::write_pos(fd, pos, buffer, length);
		else	
			result = ::write(fd, buffer, length);
#endif

		if (result == -1)
			result = -1;
	}
	return result;
}


/*! \param fd the file descriptor
	\param pos the relative new position of the read/write pointer in bytes
	\param mode \c SEEK_SET/\c SEEK_END/\c SEEK_CUR to indicate that \a pos
		   is relative to the file's beginning/end/current read/write pointer
	\return the new position of the read/write pointer relative to the
			beginning of the file, or an error code
*/
off_t
_kern_seek(int fd, off_t pos, int seekType)
{
#if defined(_WIN32)
	off_t result = ::_lseeki64(fd, pos, seekType);
#else
	off_t result = ::lseek(fd, pos, seekType);
#endif

	if (result == -1)
		result = errno;
	return result;
}


ssize_t
_kern_read_dir(int dir, DIR** dirDir, struct dirent *buffer, size_t length, uint32 maxcount)
{
	ssize_t result = 0;

	if (buffer == NULL)
		return B_BAD_VALUE;

#if defined(_WIN32)
	(void)dir;
	// On Windows, DIR* is managed separately from fd
	// We rely on the caller maintaining the DIR* pointer
	if (*dirDir == NULL)
		return B_BAD_VALUE;
#else
	// Initialize a DIR structure
	if (*dirDir == NULL)
		*dirDir = opendirfd(dir);
#endif

	if (maxcount > 0) {
		// read one entry and copy it into the buffer
		errno = 0;
		if (dirent *entry = readdir(*dirDir)) {
			// Don't trust entry->d_reclen.
			// Unlike stated in BeBook::BEntryList, the value is not the length
			// of the whole structure, but only of the name. Some FSs count
			// the terminating '\0', others don't.
			// So we calculate the size ourselves (including the '\0'):
			size_t entryLen = entry->d_name + strlen(entry->d_name) + 1
							  - (char*)entry;
			if (length >= entryLen) {
				memcpy(buffer, entry, entryLen);
				result = 1;
			} else	// buffer too small
				result = B_BAD_VALUE;
		}
	}

	return result;
}


status_t
_kern_rewind_dir(DIR* dir)
{
	if (dir != NULL) {
		::rewinddir(dir);
		return B_OK;
	}

	return B_BAD_VALUE;
}


status_t
_kern_close(int fd)
{
	BPrivate::Storage::unregister_symlink_fd_path(fd);
#if defined(_WIN32)
	return (::_close(fd) == -1) ? errno : B_OK;
#else
	return (::close(fd) == -1) ? errno : B_OK;
#endif
}


int
_kern_dup(int fd)
{
	int duplicatedFD;
#if defined(_WIN32)
	duplicatedFD = ::_dup(fd);
#else
	duplicatedFD = ::dup(fd);
#endif
	if (duplicatedFD >= 0)
		BPrivate::Storage::inherit_symlink_fd_path(fd, duplicatedFD);

	return duplicatedFD;
}


