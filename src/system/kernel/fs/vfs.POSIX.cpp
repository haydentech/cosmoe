/*
 * Copyright 2005-2013, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2018, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
/*!
	\file kernel_interface.POSIX.cpp
	Implementation of the Haiku kernel interface mapped to POSIX api calls.
*/

#include "kernel_interface.h"
#include "storage_support.h"

#include <OS.h>
#include <fs_info.h>	//  File sytem information functions, structs, defines
#include <fs_attr.h>	//  BeOS's C-based attribute functions
#include <fs_query.h>	//  BeOS's C-based query functions
#include <Entry.h>		// entry_ref


#include <fsproto.h>

#include <algorithm>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utime.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <syscalls.h>

// This is just for cout while developing; shouldn't need it
// when all is said and done.
#include <iostream>

using namespace std;

// Forward declaration of platform-independent helper (defined in kernel_interface.cpp)
status_t convertErrno(int result);

struct DIR_STRUCT
{
	int fd;
};


namespace {

// Use function-local statics to avoid static init order problems
std::mutex& sNodeLockSetLock()
{
	static std::mutex lock;
	return lock;
}

std::unordered_map<unsigned long long, std::unordered_set<int> >& sLockedNodeOwners()
{
	static std::unordered_map<unsigned long long, std::unordered_set<int> > m;
	return m;
}

static bool
node_lock_key_for_fd(int fd, unsigned long long& key)
{
	struct stat st;
	if (::fstat(fd, &st) != 0)
		return false;

	key = (((unsigned long long)st.st_dev) << 32)
		^ (unsigned long long)st.st_ino;
	return true;
}

static bool
node_lock_key_for_path(const char* path, unsigned long long& key)
{
	if (path == NULL)
		return false;

	struct stat st;
	if (::lstat(path, &st) != 0)
		return false;

	key = (((unsigned long long)st.st_dev) << 32)
		^ (unsigned long long)st.st_ino;
	return true;
}


static bool
is_node_locked(unsigned long long key)
{
	auto it = sLockedNodeOwners().find(key);
	if (it == sLockedNodeOwners().end())
		return false;

	std::unordered_set<int>& owners = it->second;
	for (auto ownerIt = owners.begin(); ownerIt != owners.end();) {
		unsigned long long ownerKey = 0;
		bool validOwner = false;

		char symlinkPath[B_PATH_NAME_LENGTH];
		if (BPrivate::Storage::get_symlink_fd_path(*ownerIt, symlinkPath,
			sizeof(symlinkPath)) == B_OK) {
			validOwner = node_lock_key_for_path(symlinkPath, ownerKey);
		} else
			validOwner = node_lock_key_for_fd(*ownerIt, ownerKey);

		if (!validOwner || ownerKey != key)
			ownerIt = owners.erase(ownerIt);
		else
			++ownerIt;
	}

	if (owners.empty()) {
		sLockedNodeOwners().erase(it);
		return false;
	}

	return true;
}

}


static status_t
resolve_open_path(int fd, const char* path, char* resolved, size_t resolvedSize)
{
	if (path == NULL || resolved == NULL)
		return B_BAD_VALUE;

	if (path[0] == '/') {
		size_t pathLen = strlen(path);
		if (pathLen >= resolvedSize)
			return B_NAME_TOO_LONG;
		memcpy(resolved, path, pathLen + 1);
		return B_OK;
	}

	char basePath[B_PATH_NAME_LENGTH];
	if (fd >= 0) {
		status_t error = BPrivate::Storage::dir_to_path(fd, basePath,
			sizeof(basePath));
		if (error != B_OK)
			return error;
	} else {
		if (getcwd(basePath, sizeof(basePath)) == NULL)
			return B_ERROR;
	}

	size_t baseLen = strlen(basePath);
	bool needSlash = baseLen > 0 && basePath[baseLen - 1] != '/';
	size_t totalLen = baseLen + (needSlash ? 1 : 0) + strlen(path);
	if (totalLen >= resolvedSize)
		return B_NAME_TOO_LONG;

	memcpy(resolved, basePath, baseLen + 1);
	if (needSlash)
		strcat(resolved, "/");
	strcat(resolved, path);
	return B_OK;
}


/* Open a directory stream on a fd.  */
DIR* opendirfd(int fd)
{
	DIR* dir = opendir("/");

	// Inject our fd into the private structure, now that the DIR*
	// is all set up for us.  If an OS does not have an int representing
	// the file descriptor as the first argument, things could get ugly.
	// Any BSD (including OS X) or Linux should be OK.
	if (dir)
		((DIR_STRUCT*)dir)->fd = fd;

	return dir;
}


//------------------------------------------------------------------------------
// File Functions
//------------------------------------------------------------------------------

// Device/utility functions implemented in platform-independent file
// (moved to `kernel_interface.cpp`).


/*!	\brief Opens a node specified by a FD + path pair.

	At least one of \a fd and \a path must be specified.
	If only \a fd is given, the function opens the node identified by this
	FD. If only a path is given, this path is opened. If both are given and
	the path is absolute, \a fd is ignored; a relative path is reckoned off
	of the directory (!) identified by \a fd.

	\param fd The FD. May be < 0.
	\param path The absolute or relative path. May be \c NULL.
	\param openMode The open mode.
	\return A FD referring to the newly opened node, or an error code,
			if an error occurs.
*/
status_t
_kern_open(int fd, const char *path, uint32 flags, uint32 creationFlags)
{
	int result;

	if (path == NULL) {
		if (fd < 0) {
			result = -1;
			return B_BAD_VALUE;
		}

		// POSIX cannot reopen by fd with different mode flags. Duplicate the fd
		// to preserve "fd-only" semantics from the kernel API contract.
		result = _kern_dup(fd);
		return (result < 0) ? convertErrno(errno) : result;
	}

	// Match legacy node lock visibility: opening a currently locked node should
	// fail with B_BUSY.
	{
		int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);
		int atFlags = (flags & O_NOTRAVERSE) ? AT_SYMLINK_NOFOLLOW : 0;
		struct stat st;
		if (::fstatat(baseFD, path, &st, atFlags) == 0) {
			unsigned long long key = (((unsigned long long)st.st_dev) << 32)
				^ (unsigned long long)st.st_ino;
			std::lock_guard<std::mutex> guard(sNodeLockSetLock());
			if (is_node_locked(key))
				return B_BUSY;
		}
	}

	// Open/Create the file and return the proper error code
	if (fd >= 0)
		result = ::openat(fd, path, flags, creationFlags);
	else
		result = ::open(path, flags, creationFlags);

	if (result >= 0)
		return result;

	// Linux/POSIX open(O_NOFOLLOW) returns ELOOP for symlinks. Haiku's
	// O_NOTRAVERSE semantics require BNode/BSymLink to still initialize and
	// keep symlink identity for stat/readlink behavior.
	if ((flags & O_NOTRAVERSE) != 0 && errno == ELOOP) {
		int openError = errno;
		char resolvedPath[B_PATH_NAME_LENGTH];
		status_t pathError = resolve_open_path(fd, path, resolvedPath,
			sizeof(resolvedPath));

		uint32 followFlags = flags & ~O_NOTRAVERSE;
		uint32 accessMode = followFlags & O_RWMASK;
		if (fd >= 0)
			result = ::openat(fd, path, followFlags, creationFlags);
		else
			result = ::open(path, followFlags, creationFlags);
		int followError = errno;

		if (result < 0 && accessMode == O_RDWR) {
			uint32 readOnlyFlags = (followFlags & ~O_RWMASK) | O_RDONLY;
			if (fd >= 0)
				result = ::openat(fd, path, readOnlyFlags, creationFlags);
			else
				result = ::open(path, readOnlyFlags, creationFlags);
			followError = errno;
		}

		if (result >= 0) {
			if (pathError == B_OK)
				BPrivate::Storage::register_symlink_fd_path(result, resolvedPath);
			return result;
		}

#if defined(O_PATH) && defined(O_NOFOLLOW)
		// For dangling/cyclic links, follow-open fails but O_PATH|O_NOFOLLOW can
		// still produce a descriptor for the symlink itself.
		int pathFlags = O_PATH | O_NOFOLLOW;
#ifdef O_CLOEXEC
		pathFlags |= O_CLOEXEC;
#endif
		if (fd >= 0)
			result = ::openat(fd, path, pathFlags);
		else
			result = ::open(path, pathFlags);

		if (result >= 0) {
			if (pathError == B_OK)
				BPrivate::Storage::register_symlink_fd_path(result, resolvedPath);
			return result;
		}
#endif

		errno = (followError != 0) ? followError : openError;
	}

	return (result < 0) ? convertErrno(errno) : result;
}


status_t
_kern_fsync(int file, bool dataOnly)
{
	(void)dataOnly;
	return (fsync(file) == -1) ? errno : B_OK ;
}

//! /todo Get rid of DumpLock() at some point (it's only for debugging)
void DumpLock(struct flock &lock)
{
	cout << endl;
	cout << "type   == ";
	switch (lock.l_type) {
		case F_RDLCK:
			cout << "F_RDLCK";
			break;
			
		case F_WRLCK:
			cout << "F_WRLCK";
			break;
			
		case F_UNLCK:
			cout << "F_UNLCK";
			break;
			
		default:
			cout << lock.l_type;
			break;
	}
	cout << endl;

	cout << "whence == " << lock.l_whence << endl;
	cout << "start  == " << lock.l_start << endl;
	cout << "len    == " << lock.l_len << endl;
	cout << "pid    == " << lock.l_pid << endl;
	cout << endl;
}


status_t
_kern_lock_node(int file)
{
	unsigned long long key = 0;
	bool syntheticSymlinkHandle = false;
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		syntheticSymlinkHandle = true;
		if (!node_lock_key_for_path(symlinkPath, key))
			return B_BAD_VALUE;
	} else {
		if (!node_lock_key_for_fd(file, key))
			return B_BAD_VALUE;
	}

	{
		std::lock_guard<std::mutex> guard(sNodeLockSetLock());
		if (is_node_locked(key))
			return B_BUSY;

		if (syntheticSymlinkHandle) {
			sLockedNodeOwners()[key].insert(file);
			return B_OK;
		}
	}

	struct flock lock;
//	DumpLock(*lock);

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;				// Beginning of file...
	lock.l_len = 0;				// ...to end of file
	lock.l_pid = 0;				// Don't really care :-)

//	DumpLock(*lock);

	::fcntl(file, F_GETLK, &lock);

//	DumpLock(*lock);

	if (lock.l_type != F_UNLCK) {
		return errno;
	} 
	
	lock.l_type = F_WRLCK;
//	DumpLock(*lock);

	errno = 0;

	status_t result = (::fcntl(file, F_SETLK, &lock) == 0) ? B_OK : errno;
	if (result == B_OK) {
		std::lock_guard<std::mutex> guard(sNodeLockSetLock());
			sLockedNodeOwners()[key].insert(file);
	}

	return result;
}


status_t
_kern_unlock_node(int file)
{
	unsigned long long key = 0;
	bool haveKey = false;
	bool syntheticSymlinkHandle = false;
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		syntheticSymlinkHandle = true;
		haveKey = node_lock_key_for_path(symlinkPath, key);
	} else
		haveKey = node_lock_key_for_fd(file, key);

	if (syntheticSymlinkHandle) {
		if (!haveKey)
			return B_BAD_VALUE;

		std::lock_guard<std::mutex> guard(sNodeLockSetLock());
		auto it = sLockedNodeOwners().find(key);
		if (it == sLockedNodeOwners().end()
			|| it->second.find(file) == it->second.end()) {
			return B_BAD_VALUE;
		}

		it->second.erase(file);
		if (it->second.empty())
			sLockedNodeOwners().erase(it);
		return B_OK;
	}

	struct flock lock;
	lock.l_type = F_UNLCK;

	status_t result = (::fcntl(file, F_SETLK, &lock) == 0) ? B_OK : errno;
	if (result == B_OK && haveKey) {
		std::lock_guard<std::mutex> guard(sNodeLockSetLock());
		auto it = sLockedNodeOwners().find(key);
		if (it != sLockedNodeOwners().end()) {
			it->second.erase(file);
			if (it->second.empty())
				sLockedNodeOwners().erase(it);
		}
	}

	return result;
}


status_t
_kern_read_stat(int fd, const char* path, bool traverseLink, struct stat *stat, size_t statSize)
{
	(void)statSize; // Unused parameter

	if (stat == NULL)
		return B_BAD_VALUE;

	if (path != NULL) {
		int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);
		int atFlags = traverseLink ? 0 : AT_SYMLINK_NOFOLLOW;
		return (::fstatat(baseFD, path, stat, atFlags) == -1)
			? convertErrno(errno) : B_OK;
	} else if (fd >= 0) {
		char symlinkPath[B_PATH_NAME_LENGTH];
		if (BPrivate::Storage::get_symlink_fd_path(fd, symlinkPath,
			sizeof(symlinkPath)) == B_OK) {
			int err = traverseLink
				? ::stat(symlinkPath, stat)
				: ::lstat(symlinkPath, stat);
			return (err == -1) ? convertErrno(errno) : B_OK;
		}

		return (::fstat(fd, stat) == -1) ? convertErrno(errno) : B_OK;
	} else
		return B_BAD_VALUE;
}


/*!	\brief Writes stat data of an entity specified by a FD + path pair.

	If only \a fd is given, the stat operation associated with the type
	of the FD (node, attr, attr dir etc.) is performed. If only \a path is
	given, this path identifies the entry for whose node to write the
	stat data. If both \a fd and \a path are given and the path is absolute,
	\a fd is ignored; a relative path is reckoned off of the directory (!)
	identified by \a fd and specifies the entry whose stat data shall be
	written.

	\param fd The FD. May be < 0.
	\param path The absolute or relative path. May be \c NULL.
	\param traverseLeafLink If \a path is given, \c true specifies that the
		   function shall not stick to symlinks, but traverse them.
	\param stat The buffer containing the stat data to be written.
	\param statSize The size of the supplied stat buffer.
	\param statMask A mask specifying which parts of the stat data shall be
		   written.
	\return \c B_OK, if the the stat data have been written successfully,
			another error code otherwise.
*/
status_t
_kern_write_stat(int fd, const char* path, bool traverseLeafLink,
	const struct stat* stat, size_t statSize, int statMask)
{
	(void)statSize;

	if (stat == NULL)
		return B_BAD_VALUE;

	const struct stat& s = *stat;
	int result;

	if (path != NULL) {
		int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);
		int atFlags = traverseLeafLink ? 0 : AT_SYMLINK_NOFOLLOW;

		switch (statMask) {
			case WSTAT_MODE:
				result = ::fchmodat(baseFD, path, s.st_mode, 0);
				break;

			case WSTAT_UID:
			case WSTAT_GID:
			{
				uid_t uid = (statMask == WSTAT_UID) ? s.st_uid : (uid_t)-1;
				gid_t gid = (statMask == WSTAT_GID) ? s.st_gid : (gid_t)-1;
				result = ::fchownat(baseFD, path, uid, gid, atFlags);
				break;
			}

			case WSTAT_SIZE:
			{
				int openFlags = O_WRONLY;
#ifdef O_NOFOLLOW
				if (!traverseLeafLink)
					openFlags |= O_NOFOLLOW;
#endif
				int localFD = ::openat(baseFD, path, openFlags);
				if (localFD == -1)
					result = -1;
				else {
					result = ::ftruncate(localFD, s.st_size);
					::close(localFD);
				}
				break;
			}

			case WSTAT_ATIME:
			case WSTAT_MTIME:
			{
				struct timespec times[2];
				times[0].tv_sec = (statMask == WSTAT_ATIME) ? s.st_atime : 0;
				times[0].tv_nsec = (statMask == WSTAT_ATIME) ? 0 : UTIME_OMIT;
				times[1].tv_sec = (statMask == WSTAT_MTIME) ? s.st_mtime : 0;
				times[1].tv_nsec = (statMask == WSTAT_MTIME) ? 0 : UTIME_OMIT;
				result = ::utimensat(baseFD, path, times, atFlags);
				break;
			}

			case WSTAT_CRTIME:
			default:
				return B_BAD_VALUE;
		}
	} else if (fd >= 0) {
		switch (statMask) {
			case WSTAT_MODE:
				result = ::fchmod(fd, s.st_mode);
				break;

			case WSTAT_UID:
				result = ::fchown(fd, s.st_uid, 0xFFFFFFFF);
				break;

			case WSTAT_GID:
			{
				// Should work, but doesn't. uid is set to 0xffffffff.
				struct stat st;
				result = fstat(fd, &st);
				if (result == 0)
					result = ::fchown(fd, st.st_uid, s.st_gid);
				break;
			}

			case WSTAT_SIZE:
				result = ::ftruncate(fd, s.st_size);
				break;

			// Requires a path for utime() on this backend.
			case WSTAT_ATIME:
			case WSTAT_MTIME:
			case WSTAT_CRTIME:
			default:
				return B_BAD_VALUE;
		}
	} else {
		return B_BAD_VALUE;
	}

	return (result == -1) ? errno : B_OK;
}


//------------------------------------------------------------------------------
// Attribute Directory Functions
//------------------------------------------------------------------------------

// Attribute directory functions implemented in `kernel_interface.cpp`.


//------------------------------------------------------------------------------
// Directory Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::open_dir(const char *path, int &result, DIR** dir)
{
	result = -1;

	if (dir) {
		if ((*dir = ::opendir(path)) != NULL) {
			result = dirfd(*dir);
		}		
	} else {
		DIR* tempdir;
		if ((tempdir = ::opendir(path)) != NULL) {
			result = ::dup(dirfd(tempdir));
			closedir(tempdir);
		}
	}
	
	return (result < 0) ? B_ENTRY_NOT_FOUND : B_OK;
}


/*!	\brief Creates a directory specified by a FD + path pair.

	\a path must always be specified (it contains the name of the new directory
	at least). If only a path is given, this path identifies the location at
	which the directory shall be created. If both \a fd and \a path are given
	and the path is absolute, \a fd is ignored; a relative path is reckoned off
	of the directory (!) identified by \a fd.

	\param fd The FD. May be < 0.
	\param path The absolute or relative path. Must not be \c NULL.
	\param perms The access permissions the new directory shall have.
	\return \c B_OK, if the directory has been created successfully, another
			error code otherwise.
*/
status_t
_kern_create_dir(int fd, const char *path, mode_t mode)
{
	status_t error = (path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);
		if (::mkdirat(baseFD, path, mode) == -1)
			error = convertErrno(errno);
	}
	return error;
}


status_t
BPrivate::Storage::find_dir( int dir, DIR** dirDir, const char *name,
					  dirent *result, size_t length )
{
	if (dir < 0 || name == NULL || result == NULL)
		return B_BAD_VALUE;
	
	status_t status;
	
	status = _kern_rewind_dir(*dirDir);
	if (status == B_OK) {
		while (	_kern_read_dir(dir, dirDir, result, length, 1) == 1)
		{
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
	return B_ERROR;
	status_t status = (result ? B_OK : B_BAD_VALUE);
	LongDirEntry longEntry;
	struct dirent* entry = longEntry.dirent();

	if (status == B_OK)
		status = BPrivate::Storage::find_dir(dir, dirDir, name, entry, sizeof(entry));

	if (status == B_OK) {
		//result->device = entry.d_pdev;
		result->directory = entry->d_ino;
		status = result->set_name(entry->d_name);
	}
	return status;
}


status_t
BPrivate::Storage::dup_dir(int dir, int &result)
{
	status_t error = B_OK;
	if (dir == -1)
		result = -1;
	else {
		result = ::dup(dir);
		if (result == -1)
			error = errno;
	}
	return error;
}


status_t
BPrivate::Storage::close_dir( int dir )
{
	// init a DIR structure
	DIR* dirDir = opendirfd(dir);
	return (::closedir(dirDir) == -1) ? errno : B_OK;
}

//------------------------------------------------------------------------------
// SymLink functions
//------------------------------------------------------------------------------

/*!	The parent directory must already exist.
	\param path the link's path name
	\param linkToPath the path name the link shall point to
	\return B_OK, if everything went fine, an error code otherwise
*/
status_t
BPrivate::Storage::create_link(const char *path, const char *linkToPath)
{
	int result = 0;
	status_t error = symlink(linkToPath, path);
	if (error == B_OK)
		result = _kern_open(-1, path, O_RDWR, DEFFILEMODE & ~__gUmask);
	return (result < 0) ? errno : B_OK;
}


/*!	\brief Reads the contents of a symlink referred to by a FD + path pair.

	At least one of \a fd and \a path must be specified.
	If only \a fd is given, the function the symlink to be read is the node
	identified by this FD. If only a path is given, this path identifies the
	symlink to be read. If both are given and the path is absolute, \a fd is
	ignored; a relative path is reckoned off of the directory (!) identified
	by \a fd.
	If this function fails with B_BUFFER_OVERFLOW, the \a _bufferSize pointer
	will still be updated to reflect the required buffer size.

	\param fd The FD. May be < 0.
	\param path The absolute or relative path. May be \c NULL.
	\param buffer The buffer into which the contents of the symlink shall be
		   written.
	\param _bufferSize A pointer to the size of the supplied buffer.
	\return \c B_OK on success or an appropriate error code
*/
status_t
_kern_read_link(int fd, const char* path, char* buffer, size_t* _bufferSize)
{
	if (buffer == NULL || _bufferSize == NULL)
		return B_BAD_VALUE;
	if (path == NULL && fd < 0)
		return B_BAD_VALUE;

	size_t requestedSize = *_bufferSize;
	int baseFD = AT_FDCWD;
	const char* linkPath = path;

	// Match Haiku semantics: absolute paths ignore fd, relative paths use
	// fd when valid, otherwise they are interpreted relative to CWD.
	if (path != NULL) {
		baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);
	} else {
		char symlinkPath[B_PATH_NAME_LENGTH];
		if (BPrivate::Storage::get_symlink_fd_path(fd, symlinkPath,
			sizeof(symlinkPath)) == B_OK) {
			baseFD = AT_FDCWD;
			linkPath = symlinkPath;
		} else {
			struct stat statBuffer;
			if (::fstat(fd, &statBuffer) != 0 || !S_ISLNK(statBuffer.st_mode))
				return B_BAD_VALUE;

#if defined(__linux__)
			// Linux readlinkat() supports empty path to operate on the FD itself.
			baseFD = fd;
			linkPath = "";
#else
			char fdPath[64];

			// Best-effort fallback for non-Linux POSIX targets.
			snprintf(fdPath, sizeof(fdPath), "/dev/fd/%d", fd);
			baseFD = AT_FDCWD;
			linkPath = fdPath;
#endif
		}
	}

	size_t probeSize = std::max(requestedSize, (size_t)256);
	char* probeBuffer = NULL;
	size_t actualLen = 0;

	while (true) {
		char* probe = new(std::nothrow) char[probeSize];
		if (probe == NULL)
			return B_NO_MEMORY;

		ssize_t len = ::readlinkat(baseFD, linkPath, probe, probeSize);
		if (len < 0) {
			int error = errno;
			delete[] probe;
			if (requestedSize > 0)
				buffer[0] = 0;
			return convertErrno(error);
		}

		if ((size_t)len < probeSize) {
			probeBuffer = probe;
			actualLen = (size_t)len;
			break;
		}

		delete[] probe;
		if (probeSize > SIZE_MAX / 2)
			return B_NAME_TOO_LONG;
		probeSize *= 2;
	}

	*_bufferSize = actualLen;
	if (actualLen >= requestedSize) {
		if (requestedSize > 0) {
			size_t bytesToCopy = requestedSize - 1;
			if (bytesToCopy > 0)
				memcpy(buffer, probeBuffer, bytesToCopy);
			buffer[bytesToCopy] = 0;
		}
		delete[] probeBuffer;
		return B_BUFFER_OVERFLOW;
	}

	if (actualLen > 0)
		memcpy(buffer, probeBuffer, actualLen);
	buffer[actualLen] = 0;

	delete[] probeBuffer;
	return B_OK;
}


//------------------------------------------------------------------------------
// Query Functions
//------------------------------------------------------------------------------

// Query functions implemented in `kernel_interface.cpp`.


//------------------------------------------------------------------------------
// Miscellaneous Functions
//------------------------------------------------------------------------------

// entry_ref_to_path implemented in `kernel_interface.cpp`.


status_t
BPrivate::Storage::dir_to_path(int dir, char *result, size_t size)
{
	if (dir < 0 || result == NULL)
		return B_BAD_VALUE;

#ifdef __APPLE__
	// On macOS, use fcntl with F_GETPATH
	if (fcntl(dir, F_GETPATH, result) == 0) {
		return B_OK;
	}
	return B_ERROR;
#else
	// On Linux, use /proc/self/fd
	char path[1024];
	size_t bufsize = sizeof(path) - 1;

	snprintf(path, bufsize, "/proc/self/fd/%d", dir);
	ssize_t bytes = readlink(path, result, size);

	if (bytes > 0) {
		result[bytes] = '\0';
		return B_OK;
	}

	return B_ERROR;
#endif
}

/*!	\brief Moves an entry specified by a FD + path pair to a an entry specified
		   by another FD + path pair.

	\a oldPath and \a newPath must always be specified (they contain at least
	the name of the entry). If only a path is given, this path identifies the
	entry directly. If both a FD and a path are given and the path is absolute,
	the FD is ignored; a relative path is reckoned off of the directory (!)
	identified by the respective FD.

	\param oldFD The FD of the old location. May be < 0.
	\param oldPath The absolute or relative path of the old location. Must not
		   be \c NULL.
	\param newFD The FD of the new location. May be < 0.
	\param newPath The absolute or relative path of the new location. Must not
		   be \c NULL.
	\return \c B_OK, if the entry has been moved successfully, another
			error code otherwise.
*/
status_t
_kern_rename(int oldFD, const char* oldPath, int newFD, const char* newPath)
{
	if (oldPath == NULL || newPath == NULL)
		return B_BAD_VALUE;

	// Match Haiku semantics: absolute paths ignore FDs, relative paths use
	// their corresponding FD when valid, otherwise CWD.
	int oldBaseFD = (oldPath[0] == '/') ? AT_FDCWD : (oldFD >= 0 ? oldFD : AT_FDCWD);
	int newBaseFD = (newPath[0] == '/') ? AT_FDCWD : (newFD >= 0 ? newFD : AT_FDCWD);

	return (::renameat(oldBaseFD, oldPath, newBaseFD, newPath) == -1)
		? errno : B_OK;
}


/*!	\brief Removes an entry specified by a FD + path pair from its directory.

	\a path must always be specified (it contains at least the name of the entry
	to be deleted). If only a path is given, this path identifies the entry
	directly. If both \a fd and \a path are given and the path is absolute,
	\a fd is ignored; a relative path is reckoned off of the directory (!)
	identified by \a fd.

	\param fd The FD. May be < 0.
	\param path The absolute or relative path. Must not be \c NULL.
	\return \c B_OK, if the entry has been removed successfully, another
			error code otherwise.
*/
status_t
_kern_unlink(int fd, const char *path)
{
	if (path == NULL)
		return B_BAD_VALUE;

	// Match Haiku semantics: absolute paths ignore fd, relative paths use
	// fd when valid, otherwise they are interpreted relative to CWD.
	int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);

	if (::unlinkat(baseFD, path, 0) == 0)
		return B_OK;

	// remove() also handles empty directories, so mirror that behavior.
	if (errno == EISDIR || errno == EPERM) {
		if (::unlinkat(baseFD, path, AT_REMOVEDIR) == 0)
			return B_OK;
	}

	return errno;
}


status_t
_kern_remove_dir(int fd, const char* path)
{
	if (path == NULL)
		return B_BAD_VALUE;

	// Match Haiku semantics: absolute paths ignore fd, relative paths use
	// fd when valid, otherwise they are interpreted relative to CWD.
	int baseFD = (path[0] == '/') ? AT_FDCWD : (fd >= 0 ? fd : AT_FDCWD);

	return (::unlinkat(baseFD, path, AT_REMOVEDIR) == -1) ? errno : B_OK;
}


// Unix/POSIX-specific path canonicalization

status_t
BPrivate::Storage::get_canonical_path(const char *path, char *result, size_t size)
{
	status_t error = (path && result ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		char *dirPath = NULL;
		char *leafName = NULL;
		error = split_path(path, dirPath, leafName);
		if (error == B_OK) {
			// handle special leaf names ("." and "..")
			if (strcmp(leafName, ".") == 0 || strcmp(leafName, "..") == 0)
				error = get_canonical_dir_path(path, result, size);
			else {
				// get the canonical dir path and append the leaf name
				error = get_canonical_dir_path(dirPath, result, size);
				if (error == B_OK) {
					size_t dirPathLen = strlen(result);
					// "/" doesn't need a '/' to be appended
					bool separatorNeeded = (result[dirPathLen - 1] != '/');
					size_t neededSize = dirPathLen + (separatorNeeded ? 1 : 0)
										+ strlen(leafName) + 1;
					if (neededSize <= size) {
						if (separatorNeeded)
							strcat(result + dirPathLen, "/");
						strcat(result + dirPathLen, leafName);
					} else
						error = B_BAD_VALUE;
				}
			}
			delete[] dirPath;
			delete[] leafName;
		}
	}
	return error;
}


status_t
BPrivate::Storage::get_canonical_dir_path(const char *path, char *result, size_t size)
{
	status_t error = (path && result ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		int dir;
		error = open_dir(path, dir, NULL);
		if (error == B_OK) {
			error = dir_to_path(dir, result, size);
			close_dir(dir);
		}
	}
	return error;
}