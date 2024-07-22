/*
 * Copyright 2002-2009, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 */


#include <fsproto.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>

#include "kernel_interface.h"


// Creates an uninitialized BFile.
BFile::BFile()
	:
	fMode(0)
{
}


// Creates a copy of the supplied BFile.
BFile::BFile(const BFile& file)
	:
	fMode(0)
{
	*this = file;
}


// Creates a BFile and initializes it to the file referred to by
// the supplied entry_ref and according to the specified open mode.
BFile::BFile(const entry_ref* ref, uint32 openMode)
	:
	fMode(0)
{
	SetTo(ref, openMode);
}


// Creates a BFile and initializes it to the file referred to by
// the supplied BEntry and according to the specified open mode.
BFile::BFile(const BEntry* entry, uint32 openMode)
	:
	fMode(0)
{
	SetTo(entry, openMode);
}


// Creates a BFile and initializes it to the file referred to by
// the supplied path name and according to the specified open mode.
BFile::BFile(const char* path, uint32 openMode)
	:
	fMode(0)
{
	SetTo(path, openMode);
}


// Creates a BFile and initializes it to the file referred to by
// the supplied path name relative to the specified BDirectory and
// according to the specified open mode.
BFile::BFile(const BDirectory *dir, const char* path, uint32 openMode)
	:
	fMode(0)
{
	SetTo(dir, path, openMode);
}


// Frees all allocated resources.
BFile::~BFile()
{
	// Also called by the BNode destructor, but we rather try to avoid
	// problems with calling virtual functions in the base class destructor.
	// Depending on the compiler implementation an object may be degraded to
	// an object of the base class after the destructor of the derived class
	// has been executed.
	close_fd();
}


// Re-initializes the BFile to the file referred to by the
// supplied entry_ref and according to the specified open mode.
status_t
BFile::SetTo(const entry_ref* ref, uint32 openMode)
{
	Unset();
	char path[B_PATH_NAME_LENGTH];
	status_t error = (ref ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		error = BPrivate::Storage::entry_ref_to_path(ref, path, B_PATH_NAME_LENGTH);
	}
	if (error == B_OK)
		error = SetTo(path, openMode);
	set_status(error);
	return error;
}


// Re-initializes the BFile to the file referred to by the
// supplied BEntry and according to the specified open mode.
status_t
BFile::SetTo(const BEntry* entry, uint32 openMode)
{
	Unset();

	if (!entry)
		return (fCStatus = B_BAD_VALUE);
	if (entry->InitCheck() != B_OK)
		return (fCStatus = entry->InitCheck());
	entry_ref ref;
	status_t error;

	error = entry->GetRef(&ref);
	if (error == B_OK)
		error = SetTo(&ref, openMode);
	set_status(error);
	return error;
}


// Re-initializes the BFile to the file referred to by the
// supplied path name and according to the specified open mode.
status_t
BFile::SetTo(const char* path, uint32 openMode)
{
	Unset();
	status_t result = B_OK;
	int newFd = -1;
	if (path) {
		// analyze openMode
		// Well, it's a bit schizophrenic to convert the B_* style openMode
		// to POSIX style openFlags, but to use O_RWMASK to filter openMode.
		BPrivate::Storage::OpenFlags openFlags = 0;
		switch (openMode & O_RWMASK) {
			case B_READ_ONLY:
 				openFlags = O_RDONLY;
				break;
			case B_WRITE_ONLY:
 				openFlags = O_WRONLY;
				break;
			case B_READ_WRITE:
 				openFlags = O_RDWR;
				break;
			default:
				result = B_BAD_VALUE;
				break;
		}
		if (result == B_OK) {
			if (openMode & B_ERASE_FILE)
				openFlags |= O_TRUNC;
			if (openMode & B_OPEN_AT_END)
				openFlags |= O_APPEND;
			if (openMode & B_CREATE_FILE) {
				openFlags |= O_CREAT;
				if (openMode & B_FAIL_IF_EXISTS)
					openFlags |= O_EXCL;
				result = BPrivate::Storage::open(path, openFlags, S_IREAD | S_IWRITE,
										  newFd);
			} else
				result = BPrivate::Storage::open(path, openFlags, newFd);
			if (result == B_OK)
				fMode = openFlags;
		}
	} else
		result = B_BAD_VALUE;
	// set the new file descriptor
	if (result == B_OK) {
		result = set_fd(newFd);
		if (result != B_OK)
			BPrivate::Storage::close(newFd);
	}
	// finally set the BNode status
	set_status(result);
	return result;
}


/*! \brief Re-initializes the BFile to the file referred to by the
		   supplied path name relative to the specified BDirectory and
		   according to the specified open mode.
	\param dir the BDirectory, relative to which the file's path name is
		   given
	\param path the file's path name relative to \a dir
	\param openMode the mode in which the file should be opened
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a dir or \a path or bad \a openMode.
	- \c B_ENTRY_NOT_FOUND: File not found or failed to create file.
	- \c B_FILE_EXISTS: File exists and \c B_FAIL_IF_EXISTS was passed.
	- \c B_PERMISSION_DENIED: File permissions didn't allow operation.
	- \c B_NO_MEMORY: Insufficient memory for operation.
	- \c B_LINK_LIMIT: Indicates a cyclic loop within the file system.
	- \c B_BUSY: A node was busy.
	- \c B_FILE_ERROR: A general file error.
	- \c B_NO_MORE_FDS: The application has run out of file descriptors.
	\todo Implemented using SetTo(BEntry*, uint32). Check, if necessary
		  to reimplement!
*/
status_t
BFile::SetTo(const BDirectory* dir, const char* path, uint32 openMode)
{
	Unset();

	if (!dir)
		return (fCStatus = B_BAD_VALUE);

	status_t error = (dir && path ? B_OK : B_BAD_VALUE);
	BEntry entry;
	if (error == B_OK)
		error = entry.SetTo(dir, path);
	if (error == B_OK)
		error = SetTo(&entry, openMode);
	set_status(error);
	return error;
}


// Reports whether or not the file is readable.
bool
BFile::IsReadable() const
{
	return InitCheck() == B_OK
		&& ((fMode & O_RWMASK) == O_RDONLY || (fMode & O_RWMASK) == O_RDWR);
}


// Reports whether or not the file is writable.
bool
BFile::IsWritable() const
{
	return InitCheck() == B_OK
		&& ((fMode & O_RWMASK) == O_WRONLY || (fMode & O_RWMASK) == O_RDWR);
}


// Reads a number of bytes from the file into a buffer.
ssize_t
BFile::Read(void* buffer, size_t size)
{
	if (InitCheck() != B_OK)
		return InitCheck();
	return BPrivate::Storage::read(get_fd(), buffer, size);
}


// Reads a number of bytes from a certain position within the file
// into a buffer.
ssize_t
BFile::ReadAt(off_t location, void* buffer, size_t size)
{
	if (InitCheck() != B_OK)
		return InitCheck();
	if (location < 0)
		return B_BAD_VALUE;
	return BPrivate::Storage::read(get_fd(), buffer, location, size);
}


// Writes a number of bytes from a buffer into the file.
ssize_t
BFile::Write(const void* buffer, size_t size)
{
	if (InitCheck() != B_OK)
		return InitCheck();
	return BPrivate::Storage::write(get_fd(), buffer, size);
}


// Writes a number of bytes from a buffer at a certain position
// into the file.
ssize_t
BFile::WriteAt(off_t location, const void* buffer, size_t size)
{
	if (InitCheck() != B_OK)
		return InitCheck();
	if (location < 0)
		return B_BAD_VALUE;
	return BPrivate::Storage::write(get_fd(), buffer, location, size);
}


// Seeks to another read/write position within the file.
off_t
BFile::Seek(off_t offset, uint32 seekMode)
{
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;
	return BPrivate::Storage::seek(get_fd(), offset, seekMode);
}


// Gets the current read/write position within the file.
off_t
BFile::Position() const
{
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;
	return BPrivate::Storage::get_position(get_fd());
}


// Sets the size of the file.
status_t
BFile::SetSize(off_t size)
{
	if (InitCheck() != B_OK)
		return InitCheck();
	if (size < 0)
		return B_BAD_VALUE;
	struct stat statData;
	statData.st_size = size;
	return set_stat(statData, WSTAT_SIZE);
}


// Gets the size of the file.
status_t
BFile::GetSize(off_t* size) const
{
	return BStatable::GetSize(size);
}


// Assigns another BFile to this BFile.
BFile&
BFile::operator=(const BFile &file)
{
	if (&file != this) {
		// no need to assign us to ourselves
		Unset();
		if (file.InitCheck() == B_OK) {
			// duplicate the file descriptor
			int fd = -1;
			status_t status = BPrivate::Storage::dup(file.get_fd(), fd);
			// set it
			if (status == B_OK) {
				status = set_fd(fd);
				if (status == B_OK)
					fMode = file.fMode;
				else
					BPrivate::Storage::close(fd);
			}
			set_status(status);
		}
	}
	return *this;
}


// FBC
void BFile::_PhiloFile1() {}
void BFile::_PhiloFile2() {}
void BFile::_PhiloFile3() {}
void BFile::_PhiloFile4() {}
void BFile::_PhiloFile5() {}
void BFile::_PhiloFile6() {}


/*!	Gets the file descriptor of the BFile.

	To be used instead of accessing the BNode's private \c fFd member directly.

	\returns The file descriptor, or -1 if not properly initialized.
*/
int
BFile::get_fd() const
{
	return fFd;
}


//! Overrides BNode::close_fd() for binary compatibility with BeOS R5.
void
BFile::close_fd()
{
	BNode::close_fd();
}
