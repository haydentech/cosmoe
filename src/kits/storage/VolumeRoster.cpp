// ----------------------------------------------------------------------
//  This software is part of the Haiku distribution and is covered
//  by the MIT License.
//
//  File Name:		VolumeRoster.cpp
//
//	Description:	BVolumeRoster class
// ----------------------------------------------------------------------


#include <errno.h>
#include <new>

#include <Bitmap.h>
#include <Directory.h>
#include <fs_info.h>
#include <kernel_interface.h>
#include <Node.h>
#include <NodeMonitor.h>
#include <VolumeRoster.h>
#include <List.h>


static const char kBootVolumePath[] = "/";

using namespace std;


#ifdef USE_OPENBEOS_NAMESPACE
namespace OpenBeOS {
#endif


BVolumeRoster::BVolumeRoster()
	: fCookie(0),
	  fTarget(NULL)
{
	struct mntent*	aMountEntry;
	FILE*			fstab;

	fstab = setmntent (_PATH_MOUNTED, "r");

	while ((aMountEntry = getmntent(fstab)))
	{
		// For now, we only keep items mounted at / or /media
		if (aMountEntry->mnt_dir != NULL &&
			(strcmp(aMountEntry->mnt_dir, "/") == 0	|| strncmp(aMountEntry->mnt_dir, "/media", 6) == 0)) {
			mMountList.AddItem(new BVolume(aMountEntry));
		}
	}

	if (endmntent(fstab) == 0)
	{
		int saved_errno = errno;

		_DeallocateMountList();

		errno = saved_errno;
	}
}


// Deletes the volume roster and frees all associated resources.
BVolumeRoster::~BVolumeRoster()
{
	StopWatching();
	_DeallocateMountList();
}


// Fills out the passed in BVolume object with the next available volume.
status_t
BVolumeRoster::GetNextVolume(BVolume *volume)
{
	// check parameter
	status_t error = (volume ? B_OK : B_BAD_VALUE);
	// get next device
	BVolume*	aMount;

	if (!error)
	{
		aMount = (BVolume*)(mMountList.ItemAt(fCookie++));

		if (aMount != NULL)
		{
			*volume = *aMount;
		}
		else
		{
			error = B_ERROR;
		}
	}

	return error;
}


// Rewinds the list of available volumes back to the first item.
void
BVolumeRoster::Rewind()
{
	fCookie = 0;
}


// Fills out the passed in BVolume object with the boot volume.
status_t
BVolumeRoster::GetBootVolume(BVolume *volume)
{
	// check parameter
	if (!volume)
		return B_BAD_VALUE;

	// get device
	for (int32 i = 0; i < mMountList.CountItems(); i++) {
		BVolume* aMount = static_cast<BVolume*>(mMountList.ItemAt(i));
		if (aMount && aMount->mMountPath == kBootVolumePath) {
			*volume = *aMount;
			return B_OK;
		}
	}

	return B_ENTRY_NOT_FOUND;
}


void	BVolumeRoster::_DeallocateMountList()
{
	BVolume *aMount;

	while ((aMount = (BVolume*)(mMountList.RemoveItem(1L))) != NULL)
	{
		delete aMount;
	}
}

// Starts watching the available volumes for changes.
status_t
BVolumeRoster::StartWatching(BMessenger messenger)
{
	StopWatching();
	status_t error = (messenger.IsValid() ? B_OK : B_ERROR);
	// clone messenger
	if (error == B_OK) {
		fTarget = new(nothrow) BMessenger(messenger);
		if (!fTarget)
			error = B_NO_MEMORY;
	}
	// start watching
	if (error == B_OK)
		error = watch_node(NULL, B_WATCH_MOUNT, messenger);
	// cleanup on failure
	if (error != B_OK && fTarget) {
		delete fTarget;
		fTarget = NULL;
	}
	return error;
}


// Stops watching volumes initiated by StartWatching().
void
BVolumeRoster::StopWatching()
{
	if (fTarget) {
		stop_watching(*fTarget);
		delete fTarget;
		fTarget = NULL;
	}
}


// Returns the messenger currently watching the volume list.
BMessenger
BVolumeRoster::Messenger() const
{
	return (fTarget ? *fTarget : BMessenger());
}


// FBC
void BVolumeRoster::_SeveredVRoster1() {}
void BVolumeRoster::_SeveredVRoster2() {}


#ifdef USE_OPENBEOS_NAMESPACE
}
#endif
