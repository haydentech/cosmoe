/*
 * Copyright 2002-2009, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include "storage_support.h"

#include <fcntl.h>
#include <string.h>

#include <compat/sys/stat.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <fs_info.h>
#include <Path.h>
#include <SymLink.h>

#include "kernel_interface.h"


BDirectory::BDirectory()
	:
	fDirFd(-1),
	fDir(NULL)
{
}


BDirectory::BDirectory(const BDirectory& dir)
	:
	fDirFd(-1),
	fDir(NULL)
{
	*this = dir;
}


BDirectory::BDirectory(const entry_ref* ref)
	:
	fDirFd(-1),
	fDir(NULL)
{
	SetTo(ref);
}


BDirectory::BDirectory(const node_ref* nref)
	:
	fDirFd(-1),
	fDir(NULL)
{
	SetTo(nref);
}


BDirectory::BDirectory(const BEntry* entry)
	:
	fDirFd(-1),
	fDir(NULL)
{
	SetTo(entry);
}


BDirectory::BDirectory(const char* path)
	:
	fDirFd(-1),
	fDir(NULL)
{
	SetTo(path);
}


/*! \brief Creates a BDirectory and initializes it to the directory referred
	to by the supplied path name relative to the specified BDirectory.
	\param dir the BDirectory, relative to which the directory's path name is
		   given
	\param path the directory's path name relative to \a dir
*/
BDirectory::BDirectory(const BDirectory *dir, const char *path)
	:
	fDirFd(-1),
	fDir(NULL)
{
	SetTo(dir, path);
}


BDirectory::~BDirectory()
{
	// Also called by the BNode destructor, but we rather try to avoid
	// problems with calling virtual functions in the base class destructor.
	// Depending on the compiler implementation an object may be degraded to
	// an object of the base class after the destructor of the derived class
	// has been executed.

	// Cosmoe
	if (fDir)
		::closedir(fDir);

	close_fd();
}


status_t
BDirectory::SetTo(const entry_ref* ref)
{
	Unset();	
	char path[B_PATH_NAME_LENGTH];
	status_t error = (ref ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		error = BPrivate::Storage::entry_ref_to_path(ref, path,
													 B_PATH_NAME_LENGTH);
	}
	if (error == B_OK)
		error = SetTo(path);
	set_status(error);
	return error;
}


status_t
BDirectory::SetTo(const node_ref* nref)
{
	Unset();
	status_t error = (nref ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		entry_ref ref(nref->device, nref->node, ".");
		error = SetTo(&ref);
	}
	set_status(error);
	return error;
}


status_t
BDirectory::SetTo(const BEntry* entry)
{
	if (!entry) {
		Unset();
		return (fCStatus = B_BAD_VALUE);
	}

	// open node
	entry_ref ref;
	status_t error = (entry ? B_OK : B_BAD_VALUE);
	if (error == B_OK && entry->InitCheck() != B_OK)
		error = B_BAD_VALUE;
	if (error == B_OK)
		error = entry->GetRef(&ref);
	if (error == B_OK)
		error = SetTo(&ref);
	set_status(error);
	return error;
}


status_t
BDirectory::SetTo(const char* path)
{
	Unset();

	if (!path)
		return (fCStatus = B_BAD_VALUE);

	if (path && strlen(path) > 256)
		return (fCStatus = B_NAME_TOO_LONG);

	struct stat path_stat;
	int exists = (stat(path, &path_stat) == 0);
	if (!exists)
		return (fCStatus = B_ENTRY_NOT_FOUND);

	if (!S_ISDIR(path_stat.st_mode))
		return (fCStatus = B_NOT_A_DIRECTORY);
	
	int newDirFd = -1;
	status_t result = BPrivate::Storage::open_dir(path, newDirFd, &fDir);
	if (result == B_OK) {
		// // We have to take care that BNode doesn't stick to a symbolic link.
		// // open_dir() does always traverse those. Therefore we open the FD for
		// // BNode (without the O_NOTRAVERSE flag).
		// int fd = -1;
		// result = BPrivate::Storage::open(path, O_RDWR, fd, true);
		// if (result == B_OK) {
		// 	result = set_fd(fd);
		// 	printf("set_fd result is %d\n", result);
		// 	if (result != B_OK)
		// 		BPrivate::Storage::close(fd);
		// }
		// 	else
		// {
		// 	printf("open result is %d\n", result);
		// }
		// if (result == B_OK)
		fDirFd = newDirFd;
		// else
		// 	BPrivate::Storage::close_dir(newDirFd);
	} 
	// finally set the BNode status
	set_status(result);
	//printf("bdirectory result 2 is %d\n", result);
	return result;
}


status_t
BDirectory::SetTo(const BDirectory* dir, const char* path)
{
	if (!dir || !path || BPrivate::Storage::is_absolute_path(path)) {
		Unset();
		return (fCStatus = B_BAD_VALUE);
	}

	if (strlen(path) == 0)
		return (fCStatus = B_ENTRY_NOT_FOUND);

	status_t error = (dir && path ? B_OK : B_BAD_VALUE);
	if (error == B_OK && BPrivate::Storage::is_absolute_path(path))
		error = B_BAD_VALUE;
	BEntry entry;
	if (error == B_OK)
		error = entry.SetTo(dir, path);
	if (error == B_OK)
		error = SetTo(&entry);
	set_status(error);
	return error;
}


status_t
BDirectory::GetEntry(BEntry* entry) const
{
	char output[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::dir_to_path(fDirFd, output, sizeof(output)-1) == B_OK) {
		return entry->SetTo(output);
	}

	return B_ERROR;
}


bool
BDirectory::IsRootDirectory() const
{
	char output[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::dir_to_path(fDirFd, output, sizeof(output)-1) == B_OK) {
		return (strcmp(output, "/") == 0);
	}

	return false;
}


status_t
BDirectory::FindEntry(const char* path, BEntry* entry, bool traverse) const
{
	if (path == NULL || entry == NULL)
		return B_BAD_VALUE;

	entry->Unset();

	// init a potentially abstract entry
	status_t status;
	if (InitCheck() == B_OK)
		status = entry->SetTo(this, path, traverse);
	else
		status = entry->SetTo(path, traverse);

	// fail, if entry is abstract
	if (status == B_OK && !entry->Exists()) {
		status = B_ENTRY_NOT_FOUND;
		entry->Unset();
	}

	return status;
}


bool
BDirectory::Contains(const char* path, int32 nodeFlags) const
{
	// check initialization and parameters
	if (InitCheck() != B_OK)
		return false;
	if (!path)
		return true;	// mimic R5 behavior

	// turn the path into a BEntry and let the other version do the work
	BEntry entry;
	if (BPrivate::Storage::is_absolute_path(path))
		entry.SetTo(path);
	else
		entry.SetTo(this, path);

	return Contains(&entry, nodeFlags);
}


bool
BDirectory::Contains(const BEntry* entry, int32 nodeFlags) const
{
	// check, if the entry exists at all
	if (entry == NULL || !entry->Exists() || InitCheck() != B_OK)
		return false;

	if (nodeFlags != B_ANY_NODE) {
		// test the node kind
		bool result = false;
		if ((nodeFlags & B_FILE_NODE) != 0)
			result = entry->IsFile();
		if (!result && (nodeFlags & B_DIRECTORY_NODE) != 0)
			result = entry->IsDirectory();
		if (!result && (nodeFlags & B_SYMLINK_NODE) != 0)
			result = entry->IsSymLink();
		if (!result)
			return false;
	}

	// If the directory is initialized, get the canonical paths of the dir and
	// the entry and check, if the latter is a prefix of the first one.
	BPath dirPath(this, ".", true);
	BPath entryPath(entry);
	if (dirPath.InitCheck() != B_OK || entryPath.InitCheck() != B_OK)
		return false;

	uint32 dirLen = strlen(dirPath.Path());

	if (!strncmp(dirPath.Path(), entryPath.Path(), dirLen)) {
		// if the paths are identical, return a match to stay consistent with
		// BeOS behavior.
		if (entryPath.Path()[dirLen] == '\0' || entryPath.Path()[dirLen] == '/')
			return true;
	}
	return false;
}


/*!	\brief Returns the BDirectory's next entry as a BEntry.
	Unlike GetNextDirents() this method ignores the entries "." and "..".
	\param entry a pointer to a BEntry to be initialized to the found entry
	\param traverse specifies whether to follow it, if the found entry
		   is a symbolic link.
	\note The iterator used by this method is the same one used by
		  GetNextRef(), GetNextDirents(), Rewind() and CountEntries().
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a entry.
	- \c B_ENTRY_NOT_FOUND: No more entries found.
	- \c B_PERMISSION_DENIED: Directory permissions didn't allow operation.
	- \c B_NO_MEMORY: Insufficient memory for operation.
	- \c B_LINK_LIMIT: Indicates a cyclic loop within the file system.
	- \c B_BUSY: A node was busy.
	- \c B_FILE_ERROR: A general file error.
	- \c B_NO_MORE_FDS: The application has run out of file descriptors.
*/
status_t
BDirectory::GetNextEntry(BEntry* entry, bool traverse)
{
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;

	size_t bufSize = sizeof(dirent) + B_FILE_NAME_LENGTH;
	char buffer[bufSize];
	dirent *ents = (dirent *)buffer;

	while (GetNextDirents(ents, bufSize, 1) == 1) {
		if ((strcmp(ents->d_name, ".") == 0) || (strcmp(ents->d_name, "..") == 0))
			continue;
		
		return entry->SetTo(this, ents->d_name, false);
	}

	return B_ENTRY_NOT_FOUND;

}

/*!	\brief Returns the BDirectory's next entry as an entry_ref.
	Unlike GetNextDirents() this method ignores the entries "." and "..".
	\param ref a pointer to an entry_ref to be filled in with the data of the
		   found entry
	\param traverse specifies whether to follow it, if the found entry
		   is a symbolic link.
	\note The iterator used be this method is the same one used by
		  GetNextEntry(), GetNextDirents(), Rewind() and CountEntries().
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a ref.
	- \c B_ENTRY_NOT_FOUND: No more entries found.
	- \c B_PERMISSION_DENIED: Directory permissions didn't allow operation.
	- \c B_NO_MEMORY: Insufficient memory for operation.
	- \c B_LINK_LIMIT: Indicates a cyclic loop within the file system.
	- \c B_BUSY: A node was busy.
	- \c B_FILE_ERROR: A general file error.
	- \c B_NO_MORE_FDS: The application has run out of file descriptors.
*/
status_t
BDirectory::GetNextRef(entry_ref* ref)
{
	if (ref == NULL)
		return B_BAD_VALUE;
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;

	// Cosmoe needs the full path in the entry_ref
	char dirPath[B_FILE_NAME_LENGTH];
	BPrivate::Storage::dir_to_path(fDirFd, dirPath, B_FILE_NAME_LENGTH);

	BPrivate::Storage::LongDirEntry longEntry;
	struct dirent* entry = longEntry.dirent();
	bool next = true;
	while (next) {
		if (GetNextDirents(entry, sizeof(longEntry), 1) != 1)
			return B_ENTRY_NOT_FOUND;

		next = (!strcmp(entry->d_name, ".")
			|| !strcmp(entry->d_name, ".."));
	}

	strlcat(dirPath, "/", B_FILE_NAME_LENGTH);
	strlcat(dirPath, entry->d_name, B_FILE_NAME_LENGTH);

	ref->device = 0;
	ref->directory = entry->d_ino;
	return ref->set_name(dirPath);
}

/*!	\brief Returns the BDirectory's next entries as dirent structures.
	Unlike GetNextEntry() and GetNextRef(), this method returns also
	the entries "." and "..".
	\param buf a pointer to a buffer to be filled with dirent structures of
		   the found entries
	\param count the maximal number of entries to be returned.
	\note The iterator used by this method is the same one used by
		  GetNextEntry(), GetNextRef(), Rewind() and CountEntries().
	\return
	- The number of dirent structures stored in the buffer, 0 when there are
	  no more entries to be returned.
	- \c B_BAD_VALUE: \c NULL \a buf.
	- \c B_PERMISSION_DENIED: Directory permissions didn't allow operation.
	- \c B_NO_MEMORY: Insufficient memory for operation.
	- \c B_NAME_TOO_LONG: The entry's name is too long for the buffer.
	- \c B_LINK_LIMIT: Indicates a cyclic loop within the file system.
	- \c B_BUSY: A node was busy.
	- \c B_FILE_ERROR: A general file error.
	- \c B_NO_MORE_FDS: The application has run out of file descriptors.
*/
int32
BDirectory::GetNextDirents(dirent* buf, size_t bufSize, int32 count)
{
	if (buf == NULL)
		return B_BAD_VALUE;
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;
	return BPrivate::Storage::read_dir(fDirFd, &fDir, buf, bufSize, count);
}


status_t
BDirectory::Rewind()
{
	if (InitCheck() != B_OK)
		return B_FILE_ERROR;
	return BPrivate::Storage::rewind_dir(fDir);
}


int32
BDirectory::CountEntries()
{
	status_t error = Rewind();
	if (error != B_OK)
		return error;
	int32 count = 0;
	BPrivate::Storage::LongDirEntry longEntry;
	struct dirent* entry = longEntry.dirent();
	while (error == B_OK) {
		if (GetNextDirents(entry, sizeof(longEntry), 1) != 1)
			break;
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
			count++;
	}
	Rewind();
	return (error == B_OK ? count : error);
}


status_t
BDirectory::CreateDirectory(const char* path, BDirectory* dir)
{
	if (!path)
		return B_BAD_VALUE;

	// get the actual (absolute) path using BEntry's help
	BEntry entry;
	if (InitCheck() == B_OK && !BPrivate::Storage::is_absolute_path(path))
		entry.SetTo(this, path);
	else
		entry.SetTo(path);
	status_t error = entry.InitCheck();
	BPath realPath;
	if (error == B_OK)
		error = entry.GetPath(&realPath);
	if (error == B_OK)
		error = BPrivate::Storage::create_dir(realPath.Path());
	if (error == B_OK && dir)
		error = dir->SetTo(realPath.Path());

	return error;
}


status_t
BDirectory::CreateFile(const char* path, BFile* file, bool failIfExists)
{
	if (!path)
		return B_BAD_VALUE;

	// Let BFile do the dirty job.
	uint32 openMode = B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE
		| (failIfExists ? B_FAIL_IF_EXISTS : 0);
	BFile tmpFile;
	BFile* realFile = file ? file : &tmpFile;
	status_t error = B_OK;
	if (InitCheck() == B_OK && !BPrivate::Storage::is_absolute_path(path))
		error = realFile->SetTo(this, path, openMode);
	else
		error = realFile->SetTo(path, openMode);
	if (error != B_OK && file) // mimic R5 behavior
		file->Unset();
	return error;
}


status_t
BDirectory::CreateSymLink(const char* path, const char* linkToPath,
	BSymLink* link)
{
	if (!path || !linkToPath)
		return B_BAD_VALUE;

	// get the actual (absolute) path using BEntry's help
	BEntry entry;
	if (InitCheck() == B_OK && !BPrivate::Storage::is_absolute_path(path))
		entry.SetTo(this, path);
	else
		entry.SetTo(path);
	status_t error = entry.InitCheck();
	BPath realPath;
	if (error == B_OK)
		error = entry.GetPath(&realPath);
	if (error == B_OK)
		error = BPrivate::Storage::create_link(realPath.Path(), linkToPath);
	if (error == B_OK && link)
		error = link->SetTo(realPath.Path());

	return error;
}


BDirectory&
BDirectory::operator=(const BDirectory& dir)
{
	if (&dir != this) {	// no need to assign us to ourselves
		Unset();
		if (dir.InitCheck() == B_OK) {
			*((BNode*)this) = dir;
			if (InitCheck() == B_OK) {
				// duplicate the file descriptor
				status_t status = BPrivate::Storage::dup_dir(dir.fDirFd, fDirFd);
				if (status != B_OK)
					Unset();
				set_status(status);
			}
		}
	}
	return *this;
}


status_t
BDirectory::GetStatFor(const char* path, struct stat* st) const
{
	return _GetStatFor(path, st);
}


status_t
BDirectory::_GetStatFor(const char* path, struct stat* st) const
{
	if (!st)
		return B_BAD_VALUE;
	if (InitCheck() != B_OK)
		return B_NO_INIT;
	status_t error = B_OK;
	if (path != NULL) {
		if (path[0] == '\0')
			return B_ENTRY_NOT_FOUND;
		else {
			BEntry entry(this, path);
			error = entry.InitCheck();
			if (error == B_OK)
				error = entry.GetStat(st);
		}
	} else
		error = GetStat(st);
	return error;
}


status_t
BDirectory::_GetStatFor(const char* path, struct stat_beos* st) const
{
	struct stat newStat;
	status_t error = _GetStatFor(path, &newStat);
	if (error != B_OK)
		return error;

	convert_to_stat_beos(&newStat, st);
	return B_OK;
}


// FBC
void BDirectory::_ErectorDirectory1() {}
void BDirectory::_ErectorDirectory2() {}
void BDirectory::_ErectorDirectory3() {}
void BDirectory::_ErectorDirectory4() {}
void BDirectory::_ErectorDirectory5() {}
void BDirectory::_ErectorDirectory6() {}


//! Closes the BDirectory's file descriptor.
void
BDirectory::close_fd()
{
	if (fDirFd >= 0) {
		BPrivate::Storage::close_dir(fDirFd);
		fDirFd = -1;
	}
	BNode::close_fd();
}


int
BDirectory::get_fd() const
{
	return fDirFd;
}


//	#pragma mark - C functions


// TODO: Check this method for efficiency.
status_t
create_directory(const char* path, mode_t mode)
{
	if (!path)
		return B_BAD_VALUE;

	// That's the strategy: We start with the first component of the supplied
	// path, create a BPath object from it and successively add the following
	// components. Each time we get a new path, we check, if the entry it
	// refers to exists and is a directory. If it doesn't exist, we try
	// to create it. This goes on, until we're done with the input path or
	// an error occurs.
	BPath dirPath;
	char* component;
	int32 nextComponent;
	do {
		// get the next path component
		status_t error = BPrivate::Storage::parse_first_path_component(path,
			component, nextComponent);
		if (error != B_OK)
			return error;

		// append it to the BPath
		if (dirPath.InitCheck() == B_NO_INIT)	// first component
			error = dirPath.SetTo(component);
		else
			error = dirPath.Append(component);
		delete[] component;
		if (error != B_OK)
			return error;
		path += nextComponent;

		// create a BEntry from the BPath
		BEntry entry;
		error = entry.SetTo(dirPath.Path(), true);
		if (error != B_OK)
			return error;

		// check, if it exists
		if (entry.Exists()) {
			// yep, it exists
			if (!entry.IsDirectory())	// but is no directory
				return B_NOT_A_DIRECTORY;
		} else {
			// it doesn't exist -- create it
			error = BPrivate::Storage::create_dir(dirPath.Path(), mode);
			if (error != B_OK)
				return error;
		}
	} while (nextComponent != 0);
	return B_OK;
}


// #pragma mark - symbol versions


