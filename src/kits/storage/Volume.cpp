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

#include <Bitmap.h>
#include <Directory.h>
#include <fs_info.h>
#include <kernel_interface.h>
#include <Node.h>
#include <Path.h>
#include <Volume.h>



/*!
	\class BVolume
	\brief Represents a disk volume
	
	Provides an interface for querying information about a volume.

	The class is a simple wrapper for a \c dev_t and the function
	fs_stat_dev. The only exception is the method is SetName(), which
	sets the name of the volume.

	\author Vincent Dominguez
	\author <a href='mailto:bonefish@users.sf.net'>Ingo Weinhold</a>
	
	\version 0.0.0
*/

/*!	\var dev_t BVolume::fDevice
	\brief The volume's device ID.
*/

/*!	\var dev_t BVolume::fCStatus
	\brief The object's initialization status.
*/

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

BVolume::BVolume(struct mntent* inMountEntry)
{
	char* deviceOptionsList;

	// Extract the device number

	deviceOptionsList = strstr(inMountEntry->mnt_opts, "dev=");

	if (deviceOptionsList)
	{
		int offset = 4;

		if (deviceOptionsList[5] == 'x' || deviceOptionsList[5] == 'X')
			offset += 2;

		fDevice = atoi(deviceOptionsList + offset);
	}
	else
		fDevice = (dev_t) -1;

	// Get the properties

	mPropertiesLoaded = true;

	mIsShared = false;	// FIXME
	mIsRemovable = false;	// FIXME
	mIsReadOnly = ( hasmntopt(inMountEntry, MNTOPT_RO) != NULL );
	mIsPersistent = true;	// FIXME
	mCapacity = 0L;	// FIXME
	mFreeBytes = 0L;	// FIXME
	mName = "Bocephus";	// FIXME
	mDevicePath.SetTo(inMountEntry->mnt_fsname);
	mMountPath.SetTo(inMountEntry->mnt_dir);

	fCStatus = (fDevice == -1) ? -1 : 0;
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
	// check the parameter
	status_t error = (device >= 0 ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
//FIXME
	}
	// set the new value
	if (error == B_OK)
		fDevice = device;
	mPropertiesLoaded = false;

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
	mPropertiesLoaded = false;
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
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	directory->SetTo(mMountPath.Path());

	return B_OK;
}


// Returns the total storage capacity of the volume.
off_t
BVolume::Capacity() const
{
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mCapacity;
}


// Returns the amount of unused space on the volume (in bytes).
off_t
BVolume::FreeBytes() const
{
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mFreeBytes;
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
		return errno;

	return info.block_size;
}


// Copies the name of the volume into the provided buffer.
status_t
BVolume::GetName(char *name) const
{
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	if (mMountPath.Path() != NULL)
		strcpy(name, mMountPath.Path());
	else if (mDevicePath.Path() != NULL)
		strcpy(name, mDevicePath.Path());
	else
		return -1;

	return B_OK;
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
	status_t error = (InitCheck() == B_OK ? B_OK : B_BAD_VALUE);
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
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mIsRemovable;
}


// Returns whether or not the volume is read-only.
bool
BVolume::IsReadOnly(void) const
{
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mIsReadOnly;
}


// Returns whether or not the volume is persistent.
bool
BVolume::IsPersistent(void) const
{
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mIsPersistent;
}


// Returns whether or not the volume is shared.
bool
BVolume::IsShared(void) const
{
	// check initialization
	if (!mPropertiesLoaded)
		_LoadVolumeProperties();

	return mIsShared;
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
	return false;
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
		fDevice = volume.fDevice;

		mPropertiesLoaded = volume.mPropertiesLoaded;
		mIsShared = volume.mIsShared;
		mIsRemovable = volume.mIsRemovable;
		mIsReadOnly = volume.mIsReadOnly;
		mIsPersistent = volume.mIsPersistent;
		mCapacity = volume.mCapacity;
		mFreeBytes = volume.mFreeBytes;

		mDevicePath = volume.mDevicePath;
		mMountPath = volume.mMountPath;
	}
	return *this;
}


void		BVolume::_LoadVolumeProperties() const
{
	mPropertiesLoaded = true;
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
