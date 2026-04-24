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
#include <config.h>

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#ifndef ENOATTR
#define ENOATTR ENODATA
#endif
#endif

mode_t __gUmask = 022;


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
		case EINVAL:
			error = B_BAD_VALUE;
			break;
		case ENAMETOOLONG:
			error = B_NAME_TOO_LONG;
			break;
		case ENOTDIR:
			error = B_NOT_A_DIRECTORY;
			break;
		case EISDIR:
			error = B_IS_A_DIRECTORY;
			break;
		case ENOTEMPTY:
			error = B_DIRECTORY_NOT_EMPTY;
			break;
		case ENOSPC:
			error = B_DEVICE_FULL;
			break;
		case EROFS:
			error = B_READ_ONLY_DEVICE;
			break;
		case EXDEV:
			error = B_CROSS_DEVICE_LINK;
			break;
		case ELOOP:
			error = B_LINK_LIMIT;
			break;
		case EMFILE:
		case ENFILE:
			error = B_NO_MORE_FDS;
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

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
namespace {

std::mutex sSymlinkAttrTypeLock;
std::unordered_map<std::string, uint32> sSymlinkAttrTypes;

static status_t
build_xattr_name(const char* attribute, char* buffer, size_t bufferSize)
{
	if (attribute == NULL || buffer == NULL)
		return B_BAD_VALUE;

	if (strlen(attribute) > B_ATTR_NAME_LENGTH)
		return B_NAME_TOO_LONG;

	if (snprintf(buffer, bufferSize, "user.%s", attribute) >= (int)bufferSize)
		return B_NAME_TOO_LONG;

	return B_OK;
}


static std::string
symlink_attr_key(const char* path, const char* attribute)
{
	std::string key(path ? path : "");
	key.push_back('\n');
	key += (attribute ? attribute : "");
	return key;
}

}
#endif

ssize_t
_kern_read_attr(int file, const char *attribute,
						uint32 type, off_t pos, void *buf, size_t count)
{
	(void)type;
	(void)pos;

	if (attribute == NULL || buf == NULL)
		return B_BAD_VALUE;

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		char xattrName[B_ATTR_NAME_LENGTH + 6];
		status_t nameError = build_xattr_name(attribute, xattrName,
			sizeof(xattrName));
		if (nameError != B_OK)
			return nameError;

		ssize_t result = lgetxattr(symlinkPath, xattrName, buf, count);
		if (result >= 0)
			return result;

		if (errno == ENODATA || errno == ENOATTR)
			return B_ENTRY_NOT_FOUND;

		if (errno == EOPNOTSUPP || errno == ENOTSUP || errno == EPERM) {
			ssize_t fallback = fs_read_attr(file, attribute, type, pos, buf,
				count);
			if (fallback >= 0)
				return fallback;
			if (fallback == -1)
				return errno;
			return fallback;
		}

		return convertErrno(errno);
	}
#endif

	ssize_t result = fs_read_attr(file, attribute, type, pos, buf, count);
	if (result >= 0)
		return result;
	if (result == -1)
		return errno;
	return result;
}


ssize_t
_kern_write_attr(int file,
						 const char *attribute, uint32 type, off_t pos,
						 const void *buf, size_t count)
{
	(void)pos;

	if (attribute == NULL || buf == NULL)
		return B_BAD_VALUE;

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		char xattrName[B_ATTR_NAME_LENGTH + 6];
		status_t nameError = build_xattr_name(attribute, xattrName,
			sizeof(xattrName));
		if (nameError != B_OK)
			return nameError;

		int error = lsetxattr(symlinkPath, xattrName, buf, count, 0);
		if (error == 0) {
			std::lock_guard<std::mutex> guard(sSymlinkAttrTypeLock);
			sSymlinkAttrTypes[symlink_attr_key(symlinkPath, attribute)] = type;
			return count;
		}

		return convertErrno(errno);
	}
#endif

	ssize_t result = fs_write_attr(file, attribute, type, pos, buf, count);
	if (result >= 0)
		return result;
	if (result == -1)
		return errno;
	return result;
}


status_t
_kern_rename_attr(int file, const char *oldName, int toFile,
						const char *newName)
{
	if (file < 0 || toFile < 0 || oldName == NULL || newName == NULL)
		return B_BAD_VALUE;

	if (strnlen(oldName, B_ATTR_NAME_LENGTH + 1) > B_ATTR_NAME_LENGTH
		|| strnlen(newName, B_ATTR_NAME_LENGTH + 1) > B_ATTR_NAME_LENGTH) {
		return B_NAME_TOO_LONG;
	}

	struct stat fromStat;
	status_t error = _kern_read_stat(file, NULL, false, &fromStat,
		sizeof(fromStat));
	if (error != B_OK)
		return error;

	struct stat toStat;
	error = _kern_read_stat(toFile, NULL, false, &toStat, sizeof(toStat));
	if (error != B_OK)
		return error;

	bool sameNode = fromStat.st_dev == toStat.st_dev
		&& fromStat.st_ino == toStat.st_ino;
	if (!sameNode)
		return B_NOT_SUPPORTED;

	if (strcmp(oldName, newName) == 0)
		return B_OK;

	attr_info info;
	error = _kern_stat_attr(file, oldName, &info);
	if (error != B_OK)
		return error;

	if (info.size < 0)
		return B_ERROR;

	size_t dataSize = (size_t)info.size;
	std::vector<char> data(std::max((size_t)1, dataSize));

	ssize_t bytesRead = _kern_read_attr(file, oldName, info.type, 0,
		data.data(), dataSize);
	if (bytesRead < 0)
		return bytesRead;
	if ((size_t)bytesRead != dataSize)
		return B_ERROR;

	ssize_t bytesWritten = _kern_write_attr(toFile, newName, info.type, 0,
		data.data(), dataSize);
	if (bytesWritten < 0)
		return bytesWritten;
	if ((size_t)bytesWritten != dataSize)
		return B_ERROR;

	error = _kern_remove_attr(file, oldName);
	if (error != B_OK) {
		// Best-effort rollback to avoid leaving duplicate attrs on failure.
		_kern_remove_attr(toFile, newName);
		return error;
	}

	return B_OK;
}


status_t
_kern_remove_attr(int file, const char *attr)
{
	if (attr == NULL)
		return B_BAD_VALUE;	

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		char xattrName[B_ATTR_NAME_LENGTH + 6];
		status_t nameError = build_xattr_name(attr, xattrName,
			sizeof(xattrName));
		if (nameError != B_OK)
			return nameError;

		if (lremovexattr(symlinkPath, xattrName) != 0) {
			int removeErrno = errno;

			// Some hosts reject or do not persist symlink xattrs. Fall back to
			// fd-based attribute removal to stay consistent with read/write paths.
			if (removeErrno == ENODATA || removeErrno == ENOATTR
				|| removeErrno == EOPNOTSUPP || removeErrno == ENOTSUP
				|| removeErrno == EPERM) {
				int result = fs_remove_attr(file, attr);
				if (result == 0) {
					std::lock_guard<std::mutex> guard(sSymlinkAttrTypeLock);
					sSymlinkAttrTypes.erase(symlink_attr_key(symlinkPath, attr));
					return B_OK;
				}

				if (result == -1)
					return errno;
				return result;
			}

			return convertErrno(removeErrno);
		}

		std::lock_guard<std::mutex> guard(sSymlinkAttrTypeLock);
		sSymlinkAttrTypes.erase(symlink_attr_key(symlinkPath, attr));
		return B_OK;
	}
#endif

	// fs_remove_attr is supposed to set errno properly upon failure,
	// but currently does not appear to. It isn't set consistent
	// with what is returned by R5::BNode::RemoveAttr(), and it isn't
	// set consistent with what the BeBook's claims it is set to either.
	int result = fs_remove_attr(file, attr);
	if (result == 0)
		return B_OK;
	if (result == -1)
		return errno;
	return result;
}


status_t
_kern_stat_attr(int file, const char *name, struct attr_info *ai)
{
	if (name == NULL || ai == NULL)
		return B_BAD_VALUE;

#if !defined(_WIN32) && defined(HAVE_SYS_XATTR_H)
	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		char xattrName[B_ATTR_NAME_LENGTH + 6];
		status_t nameError = build_xattr_name(name, xattrName,
			sizeof(xattrName));
		if (nameError != B_OK)
			return nameError;

		ssize_t size = lgetxattr(symlinkPath, xattrName, NULL, 0);
		if (size < 0) {
			if (errno == ENODATA || errno == ENOATTR)
				return B_ENTRY_NOT_FOUND;

			if (errno == EOPNOTSUPP || errno == ENOTSUP || errno == EPERM) {
				int fallback = fs_stat_attr(file, name, ai);
				if (fallback == 0)
					return B_OK;
				if (fallback == -1)
					return errno;
				return fallback;
			}

			return convertErrno(errno);
		}

		ai->size = size;
		ai->type = B_RAW_TYPE;
		{
			std::lock_guard<std::mutex> guard(sSymlinkAttrTypeLock);
			auto it = sSymlinkAttrTypes.find(symlink_attr_key(symlinkPath,
				name));
			if (it != sSymlinkAttrTypes.end())
				ai->type = it->second;
		}

		return B_OK;
	}
#endif

	int result = fs_stat_attr(file, name, ai);
	if (result == 0)
		return B_OK;
	if (result == -1)
		return errno;
	return result;
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
	char symlinkPath[B_PATH_NAME_LENGTH];
	bool isSymlinkFD = BPrivate::Storage::get_symlink_fd_path(fd, symlinkPath,
		sizeof(symlinkPath)) == B_OK;
	names.clear();

	auto append_attr_names = [&](ssize_t listSize,
		auto listCall) -> status_t {
		if (listSize < 0)
			return errno;
		if (listSize == 0)
			return B_OK;

		std::vector<char> list((size_t)listSize);
		ssize_t bytes = listCall(list.data(), list.size());
		if (bytes < 0)
			return errno;

		for (size_t i = 0; i < (size_t)bytes; ) {
			const char* entry = list.data() + i;
			size_t len = strlen(entry);
			if (len == 0)
				break;

			// Match fs_read_attr/fs_write_attr naming: expose without user. prefix.
			if (strncmp(entry, "user.", 5) == 0) {
				std::string stripped(entry + 5);
				if (std::find(names.begin(), names.end(), stripped)
					== names.end()) {
					names.emplace_back(std::move(stripped));
				}
			}

			i += len + 1;
		}

		return B_OK;
	};

	if (isSymlinkFD) {
		status_t error = append_attr_names(
			llistxattr(symlinkPath, NULL, 0),
			[&](char* buffer, size_t size) {
				return llistxattr(symlinkPath, buffer, size);
			});
		if (error != B_OK)
			return error;

		error = append_attr_names(
			flistxattr(fd, NULL, 0),
			[&](char* buffer, size_t size) {
				return flistxattr(fd, buffer, size);
			});
		if (error != B_OK)
			return error;
	} else {
		status_t error = append_attr_names(
			flistxattr(fd, NULL, 0),
			[&](char* buffer, size_t size) {
				return flistxattr(fd, buffer, size);
			});
		if (error != B_OK)
			return error;
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

	char symlinkPath[B_PATH_NAME_LENGTH];
	if (BPrivate::Storage::get_symlink_fd_path(file, symlinkPath,
		sizeof(symlinkPath)) == B_OK) {
		BPrivate::Storage::register_symlink_fd_path(attrFD, symlinkPath);
	}

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

	status_t error = load_attr_names_for_fd(dir, it->second.names);
	if (error != B_OK)
		return error;

	it->second.index = 0;
	return B_OK;
}


status_t
BPrivate::Storage::read_attr_dir(int dir, dirent& buffer)
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
	memset(&buffer, 0, sizeof(dirent));
	strlcpy(buffer.d_name, name.c_str(), sizeof(buffer.d_name));
	buffer.d_reclen = sizeof(dirent);
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
_kern_entry_ref_to_path(dev_t device, ino_t inode,
						const char *leaf, char *userPath, size_t pathLength)
{
	if (leaf == NULL || userPath == NULL)
		return B_BAD_VALUE;

	strlcpy(userPath, leaf, pathLength);
	return B_OK;
}


