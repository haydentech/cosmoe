//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//----------------------------------------------------------------------
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
#include <new>
#include <utime.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <syscalls.h>

// This is just for cout while developing; shouldn't need it
// when all is said and done.
#include <iostream>

using namespace std;

// Forward declaration of platform-independent helper (defined in kernel_interface.cpp)
status_t convertErrno(int result);

// For convenience:
//struct LongDIR : DIR { char _buffer[B_FILE_NAME_LENGTH]; };
struct DIR_STRUCT
{
	int fd;
};


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
_kern_open(int fd, const char *path, uint32 flags,
				  uint32 creationFlags, int &result)
{
	if (path == NULL) {
		if (fd < 0) {
			result = -1;
			return B_BAD_VALUE;
		}

		// POSIX cannot reopen by fd with different mode flags. Duplicate the fd
		// to preserve "fd-only" semantics from the kernel API contract.
		result = ::dup(fd);
		return (result == -1) ? convertErrno(errno) : B_OK;
	}

	// Open/Create the file and return the proper error code
	if (fd >= 0)
		result = ::openat(fd, path, flags, creationFlags);
	else
		result = ::open(path, flags, creationFlags);
	return (result == -1) ? convertErrno(errno) : B_OK;
}


status_t
_kern_close(int file)
{
	return (::close(file) == -1) ? errno : B_OK ;
}


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
		if (pos >= 0)
			result = ::read_pos(fd, pos, buffer, bufferSize);
		else
			result = ::read(fd, buffer, bufferSize);
		if (result == -1)
			result = -1;
	}
	return result;
}

/*! \param fd the file descriptor
	\param buf the buffer containing the data to be written
	\param len the number of bytes to be written
	\return the number of bytes actually written or an error code
*/
ssize_t
_kern_write(int fd, off_t pos, const void *buffer, size_t bufferSize)
{
	ssize_t result = (buffer == NULL ? B_BAD_VALUE : B_OK);
	if (result == B_OK) {
		if (pos >= 0)
			result = ::write_pos(fd, pos, buffer, bufferSize);
		else	
			result = ::write(fd, buffer, bufferSize);

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
	off_t result = ::lseek(fd, pos, seekType);
	if (result == -1)
		result = errno;
	return result;
}


int
_kern_dup(int file)
{
	return ::dup(file);
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

	return (::fcntl(file, F_SETLK, &lock) == 0) ? B_OK : errno;
}


status_t
_kern_unlock_node(int file)
{
	struct flock lock;
	lock.l_type = F_UNLCK;

	return (::fcntl(file, F_SETLK, &lock) == 0) ? B_OK : errno;
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
		return (::fstatat(baseFD, path, stat, atFlags) == -1) ? errno : B_OK;
	} else if (fd >= 0) {
		return (::fstat(fd, stat) == -1) ? errno : B_OK;
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
			result = dirfd(tempdir);
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


/*!	\param dir the directory
	\param buffer the dirent structure to be filled
	\param length the size of the dirent structure
	\param maxcount the maximal number of entries to be read
	\return
	- the number of entries stored in the supplied buffer,
	- \c 0, if at the end of the entry list,
	- \c B_BAD_VALUE, if \a buffer is NULL, or the supplied buffer is too small
*/
ssize_t
_kern_read_dir(int dir, DIR** dirDir, struct dirent *buffer, size_t length, uint32 maxcount)
{
	// init a DIR structure
	if (*dirDir == NULL)
		*dirDir = opendirfd(dir);
	// check parameters
	ssize_t result = (buffer == NULL ? B_BAD_VALUE : 0);
	if (result == 0 && maxcount > 0) {
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
BPrivate::Storage::find_dir( int dir, DIR** dirDir, const char *name,
					  DirEntry *result, size_t length )
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
BPrivate::Storage::find_dir( int dir, DIR** dirDir, const char *name, entry_ref *result )
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
BPrivate::Storage::create_link( const char *path, const char *linkToPath )
{
	status_t error = (path && linkToPath ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		if (symlink(linkToPath, path) == -1)
			error = errno;
	}
	return error;
}

/*!	The parent directory must already exist.
	\param path the link's path name
	\param linkToPath the path name the link shall point to
	\param result set to a file descriptor for the new symbolic link
	\return B_OK, if everything went fine, an error code otherwise
*/
status_t
BPrivate::Storage::create_link( const char *path, const char *linkToPath,
						 int &result)
{
	status_t error = create_link(path, linkToPath);
	if (error == B_OK)
		error = _kern_open(-1, path, O_RDWR, DEFFILEMODE & ~__gUmask, result);
	return error;
}

ssize_t
BPrivate::Storage::read_link( const char *path, char *result, size_t size )
{
	if (result == NULL)
		return B_BAD_VALUE;
	// Don't null terminate, when the buffer is too small. That would make
	// things more difficult. BTW: readlink() returns the actual length of
	// the link contents and so do we.
	int len = ::readlink(path, result, size);
	if (len == -1) {
		if (size > 0)
			result[0] = 0;		// Null terminate
		return errno;	
	} else {
		if (len < (int)size)
			result[len] = 0;	// Null terminate
		return len;
	}
}

ssize_t
BPrivate::Storage::read_link( int fd, char *result, size_t size )
{
	ssize_t error = (result ? B_OK : B_BAD_VALUE);
	// no way to implement it :-(
	if (error == B_OK)
		error = B_ERROR;
	return error;
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
BPrivate::Storage::dir_to_self_entry_ref(int dir, entry_ref *result)
{
	if (dir == -1 || result == NULL)
		return B_BAD_VALUE;

	return find_dir(dir, NULL, ".", result);
}


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


/*!
	\param device The device ID of the volume in question.
	\param name The volume's new name. Must not be longer than
		   \c B_FILE_NAME_LENGTH (including the terminating null).
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a name.
	- \c B_NAME_TOO_LONG: \a name is longer than \c B_FILE_NAME_LENGTH.
	- another error code
*/
// set_volume_name implemented in `kernel_interface.cpp`.


