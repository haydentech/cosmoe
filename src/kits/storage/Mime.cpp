/*
 * Copyright 2002-2008, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include <errno.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#endif
#include <unistd.h>

#include <AutoDeleter.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <fs_attr.h>
#include <fs_info.h>
#include <IconUtils.h>
#include <Locker.h>
#include <Message.h>
#include <Mime.h>
#include <MimeType.h>
#include <Node.h>
#include <Path.h>
#include <RegistrarDefs.h>
#include <Roster.h>
#include <RosterPrivate.h>

#include <mime/Database.h>
#include <mime/DatabaseLocation.h>
#include <mime/database_support.h>


using namespace BPrivate;
using namespace BPrivate::Storage::Mime;


static BLocker sMimeDatabaseLock("mime database update");


static status_t
default_database_status(Database*& database)
{
	database = default_database();
	if (database == NULL)
		return B_NO_MEMORY;

	return database->InitCheck();
}


static status_t
update_icon(BAppFileInfo& appFileInfoRead, BAppFileInfo& appFileInfoWrite,
	const char* type, BBitmap& icon, icon_size iconSize)
{
	status_t status = appFileInfoRead.GetIconForType(type, &icon, iconSize);
	if (status == B_OK) {
		status = appFileInfoWrite.SetIconForType(type, &icon, iconSize, false);
	} else if (status == B_ENTRY_NOT_FOUND) {
		status = appFileInfoWrite.SetIconForType(type, (const BBitmap*)NULL,
			iconSize, false);
	}

	return status;
}


static status_t
update_icon(BAppFileInfo& appFileInfoRead, BAppFileInfo& appFileInfoWrite,
	const char* type)
{
	uint8* data = NULL;
	size_t size = 0;

	status_t status = appFileInfoRead.GetIconForType(type, &data, &size);
	if (status == B_OK) {
		status = appFileInfoWrite.SetIconForType(type, data, size, false);
	} else if (status == B_ENTRY_NOT_FOUND) {
		status = appFileInfoWrite.SetIconForType(type, (const uint8*)NULL, size,
			false);
	}

	free(data);
	return status;
}


static bool
is_shared_object_mime_type(const BString& type)
{
	return type.ICompare(B_APP_MIME_TYPE) == 0;
}


static status_t
update_mime_info_entry(const entry_ref& entry, bool* entryIsDir, int32 force,
	Database* database)
{
	bool updateType = false;
	bool updateAppInfo = false;
	BNode node;
	status_t status = node.SetTo(&entry);
	if (status != B_OK)
		return status;

	if (entryIsDir != NULL)
		*entryIsDir = node.IsDirectory();

	attr_info info;
	if (force == B_UPDATE_MIME_INFO_FORCE_UPDATE_ALL
		|| node.GetAttrInfo(kFileTypeAttr, &info) == B_ENTRY_NOT_FOUND) {
		updateType = true;
	}
	updateAppInfo = updateType
		|| force == B_UPDATE_MIME_INFO_FORCE_KEEP_TYPE;

	BString type;
	if (updateType || updateAppInfo) {
		sMimeDatabaseLock.Lock();
		status = database->GuessMimeType(&entry, &type);
		sMimeDatabaseLock.Unlock();
		if (status != B_OK)
			return status;
	}

	if (updateType) {
		ssize_t length = type.Length() + 1;
		ssize_t bytes = node.WriteAttr(kFileTypeAttr, kFileTypeType, 0,
			type.String(), length);
		if (bytes < B_OK)
			return bytes;
		if (bytes != length)
			return B_FILE_ERROR;
	}

	if (!updateAppInfo || !node.IsFile() || !is_shared_object_mime_type(type))
		return B_OK;

	BFile file;
	status = file.SetTo(&entry, B_READ_WRITE);
	if (status != B_OK)
		return status;

	BAppFileInfo appFileInfoRead;
	BAppFileInfo appFileInfoWrite;
	status = appFileInfoRead.SetTo(&file);
	if (status != B_OK)
		return status;
	status = appFileInfoWrite.SetTo(&file);
	if (status != B_OK)
		return status;

	appFileInfoRead.SetInfoLocation(B_USE_RESOURCES);
	appFileInfoWrite.SetInfoLocation(B_USE_ATTRIBUTES);

	char signature[B_MIME_TYPE_LENGTH];
	status = appFileInfoRead.GetSignature(signature);
	if (status == B_OK)
		status = appFileInfoWrite.SetSignature(signature);
	else if (status == B_ENTRY_NOT_FOUND)
		status = appFileInfoWrite.SetSignature(NULL);
	if (status != B_OK)
		return status;

	char catalogEntry[B_MIME_TYPE_LENGTH * 3];
	status = appFileInfoRead.GetCatalogEntry(catalogEntry);
	if (status == B_OK)
		status = appFileInfoWrite.SetCatalogEntry(catalogEntry);
	else if (status == B_ENTRY_NOT_FOUND)
		status = appFileInfoWrite.SetCatalogEntry(NULL);
	if (status != B_OK)
		return status;

	uint32 appFlags;
	status = appFileInfoRead.GetAppFlags(&appFlags);
	if (status == B_OK) {
		status = appFileInfoWrite.SetAppFlags(appFlags);
	} else if (status == B_ENTRY_NOT_FOUND) {
		status = file.RemoveAttr("BEOS:APP_FLAGS");
		if (status == B_ENTRY_NOT_FOUND)
			status = B_OK;
	}
	if (status != B_OK)
		return status;

	BMessage supportedTypes;
	bool hasSupportedTypes = false;
	status = appFileInfoRead.GetSupportedTypes(&supportedTypes);
	if (status == B_OK) {
		status = appFileInfoWrite.SetSupportedTypes(&supportedTypes, false,
			false);
		hasSupportedTypes = true;
	} else if (status == B_ENTRY_NOT_FOUND) {
		status = appFileInfoWrite.SetSupportedTypes(NULL, false, false);
	}
	if (status != B_OK)
		return status;

	status = update_icon(appFileInfoRead, appFileInfoWrite, NULL);
	if (status != B_OK)
		return status;

	BBitmap smallIcon(BRect(0, 0, 15, 15), B_BITMAP_NO_SERVER_LINK, B_CMAP8);
	if (smallIcon.InitCheck() != B_OK)
		return smallIcon.InitCheck();
	status = update_icon(appFileInfoRead, appFileInfoWrite, NULL, smallIcon,
		B_MINI_ICON);
	if (status != B_OK)
		return status;

	BBitmap largeIcon(BRect(0, 0, 31, 31), B_BITMAP_NO_SERVER_LINK, B_CMAP8);
	if (largeIcon.InitCheck() != B_OK)
		return largeIcon.InitCheck();
	status = update_icon(appFileInfoRead, appFileInfoWrite, NULL, largeIcon,
		B_LARGE_ICON);
	if (status != B_OK)
		return status;

	const version_kind versionKinds[] = { B_APP_VERSION_KIND,
		B_SYSTEM_VERSION_KIND };
	for (int32 i = 0; i < 2; i++) {
		version_info versionInfo;
		status = appFileInfoRead.GetVersionInfo(&versionInfo, versionKinds[i]);
		if (status == B_OK) {
			status = appFileInfoWrite.SetVersionInfo(&versionInfo,
				versionKinds[i]);
		} else if (status == B_ENTRY_NOT_FOUND) {
			status = appFileInfoWrite.SetVersionInfo(NULL, versionKinds[i]);
		}
		if (status != B_OK)
			return status;
	}

	if (!hasSupportedTypes)
		return B_OK;

	const char* supportedType;
	for (int32 i = 0;
		supportedTypes.FindString("types", i, &supportedType) == B_OK; i++) {
		status = update_icon(appFileInfoRead, appFileInfoWrite, supportedType);
		if (status != B_OK)
			return status;

		status = update_icon(appFileInfoRead, appFileInfoWrite, supportedType,
			smallIcon, B_MINI_ICON);
		if (status != B_OK)
			return status;

		status = update_icon(appFileInfoRead, appFileInfoWrite, supportedType,
			largeIcon, B_LARGE_ICON);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


static status_t
create_app_meta_mime_entry(const entry_ref& entry, bool* entryIsDir,
	int32 force, Database* database)
{
	BFile file;
	status_t status = file.SetTo(&entry, B_READ_ONLY);
	if (status != B_OK)
		return status;

	bool isDir = file.IsDirectory();
	if (entryIsDir != NULL)
		*entryIsDir = isDir;
	if (isDir || !file.IsFile())
		return B_OK;

	BAppFileInfo appInfo(&file);
	status = appInfo.InitCheck();
	if (status != B_OK)
		return status;

	BString signature;
	status = file.ReadAttrString("BEOS:APP_SIG", &signature);
	if (status != B_OK || !BMimeType::IsValid(signature.String()))
		return B_BAD_TYPE;

	BNode typeNode;
	sMimeDatabaseLock.Lock();
	if (!database->Location()->IsInstalled(signature.String()))
		status = database->Install(signature.String());
	if (status == B_OK)
		status = database->Location()->OpenType(signature.String(), typeNode);
	sMimeDatabaseLock.Unlock();
	if (status != B_OK)
		return status;

	attr_info info;
	if (force || typeNode.GetAttrInfo(kPreferredAppAttr, &info) != B_OK) {
		sMimeDatabaseLock.Lock();
		status = database->SetPreferredApp(signature.String(),
			signature.String());
		sMimeDatabaseLock.Unlock();
		if (status != B_OK)
			return status;
	}

	if (force || typeNode.GetAttrInfo(kShortDescriptionAttr, &info) != B_OK) {
		sMimeDatabaseLock.Lock();
		status = database->SetShortDescription(signature.String(), entry.name);
		sMimeDatabaseLock.Unlock();
		if (status != B_OK)
			return status;
	}

	if (force || typeNode.GetAttrInfo(kAppHintAttr, &info) != B_OK) {
		sMimeDatabaseLock.Lock();
		status = database->SetAppHint(signature.String(), &entry);
		sMimeDatabaseLock.Unlock();
		if (status != B_OK)
			return status;
	}

	if (force || typeNode.GetAttrInfo(kIconAttr, &info) != B_OK) {
		uint8* data = NULL;
		size_t size = 0;
		if (appInfo.GetIcon(&data, &size) == B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIcon(signature.String(), data, size);
			sMimeDatabaseLock.Unlock();
			free(data);
			if (status != B_OK)
				return status;
		}
	}

	BBitmap miniIcon(BRect(0, 0, 15, 15), B_BITMAP_NO_SERVER_LINK, B_CMAP8);
	if (miniIcon.InitCheck() != B_OK)
		return miniIcon.InitCheck();
	if (force || typeNode.GetAttrInfo(kMiniIconAttr, &info) != B_OK) {
		if (appInfo.GetIcon(&miniIcon, B_MINI_ICON) == B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIcon(signature.String(), &miniIcon,
				B_MINI_ICON);
			sMimeDatabaseLock.Unlock();
			if (status != B_OK)
				return status;
		}
	}

	BBitmap largeIcon(BRect(0, 0, 31, 31), B_BITMAP_NO_SERVER_LINK, B_CMAP8);
	if (largeIcon.InitCheck() != B_OK)
		return largeIcon.InitCheck();
	if (force || typeNode.GetAttrInfo(kLargeIconAttr, &info) != B_OK) {
		if (appInfo.GetIcon(&largeIcon, B_LARGE_ICON) == B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIcon(signature.String(), &largeIcon,
				B_LARGE_ICON);
			sMimeDatabaseLock.Unlock();
			if (status != B_OK)
				return status;
		}
	}

	BMessage supportedTypes;
	bool setSupportedTypes = false;
	if (force || typeNode.GetAttrInfo(kSupportedTypesAttr, &info) != B_OK) {
		if (appInfo.GetSupportedTypes(&supportedTypes) == B_OK)
			setSupportedTypes = true;
	}

	const char* supportedType;
	for (int32 i = 0;
		supportedTypes.FindString("types", i, &supportedType) == B_OK; i++) {
		sMimeDatabaseLock.Lock();
		database->DeferInstallNotification(supportedType);
		sMimeDatabaseLock.Unlock();
	}

	if (setSupportedTypes) {
		sMimeDatabaseLock.Lock();
		status = database->SetSupportedTypes(signature.String(),
			&supportedTypes, true);
		sMimeDatabaseLock.Unlock();
		if (status != B_OK)
			return status;
	}

	for (int32 i = 0;
		supportedTypes.FindString("types", i, &supportedType) == B_OK; i++) {
		uint8* data = NULL;
		size_t size = 0;
		if (appInfo.GetIconForType(supportedType, &data, &size) == B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIconForType(signature.String(), supportedType,
				data, size);
			sMimeDatabaseLock.Unlock();
			free(data);
			if (status != B_OK)
				return status;
		}

		if (appInfo.GetIconForType(supportedType, &miniIcon, B_MINI_ICON)
			== B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIconForType(signature.String(), supportedType,
				&miniIcon, B_MINI_ICON);
			sMimeDatabaseLock.Unlock();
			if (status != B_OK)
				return status;
		}

		if (appInfo.GetIconForType(supportedType, &largeIcon, B_LARGE_ICON)
			== B_OK) {
			sMimeDatabaseLock.Lock();
			status = database->SetIconForType(signature.String(), supportedType,
				&largeIcon, B_LARGE_ICON);
			sMimeDatabaseLock.Unlock();
			if (status != B_OK)
				return status;
		}
	}

	for (int32 i = 0;
		supportedTypes.FindString("types", i, &supportedType) == B_OK; i++) {
		sMimeDatabaseLock.Lock();
		database->UndeferInstallNotification(supportedType);
		sMimeDatabaseLock.Unlock();
	}

	return B_OK;
}


static status_t
do_mime_update_entry(int32 what, const entry_ref& entry, bool recursive,
	int32 force, Database* database)
{
	bool entryIsDir = false;
	switch (what) {
		case B_REG_MIME_UPDATE_MIME_INFO:
			(void)update_mime_info_entry(entry, &entryIsDir, force, database);
			break;

		case B_REG_MIME_CREATE_APP_META_MIME:
			(void)create_app_meta_mime_entry(entry, &entryIsDir, force, database);
			break;

		default:
			return B_BAD_VALUE;
	}

	if (!recursive || !entryIsDir)
		return B_OK;

	BDirectory directory;
	status_t status = directory.SetTo(&entry);
	if (status != B_OK)
		return status;

	entry_ref childEntry;
	while ((status = directory.GetNextRef(&childEntry)) == B_OK) {
		status = do_mime_update_entry(what, childEntry, true, force, database);
		if (status != B_OK)
			return status;
	}

	return status == B_ENTRY_NOT_FOUND ? B_OK : status;
}


// Helper function that takes care of mime update calls
status_t
do_mime_update(int32 what, const char* path, int recursive,
	int synchronous, int force)
{
	BEntry root;
	entry_ref ref;
	Database* database = NULL;

	status_t status = root.SetTo(path ? path : "/");
	if (status == B_OK)
		status = root.GetRef(&ref);
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	// Cosmoe does not have a registrar-side MIME worker, so perform the
	// update in-process and synchronously (for now).
	(void)synchronous;
	return do_mime_update_entry(what, ref, recursive, force, database);
}


// Updates the MIME information (i.e MIME type) for one or more files.
int
update_mime_info(const char* path, int recursive, int synchronous, int force)
{
	// Force recursion when given a NULL path
	if (!path)
		recursive = true;

	return do_mime_update(B_REG_MIME_UPDATE_MIME_INFO, path, recursive,
		synchronous, force);
}


// Creates a MIME database entry for one or more applications.
status_t
create_app_meta_mime(const char* path, int recursive, int synchronous,
	int force)
{
	// Force recursion when given a NULL path
	if (!path)
		recursive = true;

	return do_mime_update(B_REG_MIME_CREATE_APP_META_MIME, path, recursive,
		synchronous, force);
}


// Retrieves an icon associated with a given device.
status_t
get_device_icon(const char* device, void* icon, int32 size)
{
	if (device == NULL || icon == NULL
		|| (size != B_LARGE_ICON && size != B_MINI_ICON))
		return B_BAD_VALUE;

	int fd = open(device, O_RDONLY);
	if (fd < 0)
		return errno;

	// ToDo: The mounted directories for volumes can also have META:X:STD_ICON
	// attributes. Should those attributes override the icon returned by
	// ioctl(,B_GET_ICON,)?

	uint8* data;
	size_t dataSize;
	type_code type;
	status_t status = get_device_icon(device, &data, &dataSize, &type);
	if (status == B_OK) {
		BBitmap* icon32 = new(std::nothrow) BBitmap(
			BRect(0, 0, size - 1, size - 1), B_BITMAP_NO_SERVER_LINK,
			B_RGBA32);
		BBitmap* icon8 = new(std::nothrow) BBitmap(
			BRect(0, 0, size - 1, size - 1), B_BITMAP_NO_SERVER_LINK,
			B_CMAP8);

		ArrayDeleter<uint8> dataDeleter(data);
		ObjectDeleter<BBitmap> icon32Deleter(icon32);
		ObjectDeleter<BBitmap> icon8Deleter(icon8);

		if (icon32 == NULL || icon32->InitCheck() != B_OK || icon8 == NULL
			|| icon8->InitCheck() != B_OK) {
			return B_NO_MEMORY;
		}

		status = BIconUtils::GetVectorIcon(data, dataSize, icon32);
		if (status == B_OK)
			status = BIconUtils::ConvertToCMAP8(icon32, icon8);
		if (status == B_OK)
			memcpy(icon, icon8->Bits(), icon8->BitsLength());

		return status;
	}
	return errno;
}


// Retrieves an icon associated with a given device.
status_t
get_device_icon(const char* device, BBitmap* icon, icon_size which)
{
	// check parameters
	if (device == NULL || icon == NULL)
		return B_BAD_VALUE;

	uint8* data;
	size_t size;
	type_code type;
	status_t status = get_device_icon(device, &data, &size, &type);
	if (status == B_OK) {
		status = BIconUtils::GetVectorIcon(data, size, icon);
		delete[] data;
		return status;
	}

	// Vector icon was not available, try old one

	BRect rect;
	if (which == B_MINI_ICON)
		rect.Set(0, 0, 15, 15);
	else if (which == B_LARGE_ICON)
		rect.Set(0, 0, 31, 31);

	BBitmap* bitmap = icon;
	int32 iconSize = which;

	if (icon->ColorSpace() != B_CMAP8
		|| (which != B_MINI_ICON && which != B_LARGE_ICON)) {
		if (which < B_LARGE_ICON)
			iconSize = B_MINI_ICON;
		else
			iconSize = B_LARGE_ICON;

		bitmap = new(std::nothrow) BBitmap(
			BRect(0, 0, iconSize - 1, iconSize -1), B_BITMAP_NO_SERVER_LINK,
			B_CMAP8);
		if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
			delete bitmap;
			return B_NO_MEMORY;
		}
	}

	// get the icon, convert temporary data into bitmap if necessary
	status = get_device_icon(device, bitmap->Bits(), iconSize);
	if (status == B_OK && icon != bitmap)
		status = BIconUtils::ConvertFromCMAP8(bitmap, icon);

	if (icon != bitmap)
		delete bitmap;

	return status;
}


status_t
get_device_icon(const char* device, uint8** _data, size_t* _size,
	type_code* _type)
{
	if (device == NULL || _data == NULL || _size == NULL || _type == NULL)
		return B_BAD_VALUE;

	// Unimplemented

	return B_ERROR;
#if 0
	int fd = open(device, O_RDONLY);
	if (fd < 0)
		return errno;

	// Try to get the icon by name first

	char name[B_FILE_NAME_LENGTH];
	if (ioctl(fd, B_GET_ICON_NAME, name, sizeof(name)) >= 0) {
		status_t status = get_named_icon(name, _data, _size, _type);
		if (status == B_OK) {
			close(fd);
			return B_OK;
		}
	}

	// Getting the named icon failed, try vector icon next

	// NOTE: The actual icon size is unknown as of yet. After the first call
	// to B_GET_VECTOR_ICON, the actual size is known and the final buffer
	// is allocated with the correct size. If the buffer needed to be
	// larger, then the temporary buffer above will not yet contain the
	// valid icon data. In that case, a second call to B_GET_VECTOR_ICON
	// retrieves it into the final buffer.
	uint8 data[8192];
	device_icon iconData = {sizeof(data), data};
	status_t status = ioctl(fd, B_GET_VECTOR_ICON, &iconData,
		sizeof(device_icon));
	if (status != 0)
		status = errno;

	if (status == B_OK) {
		*_data = new(std::nothrow) uint8[iconData.icon_size];
		if (*_data == NULL)
			status = B_NO_MEMORY;
	}

	if (status == B_OK) {
		if (iconData.icon_size > (int32)sizeof(data)) {
			// the stack buffer does not contain the data, see NOTE above
			iconData.icon_data = *_data;
			status = ioctl(fd, B_GET_VECTOR_ICON, &iconData,
				sizeof(device_icon));
			if (status != 0)
				status = errno;
		} else
			memcpy(*_data, data, iconData.icon_size);

		*_size = iconData.icon_size;
		*_type = B_VECTOR_ICON_TYPE;
	}

	// TODO: also support getting the old icon?
	close(fd);
	return status;
#endif
}


status_t
get_named_icon(const char* name, BBitmap* icon, icon_size which)
{
	// check parameters
	if (name == NULL || icon == NULL)
		return B_BAD_VALUE;

	BRect rect;
	if (which == B_MINI_ICON)
		rect.Set(0, 0, 15, 15);
	else if (which == B_LARGE_ICON)
		rect.Set(0, 0, 31, 31);
	else
		return B_BAD_VALUE;

	if (icon->Bounds() != rect)
		return B_BAD_VALUE;

	uint8* data;
	size_t size;
	type_code type;
	status_t status = get_named_icon(name, &data, &size, &type);
	if (status == B_OK) {
		status = BIconUtils::GetVectorIcon(data, size, icon);
		delete[] data;
	}

	return status;
}


status_t
get_named_icon(const char* name, uint8** _data, size_t* _size, type_code* _type)
{
	if (name == NULL || _data == NULL || _size == NULL || _type == NULL)
		return B_BAD_VALUE;

	directory_which kWhich[] = {
		B_USER_NONPACKAGED_DATA_DIRECTORY,
		B_USER_DATA_DIRECTORY,
		B_SYSTEM_NONPACKAGED_DATA_DIRECTORY,
		B_SYSTEM_DATA_DIRECTORY,
	};

	status_t status = B_ENTRY_NOT_FOUND;
	BFile file;
	off_t size;

	for (uint32 i = 0; i < sizeof(kWhich) / sizeof(kWhich[0]); i++) {
		BPath path;
		if (find_directory(kWhich[i], &path) != B_OK)
			continue;

		path.Append("icons");
		path.Append(name);

		status = file.SetTo(path.Path(), B_READ_ONLY);
		if (status == B_OK) {
			status = file.GetSize(&size);
			if (size > 1024 * 1024)
				status = B_ERROR;
		}
		if (status == B_OK)
			break;
	}

	if (status != B_OK)
		return status;

	*_data = new(std::nothrow) uint8[size];
	if (*_data == NULL)
		return B_NO_MEMORY;

	if (file.Read(*_data, size) != size) {
		delete[] *_data;
		return B_ERROR;
	}

	*_size = size;
	*_type = B_VECTOR_ICON_TYPE;
		// TODO: for now

	return B_OK;
}
