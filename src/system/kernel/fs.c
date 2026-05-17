/*------------------------------------------------------------------------------
//	Copyright (c) 2004-2025, Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		fs.c
//	Authors:		Bill Hayden (hayden@haydentech.com)
//----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef __linux__
#include <mntent.h>
#include <sys/statvfs.h>
#elif defined(__APPLE__)
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#include <fs_attr.h>
#include <fs_info.h>

#include <TypeConstants.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif

#include "../../../config.h"

#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#else
#if !defined(_WIN32)
#warning Cosmoe does not support attributes on this platform
#endif
#endif


status_t _kstart_watching_vnode_(dev_t device, ino_t node,
								 uint32 flags, port_id port, int32 handlerToken);
status_t _kstop_watching_vnode_(dev_t device, ino_t node,
								port_id port, int32 handlerToken);
status_t _kstop_notifying_(port_id port, int32 handlerToken);


typedef struct attr_type_entry {
	dev_t device;
	ino_t inode;
	char* name;
	uint32 type;
	struct attr_type_entry* next;
} attr_type_entry;


static pthread_mutex_t sAttrTypeLock = PTHREAD_MUTEX_INITIALIZER;
static attr_type_entry* sAttrTypeHead = NULL;

#if defined(_WIN32)

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define COSMOE_ATTR_STREAM_PREFIX "cosmoe.attr."
#define COSMOE_ATTR_TYPE_STREAM_PREFIX "cosmoe.attrtype."

typedef struct attr_dir_wrapper {
	HANDLE handle;
	WIN32_FIND_STREAM_DATA streamData;
	int firstPending;
	int atEnd;
	WCHAR path[MAX_PATH];
	struct dirent entry;
} attr_dir_wrapper;


static int
_GetPathForFD(int fd, char* path, size_t pathSize)
{
	if (fd < 0 || path == NULL || pathSize == 0) {
		errno = EINVAL;
		return 0;
	}

	intptr_t osHandle = _get_osfhandle(fd);
	if (osHandle == -1) {
		errno = EBADF;
		return 0;
	}

	DWORD length = GetFinalPathNameByHandleA((HANDLE)osHandle, path,
		(DWORD)pathSize, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if (length == 0 || length >= pathSize) {
		errno = ENOENT;
		return 0;
	}

	if (strncmp(path, "\\\\?\\", 4) == 0)
		memmove(path, path + 4, strlen(path + 4) + 1);

	return 1;
}


static char
_HexDigit(unsigned int value)
{
	return (value < 10) ? ('0' + value) : ('a' + value - 10);
}


static char*
_HexEncodeAttributeName(const char* attribute)
{
	size_t length;
	char* encoded;
	size_t i;

	if (attribute == NULL)
		return NULL;

	length = strlen(attribute);
	encoded = (char*)malloc(length * 2 + 1);
	if (encoded == NULL)
		return NULL;

	for (i = 0; i < length; i++) {
		unsigned char value = (unsigned char)attribute[i];
		encoded[i * 2] = _HexDigit((value >> 4) & 0xf);
		encoded[i * 2 + 1] = _HexDigit(value & 0xf);
	}
	encoded[length * 2] = '\0';
	return encoded;
}


static int
_HexNibble(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}


static int
_HexDecodeAttributeName(const char* encoded, char* attribute, size_t attributeSize)
{
	size_t encodedLength;
	size_t decodedLength;
	size_t i;

	if (encoded == NULL || attribute == NULL || attributeSize == 0)
		return 0;

	encodedLength = strlen(encoded);
	if ((encodedLength & 1) != 0)
		return 0;

	decodedLength = encodedLength / 2;
	if (decodedLength >= attributeSize)
		return 0;

	for (i = 0; i < decodedLength; i++) {
		int high = _HexNibble(encoded[i * 2]);
		int low = _HexNibble(encoded[i * 2 + 1]);
		if (high < 0 || low < 0)
			return 0;
		attribute[i] = (char)((high << 4) | low);
	}

	attribute[decodedLength] = '\0';
	return 1;
}


static void
_SetErrnoFromWin32Error(DWORD error)
{
	switch (error) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_INVALID_NAME:
		case ERROR_HANDLE_EOF:
			errno = B_ENTRY_NOT_FOUND;
			break;
		case ERROR_ACCESS_DENIED:
			errno = B_PERMISSION_DENIED;
			break;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			errno = B_FILE_EXISTS;
			break;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			errno = B_NO_MEMORY;
			break;
		default:
			errno = B_ERROR;
			break;
	}
}


static char*
_BuildAttrStreamPath(const char* basePath, const char* attribute,
	const char* prefix)
{
	char* encodedName;
	char* result;
	size_t totalLength;

	if (basePath == NULL || attribute == NULL || prefix == NULL)
		return NULL;

	encodedName = _HexEncodeAttributeName(attribute);
	if (encodedName == NULL)
		return NULL;

	totalLength = strlen(basePath) + 1 + strlen(prefix) + strlen(encodedName) + 1;
	result = (char*)malloc(totalLength);
	if (result != NULL)
		snprintf(result, totalLength, "%s:%s%s", basePath, prefix, encodedName);

	free(encodedName);
	return result;
}


static int
_IsDirectoryPath(const char* path, BOOL* isDirectory)
{
	DWORD attributes;

	if (path == NULL) {
		errno = EINVAL;
		return 0;
	}

	attributes = GetFileAttributesA(path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		_SetErrnoFromWin32Error(GetLastError());
		return 0;
	}

	if (isDirectory != NULL)
		*isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	return 1;
}


static int
_OpenADSStreamFD(const char* basePath, const char* attribute,
	const char* prefix, int openMode)
{
	char* streamPath;
	BOOL isDirectory = FALSE;
	DWORD desiredAccess = 0;
	DWORD creationDisposition = OPEN_EXISTING;
	DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE handle;
	int fd;
	int accessMode;

	if (!_IsDirectoryPath(basePath, &isDirectory))
		return -1;

	streamPath = _BuildAttrStreamPath(basePath, attribute, prefix);
	if (streamPath == NULL) {
		errno = B_NO_MEMORY;
		return -1;
	}

	accessMode = openMode & O_ACCMODE;
	if (accessMode == O_WRONLY)
		desiredAccess = GENERIC_WRITE;
	else if (accessMode == O_RDWR)
		desiredAccess = GENERIC_READ | GENERIC_WRITE;
	else
		desiredAccess = GENERIC_READ;

	if ((openMode & O_TRUNC) != 0) {
		creationDisposition = (openMode & O_CREAT) ? CREATE_ALWAYS : TRUNCATE_EXISTING;
	} else if ((openMode & O_CREAT) != 0)
		creationDisposition = OPEN_ALWAYS;

	if (isDirectory)
		flagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;

	handle = CreateFileA(streamPath, desiredAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		creationDisposition, flagsAndAttributes, NULL);
	free(streamPath);
	if (handle == INVALID_HANDLE_VALUE) {
		_SetErrnoFromWin32Error(GetLastError());
		return -1;
	}

	fd = _open_osfhandle((intptr_t)handle, openMode | O_BINARY);
	if (fd < 0) {
		CloseHandle(handle);
		errno = B_ERROR;
		return -1;
	}

	return fd;
}


static int
_WriteAttrTypeFile(const char* basePath, const char* attribute, uint32 type)
{
	int fd;
	ssize_t written;

	fd = _OpenADSStreamFD(basePath, attribute, COSMOE_ATTR_TYPE_STREAM_PREFIX,
		O_CREAT | O_TRUNC | O_WRONLY);
	if (fd < 0)
		return 0;

	written = _write(fd, &type, sizeof(type));
	_close(fd);
	if (written != (ssize_t)sizeof(type)) {
		errno = EIO;
		return 0;
	}

	return 1;
}


static int
_ReadAttrTypeFile(const char* basePath, const char* attribute, uint32* type)
{
	int fd;
	ssize_t bytesRead;
	uint32 storedType;

	fd = _OpenADSStreamFD(basePath, attribute, COSMOE_ATTR_TYPE_STREAM_PREFIX,
		O_RDONLY);
	if (fd < 0)
		return 0;

	bytesRead = _read(fd, &storedType, sizeof(storedType));
	_close(fd);
	if (bytesRead != (ssize_t)sizeof(storedType))
		return 0;

	if (type != NULL)
		*type = storedType;
	return 1;
}


static int
_OpenAttrDataFD(const char* basePath, const char* attribute, int openMode)
{
	return _OpenADSStreamFD(basePath, attribute, COSMOE_ATTR_STREAM_PREFIX,
		openMode);
}


static int
_DeleteAttrStream(const char* basePath, const char* attribute, const char* prefix)
{
	char* streamPath = _BuildAttrStreamPath(basePath, attribute, prefix);
	if (streamPath == NULL) {
		errno = B_NO_MEMORY;
		return 0;
	}

	if (!DeleteFileA(streamPath)) {
		free(streamPath);
		_SetErrnoFromWin32Error(GetLastError());
		return 0;
	}

	free(streamPath);
	return 1;
}


static int
_OpenBasePathFD(const char* path)
{
	DWORD attributes;
	HANDLE handle;
	int fd;

	fd = _open(path, O_RDONLY | O_BINARY);
	if (fd >= 0)
		return fd;

	attributes = GetFileAttributesA(path);
	if (attributes == INVALID_FILE_ATTRIBUTES
		|| (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		return -1;
	}

	handle = CreateFileA(path, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return -1;

	fd = _open_osfhandle((intptr_t)handle, O_RDONLY | O_BINARY);
	if (fd < 0)
		CloseHandle(handle);

	return fd;
}

#endif


static int
_GetFDIdentity(int fd, dev_t* device, ino_t* inode)
{
	struct stat st;
	if (fstat(fd, &st) != 0)
		return 0;

	if (device)
		*device = st.st_dev;
	if (inode)
		*inode = st.st_ino;
	return 1;
}


static void
_SetAttrTypeForFD(int fd, const char* attribute, uint32 type)
{
	dev_t device;
	ino_t inode;
	if (!_GetFDIdentity(fd, &device, &inode) || attribute == NULL)
		return;

	pthread_mutex_lock(&sAttrTypeLock);

	for (attr_type_entry* entry = sAttrTypeHead; entry != NULL;
		entry = entry->next) {
		if (entry->device == device && entry->inode == inode
			&& strcmp(entry->name, attribute) == 0) {
			entry->type = type;
			pthread_mutex_unlock(&sAttrTypeLock);
			return;
		}
	}

	attr_type_entry* entry = (attr_type_entry*)malloc(sizeof(attr_type_entry));
	if (entry != NULL) {
		entry->name = strdup(attribute);
		if (entry->name != NULL) {
			entry->device = device;
			entry->inode = inode;
			entry->type = type;
			entry->next = sAttrTypeHead;
			sAttrTypeHead = entry;
		} else
			free(entry);
	}

	pthread_mutex_unlock(&sAttrTypeLock);
}


static int
_GetAttrTypeForFD(int fd, const char* attribute, uint32* type)
{
	dev_t device;
	ino_t inode;
	if (!_GetFDIdentity(fd, &device, &inode) || attribute == NULL)
		return 0;

	pthread_mutex_lock(&sAttrTypeLock);

	for (attr_type_entry* entry = sAttrTypeHead; entry != NULL;
		entry = entry->next) {
		if (entry->device == device && entry->inode == inode
			&& strcmp(entry->name, attribute) == 0) {
			if (type)
				*type = entry->type;
			pthread_mutex_unlock(&sAttrTypeLock);
			return 1;
		}
	}

	pthread_mutex_unlock(&sAttrTypeLock);
	return 0;
}


static void
_RemoveAttrTypeForFD(int fd, const char* attribute)
{
	dev_t device;
	ino_t inode;
	if (!_GetFDIdentity(fd, &device, &inode) || attribute == NULL)
		return;

	pthread_mutex_lock(&sAttrTypeLock);

	attr_type_entry* previous = NULL;
	attr_type_entry* entry = sAttrTypeHead;
	while (entry != NULL) {
		if (entry->device == device && entry->inode == inode
			&& strcmp(entry->name, attribute) == 0) {
			if (previous)
				previous->next = entry->next;
			else
				sAttrTypeHead = entry->next;

			free(entry->name);
			free(entry);
			break;
		}

		previous = entry;
		entry = entry->next;
	}

	pthread_mutex_unlock(&sAttrTypeLock);
}



ssize_t  read_pos(int fd, off_t pos, void *buffer, size_t count)
{
	off_t origPos = lseek(fd, 0, SEEK_CUR);
	if (origPos < 0)
		return -1;
	
	if (lseek(fd, pos, SEEK_SET) < 0)
		return -1;
	
	ssize_t result = read(fd, buffer, count);
	
	// Restore original position (best effort)
	lseek(fd, origPos, SEEK_SET);
	return result;
}

ssize_t  write_pos(int fd, off_t pos, const void *buffer, size_t count)
{
	off_t origPos = lseek(fd, 0, SEEK_CUR);
	if (origPos < 0)
		return -1;
	
	if (lseek(fd, pos, SEEK_SET) < 0)
		return -1;
	
	ssize_t result = write(fd, buffer, count);
	
	// Restore original position (best effort)
	lseek(fd, origPos, SEEK_SET);
	return result;
}

// We're in a bit of a bind here since dev_t is unsigned on Linux, but
// was signed on BeOS. So we treat -1 as invalid, and everything else as valid.
dev_t dev_for_path(const char *path)
{
	if (!path) {
		return (dev_t)-1;
	}
	struct stat st;
	if (stat(path, &st) != 0) {
		return (dev_t)-1;
	}
	return st.st_dev;
}


static int
_StartsWith(const char* value, const char* prefix)
{
	if (value == NULL || prefix == NULL)
		return 0;

	while (*prefix != '\0') {
		if (*value++ != *prefix++)
			return 0;
	}

	return 1;
}


#ifdef __linux__
static int
_IsLinuxPseudoFilesystem(const char* fsType)
{
	static const char* const kPseudoTypes[] = {
		"proc", "procfs", "sysfs", "devtmpfs", "devpts", "tmpfs",
		"mqueue", "cgroup", "cgroup2", "pstore", "securityfs",
		"configfs", "debugfs", "tracefs", "fusectl", "rpc_pipefs",
		"hugetlbfs", "overlay", "nsfs", "ramfs"
	};

	if (fsType == NULL)
		return 1;

	for (size_t i = 0; i < sizeof(kPseudoTypes) / sizeof(kPseudoTypes[0]); i++) {
		if (strcmp(fsType, kPseudoTypes[i]) == 0)
			return 1;
	}

	return 0;
}


static int
_IsLinuxInternalMountPoint(const char* mountPoint)
{
	if (mountPoint == NULL)
		return 1;

	if (_StartsWith(mountPoint, "/proc")
		|| _StartsWith(mountPoint, "/sys")
		|| _StartsWith(mountPoint, "/dev")) {
		return 1;
	}

	if (_StartsWith(mountPoint, "/run")
		&& !_StartsWith(mountPoint, "/run/media")) {
		return 1;
	}

	return 0;
}


static int
_IsUserVisibleLinuxMount(const struct mntent* ent)
{
	if (ent == NULL)
		return 0;

	if (_IsLinuxPseudoFilesystem(ent->mnt_type)
		|| _IsLinuxInternalMountPoint(ent->mnt_dir)) {
		return 0;
	}

	if (!_StartsWith(ent->mnt_fsname, "/dev/"))
		return 0;

	if (_StartsWith(ent->mnt_fsname, "/dev/loop")
		|| _StartsWith(ent->mnt_fsname, "/dev/ram")
		|| _StartsWith(ent->mnt_fsname, "/dev/zram")) {
		return 0;
	}

	return 1;
}
#elif defined(__APPLE__)
static int
_IsUserVisibleMacMount(const struct statfs* mount)
{
	if (mount == NULL)
		return 0;

	if (!_StartsWith(mount->f_mntfromname, "/dev/"))
		return 0;

	if ((mount->f_flags & MNT_DONTBROWSE) != 0)
		return 0;

	if (strcmp(mount->f_fstypename, "devfs") == 0
		|| strcmp(mount->f_fstypename, "autofs") == 0
		|| strcmp(mount->f_fstypename, "procfs") == 0
		|| strcmp(mount->f_fstypename, "fdesc") == 0
		|| strcmp(mount->f_fstypename, "volfs") == 0) {
		return 0;
	}

	return 1;
}
#endif

dev_t next_dev(int32 *pos)
{
	if (!pos || *pos < 0)
		return (dev_t)-1;

#ifdef __linux__
	// Linux-specific: Use /proc/mounts to enumerate mounted filesystems
	FILE* mounts = setmntent("/proc/mounts", "r");
	if (!mounts)
		return (dev_t)-1;

	dev_t result = (dev_t)-1;
	int32 targetIndex = *pos;
	int32 found = 0;

	/* Keep a small list of seen devices to avoid duplicates */
	dev_t seen[256];
	int seenCount = 0;

	struct mntent *ent;
	while ((ent = getmntent(mounts)) != NULL) {
		if (!_IsUserVisibleLinuxMount(ent))
			continue;

		struct stat st;
		if (stat(ent->mnt_dir, &st) != 0)
			continue;

		/* dedupe */
		int alreadySeen = 0;
		for (int i = 0; i < seenCount; i++) {
			if (seen[i] == st.st_dev) {
				alreadySeen = 1;
				break;
			}
		}
		if (alreadySeen)
			continue;
		if (seenCount < (int)(sizeof(seen) / sizeof(seen[0])))
			seen[seenCount++] = st.st_dev;

		if (found == targetIndex) {
			result = st.st_dev;
			*pos = targetIndex + 1; /* advance cookie */
			break;
		}
		found++;
	}

	endmntent(mounts);
	return result;
#elif defined(__APPLE__)
	// macOS: Use getfsstat() to enumerate mounted filesystems
	int numMounts = getfsstat(NULL, 0, MNT_NOWAIT);
	if (numMounts <= 0)
		return (dev_t)-1;
	
	struct statfs *mounts = malloc(sizeof(struct statfs) * numMounts);
	if (!mounts)
		return (dev_t)-1;
	
	numMounts = getfsstat(mounts, sizeof(struct statfs) * numMounts, MNT_NOWAIT);
	if (numMounts <= 0) {
		free(mounts);
		return (dev_t)-1;
	}
	
	dev_t result = (dev_t)-1;
	int32 targetIndex = *pos;
	
	// Keep a small list of seen devices to avoid duplicates
	dev_t seen[256];
	int seenCount = 0;
	int found = 0;
	
	for (int i = 0; i < numMounts; i++) {
		if (!_IsUserVisibleMacMount(&mounts[i]))
			continue;

		struct stat st;
		if (stat(mounts[i].f_mntonname, &st) != 0)
			continue;
		
		// Dedupe
		int alreadySeen = 0;
		for (int j = 0; j < seenCount; j++) {
			if (seen[j] == st.st_dev) {
				alreadySeen = 1;
				break;
			}
		}
		if (alreadySeen)
			continue;
		if (seenCount < (int)(sizeof(seen) / sizeof(seen[0])))
			seen[seenCount++] = st.st_dev;
		
		if (found == targetIndex) {
			result = st.st_dev;
			*pos = targetIndex + 1; // advance cookie
			break;
		}
		found++;
	}
	
	free(mounts);
	return result;
#else
	// TODO: Implement Windows
	return (dev_t)-1;  // Placeholder
#endif
}

int	fs_stat_dev(dev_t dev, fs_info *info)
{
	if (dev == (dev_t)-1 || info == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

#ifdef __linux__
	// Linux-specific: Use /proc/mounts to find filesystem information
	FILE* mounts = setmntent("/proc/mounts", "r");
	if (!mounts) {
		errno = B_BAD_VALUE;
		return -1;
	}

	struct mntent *ent;
	int ret = -1;
	
	while ((ent = getmntent(mounts)) != NULL) {
		struct stat st;
		if (stat(ent->mnt_dir, &st) != 0)
			continue;
		if (st.st_dev != dev)
			continue;

		/* Found matching mount; gather statvfs info */
		struct statvfs vfs;
		if (statvfs(ent->mnt_dir, &vfs) != 0)
			break;

		memset(info, 0, sizeof(*info));
		info->dev = dev;
		info->root = st.st_ino;
		info->block_size = (off_t)vfs.f_bsize;
		info->total_blocks = (off_t)vfs.f_blocks;
		info->free_blocks = (off_t)vfs.f_bfree;

		strncpy(info->device_name, ent->mnt_fsname, sizeof(info->device_name)-1);
		info->device_name[sizeof(info->device_name)-1] = '\0';

		/* Use mount point as volume name if available */
		const char* base = strrchr(ent->mnt_dir, '/');
		if (base && base[1] != '\0')
			strncpy(info->volume_name, base + 1, sizeof(info->volume_name)-1);
		else
			strncpy(info->volume_name, ent->mnt_dir, sizeof(info->volume_name)-1);
		info->volume_name[sizeof(info->volume_name)-1] = '\0';

		strncpy(info->fsh_name, ent->mnt_type, sizeof(info->fsh_name)-1);
		info->fsh_name[sizeof(info->fsh_name)-1] = '\0';

		/* Set flags (basic) */
		info->flags = 0;
		info->flags |= B_FS_IS_PERSISTENT;
		if (vfs.f_flag & ST_RDONLY)
			info->flags |= B_FS_IS_READONLY;
		if (vfs.f_flag & ST_NOSUID)
			; /* not mapped but kept for future */

		ret = 0;
		break;
	}
	endmntent(mounts);
	errno = (ret == 0) ? 0 : B_ENTRY_NOT_FOUND;
	return ret;
#elif defined(__APPLE__)
	// macOS: Use getfsstat() to find filesystem information
	int numMounts = getfsstat(NULL, 0, MNT_NOWAIT);
	if (numMounts <= 0) {
		errno = B_BAD_VALUE;
		return -1;
	}
	
	struct statfs *mounts = malloc(sizeof(struct statfs) * numMounts);
	if (!mounts) {
		errno = B_NO_MEMORY;
		return -1;
	}
	
	numMounts = getfsstat(mounts, sizeof(struct statfs) * numMounts, MNT_NOWAIT);
	if (numMounts <= 0) {
		free(mounts);
		errno = B_BAD_VALUE;
		return -1;
	}
	
	int ret = -1;
	for (int i = 0; i < numMounts; i++) {
		struct stat st;
		if (stat(mounts[i].f_mntonname, &st) != 0)
			continue;
		if (st.st_dev != dev)
			continue;
		
		// Found matching mount
		memset(info, 0, sizeof(*info));
		info->dev = dev;
		info->root = st.st_ino;
		info->block_size = (off_t)mounts[i].f_bsize;
		info->total_blocks = (off_t)mounts[i].f_blocks;
		info->free_blocks = (off_t)mounts[i].f_bfree;
		
		// Device name from f_mntfromname
		strncpy(info->device_name, mounts[i].f_mntfromname, sizeof(info->device_name)-1);
		info->device_name[sizeof(info->device_name)-1] = '\0';
		
		// Volume name from mount point
		const char* base = strrchr(mounts[i].f_mntonname, '/');
		if (base && base[1] != '\0')
			strncpy(info->volume_name, base + 1, sizeof(info->volume_name)-1);
		else
			strncpy(info->volume_name, mounts[i].f_mntonname, sizeof(info->volume_name)-1);
		info->volume_name[sizeof(info->volume_name)-1] = '\0';
		
		// Filesystem type name
		strncpy(info->fsh_name, mounts[i].f_fstypename, sizeof(info->fsh_name)-1);
		info->fsh_name[sizeof(info->fsh_name)-1] = '\0';
		
		// Set flags
		info->flags = 0;
		info->flags |= B_FS_IS_PERSISTENT;
		if (mounts[i].f_flags & MNT_RDONLY)
			info->flags |= B_FS_IS_READONLY;
		if (mounts[i].f_flags & MNT_REMOVABLE)
			info->flags |= B_FS_IS_REMOVABLE;
		
		ret = 0;
		break;
	}
	
	free(mounts);
	errno = (ret == 0) ? 0 : B_ENTRY_NOT_FOUND;
	return ret;
#else
	// TODO: Implement Windows
	errno = B_NOT_SUPPORTED;
	return -1;  // Placeholder
#endif
}


ssize_t	fs_write_attr(int fd, const char *attribute, uint32 type, off_t pos, const void *buffer, size_t writeBytes)
{
	if (!attribute) {
		errno = B_BAD_VALUE;
		return -1;
	}
	
	if (strlen(attribute) > B_ATTR_NAME_LENGTH) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return -1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, sizeof(attrName), "user.%s", attribute);
#ifdef __APPLE__
	int err = fsetxattr(fd, attrName, buffer, writeBytes, 0, 0);  // macOS: position, options
#else
	int err = fsetxattr(fd, attrName, buffer, writeBytes, 0);     // Linux
#endif
	if (err != 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_BAD_VALUE;
		return (ssize_t)-1;
	}

	_SetAttrTypeForFD(fd, attribute, type);
	
	errno = 0;
	return (ssize_t)writeBytes;
#elif defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];
	int attrFD;
	ssize_t result;

	if (buffer == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return -1;
	}

	attrFD = _OpenAttrDataFD(basePath, attribute, O_CREAT | O_RDWR);
	if (attrFD < 0)
		return -1;

	if (pos == 0 && _chsize_s(attrFD, 0) != 0) {
		_close(attrFD);
		return -1;
	}

	if (_lseeki64(attrFD, pos, SEEK_SET) == -1) {
		_close(attrFD);
		return -1;
	}

	result = _write(attrFD, buffer, (unsigned int)writeBytes);
	_close(attrFD);
	if (result < 0)
		return -1;

	if (!_WriteAttrTypeFile(basePath, attribute, type)) {
		errno = B_ERROR;
		return -1;
	}

	_SetAttrTypeForFD(fd, attribute, type);
	errno = 0;
	return result;
#else
	printf( "Cosmoe: fs_write_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return -1;
#endif
}


ssize_t	fs_read_attr(int fd, const char *attribute, uint32 type, off_t pos, void *buffer, size_t readBytes)
{
	if (!attribute) {
		errno = B_BAD_VALUE;
		return -1;
	}
	
	if (strlen(attribute) > B_ATTR_NAME_LENGTH) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return -1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, sizeof(attrName), "user.%s", attribute);

#ifdef __APPLE__
	ssize_t err = fgetxattr(fd, attrName, buffer, readBytes, 0, 0);  // macOS
#else
	ssize_t err = fgetxattr(fd, attrName, buffer, readBytes);        // Linux
#endif
	if (err < 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_ENTRY_NOT_FOUND;
		return (ssize_t)-1;
	}

	errno = 0;
	return err;
#elif defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];
	int attrFD;
	ssize_t result;

	if (buffer == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return -1;
	}

	attrFD = _OpenAttrDataFD(basePath, attribute, O_RDONLY);
	if (attrFD < 0) {
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	if (_lseeki64(attrFD, pos, SEEK_SET) == -1) {
		_close(attrFD);
		return -1;
	}

	result = _read(attrFD, buffer, (unsigned int)readBytes);
	_close(attrFD);
	if (result < 0)
		return -1;

	errno = 0;
	return result;
#else
	printf( "Cosmoe: fs_read_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return -1;
#endif
}


int	fs_remove_attr(int fd, const char *attribute)
{
	if (!attribute) {
		errno = B_BAD_VALUE;
		return -1;
	}
	
	if (strlen(attribute) > B_ATTR_NAME_LENGTH) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return -1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, sizeof(attrName), "user.%s", attribute);
#ifdef __APPLE__
	int err = fremovexattr(fd, attrName, 0);  // macOS: options
#else
	int err = fremovexattr(fd, attrName);     // Linux
#endif
	if (err < 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	_RemoveAttrTypeForFD(fd, attribute);

	errno = 0;
	return B_OK;
#elif defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];
	int removed = 0;

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return -1;
	}

	if (_DeleteAttrStream(basePath, attribute, COSMOE_ATTR_STREAM_PREFIX))
		removed = 1;
	if (_DeleteAttrStream(basePath, attribute, COSMOE_ATTR_TYPE_STREAM_PREFIX))
		removed = 1;

	if (!removed) {
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	_RemoveAttrTypeForFD(fd, attribute);
	errno = 0;
	return B_OK;
#else
	printf( "Cosmoe: fs_remove_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return -1;
#endif
}


int	fs_stat_attr(int fd, const char *attribute, struct attr_info *attrInfo)
{
	if (!attribute) {
		errno = B_BAD_VALUE;
		return -1;
	}
	
#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, sizeof(attrName), "user.%s", attribute);

#ifdef __APPLE__
	int size = fgetxattr(fd, attrName, NULL, 0, 0, 0);  // macOS
#else
	int size = fgetxattr(fd, attrName, NULL, 0);        // Linux
#endif

	if (size < 0)
	{
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	if (attrInfo) {
		attrInfo->size = size;
		if (!_GetAttrTypeForFD(fd, attribute, &attrInfo->type))
			attrInfo->type = B_RAW_TYPE;
	}

	return B_OK;
#elif defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];
	int attrFD;
	struct _stat64 st;

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return -1;
	}

	attrFD = _OpenAttrDataFD(basePath, attribute, O_RDONLY);
	if (attrFD < 0) {
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	if (_fstat64(attrFD, &st) != 0) {
		_close(attrFD);
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}
	_close(attrFD);

	if (attrInfo != NULL) {
		attrInfo->size = st.st_size;
		if (!_ReadAttrTypeFile(basePath, attribute, &attrInfo->type)
			&& !_GetAttrTypeForFD(fd, attribute, &attrInfo->type)) {
			attrInfo->type = B_RAW_TYPE;
		}
	}

	errno = 0;
	return B_OK;
#else
	printf( "Cosmoe: fs_stat_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return -1;
#endif
}


int
fs_open_attr(const char *path, const char *attribute, uint32 type, int openMode)
{
#if defined(_WIN32)
	int fileFD;
	int attrFD;

	if (path == NULL || attribute == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

	fileFD = _OpenBasePathFD(path);
	if (fileFD < 0)
		return -1;

	attrFD = fs_fopen_attr(fileFD, attribute, type, openMode);
	_close(fileFD);
	return attrFD;
#else
	(void)path;
	(void)attribute;
	(void)type;
	(void)openMode;
	errno = B_NOT_SUPPORTED;
	return -1;
#endif
}


int
fs_fopen_attr(int fd, const char *attribute, uint32 type, int openMode)
{
#if defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];
	int attrFD;
	int accessMode;

	if (attribute == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return -1;
	}

	accessMode = openMode & O_ACCMODE;
	if (accessMode == O_WRONLY || accessMode == O_RDWR || (openMode & O_CREAT)) {
		if (!_WriteAttrTypeFile(basePath, attribute, type)) {
			errno = B_ERROR;
			return -1;
		}
		_SetAttrTypeForFD(fd, attribute, type);
	}

	attrFD = _OpenAttrDataFD(basePath, attribute, openMode);
	if (attrFD < 0 && (openMode & O_CREAT))
		attrFD = _OpenAttrDataFD(basePath, attribute, openMode | O_CREAT);

	return attrFD;
#else
	(void)fd;
	(void)attribute;
	(void)type;
	(void)openMode;
	errno = B_NOT_SUPPORTED;
	return -1;
#endif
}


int
fs_close_attr(int fd)
{
	return close(fd);
}


DIR*
fs_open_attr_dir(const char *path)
{
#if defined(_WIN32)
	attr_dir_wrapper* wrapper;
	int charsNeeded;

	if (path == NULL) {
		errno = B_BAD_VALUE;
		return NULL;
	}

	charsNeeded = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	if (charsNeeded <= 0 || charsNeeded > MAX_PATH) {
		errno = B_BAD_VALUE;
		return NULL;
	}

	wrapper = (attr_dir_wrapper*)calloc(1, sizeof(attr_dir_wrapper));
	if (wrapper == NULL) {
		errno = B_NO_MEMORY;
		return NULL;
	}

	MultiByteToWideChar(CP_UTF8, 0, path, -1, wrapper->path, MAX_PATH);
	wrapper->handle = INVALID_HANDLE_VALUE;
	wrapper->firstPending = 0;
	wrapper->atEnd = 0;
	return (DIR*)wrapper;
#else
	(void)path;
	errno = B_NOT_SUPPORTED;
	return NULL;
#endif
}


DIR*
fs_fopen_attr_dir(int fd)
{
#if defined(_WIN32)
	char basePath[B_PATH_NAME_LENGTH];

	if (!_GetPathForFD(fd, basePath, sizeof(basePath))) {
		errno = B_BAD_VALUE;
		return NULL;
	}

	return fs_open_attr_dir(basePath);
#else
	(void)fd;
	errno = B_NOT_SUPPORTED;
	return NULL;
#endif
}


int
fs_close_attr_dir(DIR *dir)
{
#if defined(_WIN32)
	attr_dir_wrapper* wrapper = (attr_dir_wrapper*)dir;

	if (wrapper == NULL) {
		errno = B_BAD_VALUE;
		return -1;
	}

	if (wrapper->handle != INVALID_HANDLE_VALUE)
		FindClose(wrapper->handle);
	free(wrapper);
	return 0;
#else
	(void)dir;
	errno = B_NOT_SUPPORTED;
	return -1;
#endif
}


struct dirent*
fs_read_attr_dir(DIR *dir)
{
#if defined(_WIN32)
	attr_dir_wrapper* wrapper = (attr_dir_wrapper*)dir;
	char streamName[256];
	char* prefixPos;
	char* endPos;
	char decoded[B_ATTR_NAME_LENGTH + 1];

	if (wrapper == NULL) {
		errno = B_BAD_VALUE;
		return NULL;
	}

	for (;;) {
		if (wrapper->atEnd)
			return NULL;

		if (!wrapper->firstPending) {
			wrapper->handle = FindFirstStreamW(wrapper->path,
				FindStreamInfoStandard, &wrapper->streamData, 0);
			if (wrapper->handle == INVALID_HANDLE_VALUE) {
				wrapper->atEnd = 1;
				return NULL;
			}
			wrapper->firstPending = 1;
		} else if (wrapper->handle == INVALID_HANDLE_VALUE
			|| !FindNextStreamW(wrapper->handle, &wrapper->streamData)) {
			if (wrapper->handle != INVALID_HANDLE_VALUE)
				FindClose(wrapper->handle);
			wrapper->handle = INVALID_HANDLE_VALUE;
			wrapper->atEnd = 1;
			return NULL;
		}

		if (WideCharToMultiByte(CP_UTF8, 0, wrapper->streamData.cStreamName, -1,
				streamName, sizeof(streamName), NULL, NULL) <= 0) {
			continue;
		}

		prefixPos = strstr(streamName, COSMOE_ATTR_STREAM_PREFIX);
		if (prefixPos == NULL || prefixPos != streamName + 1)
			continue;

		endPos = strstr(prefixPos, ":$DATA");
		if (endPos == NULL)
			continue;
		*endPos = '\0';
		if (!_HexDecodeAttributeName(prefixPos + strlen(COSMOE_ATTR_STREAM_PREFIX),
				decoded, sizeof(decoded))) {
			continue;
		}

		memset(&wrapper->entry, 0, sizeof(wrapper->entry));
		strncpy(wrapper->entry.d_name, decoded, sizeof(wrapper->entry.d_name) - 1);
		wrapper->entry.d_name[sizeof(wrapper->entry.d_name) - 1] = '\0';
		wrapper->entry.d_reclen = sizeof(struct dirent);
		return &wrapper->entry;
	}
#else
	(void)dir;
	errno = B_NOT_SUPPORTED;
	return NULL;
#endif
}


void
fs_rewind_attr_dir(DIR *dir)
{
#if defined(_WIN32)
	attr_dir_wrapper* wrapper = (attr_dir_wrapper*)dir;
	if (wrapper != NULL) {
		if (wrapper->handle != INVALID_HANDLE_VALUE)
			FindClose(wrapper->handle);
		wrapper->handle = INVALID_HANDLE_VALUE;
		wrapper->firstPending = 0;
		wrapper->atEnd = 0;
	}
#else
	(void)dir;
#endif
}


status_t _kstart_watching_vnode_(dev_t device, ino_t node,
										uint32 flags, port_id port,
										int32 handlerToken)
{
	return B_ERROR;
}/*!	\brief Unsubscribes a target from watching a node.
	\param device The device the node resides on (node_ref::device).
	\param node The node ID of the node.
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
status_t _kstop_watching_vnode_(dev_t device, ino_t node,
									   port_id port, int32 handlerToken)
{
	return B_ERROR;
}
/*!	\brief Unsubscribes a target from node and mount monitoring.
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
status_t _kstop_notifying_(port_id port, int32 handlerToken)
{
	return B_ERROR;
}


