//----------------------------------------------------------------------
//  This software is part of the Haiku distribution and is covered 
//  by the MIT license.
//---------------------------------------------------------------------
/*!
	\file FindDirectory.cpp
	find_directory() implementations.	
*/

#include <FindDirectory.h>

#include <errno.h>
#include <string.h>

#include <Directory.h>
#include <Entry.h>
#include <fs_info.h>
#include <Path.h>
#include <Volume.h>


enum {
	NOT_IMPLEMENTED	= B_ERROR,
};

// find_directory
/*!	\brief Internal find_directory() helper function, that does the real work.
	\param which the directory_which constant specifying the directory
	\param path a BPath object to be initialized to the directory's path
	\param createIt \c true, if the directory shall be created, if it doesn't
		   already exist, \c false otherwise.
	\param device the volume on which the directory is located
	\return \c B_OK if everything went fine, an error code otherwise.
*/
static
status_t
find_directory(directory_which which, BPath &path, bool createIt, dev_t device)
{
	status_t error = B_BAD_VALUE;
	switch (which) {
	/* Per volume directories */
		case B_DESKTOP_DIRECTORY:
			error = path.SetTo("~/Desktop");
			break;

		case B_TRASH_DIRECTORY:
			error = path.SetTo("/cosmoe/trash");
			break;

	/* System directories */

		case B_BEOS_SYSTEM_DIRECTORY:
		case B_SYSTEM_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_DIRECTORY:
			error = path.SetTo("/cosmoe");
			break;

		case B_SYSTEM_ADDONS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY:
			error = path.SetTo("/cosmoe/addons");
			break;

		case B_SYSTEM_BOOT_DIRECTORY:
			error = path.SetTo("/cosmoe");
			break;

		case B_SYSTEM_FONTS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_FONTS_DIRECTORY:
			error = path.SetTo("/usr/share/fonts/ttf/cosmoe");
			break;

		case B_SYSTEM_LIB_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_LIB_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_SERVERS_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		
		case B_SYSTEM_APPS_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		
		case B_SYSTEM_BIN_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_BIN_DIRECTORY:
		case B_APPS_DIRECTORY:
		case B_UTILITIES_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		
		case B_SYSTEM_DOCUMENTATION_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_DOCUMENTATION_DIRECTORY:
			error = path.SetTo("/usr/share/doc");
			break;
		
		case B_SYSTEM_PREFERENCES_DIRECTORY:
		case B_PREFERENCES_DIRECTORY:
		case B_SYSTEM_SETTINGS_DIRECTORY:
			error = path.SetTo("/etc");
			break;
		
		case B_SYSTEM_TRANSLATORS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_TRANSLATORS_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
			break;
		
		case B_SYSTEM_MEDIA_NODES_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_MEDIA_NODES_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_SOUNDS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_SOUNDS_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_DATA_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_DATA_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_DEVELOP_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_DEVELOP_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_PACKAGES_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		
		case B_SYSTEM_HEADERS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_HEADERS_DIRECTORY:
			error = path.SetTo("/usr/include");
			break;
		
		case B_SYSTEM_DESKBAR_DIRECTORY:
			error = path.SetTo("/cosmoe/deskbar");
			break;

		case B_SYSTEM_ETC_DIRECTORY:
		case B_BEOS_ETC_DIRECTORY:
			error = path.SetTo("/etc");
			break;

		case B_SYSTEM_SPOOL_DIRECTORY:
			error = path.SetTo("/var/spool");
			break;

		case B_SYSTEM_TEMP_DIRECTORY:
		case B_SYSTEM_CACHE_DIRECTORY:
			error = path.SetTo("/var/tmp");
			break;

		case B_SYSTEM_VAR_DIRECTORY:
			error = path.SetTo("/var");
			break;

		case B_SYSTEM_LOG_DIRECTORY:
			error = path.SetTo("/var/log");
			break;

		case B_PACKAGE_LINKS_DIRECTORY:
			error= path.SetTo("/cosmoe/package");
			break;

	/* User directories. These are interpreted in the context
	   of the user making the find_directory call. */
		case B_USER_DIRECTORY:
		case B_USER_NONPACKAGED_DIRECTORY:
			error = path.SetTo("~");
			break;

		case B_USER_CONFIG_DIRECTORY:
			error = path.SetTo("~/cosmoe");
			break;

		case B_USER_ADDONS_DIRECTORY:
		case B_USER_NONPACKAGED_ADDONS_DIRECTORY:
			error = path.SetTo("~/cosmoe/addons");
			break;

		case B_USER_BOOT_DIRECTORY:
			error = path.SetTo("~/cosmoe");
			break;

		case B_USER_FONTS_DIRECTORY:
		case B_USER_NONPACKAGED_FONTS_DIRECTORY:
			error = path.SetTo("~/cosmoe/fonts");
			break;

		case B_USER_LIB_DIRECTORY:
		case B_USER_NONPACKAGED_LIB_DIRECTORY:
			error = path.SetTo("~/cosmoe/lib");
			break;

		case B_USER_SETTINGS_DIRECTORY:
			error = path.SetTo("~/cosmoe/settings");
			break;

		case B_USER_DESKBAR_DIRECTORY:
			error = path.SetTo("~/cosmoe/deskbar");
			break;

		case B_USER_PRINTERS_DIRECTORY:
			error = path.SetTo("~/cosmoe/printers");
			break;

		case B_USER_TRANSLATORS_DIRECTORY:
		case B_USER_NONPACKAGED_TRANSLATORS_DIRECTORY:
			error = path.SetTo("~/cosmoe/translators");
			break;

		case B_USER_MEDIA_NODES_DIRECTORY:
		case B_USER_NONPACKAGED_MEDIA_NODES_DIRECTORY:
			error = path.SetTo("~/cosmoe/media");
			break;

		case B_USER_SOUNDS_DIRECTORY:
		case B_USER_NONPACKAGED_SOUNDS_DIRECTORY:
			error = path.SetTo("~/cosmoe/sounds");
			break;

		case B_USER_DATA_DIRECTORY:
		case B_USER_NONPACKAGED_DATA_DIRECTORY:
			error = path.SetTo("~/cosmoe/data");
			break;

		case B_USER_CACHE_DIRECTORY:
			error = path.SetTo("~/cosmoe/cache");
			break;

		case B_USER_PACKAGES_DIRECTORY:
			error = path.SetTo("~/cosmoe/packages");
			break;

		case B_USER_HEADERS_DIRECTORY:
		case B_USER_NONPACKAGED_HEADERS_DIRECTORY:
			error = path.SetTo("~/cosmoe/headers");
			break;
	
		case B_USER_DEVELOP_DIRECTORY:
		case B_USER_NONPACKAGED_DEVELOP_DIRECTORY:
			error = path.SetTo("~/cosmoe/develop");
			break;

		case B_USER_DOCUMENTATION_DIRECTORY:
		case B_USER_NONPACKAGED_DOCUMENTATION_DIRECTORY:
			error = path.SetTo("~/cosmoe/doc");
			break;

		case B_USER_SERVERS_DIRECTORY:
			error = path.SetTo("~/cosmoe/servers");
			break;
		
		case B_USER_APPS_DIRECTORY:
			error = path.SetTo("~/cosmoe/apps");
			break;

		case B_USER_BIN_DIRECTORY:
		case B_USER_NONPACKAGED_BIN_DIRECTORY:
			error = path.SetTo("~/cosmoe/bin");
			break;

		case B_USER_PREFERENCES_DIRECTORY:
			error = path.SetTo("~/cosmoe/prefs");
			break;

		case B_USER_ETC_DIRECTORY:
			error = path.SetTo("~/cosmoe/etc");
			break;

		case B_USER_LOG_DIRECTORY:
			error = path.SetTo("~/cosmoe/log");
			break;

		case B_USER_SPOOL_DIRECTORY:
			error = path.SetTo("~/cosmoe/spool");
			break;

		case B_USER_VAR_DIRECTORY:
			error = path.SetTo("~/cosmoe/var");
			break;
	}
#if 0
	switch (which) {
		// volume relative dirs
		case B_DESKTOP_DIRECTORY:
		{
			error = path.SetTo("~/Desktop");
			break;
		}
		case B_TRASH_DIRECTORY:
		{
			error = B_ENTRY_NOT_FOUND;
			break;
		}
		// BeOS directories.  These are mostly accessed read-only.
		case B_BEOS_DIRECTORY:
			error = path.SetTo("/cosmoe");
			break;
		case B_BEOS_SYSTEM_DIRECTORY:
			error = path.SetTo("/cosmoe");
			break;
		case B_BEOS_ADDONS_DIRECTORY:
			error = path.SetTo("/cosmoe/add-ons");
			break;
		case B_BEOS_BOOT_DIRECTORY:
			error = path.SetTo("/cosmoe/boot");
			break;
		case B_BEOS_FONTS_DIRECTORY:
			error = path.SetTo("/usr/share/fonts/ttf/cosmoe");
			break;
		case B_BEOS_LIB_DIRECTORY:
			error = path.SetTo("/usr/local/lib");
			break;
 		case B_BEOS_SERVERS_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
			break;
		case B_BEOS_APPS_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
			break;
		case B_BEOS_BIN_DIRECTORY:
			error = path.SetTo("/bin");
			break;
		case B_BEOS_ETC_DIRECTORY:
			error = path.SetTo("/etc");
			break;
		case B_BEOS_DOCUMENTATION_DIRECTORY:
			error = path.SetTo("/boot/beos/documentation");
			break;
		case B_BEOS_PREFERENCES_DIRECTORY:
			error = path.SetTo("/boot/beos/preferences");
			break;
		case B_BEOS_TRANSLATORS_DIRECTORY:
			error = path.SetTo("/cosmoe/add-ons/Translators");
			break;
		case B_BEOS_MEDIA_NODES_DIRECTORY:
			error = path.SetTo("/cosmoe/add-ons/media");
			break;
		case B_BEOS_SOUNDS_DIRECTORY:
			error = path.SetTo("/boot/beos/etc/sounds");
			break;
		// Common directories, shared among all users.
		case B_COMMON_DIRECTORY:
			error = path.SetTo("/boot/home");
			break;
		case B_COMMON_SYSTEM_DIRECTORY:
			error = path.SetTo("/boot/home/config");
			break;
		case B_COMMON_ADDONS_DIRECTORY:
			error = path.SetTo("/boot/home/config/add-ons");
			break;
		case B_COMMON_BOOT_DIRECTORY:
			error = path.SetTo("/boot/home/config/boot");
			break;
		case B_COMMON_FONTS_DIRECTORY:
			error = path.SetTo("/usr/share/fonts/cosmoe");
			break;
		case B_COMMON_LIB_DIRECTORY:
			error = path.SetTo("/usr/lib");
			break;
		case B_COMMON_SERVERS_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		case B_COMMON_BIN_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		case B_COMMON_ETC_DIRECTORY:
			error = path.SetTo("/etc");
			break;
		case B_COMMON_DOCUMENTATION_DIRECTORY:
			error = path.SetTo("/boot/home/config/documentation");
			break;
		case B_COMMON_SETTINGS_DIRECTORY:
			error = path.SetTo("/etc");
			break;
		case B_COMMON_DEVELOP_DIRECTORY:
			error = path.SetTo("/boot/develop");
			break;
		case B_COMMON_LOG_DIRECTORY:
			error = path.SetTo("/var/log");
			break;
		case B_COMMON_SPOOL_DIRECTORY:
			error = path.SetTo("/var/spool");
			break;
		case B_COMMON_TEMP_DIRECTORY:
			error = path.SetTo("/var/tmp");
			break;
		case B_COMMON_VAR_DIRECTORY:
			error = path.SetTo("/var");
			break;
		case B_COMMON_TRANSLATORS_DIRECTORY:
			error = path.SetTo("/usr/local/share/cosmoe/add-ons/Translators");
			break;
		case B_COMMON_MEDIA_NODES_DIRECTORY:
			error = path.SetTo("/boot/home/config/add-ons/media");
			break;
		case B_COMMON_SOUNDS_DIRECTORY:
			error = path.SetTo("/boot/home/config/sounds");
			break;
		// User directories.  These are interpreted in the context
		// of the user making the find_directory call.
		case B_USER_DIRECTORY:
			error = path.SetTo("~");
			break;
		case B_USER_CONFIG_DIRECTORY:
			error = path.SetTo("/boot/home/config");
			break;
		case B_USER_ADDONS_DIRECTORY:
			error = path.SetTo("/boot/home/config/add-ons");
			break;
		case B_USER_BOOT_DIRECTORY:
			error = path.SetTo("/boot/home/config/boot");
			break;
		case B_USER_FONTS_DIRECTORY:
			error = path.SetTo("/boot/home/config/fonts");
			break;
		case B_USER_LIB_DIRECTORY:
			error = path.SetTo("/boot/home/config/lib");
			break;
		case B_USER_SETTINGS_DIRECTORY:
			error = path.SetTo("/boot/home/config/settings");
			break;
		case B_USER_DESKBAR_DIRECTORY:
			error = path.SetTo("/boot/home/config/be");
			break;
		case B_USER_PRINTERS_DIRECTORY:
			error = path.SetTo("/boot/home/config/settings/printers");
			break;
		case B_USER_TRANSLATORS_DIRECTORY:
			error = path.SetTo("/boot/home/config/add-ons/Translators");
			break;
		case B_USER_MEDIA_NODES_DIRECTORY:
			error = path.SetTo("/boot/home/config/add-ons/media");
			break;
		case B_USER_SOUNDS_DIRECTORY:
			error = path.SetTo("/boot/home/config/sounds");
			break;
		// Global directories.
		case B_APPS_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
		case B_PREFERENCES_DIRECTORY:
			error = path.SetTo("/etc");
			break;
		case B_UTILITIES_DIRECTORY:
			error = path.SetTo("/usr/bin");
			break;
	}
	#endif
	// create the directory, if desired
	if (error == B_OK && createIt)
		create_directory(path.Path(), S_IRWXU | S_IRWXG | S_IRWXO);
	return error;
}


// find_directory
//!	Returns a path of a directory specified by a directory_which constant.
/*!	If the supplied volume ID is 
	\param which the directory_which constant specifying the directory
	\param volume the volume on which the directory is located
	\param createIt \c true, if the directory shall be created, if it doesn't
		   already exist, \c false otherwise.
	\param pathString a pointer to a buffer into which the directory path
		   shall be written.
	\param length the size of the buffer
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a pathString.
	- \c E2BIG: Buffer is too small for path.
	- another error code
*/
status_t
find_directory(directory_which which, dev_t volume, bool createIt,
			   char *pathString, int32 length)
{
	status_t error = (pathString ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		BPath path;
		error = find_directory(which, path, createIt, volume);
		if (error == B_OK && (int32)strlen(path.Path()) >= length)
			error = E2BIG;
		if (error == B_OK)
			strcpy(pathString, path.Path());
	}
	return error;
}

// find_directory
//!	Returns a path of a directory specified by a directory_which constant.
/*!	\param which the directory_which constant specifying the directory
	\param path a BPath object to be initialized to the directory's path
	\param createIt \c true, if the directory shall be created, if it doesn't
		   already exist, \c false otherwise.
	\param volume the volume on which the directory is located
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a path.
	- another error code
*/
status_t
find_directory(directory_which which, BPath* path, bool createIt,
			   BVolume* volume)
{
	if (path == NULL)
		return B_BAD_VALUE;

	dev_t device = (dev_t)-1;
	if (volume && volume->InitCheck() == B_OK)
		device = volume->Device();

	status_t error = find_directory(which, *path, createIt, device);
	
	return error;
}

