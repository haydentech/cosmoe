/*
 * Copyright 2002-2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold
 */


#include <errno.h>
#include <string.h>

#ifdef __linux__
#include <mntent.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#include <sys/mount.h>
#endif

#include <sys/stat.h>

#include <Bitmap.h>
#include <Directory.h>
#include <fs_info.h>
#include <kernel_interface.h>
#include <Node.h>
#include <Path.h>
#include <Volume.h>


static status_t
_GetRootPathForDevice(dev_t device, char* path, size_t pathSize)
{
	if (path == NULL || pathSize == 0)
		return B_BAD_VALUE;

#ifdef __linux__
	FILE* mounts = setmntent("/proc/mounts", "r");
	if (mounts == NULL)
		return B_ENTRY_NOT_FOUND;

	status_t status = B_ENTRY_NOT_FOUND;
	struct mntent* ent;
	while ((ent = getmntent(mounts)) != NULL) {
		struct stat st;
		if (stat(ent->mnt_dir, &st) != 0)
			continue;
		if (st.st_dev != device)
			continue;

		strlcpy(path, ent->mnt_dir, pathSize);
		status = B_OK;
		break;
	}

	endmntent(mounts);
	return status;
#elif defined(__APPLE__)
	int count = getfsstat(NULL, 0, MNT_NOWAIT);
	if (count <= 0)
		return B_ENTRY_NOT_FOUND;

	struct statfs* mounts
		= (struct statfs*)malloc(sizeof(struct statfs) * count);
	if (mounts == NULL)
		return B_NO_MEMORY;

	status_t status = B_ENTRY_NOT_FOUND;
	count = getfsstat(mounts, sizeof(struct statfs) * count, MNT_NOWAIT);
	for (int i = 0; i < count; i++) {
		struct stat st;
		if (stat(mounts[i].f_mntonname, &st) != 0)
			continue;
		if (st.st_dev != device)
			continue;

		strlcpy(path, mounts[i].f_mntonname, pathSize);
		status = B_OK;
		break;
	}

	free(mounts);
	return status;
#else
	return B_NOT_SUPPORTED;
#endif
}

static status_t
_VolumeErrorForErrno(int error)
{
	if (error == ENODEV || error == ENOENT)
		return B_ENTRY_NOT_FOUND;

	return error != 0 ? error : B_ERROR;
}




// Creates an uninitialized BVolume object.
BVolume::BVolume()
	: fDevice((dev_t)-1),
	  fCStatus(B_NO_INIT)
{
}


// Creates a BVolume and initializes it to the volume specified by the
// supplied device ID.
BVolume::BVolume(dev_t device)
	: fDevice((dev_t)-1),
	  fCStatus(B_NO_INIT)
{
	SetTo(device);
}


// Creates a copy of the supplied BVolume object.
BVolume::BVolume(const BVolume &volume)
	: fDevice(volume.fDevice),
	  fCStatus(volume.fCStatus)
{
}


// Destroys the object and frees all associated resources.
BVolume::~BVolume()
{
}


// Returns the initialization status.
status_t
BVolume::InitCheck(void) const
{
	return fCStatus;
}


// Initializes the object to refer to the volume specified by the supplied
// device ID.
status_t
BVolume::SetTo(dev_t device)
{
	// uninitialize
	Unset();
	// check the parameter: only non-negative device IDs are valid
	status_t error = (device >= 0 ? B_OK : B_BAD_VALUE);

	if (error == B_OK) {
		fs_info info;
		if (fs_stat_dev(device, &info) != 0)
			error = _VolumeErrorForErrno(errno);
	}
	// set the new value
	if (error == B_OK)
		fDevice = device;
	// set the init status variable
	fCStatus = error;
	return fCStatus;
}


// Brings the BVolume object to an uninitialized state.
void
BVolume::Unset()
{
	fDevice = (dev_t)-1;
	fCStatus = B_NO_INIT;
}


// Returns the device ID of the volume the object refers to.
dev_t
BVolume::Device() const
{
	return fDevice;
}


// Writes the root directory of the volume referred to by this object into
// directory.
status_t
BVolume::GetRootDirectory(BDirectory *directory) const
{
	// check parameter and initialization
	status_t error = (directory && InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	char rootPath[B_PATH_NAME_LENGTH];
	if (error == B_OK)
		error = _GetRootPathForDevice(fDevice, rootPath, sizeof(rootPath));
	if (error == B_OK)
		error = directory->SetTo(rootPath);
	return error;
}


// Returns the total storage capacity of the volume.
off_t
BVolume::Capacity() const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK ? info.total_blocks * info.block_size : error);
}


// Returns the amount of unused space on the volume (in bytes).
off_t
BVolume::FreeBytes() const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK ? info.free_blocks * info.block_size : error);
}


// Returns the size of one block (in bytes).
off_t
BVolume::BlockSize() const
{
	// check initialization
	if (InitCheck() != B_OK)
		return B_NO_INIT;

	// get FS stat
	fs_info info;
	if (fs_stat_dev(fDevice, &info) != 0)
		return _VolumeErrorForErrno(errno);

	return info.block_size;
}


// Copies the name of the volume into the provided buffer.
status_t
BVolume::GetName(char *name) const
{
	// check parameter and initialization
	status_t error = (name && InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	// copy the name
	if (error == B_OK)
		strncpy(name, info.volume_name, B_FILE_NAME_LENGTH);
	return error;
}


// Sets the name of the volume.
status_t
BVolume::SetName(const char *name)
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	mName = name;
	return error;
}


// Writes the volume's icon into icon.
status_t
BVolume::GetIcon(BBitmap *icon, icon_size which) const
{
	// check initialization
	if (InitCheck() != B_OK)
		return B_BAD_VALUE;
	if (!icon)
		return B_BAD_VALUE;
	// get FS stat
	return B_ERROR;
}


status_t
BVolume::GetIcon(uint8** _data, size_t* _size, type_code* _type) const
{
	// check initialization
	if (InitCheck() != B_OK)
		return B_NO_INIT;

	return B_ERROR;
}


// Returns whether or not the volume is removable.
bool
BVolume::IsRemovable() const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK && (info.flags & B_FS_IS_REMOVABLE));
}


// Returns whether or not the volume is read-only.
bool
BVolume::IsReadOnly(void) const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK && (info.flags & B_FS_IS_READONLY));
}


// Returns whether or not the volume is persistent.
bool
BVolume::IsPersistent(void) const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK && (info.flags & B_FS_IS_PERSISTENT));
}


// Returns whether or not the volume is shared.
bool
BVolume::IsShared(void) const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK && (info.flags & B_FS_IS_SHARED));
}


// Returns whether or not the volume supports MIME-types.
bool
BVolume::KnowsMime(void) const
{
	return false;
}


// Returns whether or not the volume supports attributes.
bool
BVolume::KnowsAttr(void) const
{
	// check initialization
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
	// get FS stat
	fs_info info;
	if (error == B_OK && fs_stat_dev(fDevice, &info) != 0)
		error = _VolumeErrorForErrno(errno);
	return (error == B_OK && (info.flags & B_FS_HAS_ATTR));
}


// Returns whether or not the volume supports queries.
bool
BVolume::KnowsQuery(void) const
{
	return false;
}


// Returns whether or not the supplied BVolume object is a equal
// to this object.
bool
BVolume::operator==(const BVolume &volume) const
{
	return ((InitCheck() != B_OK && volume.InitCheck() != B_OK)
			|| fDevice == volume.fDevice);
}

// Returns whether or not the supplied BVolume object is NOT equal
// to this object.
bool
BVolume::operator!=(const BVolume &volume) const
{
	return !(*this == volume);
}


// Assigns the supplied BVolume object to this volume.
BVolume&
BVolume::operator=(const BVolume &volume)
{
	if (&volume != this) {
		this->fDevice = volume.fDevice;
		this->fCStatus = volume.fCStatus;
	}
	return *this;
}


// FBC
void BVolume::_TurnUpTheVolume1() {}
void BVolume::_TurnUpTheVolume2() {}
void BVolume::_TurnUpTheVolume3() {}
void BVolume::_TurnUpTheVolume4() {}
void BVolume::_TurnUpTheVolume5() {}
void BVolume::_TurnUpTheVolume6() {}
void BVolume::_TurnUpTheVolume7() {}
void BVolume::_TurnUpTheVolume8() {}
