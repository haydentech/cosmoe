//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//---------------------------------------------------------------------
/*!
	\file kernel_interface.h
	This is the private interface used by the Storage Kit
	to communicate with the kernel
*/

#ifndef _STORAGE_KIT_KERNEL_INTERFACE_H
#define _STORAGE_KIT_KERNEL_INTERFACE_H

#include <SupportKit.h>

// For typedefs
#include <dirent.h>			// For dirent
#include <sys/stat.h>		// For struct stat
#include <fs_info.h>		// File sytem information functions, structs, defines

// Forward Declarations
struct entry_ref;

//! Private Storage Kit Namespace
/*! Encompasses the functions used internally by the Storage Kit to
	interface with the kernel, as well as various internal support functions,
	data types, and type aliases. */
namespace BPrivate {
namespace Storage {


// For convenience:
//struct LongDirEntry : DirEntry { char _buffer[B_FILE_NAME_LENGTH]; };


//------------------------------------------------------------------------------
// Device Functions
//------------------------------------------------------------------------------
/*! Returns information about the file system on the specified device. */
status_t stat_dev(dev_t dev, fs_info* info);


//------------------------------------------------------------------------------
// Attribute Directory Functions
//------------------------------------------------------------------------------
/*! Opens the attribute directory of a given file. */
status_t open_attr_dir(int file, int &result);

/*! Rewinds the given attribute directory. */
status_t rewind_attr_dir(int dir);

/*! Returns the next item in the given attribute directory, or
	B_ENTRY_NOT_FOUND if at the end of the list. */
status_t read_attr_dir(int dir, dirent &buffer);

/*! Closes an attribute directory previously opened with open_attr_dir(). */
status_t close_attr_dir(int dir);


//------------------------------------------------------------------------------
// Directory Functions
//------------------------------------------------------------------------------
/*! \brief Opens the given directory. Sets result to a properly "unitialized" directory
	if the function fails. */
status_t open_dir(const char *path, int &result, DIR** dir);

/*! Iterates through the given directory searching for an entry whose name
	matches that given by name. On success, places the dirent in result
	and returns B_OK. On failures, returns an error code and sets result to
	BPrivate::Storage::NullDir.
	
	<b>Note:</b> This call modifies the internal position marker of dir. */
status_t find_dir(int dir, DIR** dirDir, const char *name, dirent *result,
				   size_t length);

/*! Calls the other version of BPrivate::Storage::find_dir() and stores the results
	in the given entry_ref. */
status_t find_dir(int dir, DIR** dirDir, const char *name, entry_ref *result);

/*! Creates a duplicate of the given directory and places it in result if successful,
	returning B_OK. Returns an error code and sets result to -1 if
	unsuccessful. */
status_t dup_dir(int dir, int &result);

/*! Closes the given directory. */
status_t close_dir(int dir);

//------------------------------------------------------------------------------
// SymLink functions
//------------------------------------------------------------------------------
//! Creates a new symbolic link.
status_t create_link(const char *path, const char *linkToPath);


//------------------------------------------------------------------------------
// Miscellaneous Functions
//------------------------------------------------------------------------------

/*! Converts the given directory into an absolute pathname, returning the
	result in the string of length size pointed to by result (a size of
	B_PATH_NAME_LENGTH is a good idea).
	
	Returns B_OK if successful.
	
	If dir is < 0 or result is NULL, B_BAD_VALUE
	is returned. Otherwise, an error code is returned. The state of result after
	an error is undefined.
*/
status_t dir_to_path(int dir, char *result, size_t size);

/*!	\brief Returns the canonical representation of a given path referring to an
	potentially abstract entry in an existing directory. */
status_t get_canonical_path(const char *path, char *result, size_t size);

/*!	\brief Returns the canonical representation of a given path referring to an
	existing directory. */
status_t get_canonical_dir_path(const char *path, char *result, size_t size);

/*! Associates a file descriptor with a no-traverse symlink path. */
status_t register_symlink_fd_path(int fd, const char* path);

/*! Copies no-traverse symlink metadata from one descriptor to another. */
void inherit_symlink_fd_path(int fromFD, int toFD);

/*! Retrieves a no-traverse symlink path for a descriptor. */
status_t get_symlink_fd_path(int fd, char* buffer, size_t size);

/*! Removes no-traverse symlink metadata for a descriptor. */
void unregister_symlink_fd_path(int fd);


};	// namespace Storage
};	// namespace BPrivate

#endif	// _STORAGE_KIT_KERNEL_INTERFACE_H


