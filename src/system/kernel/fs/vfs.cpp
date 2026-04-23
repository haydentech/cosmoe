//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//----------------------------------------------------------------------
/*!
	\file kernel_interface.cpp
	Platform-independent implementation of kernel interface functions
	that work identically on both POSIX and Windows platforms.
*/

#include "kernel_interface.h"
#include "storage_support.h"

#include <OS.h>
#include <fs_info.h>
#include <fs_attr.h>
#include <fs_query.h>
#include <Entry.h>

#include <algorithm>
#include <new>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <syscalls.h>

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#endif

mode_t __gUmask = 022;

//------------------------------------------------------------------------------
// Device Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::stat_dev(dev_t dev, fs_info* info)
{
	return fs_stat_dev(dev, info);
}

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

status_t convertErrno(int result)
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
// Attribute Functions
//------------------------------------------------------------------------------

ssize_t
_kern_read_attr(int file, const char *attribute,
						uint32 type, off_t pos, void *buf, size_t count)
{
	if (attribute == NULL || buf == NULL)
		return B_BAD_VALUE;

	ssize_t result = fs_read_attr(file, attribute, type, pos, buf, count);
	return (result == -1 ? errno : result);
}

ssize_t
_kern_write_attr(int file,
						 const char *attribute, uint32 type, off_t pos,
						 const void *buf, size_t count)
{
	if (attribute == NULL || buf == NULL)
		return B_BAD_VALUE;

	ssize_t result = fs_write_attr(file, attribute, type, pos, buf, count);
	return (result == -1 ? errno : result);
}

status_t
_kern_rename_attr(int file, const char *oldName, int toFile,
						const char *newName)
{
	// FIXME does not use toFile yet
	status_t error = (oldName && newName ? B_OK : B_BAD_VALUE);
	// Figure out how much data there is
	attr_info info;
	if (error == B_OK) {
		error = _kern_stat_attr(file, oldName, &info);
		if (error != B_OK)
			error = B_BAD_VALUE;	// This is what R5::BNode returns...		
	}
	// Alloc a buffer
	char *data = NULL;
	if (error == B_OK) {
		// alloc at least one byte
		data = new(std::nothrow) char[std::max(info.size, (off_t)1LL)];
		if (data == NULL)
			error = B_NO_MEMORY;		
	}
	// Read in the data
	if (error == B_OK) {
		ssize_t size = _kern_read_attr(file, oldName, info.type, 0, data, info.size);
		if (size != info.size) {
			if (size < 0)
				error = size;
			else
				error = B_ERROR;
		}		
	}
	// Write it to the new attribute
	if (error == B_OK) {
		ssize_t size = 0;
		if (info.size > 0)
			size = _kern_write_attr(file, newName, info.type, 0, data, info.size);
		if (size != info.size) {
			if (size < 0)
				error = size;
			else
				error = B_ERROR;
		}	
	}
	// free the buffer
	if (data)
		delete[] data;
	// Remove the old attribute
	if (error == B_OK)
		error = _kern_remove_attr(file, oldName);
	return error;
}

status_t
_kern_remove_attr(int file, const char *attr)
{
	if (attr == NULL)
		return B_BAD_VALUE;	

	// fs_remove_attr is supposed to set errno properly upon failure,
	// but currently does not appear to. It isn't set consistent
	// with what is returned by R5::BNode::RemoveAttr(), and it isn't
	// set consistent with what the BeBook's claims it is set to either.
	return fs_remove_attr(file, attr) == -1 ? errno : B_OK;
}

status_t
_kern_stat_attr(int file, const char *name, struct attr_info *ai)
{
	if (name == NULL || ai == NULL)
		return B_BAD_VALUE;

	return (fs_stat_attr(file, name, ai) == -1) ? errno : B_OK;
}

//------------------------------------------------------------------------------
// Attribute Directory Functions
//------------------------------------------------------------------------------

namespace {

struct AttrDirState {
	std::vector<std::string> names;
	size_t index;
};

std::mutex sAttrDirLock;
std::unordered_map<int, AttrDirState> sAttrDirs;

static status_t
load_attr_names_for_fd(int fd, std::vector<std::string>& names)
{
#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
	ssize_t listSize = flistxattr(fd, NULL, 0);
	if (listSize < 0)
		return errno;

	if (listSize == 0) {
		names.clear();
		return B_OK;
	}

	std::vector<char> list((size_t)listSize);
	ssize_t bytes = flistxattr(fd, list.data(), list.size());
	if (bytes < 0)
		return errno;

	names.clear();
	for (size_t i = 0; i < (size_t)bytes; ) {
		const char* entry = list.data() + i;
		size_t len = strlen(entry);
		if (len == 0)
			break;

		// Match fs_read_attr/fs_write_attr naming: expose without user. prefix.
		if (strncmp(entry, "user.", 5) == 0)
			names.emplace_back(entry + 5);

		i += len + 1;
	}
	return B_OK;
#else
	(void)fd;
	names.clear();
	return B_ERROR;
#endif
}

}

status_t
BPrivate::Storage::open_attr_dir(int file, int &result)
{
	result = -1;
	if (file < 0)
		return B_BAD_VALUE;

	int attrFD = _kern_dup(file);
	if (attrFD < 0)
		return errno;

	AttrDirState state;
	state.index = 0;
	status_t error = load_attr_names_for_fd(attrFD, state.names);
	if (error != B_OK) {
		_kern_close(attrFD);
		return error;
	}

	{
		std::lock_guard<std::mutex> guard(sAttrDirLock);
		sAttrDirs[attrFD] = std::move(state);
	}

	result = attrFD;
	return B_OK;
}

status_t
_kern_rewind_attr_dir(int dir)
{
	if (dir < 0)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sAttrDirLock);
	auto it = sAttrDirs.find(dir);
	if (it == sAttrDirs.end())
		return B_BAD_VALUE;
	it->second.index = 0;
	return B_OK;
}

status_t
BPrivate::Storage::read_attr_dir(int dir, BPrivate::Storage::DirEntry& buffer)
{
	if (dir < 0)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sAttrDirLock);
	auto it = sAttrDirs.find(dir);
	if (it == sAttrDirs.end())
		return B_BAD_VALUE;

	AttrDirState& state = it->second;
	if (state.index >= state.names.size())
		return B_ENTRY_NOT_FOUND;

	const std::string& name = state.names[state.index++];
	memset(&buffer, 0, sizeof(DirEntry));
	strlcpy(buffer.d_name, name.c_str(), sizeof(buffer.d_name));
	buffer.d_reclen = sizeof(DirEntry);
	return B_OK;
}

status_t
BPrivate::Storage::close_attr_dir(int dir)
{
	if (dir < 0)
		return B_BAD_VALUE;

	{
		std::lock_guard<std::mutex> guard(sAttrDirLock);
		sAttrDirs.erase(dir);
	}

	return (_kern_close(dir) == -1) ? errno : B_OK;
}

//------------------------------------------------------------------------------
// Query Functions
//------------------------------------------------------------------------------

status_t
_kern_open_query(dev_t device, const char *query, uint32 flags, int &result)
{
	if (flags & B_LIVE_QUERY)
		return B_BAD_VALUE;
	result = -1;
	return (result < 0) ? errno : B_OK;
}

//------------------------------------------------------------------------------
// Miscellaneous Functions
//------------------------------------------------------------------------------

status_t
BPrivate::Storage::entry_ref_to_path(const struct entry_ref *ref, char *result,
							   size_t size)
{
	if (ref == NULL || ref->name == NULL)
		return B_BAD_VALUE;

	strlcpy(result, ref->name, size);
	return B_OK;
}

bool
BPrivate::Storage::entry_ref_is_root_dir(const entry_ref *ref)
{
	return ref && ref->name[0] == '/' && ref->name[1] == 0;
}

status_t
BPrivate::Storage::set_volume_name(dev_t device, const char *name)
{
	// check parameter and initialization
	status_t error = (name ? B_OK : B_BAD_VALUE);
	if (error == B_OK && strlen(name) >= B_FILE_NAME_LENGTH)
		error = B_NAME_TOO_LONG;

	// replace the name and let it be written
	if (error == B_OK) {
		fs_info info;
		strncpy(info.volume_name, name, sizeof(info.volume_name));
		//error = _kern_write_fs_info(device, &info, FS_WRITE_FSINFO_NAME);
	}
	return error;
}

#ifndef _WIN32
// Unix/POSIX-specific path canonicalization
// Windows version is in kernel_interface.WIN.cpp

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
BPrivate::Storage::get_canonical_path(const char *path, char *&result)
{
	status_t error = (path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		result = new(std::nothrow) char[B_PATH_NAME_LENGTH];
		if (!result)
			error = B_NO_MEMORY;
		if (error == B_OK) {
			error = get_canonical_path(path, result, B_PATH_NAME_LENGTH);
			if (error != B_OK) {
				delete[] result;
				result = NULL;
			}
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

status_t
BPrivate::Storage::get_canonical_dir_path(const char *path, char *&result)
{
	status_t error = (path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		result = new(std::nothrow) char[B_PATH_NAME_LENGTH];
		if (!result)
			error = B_NO_MEMORY;
		if (error == B_OK) {
			error = get_canonical_dir_path(path, result, B_PATH_NAME_LENGTH);
			if (error != B_OK) {
				delete[] result;
				result = NULL;
			}
		}
	}
	return error;
}

#endif // !_WIN32
