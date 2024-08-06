/*
 * Copyright 2002-2012, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 */


#include <Entry.h>

#include <fcntl.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <compat/sys/stat.h>

#include <Directory.h>
#include <Path.h>
#include <SymLink.h>
#include "kernel_interface.h"
#include "storage_support.h"

#include <limits.h>


using namespace std;

// SYMLINK_MAX is needed by B_SYMLINK_MAX
// I don't know, why it isn't defined.
#ifndef SYMLINK_MAX
#define SYMLINK_MAX (16)
#endif

//	#pragma mark - struct entry_ref


/*! \struct entry_ref
	\brief A filesystem entry represented as a name in a concrete directory.
	
	entry_refs may refer to pre-existing (concrete) files, as well as non-existing
	(abstract) files. However, the parent directory of the file \b must exist.
	
	The result of this dichotomy is a blending of the persistence gained by referring
	to entries with a reference to their internal filesystem node and the flexibility gained
	by referring to entries by name.
	
	For example, if the directory in which the entry resides (or a
	directory further up in the hierarchy) is moved or renamed, the entry_ref will
	still refer to the correct file (whereas a pathname to the previous location of the
	file would now be invalid).
	
	On the other hand, say that the entry_ref refers to a concrete file. If the file
	itself is renamed, the entry_ref now refers to an abstract file with the old name
	(the upside in this case is that abstract entries may be represented by entry_refs
	without	preallocating an internal filesystem node for them).

	Cosmoe:
	And we throw most of the above logic out, because only the Be filesystem works like that.
	It has an index by device and inode.  But we can't throw the baby out with the
	bathwater either -- we need the source compatability, and entry_ref is used all over
	the place in Be/Haiku.  So we store a path in the entry_ref to make it all work
	as well as we can.  Device and directory are accurate, but not really used.
*/


//! Creates an unitialized entry_ref. 
entry_ref::entry_ref()
	:
	device((dev_t)-1),
	directory((ino_t)-1),
	name(NULL)
{
}

/*! \brief Creates an entry_ref initialized to the given file name in the given
	directory on the given device.
	
	\p name may refer to either a pre-existing file in the given
	directory, or a non-existent file. No explicit checking is done to verify validity of the given arguments, but
	later use of the entry_ref will fail if \p dev is not a valid device or \p dir
	is a not a directory on \p dev.
	
	\param dev the device on which the entry's parent directory resides
	\param dir the directory in which the entry resides
	\param name the leaf name of the entry, which is not required to exist
	\param dirpath the path to the entry, which is required to exist if given
*/
entry_ref::entry_ref(dev_t dev, ino_t dir, const char* name)
	:
	device(dev),
	directory(dir),
	name(NULL)
{
	set_name(name);
}


entry_ref::entry_ref(const entry_ref& ref)
	:
	device(ref.device),
	directory(ref.directory),
	name(NULL)
{
	set_name(ref.name);
}


entry_ref::~entry_ref()
{
	free(name);
}


status_t
entry_ref::set_name(const char* name)
{
	free(this->name);

	if (name == NULL) {
		this->name = NULL;
	} else {
		if (strchr(name, '/') == NULL) {
			printf("WARNING: setting entry_ref from relative path\n");
			printf("relative path: %s\n", name);
		}

		this->name = strdup(name);
		if (!this->name)
			return B_NO_MEMORY;
	}


	return B_OK;
}


bool
entry_ref::operator==(const entry_ref& ref) const
{
	//printf("this %ld, %ld, %s\n", device, directory, name);
	//printf("ref %ld, %ld, %s\n", ref.device, ref.directory, ref.name);
	return (device == ref.device
		&& directory == ref.directory
		&& (name == ref.name
			|| (name != NULL && ref.name != NULL
				&& strcmp(name, ref.name) == 0)));
}


bool
entry_ref::operator!=(const entry_ref& ref) const
{
	return !(*this == ref);
}


entry_ref&
entry_ref::operator=(const entry_ref& ref)
{
	if (this == &ref)
		return *this;

	device = ref.device;
	directory = ref.directory;
	set_name(ref.name);
	return *this;
}


//	#pragma mark - BEntry


/*!
	\class BEntry
	\brief A location in the filesystem
	
	The BEntry class defines objects that represent "locations" in the file system
	hierarchy.  Each location (or entry) is given as a name within a directory. For
	example, when you create a BEntry thus:
	
	\code
	BEntry entry("/boot/home/fido");
	\endcode
	
	...you're telling the BEntry object to represent the location of the file
	called fido within the directory \c "/boot/home".
	
	\author <a href='mailto:bonefish@users.sf.net'>Ingo Weinhold</a>
	\author <a href='mailto:tylerdauwalder@users.sf.net'>Tyler Dauwalder</a>
	\author <a href='mailto:scusack@users.sf.net'>Simon Cusack</a>
	
	\version 0.0.0
*/

//! Creates an uninitialized BEntry object.
/*!	Should be followed by a	call to one of the SetTo functions,
	or an assignment:
	- SetTo(const BDirectory*, const char*, bool)
	- SetTo(const entry_ref*, bool)
	- SetTo(const char*, bool)
	- operator=(const BEntry&)
*/
BEntry::BEntry()
	:
	fDir(NULL),
	fDirFd(-1),
	fName(NULL),
	fCStatus(B_NO_INIT)
{
}

//! Creates a BEntry initialized to the given directory and path combination.
/*!	If traverse is true and \c dir/path refers to a symlink, the BEntry will
	refer to the linked file; if false,	the BEntry will refer to the symlink itself.
	
	\param dir directory in which \a path resides
	\param path relative path reckoned off of \a dir
	\param traverse whether or not to traverse symlinks
	\see SetTo(const BDirectory*, const char *, bool)

*/
BEntry::BEntry(const BDirectory* dir, const char* path, bool traverse)
	:
	fDir(NULL),
	fDirFd(-1),
	fName(NULL),
	fCStatus(B_NO_INIT)
{
	SetTo(dir, path, traverse);
}

//! Creates a BEntry for the file referred to by the given entry_ref.
/*!	If traverse is true and \a ref refers to a symlink, the BEntry
	will refer to the linked file; if false, the BEntry will refer
	to the symlink itself.
	
	\param ref the entry_ref referring to the given file
	\param traverse whether or not symlinks are to be traversed
	\see SetTo(const entry_ref*, bool)
*/

BEntry::BEntry(const entry_ref* ref, bool traverse)
	:
	fDir(NULL),
	fDirFd(-1),
	fName(NULL),
	fCStatus(B_NO_INIT)
{
	SetTo(ref, traverse);
}

//! Creates a BEntry initialized to the given path.
/*!	If \a path is relative, it will
	be reckoned off the current working directory. If \a path refers to a symlink and
	traverse is true, the BEntry will refer to the linked file. If traverse is false,
	the BEntry will refer to the symlink itself.
	
	\param path the file of interest
	\param traverse whether or not symlinks are to be traversed	
	\see SetTo(const char*, bool)
	
*/
BEntry::BEntry(const char* path, bool traverse)
	:
	fDir(NULL),
	fDirFd(-1),
	fName(NULL),
	fCStatus(B_NO_INIT)
{
	SetTo(path, traverse);
}

//! Creates a copy of the given BEntry.
/*! \param entry the entry to be copied
	\see operator=(const BEntry&)
*/
BEntry::BEntry(const BEntry& entry)
	:
	fDir(NULL),
	fDirFd(-1),
	fName(NULL),
	fCStatus(B_NO_INIT)
{
	*this = entry;
}


BEntry::~BEntry()
{
	Unset();
}


status_t
BEntry::InitCheck() const
{
	return fCStatus;
}


bool
BEntry::Exists() const
{
	// just stat the beast
	struct stat st;
	return GetStat(&st) == B_OK;
}


/*! \brief Fills in a stat structure for the entry. The information is copied into
	the \c stat structure pointed to by \a result.
	
	\b NOTE: The BStatable object does not cache the stat structure; every time you 
	call GetStat(), fresh stat information is retrieved.
	
	\param result pointer to a pre-allocated structure into which the stat information will be copied
	\return
	- \c B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::GetStat(struct stat *result) const
{
	if (fCStatus != B_OK)
		return B_NO_INIT;

	BPath path;
	status_t status = this->GetPath(&path);
	if (status < 0)
		return status;
		
	return BPrivate::Storage::get_stat(path.Path(), result);
}


const char*
BEntry::Name() const
{
	if (fCStatus != B_OK)
		return NULL;

	return fName;
}


/*! \brief Reinitializes the BEntry to the path or directory path combination,
	resolving symlinks if traverse is true
	
	\return
	- \c B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::SetTo(const BDirectory* dir, const char* path, bool traverse)
{
	// check params
	if (!dir)
		return (fCStatus = B_BAD_VALUE);
	if (path && path[0] == '\0')	// R5 behaviour
		path = NULL;

	// if path is absolute, let the path-only SetTo() do the job
	if (BPrivate::Storage::is_absolute_path(path))
		return SetTo(path, traverse);

	Unset();

	if (dir->InitCheck() != B_OK)
		fCStatus = B_BAD_VALUE;

	fCStatus = B_OK;

	// get the dir's path
	char rootPath[B_PATH_NAME_LENGTH];
	fCStatus = BPrivate::Storage::dir_to_path(dir->get_fd(), rootPath,
										   B_PATH_NAME_LENGTH);
	// Concatenate our two path strings together
	if (fCStatus == B_OK && path) {
		// The concatenated strings must fit into our buffer.
		if (strlen(rootPath) + strlen(path) + 2 > B_PATH_NAME_LENGTH)
			fCStatus = B_NAME_TOO_LONG;
		else {
			strcat(rootPath, "/");
			strcat(rootPath, path);
		}
	}
	// set the resulting path
	if (fCStatus == B_OK)
		SetTo(rootPath, traverse);

	return fCStatus;
}
				  
/*! \brief Reinitializes the BEntry to the entry_ref, resolving symlinks if
	traverse is true

	\return
	- \c B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::SetTo(const entry_ref* ref, bool traverse)
{
	Unset();
	if (ref == NULL)
		return (fCStatus = B_BAD_VALUE);

	char path[B_PATH_NAME_LENGTH];

	fCStatus = BPrivate::Storage::entry_ref_to_path(ref, path,
													B_PATH_NAME_LENGTH);
	return (fCStatus == B_OK) ? SetTo(path, traverse) : fCStatus ;
}

/*! \brief Reinitializes the BEntry object to the path, resolving symlinks if
	traverse is true
	
	\return
	- \c B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::SetTo(const char* path, bool traverse)
{
	Unset();
	// check the argument
	fCStatus = (path ? B_OK : B_BAD_VALUE);
	if (fCStatus == B_OK)
		fCStatus = BPrivate::Storage::check_path_name(path);
	if (fCStatus == B_OK) {
		// Get the path and leaf portions of the given path
		char *pathStr, *leafStr;
		pathStr = leafStr = NULL;
		fCStatus = BPrivate::Storage::split_path(path, pathStr, leafStr);
		if (fCStatus == B_OK) {
			// Open the directory
			int dirFd;
			fCStatus = BPrivate::Storage::open_dir(pathStr, dirFd, &fDir);
			if (fCStatus == B_OK) {
				fCStatus = _SetTo(dirFd, leafStr, traverse);
				if (fCStatus != B_OK)
					BPrivate::Storage::close_dir(dirFd);		
			}
		}
		delete [] pathStr;
		delete [] leafStr;
	}
	return fCStatus;
}


void
BEntry::Unset()
{
	// Cosmoe: close the directory pointer
	if (fDir)
		::closedir(fDir);

	// Close the directory fd
	if (fDirFd >= 0) {
		BPrivate::Storage::close_dir(fDirFd);
	}
	
	// Free our leaf name
	free(fName);

	fDir = NULL;
	fDirFd = -1;
	fName = NULL;
	fCStatus = B_NO_INIT;
}

/*! \brief Gets an entry_ref structure for the BEntry.

	\param ref pointer to a preallocated entry_ref into which the result is copied
	\return
	- \c B_OK - Success
	- "error code" - Failure

 */
status_t
BEntry::GetRef(entry_ref* ref) const
{
	if (fCStatus != B_OK)
		return B_NO_INIT;

	if (ref == NULL)
		return B_BAD_VALUE;

	struct stat st;
	status_t error = BPrivate::Storage::get_stat(fDirFd, &st);
	if (error == B_OK) {
		char output[B_PATH_NAME_LENGTH];
		error = BPrivate::Storage::dir_to_path(fDirFd, output, sizeof(output)-1);
		if (error == B_OK) {
			strlcat(output, "/", B_PATH_NAME_LENGTH);
			strlcat(output, fName, B_PATH_NAME_LENGTH);
			ref->device = st.st_dev;
			ref->directory = st.st_ino;
			error = ref->set_name(output);
		}
	}
	return error;
}


status_t
BEntry::GetPath(BPath* path) const
{
	if (fCStatus != B_OK)
		return B_NO_INIT;

	if (path == NULL || fDirFd < 0)
		return B_BAD_VALUE;

	char output[B_PATH_NAME_LENGTH];

	if (BPrivate::Storage::dir_to_path(fDirFd, output, sizeof(output)-1) == B_OK) {
		strlcat(output, "/", B_PATH_NAME_LENGTH);
		strlcat(output, fName, B_PATH_NAME_LENGTH);
		return path->SetTo(output);
	}

	return B_ENTRY_NOT_FOUND;
}

/*! \brief Gets the parent of the BEntry as another BEntry.

	If the function fails, the argument is Unset(). Destructive calls to GetParent() are
	allowed, i.e.:
	
	\code
	BEntry entry("/boot/home/fido"); 
	status_t err; 
	char name[B_FILE_NAME_LENGTH]; 

	// Spit out the path components backwards, one at a time. 
	do {
		entry.GetName(name);
		printf("> %s\n", name);
	} while ((err=entry.GetParent(&entry)) == B_OK);

	// Complain for reasons other than reaching the top.
	if (err != B_ENTRY_NOT_FOUND)
		printf(">> Error: %s\n", strerror(err));
	\endcode
	
	will output:
	
	\code
	> fido
	> home
	> boot
	> .
	\endcode
	
	\param entry pointer to a pre-allocated BEntry object into which the result is stored
	\return
	- \c B_OK - Success
	- \c B_ENTRY_NOT_FOUND - Attempted to get the parent of the root directory \c "/"
	- "error code" - Failure
*/
status_t BEntry::GetParent(BEntry* entry) const
{
	// check parameter and initialization
	if (fCStatus != B_OK)
		return B_NO_INIT;
	if (entry == NULL)
		return B_BAD_VALUE;

	char parentPath[B_PATH_NAME_LENGTH];
	status_t status = BPrivate::Storage::dir_to_path(fDirFd, parentPath, B_PATH_NAME_LENGTH);
	if (status == B_OK) {
		// check whether we are the root directory
		// It is sufficient to check whether our path is "/".
		if (strcmp(parentPath, "/") == 0)
			return B_ENTRY_NOT_FOUND;
		
		entry->SetTo(parentPath);
		return entry->InitCheck();
	}
	
	// If we get this far, an error occured, so we Unset() the
	// argument as dictated by the BeBook
	entry->Unset();
	return status;
}

/*! \brief Gets the parent of the BEntry as a BDirectory. 

	If the function fails, the argument is Unset().
	
	\param dir pointer to a pre-allocated BDirectory object into which the result is stored
	\return
	- \c B_OK - Success
	- \c B_ENTRY_NOT_FOUND - Attempted to get the parent of the root directory \c "/"
	- "error code" - Failure
*/
status_t
BEntry::GetParent(BDirectory* dir) const
{
	// check parameter and initialization
	if (fCStatus != B_OK)
		return B_NO_INIT;
	if (dir == NULL)
		return B_BAD_VALUE;

	char parentPath[B_PATH_NAME_LENGTH];
	status_t status = BPrivate::Storage::dir_to_path(fDirFd, parentPath, B_PATH_NAME_LENGTH);
	if (status == B_OK) {
		// check whether we are the root directory
		// It is sufficient to check whether our path is "/".
		if (strcmp(parentPath, "/") == 0)
			return B_ENTRY_NOT_FOUND;
		
		dir->SetTo(parentPath);
		return dir->InitCheck();
	}
	
	// If we get this far, an error occured, so we Unset() the
	// argument as dictated by the BeBook
	dir->Unset();
	return status;
}

/*! \brief Gets the name of the entry's leaf.

	\c buffer must be pre-allocated and of sufficient
	length to hold the entire string. A length of \c B_FILE_NAME_LENGTH is recommended.

	\param buffer pointer to a pre-allocated string into which the result is copied
	\return
	- \c B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::GetName(char* buffer) const
{
	if (fCStatus != B_OK)
		return B_NO_INIT;
	if (buffer == NULL)
		return B_BAD_VALUE;

	strcpy(buffer, fName);
	return B_OK;
}

/*! \brief Renames the BEntry to path, replacing an existing entry if clobber is true.

	NOTE: The BEntry must refer to an existing file. If it is abstract, this method will fail.
	
	\param path Pointer to a string containing the new name for the entry.  May
	            be absolute or relative. If relative, the entry is renamed within its
	            current directory.
	\param clobber If \c false and a file with the name given by \c path already exists,
	               the method will fail. If \c true and such a file exists, it will
	               be overwritten.
	\return
	- \c B_OK - Success
	- \c B_ENTRY_EXISTS - The new location is already taken and \c clobber was \c false
	- \c B_ENTRY_NOT_FOUND - Attempted to rename an abstract entry
	- "error code" - Failure	

*/
status_t
BEntry::Rename(const char* path, bool clobber)
{
	// check parameter and initialization
	if (path == NULL)
		return B_BAD_VALUE;
	if (fCStatus != B_OK)
		return B_NO_INIT;
		
	status_t status = B_OK;
	// Convert the given path to an absolute path, if it isn't already.
	char fullPath[B_PATH_NAME_LENGTH];
	if (!BPrivate::Storage::is_absolute_path(path)) {
		// Convert our directory to an absolute pathname
		status = BPrivate::Storage::dir_to_path(fDirFd, fullPath,
												B_PATH_NAME_LENGTH);
		if (status == B_OK) {
			// Concatenate our pathname to it
			strcat(fullPath, "/");
			strcat(fullPath, path);
			path = fullPath;
		}
	}
	// Check, whether the file does already exist, if clobber is false.
	if (status == B_OK && !clobber) {
		// We're not supposed to kill an already-existing file,
		// so we'll try to figure out if it exists by stat()ing it.
		BPrivate::Storage::Stat s;
		status = BPrivate::Storage::get_stat(path, &s);
		if (status == B_OK)
			status = B_FILE_EXISTS;
		else if (status == B_ENTRY_NOT_FOUND)
			status = B_OK;
	}
	// Turn ourselves into a pathname, rename ourselves
	if (status == B_OK) {
		BPath oldPath;
		status = GetPath(&oldPath);
		if (status == B_OK) {
			status = BPrivate::Storage::rename(oldPath.Path(), path);
			if (status == B_OK)
				status = SetTo(path, false);
		}
	}
	return status;
}

/*! \brief Moves the BEntry to directory or directory+path combination, replacing an existing entry if clobber is true.

	NOTE: The BEntry must refer to an existing file. If it is abstract, this method will fail.
	
	\param dir Pointer to a pre-allocated BDirectory into which the entry should be moved.
	\param path Optional new leaf name for the entry. May be a simple leaf or a relative path;
	            either way, \c path is reckoned off of \c dir. If \c NULL, the entry retains
	            its previous leaf name.
	\param clobber If \c false and an entry already exists at the specified destination,
	               the method will fail. If \c true and such an entry exists, it will
	               be overwritten.
	\return
	- \c B_OK - Success
	- \c B_ENTRY_EXISTS - The new location is already taken and \c clobber was \c false
	- \c B_ENTRY_NOT_FOUND - Attempted to move an abstract entry
	- "error code" - Failure	
*/
status_t
BEntry::MoveTo(BDirectory* dir, const char* path, bool clobber)
{
	// check parameters and initialization
	if (fCStatus != B_OK)
		return B_NO_INIT;
	if (dir == NULL)
		return B_BAD_VALUE;
	if (dir->InitCheck() != B_OK)
		return B_BAD_VALUE;
	// NULL path simply means move without renaming
	if (path == NULL)
		path = fName;

	status_t status = B_OK;
	// Determine the absolute path of the target entry.
	if (!BPrivate::Storage::is_absolute_path(path)) {
		// Convert our directory to an absolute pathname
		char fullPath[B_PATH_NAME_LENGTH];
		status = BPrivate::Storage::dir_to_path(dir->get_fd(), fullPath,
												B_PATH_NAME_LENGTH);
		// Concatenate our pathname to it
		if (status == B_OK) {
			strcat(fullPath, "/");
			strcat(fullPath, path);
			path = fullPath;
		}
	}
	// Now let rename do the dirty work
	if (status == B_OK)
		status = Rename(path, clobber);
	return status;
}

/*! \brief Removes the entry from the file system.

	NOTE: If any file descriptors are open on the file when Remove() is called,
	the chunk of data they refer to will continue to exist until all such file
	descriptors are closed. The BEntry object, however, becomes abstract and
	no longer refers to any actual data in the filesystem.
	
	\return
	- B_OK - Success
	- "error code" - Failure
*/
status_t
BEntry::Remove()
{
	if (fCStatus != B_OK)
		return B_NO_INIT;

	BPath path;
	status_t status;

	status = GetPath(&path);
	if (status != B_OK)
		return status;
		
	return BPrivate::Storage::remove(path.Path());
}


/*! \brief	Returns true if the BEntry and \c item refer to the same entry or
			if they are both uninitialized.
			
	\return
	- true - Both BEntry objects refer to the same entry or they are both uninitialzed
	- false - The BEntry objects refer to different entries
 */
bool
BEntry::operator==(const BEntry& item) const
{
	// First check statuses
	if (this->InitCheck() != B_OK && item.InitCheck() != B_OK) {
		return true;
	} else if (this->InitCheck() == B_OK && item.InitCheck() == B_OK) {

		// Directories don't compare well directly, so we'll
		// compare entry_refs instead
		entry_ref ref1, ref2;
		if (this->GetRef(&ref1) != B_OK)
			return false;
		if (item.GetRef(&ref2) != B_OK)
			return false;
		return (ref1 == ref2);

	} else {
		return false;
	}

}

/*! \brief	Returns false if the BEntry and \c item refer to the same entry or
			if they are both uninitialized.
			
	\return
	- true - The BEntry objects refer to different entries
	- false - Both BEntry objects refer to the same entry or they are both uninitialzed
 */
bool
BEntry::operator!=(const BEntry& item) const
{
	return !(*this == item);
}


BEntry&
BEntry::operator=(const BEntry& item)
{
	if (this == &item)
		return *this;

	Unset();
	if (item.fCStatus == B_OK) {
		fCStatus = BPrivate::Storage::dup_dir(item.fDirFd, fDirFd);
		if (fDirFd >= 0)
			fCStatus = _SetName(item.fName);
		else
			fCStatus = fDirFd;

		if (fCStatus != B_OK)
			Unset();
	}

	return *this;
}


void BEntry::_PennyEntry1(){}
void BEntry::_PennyEntry2(){}
void BEntry::_PennyEntry3(){}
void BEntry::_PennyEntry4(){}
void BEntry::_PennyEntry5(){}
void BEntry::_PennyEntry6(){}


/*!	Updates the BEntry with the data from the stat structure according
	to the \a what mask.

	\param st The stat structure to set.
	\param what A mask

	\returns A status code.
	\retval B_OK Everything went fine.
	\retval B_FILE_ERROR There was an error writing to the BEntry object.
*/
status_t
BEntry::set_stat(struct stat& st, uint32 what)
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;
	
	BPath path;
	status_t status;
	
	status = GetPath(&path);
	if (status != B_OK)
		return status;
	
	return BPrivate::Storage::set_stat(path.Path(), st, what);
}


/*!	Sets the entry to point to the entry specified by the path \a path
	relative to the given directory.

	If \a traverse is \c true and the given entry is a symbolic link, the
	object is recursively set to point to the entry pointed to by the symlink.

	If \a path is an absolute path, \a dirFD is ignored.

	If \a dirFD is -1, \a path is considered relative to the current directory
	(unless it is an absolute path).

	The ownership of the file descriptor \a dirFD is transferred to the
	method, regardless of whether it succeeds or fails. The caller must not
	close the FD afterwards.

	\param dirFD File descriptor of a directory relative to which path is to
		be considered. May be -1 if the current directory shall be considered.
	\param path Pointer to a path relative to the given directory.
	\param traverse If \c true and the given entry is a symbolic link, the
		object is recursively set to point to the entry linked to by the
		symbolic link.

	\returns \c B_OK on success, or an error code on failure.
*/
status_t
BEntry::_SetTo(int dirFD, const char* path, bool traverse)
{
	// Verify that path is valid
	status_t error = BPrivate::Storage::check_entry_name(path);
	if (error != B_OK)
		return error;
	// Check whether the entry is abstract or concrete.
	// We try traversing concrete entries only.
	BPrivate::Storage::LongDirEntry dirEntry;
	struct dirent* entry = dirEntry.dirent();
	bool isConcrete = (BPrivate::Storage::find_dir(dirFD, &fDir, path, entry,
											sizeof(dirEntry)) == B_OK);
	if (traverse && isConcrete && false) {	// Cosmoe: this traversal code is broken
		// Though the link traversing strategy is iterative, we introduce
		// some recursion, since we are using BSymLink, which may be
		// (currently is) implemented using BEntry. Nevertheless this is
		// harmless, because BSymLink does, of course, not want to traverse
		// the link.

		// convert the dir FD into a BPath
		char dirPathname[B_PATH_NAME_LENGTH];
		error = BPrivate::Storage::dir_to_path(dirFD, dirPathname, B_PATH_NAME_LENGTH);

		BPath dirPath(dirPathname);
		if (error == B_OK)
			error = dirPath.InitCheck();
		BPath linkPath;
		if (error == B_OK)
			linkPath.SetTo(dirPath.Path(), path);
		if (error == B_OK) {
			// Here comes the link traversing loop: A BSymLink is created
			// from the dir and the leaf name, the link target is determined,
			// the target's dir and leaf name are got and so on.
			bool isLink = true;
			int32 linkLimit = B_MAX_SYMLINKS;
			while (error == B_OK && isLink && linkLimit > 0) {
				linkLimit--;
				// that's OK with any node, even if it's not a symlink
				BSymLink link(linkPath.Path());
				error = link.InitCheck();
				if (error == B_OK) {
					isLink = link.IsSymLink();
					if (isLink) {
						// get the path to the link target
						ssize_t linkSize = link.MakeLinkedPath(dirPath.Path(),
															   &linkPath);
						if (linkSize < 0)
							error = linkSize;
						// get the link target's dir path
						if (error == B_OK)
							error = linkPath.GetParent(&dirPath);
					}
				}
			}
			// set the new values
			if (error == B_OK) {
				if (isLink)
					error = B_LINK_LIMIT;
				else {
					int newDirFd = -1;
					error = BPrivate::Storage::open_dir(dirPath.Path(), newDirFd, NULL);
					if (error == B_OK) {
						// If we are successful, we are responsible for the
						// supplied FD. Thus we close it.
						BPrivate::Storage::close_dir(dirFD);
						dirFD = -1;
						fDirFd = newDirFd;
						// handle "/", which has a "" Leaf()
						if (linkPath == "/")
							_SetName(".");
						else
							_SetName(linkPath.Leaf());
					}
				}
			}
		}	// getting the dir path for the FD
	} else {
		// don't traverse: either the flag is not set or the entry is abstract
		fDirFd = dirFD;
		_SetName(path);
	}
	return error;
}


/*!	Handles string allocation, deallocation, and copying for the
	leaf name of the entry.

	\param name The leaf \a name of the entry.

	\returns A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \a name is \c NULL.
	\retval B_NO_MEMORY Ran out of memory trying to allocate \a name.
*/
status_t
BEntry::_SetName(const char* name)
{
	if (name == NULL)
		return B_BAD_VALUE;

	free(fName);

	fName = strdup(name);
	if (fName == NULL)
		return B_NO_MEMORY;

	return B_OK;
}


/*!	Debugging function, dumps the given entry to stdout.

	\param name A pointer to a string to be printed along with the dump for
		   identification purposes.
*/
void
BEntry::_Dump(const char* name)
{
	if (name != NULL) {
		printf("------------------------------------------------------------\n");
		printf("%s\n", name);
		printf("------------------------------------------------------------\n");
	}

	printf("fCStatus == %" B_PRId32 "\n", fCStatus);
		
	printf("leaf == '%s'\n", fName);
	printf("\n");

}


status_t
BEntry::_GetStat(struct stat* st) const
{
	if (fCStatus != B_OK)
		return B_NO_INIT;
//FIXME COSMOE
	return B_ERROR; // _kern_read_stat(fDirFd, fName, false, st, sizeof(struct stat));
}


status_t
BEntry::_GetStat(struct stat_beos* st) const
{
	struct stat newStat;
	status_t error = _GetStat(&newStat);
	if (error != B_OK)
		return error;

	convert_to_stat_beos(&newStat, st);
	return B_OK;
}


// #pragma mark -

// get_ref_for_path
/*!	\brief Returns an entry_ref for a given path.
	\param path The path name referring to the entry
	\param ref The entry_ref structure to be filled in
	\return
	- \c B_OK - Everything went fine.
	- \c B_BAD_VALUE - \c NULL \a path or \a ref.
	- \c B_ENTRY_NOT_FOUND - A (non-leaf) path component does not exist.
	- \c B_NO_MEMORY - Insufficient memory for successful completion.
*/
status_t
get_ref_for_path(const char* path, entry_ref* ref)
{
	status_t error = path && ref ? B_OK : B_BAD_VALUE;
	if (error == B_OK) {
		BEntry entry(path);
		error = entry.InitCheck();
		if (error == B_OK)
			error = entry.GetRef(ref);
	}
	return error;
}


bool
operator<(const entry_ref& a, const entry_ref& b)
{
	return (a.device < b.device
		|| (a.device == b.device
			&& (a.directory < b.directory
			|| (a.directory == b.directory
				&& ((a.name == NULL && b.name != NULL)
				|| (a.name != NULL && b.name != NULL
					&& strcmp(a.name, b.name) < 0))))));
}



