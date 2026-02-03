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
#include <String.h>
#include <Entry.h>
#include <fs_info.h>
#include <Path.h>
#include <Volume.h>

#if defined(__linux__) || defined(__unix__)
#include <dlfcn.h>
#endif


enum {
	NOT_IMPLEMENTED	= B_ERROR,
};

// get_system_lib_directory
/*!	\brief Helper function to detect the system library directory at runtime.
	Uses dladdr() on Linux/Unix to find where libbe.so is actually installed,
	which handles multiarch paths like /usr/lib/x86_64-linux-gnu automatically.
	\param path a BPath object to be initialized to the library directory path
	\return \c B_OK if successful, an error code otherwise.
*/
static
status_t
get_system_lib_directory(BPath &path)
{
#if defined(__linux__)
	// On Linux, use dladdr to find where libbe.so is actually installed
	// This handles /usr/local/lib, /usr/lib, /usr/lib/x86_64-linux-gnu, etc.
	Dl_info info;
	// Use the address of any function in libbe to find where it's loaded
	void* symbol = dlsym(RTLD_DEFAULT, "find_directory");
	if (symbol != NULL && dladdr(symbol, &info) != 0 && info.dli_fname != NULL) {
		// info.dli_fname contains the full path to libbe.so
		BPath libPath(info.dli_fname);
		if (libPath.InitCheck() == B_OK) {
			// Get the directory containing the library
			BPath parentPath;
			if (libPath.GetParent(&parentPath) == B_OK) {
				return path.SetTo(parentPath.Path());
			}
		}
	}
	// Fallback to standard location if dladdr fails
	return path.SetTo("/usr/local/lib");
#else
	return path.SetTo("/usr/local/lib");
#endif
}

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
	BString userpath;

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
			error = get_system_lib_directory(path);
			if (error == B_OK)
				path.Append("addons");
			break;

		case B_SYSTEM_BOOT_DIRECTORY:
			error = path.SetTo("/cosmoe");
			break;

		case B_SYSTEM_FONTS_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_FONTS_DIRECTORY:
#ifdef __APPLE__
			// macOS system fonts are in /Library/Fonts and /System/Library/Fonts
			// We'll use /Library/Fonts as the primary location
			error = path.SetTo("/Library/Fonts");
#elif defined(_WIN32)
			// Windows fonts directory - works both native and under Wine
			error = path.SetTo("C:/Windows/Fonts");
#else
			error = path.SetTo("/usr/share/fonts/ttf/cosmoe");
#endif
			break;

		case B_SYSTEM_LIB_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_LIB_DIRECTORY:
			error = get_system_lib_directory(path);
			break;
		
		case B_SYSTEM_SERVERS_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
			break;
		
		case B_SYSTEM_APPS_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
			break;
		
		case B_SYSTEM_BIN_DIRECTORY:
		case B_SYSTEM_NONPACKAGED_BIN_DIRECTORY:
		case B_APPS_DIRECTORY:
		case B_UTILITIES_DIRECTORY:
			error = path.SetTo("/usr/local/bin");
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
			error = get_system_lib_directory(path);
			if (error == B_OK)
				path.Append("addons/Translators");
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
			error = path.SetTo("/usr/local/include");
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
			userpath << getenv("HOME");
			error = path.SetTo(userpath);
			break;

		case B_USER_CONFIG_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe";
			error = path.SetTo(userpath);
			break;

		case B_USER_ADDONS_DIRECTORY:
		case B_USER_NONPACKAGED_ADDONS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/addons";
			error = path.SetTo(userpath);
			break;

		case B_USER_BOOT_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe";
			error = path.SetTo(userpath);
			break;

		case B_USER_FONTS_DIRECTORY:
		case B_USER_NONPACKAGED_FONTS_DIRECTORY:
#ifdef __APPLE__
			// macOS user fonts are in ~/Library/Fonts
			userpath << getenv("HOME") << "/Library/Fonts/Cosmoe";
#else
			userpath << getenv("HOME") << "/cosmoe/fonts";
#endif
			error = path.SetTo(userpath);
			break;

		case B_USER_LIB_DIRECTORY:
		case B_USER_NONPACKAGED_LIB_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/lib";
			error = path.SetTo(userpath);
			break;

		case B_USER_SETTINGS_DIRECTORY:
			userpath << getenv("HOME");
			error = path.SetTo(userpath);
			break;

		case B_USER_DESKBAR_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/deskbar";
			error = path.SetTo(userpath);
			break;

		case B_USER_PRINTERS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/printers";
			error = path.SetTo(userpath);
			break;

		case B_USER_TRANSLATORS_DIRECTORY:
		case B_USER_NONPACKAGED_TRANSLATORS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/addons/Translators";
			error = path.SetTo(userpath);
			break;

		case B_USER_MEDIA_NODES_DIRECTORY:
		case B_USER_NONPACKAGED_MEDIA_NODES_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/media";
			error = path.SetTo(userpath);
			break;

		case B_USER_SOUNDS_DIRECTORY:
		case B_USER_NONPACKAGED_SOUNDS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/sounds";
			error = path.SetTo(userpath);
			break;

		case B_USER_DATA_DIRECTORY:
		case B_USER_NONPACKAGED_DATA_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/data";
			error = path.SetTo(userpath);
			break;

		case B_USER_CACHE_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/cache";
			error = path.SetTo(userpath);
			break;

		case B_USER_PACKAGES_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/packages";
			error = path.SetTo(userpath);
			break;

		case B_USER_HEADERS_DIRECTORY:
		case B_USER_NONPACKAGED_HEADERS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/headers";
			error = path.SetTo(userpath);
			break;
	
		case B_USER_DEVELOP_DIRECTORY:
		case B_USER_NONPACKAGED_DEVELOP_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/develop";
			error = path.SetTo(userpath);
			break;

		case B_USER_DOCUMENTATION_DIRECTORY:
		case B_USER_NONPACKAGED_DOCUMENTATION_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/doc";
			error = path.SetTo(userpath);
			break;

		case B_USER_SERVERS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/servers";
			error = path.SetTo(userpath);
			break;
		
		case B_USER_APPS_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/apps";
			error = path.SetTo(userpath);
			break;

		case B_USER_BIN_DIRECTORY:
		case B_USER_NONPACKAGED_BIN_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/bin";
			error = path.SetTo(userpath);
			break;

		case B_USER_PREFERENCES_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/prefs";
			error = path.SetTo(userpath);
			break;

		case B_USER_ETC_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/etc";
			error = path.SetTo(userpath);
			break;

		case B_USER_LOG_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/log";
			error = path.SetTo(userpath);
			break;

		case B_USER_SPOOL_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/spool";
			error = path.SetTo(userpath);
			break;

		case B_USER_VAR_DIRECTORY:
			userpath << getenv("HOME") << "/cosmoe/var";
			error = path.SetTo(userpath);
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

