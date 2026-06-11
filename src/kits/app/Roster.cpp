/*
 * Copyright 2001-2015 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Ingo Weinhold, ingo_weinhold@gmx.de
 */


#include <Roster.h>

#include <ctype.h>
#include <mutex>
#include <new>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#ifdef _WIN32
#include <process.h>
#endif

#include <Alert.h>
#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <fs_index.h>
#include <fs_info.h>
#include <image.h>
#include <List.h>
#include <Mime.h>
#include <Node.h>
#include <NodeInfo.h>
#include <OS.h>
#include <Path.h>
#include <Query.h>
#include <RegistrarDefs.h>
#include <String.h>
#include <SymLink.h>
#include <Volume.h>
#include <VolumeRoster.h>

#include <locks.h>

#include <AppMisc.h>
#include <DesktopLink.h>
#include <MessengerPrivate.h>
#include <PortLink.h>
#include <RosterPrivate.h>
#include <ServerProtocol.h>

// On macOS, environ needs to be declared explicitly
#ifdef __APPLE__
extern char **environ;
#endif


using namespace std;
using namespace BPrivate;


#ifdef __linux__
static void
desktop_id_from_signature(const char* signature, char* desktopID,
	size_t desktopIDSize)
{
	if (desktopID == NULL || desktopIDSize == 0)
		return;

	desktopID[0] = '\0';
	if (signature == NULL || signature[0] == '\0')
		return;

	const char* normalized = signature;
	if (strncasecmp(normalized, "application/", 12) == 0)
		normalized += 12;

	strlcpy(desktopID, normalized, desktopIDSize);
}


static void
canonical_signature_from_desktop_id(const char* desktopID, char* signature,
	size_t signatureSize)
{
	if (signature == NULL || signatureSize == 0)
		return;

	signature[0] = '\0';
	if (desktopID == NULL || desktopID[0] == '\0')
		return;

	if (strncasecmp(desktopID, "application/", 12) == 0) {
		strlcpy(signature, desktopID, signatureSize);
		return;
	}

	strlcpy(signature, "application/", signatureSize);
	strlcat(signature, desktopID, signatureSize);
}


static status_t
desktop_entry_value(const BPath& desktopPath, const char* key, char* value,
	size_t valueSize);


static status_t
normalize_xdg_lookup_key(const char* input, char* output, size_t outputSize)
{
	if (output == NULL || outputSize == 0)
		return B_BAD_VALUE;

	output[0] = '\0';
	if (input == NULL || input[0] == '\0')
		return B_BAD_VALUE;

	size_t out = 0;
	for (size_t i = 0; input[i] != '\0' && out + 1 < outputSize; i++) {
		unsigned char c = (unsigned char)input[i];
		if (isalnum(c))
			output[out++] = (char)tolower(c);
		else if (c == '.' || c == '-' || c == '_')
			output[out++] = '-';
		else if (isspace(c))
			output[out++] = '-';
	}
	output[out] = '\0';
	return output[0] != '\0' ? B_OK : B_BAD_VALUE;
}


static status_t
find_xdg_desktop_file(const char* signature, BPath& desktopPath,
	char* matchedDesktopID, size_t matchedDesktopIDSize)
{
	char desktopID[B_MIME_TYPE_LENGTH];
	desktop_id_from_signature(signature, desktopID, sizeof(desktopID));
	if (desktopID[0] == '\0')
		return B_BAD_VALUE;

	if (matchedDesktopID != NULL && matchedDesktopIDSize > 0)
		matchedDesktopID[0] = '\0';

	char normalizedDesktopID[B_MIME_TYPE_LENGTH];
	normalize_xdg_lookup_key(desktopID, normalizedDesktopID,
		sizeof(normalizedDesktopID));

	const char* dataDirs = getenv("XDG_DATA_DIRS");
	if (dataDirs == NULL || dataDirs[0] == '\0')
		dataDirs = "/usr/local/share:/usr/share";

	char* dataDirsCopy = strdup(dataDirs);
	if (dataDirsCopy == NULL)
		return B_NO_MEMORY;

	status_t status = B_ENTRY_NOT_FOUND;
	char* last = NULL;
	for (char* root = strtok_r(dataDirsCopy, ":", &last);
		root != NULL; root = strtok_r(NULL, ":", &last)) {
		if (root[0] == '\0')
			continue;

		char desktopFileName[B_MIME_TYPE_LENGTH + 16];
		strlcpy(desktopFileName, desktopID, sizeof(desktopFileName));
		strlcat(desktopFileName, ".desktop", sizeof(desktopFileName));

		BPath candidate(root, "applications");
		candidate.Append(desktopFileName);

		BEntry entry(candidate.Path());
		if (entry.Exists() && entry.IsFile()) {
			desktopPath = candidate;
			if (matchedDesktopID != NULL && matchedDesktopIDSize > 0)
				strlcpy(matchedDesktopID, desktopID, matchedDesktopIDSize);
			status = B_OK;
			break;
		}

		BPath applicationsPath;
		candidate.GetParent(&applicationsPath);
		BDirectory applications(applicationsPath.Path());
		if (applications.InitCheck() != B_OK)
			continue;

		BEntry appEntry;
		while (applications.GetNextEntry(&appEntry) == B_OK) {
			if (!appEntry.IsFile())
				continue;

			BPath appPath;
			if (appEntry.GetPath(&appPath) != B_OK)
				continue;

			const char* leaf = appPath.Leaf();
			if (leaf == NULL)
				continue;
			size_t leafLength = strlen(leaf);
			if (leafLength <= 8 || strcasecmp(leaf + leafLength - 8, ".desktop") != 0)
				continue;

			char candidateDesktopID[B_MIME_TYPE_LENGTH];
			strlcpy(candidateDesktopID, leaf, sizeof(candidateDesktopID));
			candidateDesktopID[leafLength - 8] = '\0';

			char normalizedCandidateID[B_MIME_TYPE_LENGTH];
			normalize_xdg_lookup_key(candidateDesktopID, normalizedCandidateID,
				sizeof(normalizedCandidateID));
			if (strcmp(normalizedCandidateID, normalizedDesktopID) == 0) {
				desktopPath = appPath;
				if (matchedDesktopID != NULL && matchedDesktopIDSize > 0)
					strlcpy(matchedDesktopID, candidateDesktopID,
						matchedDesktopIDSize);
				status = B_OK;
				break;
			}

			char value[B_PATH_NAME_LENGTH];
			if (desktop_entry_value(appPath, "StartupWMClass", value,
					sizeof(value)) == B_OK) {
				char normalizedValue[B_PATH_NAME_LENGTH];
				normalize_xdg_lookup_key(value, normalizedValue,
					sizeof(normalizedValue));
				if (strcmp(normalizedValue, normalizedDesktopID) == 0) {
					desktopPath = appPath;
					if (matchedDesktopID != NULL && matchedDesktopIDSize > 0)
						strlcpy(matchedDesktopID, candidateDesktopID,
							matchedDesktopIDSize);
					status = B_OK;
					break;
				}
			}

			if (desktop_entry_value(appPath, "Name", value, sizeof(value)) == B_OK) {
				char normalizedValue[B_PATH_NAME_LENGTH];
				normalize_xdg_lookup_key(value, normalizedValue,
					sizeof(normalizedValue));
				if (strcmp(normalizedValue, normalizedDesktopID) == 0) {
					desktopPath = appPath;
					if (matchedDesktopID != NULL && matchedDesktopIDSize > 0)
						strlcpy(matchedDesktopID, candidateDesktopID,
							matchedDesktopIDSize);
					status = B_OK;
					break;
				}
			}
		}

		if (status == B_OK)
			break;
	}

	free(dataDirsCopy);
	return status;
}


static status_t
desktop_entry_value(const BPath& desktopPath, const char* key, char* value,
	size_t valueSize)
{
	if (key == NULL || value == NULL || valueSize == 0)
		return B_BAD_VALUE;

	value[0] = '\0';
	FILE* file = fopen(desktopPath.Path(), "r");
	if (file == NULL)
		return errno != 0 ? errno : B_ENTRY_NOT_FOUND;

	char prefix[128];
	strlcpy(prefix, key, sizeof(prefix));
	strlcat(prefix, "=", sizeof(prefix));
	size_t prefixLength = strlen(prefix);

	bool inDesktopEntry = false;
	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		size_t length = strlen(line);
		while (length > 0 && (line[length - 1] == '\n'
			|| line[length - 1] == '\r')) {
			line[--length] = '\0';
		}

		if (strcmp(line, "[Desktop Entry]") == 0) {
			inDesktopEntry = true;
			continue;
		}

		if (line[0] == '[') {
			inDesktopEntry = false;
			continue;
		}

		if (inDesktopEntry && strncmp(line, prefix, prefixLength) == 0) {
			strlcpy(value, line + prefixLength, valueSize);
			fclose(file);
			return B_OK;
		}
	}

	fclose(file);
	return B_ENTRY_NOT_FOUND;
}


static status_t
resolve_exec_to_entry_ref(const char* execLine, entry_ref* appRef)
{
	if (execLine == NULL || appRef == NULL)
		return B_BAD_VALUE;

	while (isspace((unsigned char)*execLine))
		execLine++;
	if (*execLine == '\0')
		return B_BAD_VALUE;

	char command[B_PATH_NAME_LENGTH];
	size_t index = 0;
	if (*execLine == '"') {
		execLine++;
		while (*execLine != '\0' && *execLine != '"'
			&& index + 1 < sizeof(command)) {
			command[index++] = *execLine++;
		}
	} else {
		while (*execLine != '\0' && !isspace((unsigned char)*execLine)
			&& *execLine != '%'
			&& index + 1 < sizeof(command)) {
			command[index++] = *execLine++;
		}
	}
	command[index] = '\0';

	if (command[0] == '\0')
		return B_BAD_VALUE;

	BEntry entry;
	if (strchr(command, '/') != NULL) {
		if (entry.SetTo(command, true) == B_OK && entry.IsFile())
			return entry.GetRef(appRef);
		return B_ENTRY_NOT_FOUND;
	}

	const char* searchPathes = getenv("PATH");
	if (searchPathes == NULL || searchPathes[0] == '\0')
		searchPathes = "/usr/local/bin:/usr/bin:/bin";

	char* searchBuffer = strdup(searchPathes);
	if (searchBuffer == NULL)
		return B_NO_MEMORY;

	status_t status = B_ENTRY_NOT_FOUND;
	char* last = NULL;
	for (char* path = strtok_r(searchBuffer, ":", &last);
		path != NULL; path = strtok_r(NULL, ":", &last)) {
		if (path[0] == '\0')
			continue;

		BPath candidate(path, command);
		if (entry.SetTo(candidate.Path(), true) == B_OK && entry.IsFile()
			&& entry.GetRef(appRef) == B_OK) {
			status = B_OK;
			break;
		}
	}

	free(searchBuffer);
	return status;
}


static status_t
resolve_xdg_desktop_app(const char* signature, entry_ref* appRef,
	char* canonicalSignature, size_t canonicalSignatureSize)
{
	BPath desktopPath;
	char matchedDesktopID[B_MIME_TYPE_LENGTH];
	status_t status = find_xdg_desktop_file(signature, desktopPath,
		matchedDesktopID, sizeof(matchedDesktopID));
	if (status != B_OK)
		return status;

	char execLine[B_PATH_NAME_LENGTH];
	status = desktop_entry_value(desktopPath, "Exec", execLine,
		sizeof(execLine));
	if (status != B_OK)
		return status;

	status = resolve_exec_to_entry_ref(execLine, appRef);
	if (status != B_OK)
		return status;

	canonical_signature_from_desktop_id(matchedDesktopID, canonicalSignature,
		canonicalSignatureSize);
	return B_OK;
}
#endif


// debugging
//#define DBG(x) x
#define DBG(x)
#ifdef DEBUG_PRINTF
#	define OUT DEBUG_PRINTF
#else
#	define OUT printf
#endif


const BRoster* be_roster;

// In-process storage for recent documents and folders
static std::mutex sRecentListsMutex;
static BList sRecentDocuments;
static BList sRecentFolders;
static BList sRecentApps;
static const int32 kMaxRecentItems = 10;

struct app_watcher_entry {
	BMessenger target;
	uint32 eventMask;

	app_watcher_entry(const BMessenger& messenger, uint32 mask)
		:
		target(messenger),
		eventMask(mask)
	{
	}
};

static std::mutex sAppWatchersMutex;
static std::vector<app_watcher_entry> sAppWatchers;
static bool sBackendAppWatcherInstalled = false;

static void roster_app_watcher_callback(cosmoe_display_t display, int32_t event,
	int32_t teamID, void* userData);
static bool roster_has_app_watchers_locked();
static status_t roster_set_backend_app_watcher();
static status_t roster_clear_backend_app_watcher();


//	#pragma mark - Helper functions

#if 0
/*!	Extracts an app_info from a BMessage.

	The function searchs for a field "app_info" typed B_REG_APP_INFO_TYPE
	and initializes \a info with the found data.

	\param message The message
	\param info A pointer to a pre-allocated app_info to be filled in with the
	       info found in the message.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \c NULL \a message or \a info.
*/
static status_t
find_message_app_info(BMessage* message, app_info* info)
{
	status_t error = (message && info ? B_OK : B_BAD_VALUE);
	const flat_app_info* flatInfo = NULL;
	ssize_t size = 0;
	// find the flat app info in the message
	if (error == B_OK) {
		error = message->FindData("app_info", B_REG_APP_INFO_TYPE,
			(const void**)&flatInfo, &size);
	}
	// unflatten the flat info
	if (error == B_OK) {
		if (size == sizeof(flat_app_info)) {
			info->thread = flatInfo->thread;
			info->team = flatInfo->team;
			info->port = flatInfo->port;
			info->flags = flatInfo->flags;
			info->ref.device = flatInfo->ref_device;
			info->ref.directory = flatInfo->ref_directory;
			info->ref.name = NULL;
			memcpy(info->signature, flatInfo->signature, B_MIME_TYPE_LENGTH);
			if (strlen(flatInfo->ref_name) > 0)
				info->ref.set_name(flatInfo->ref_name);
		} else
			error = B_ERROR;
	}

	return error;
}

#endif


static void
normalize_backend_signature(const char* backendSignature, char* normalized,
	size_t normalizedSize)
{
	if (normalized == NULL || normalizedSize == 0)
		return;

	normalized[0] = '\0';
	if (backendSignature == NULL || backendSignature[0] == '\0')
		return;

	if (strncasecmp(backendSignature, "application/", 12) == 0) {
		strlcpy(normalized, backendSignature, normalizedSize);
		return;
	}

	if (strncasecmp(backendSignature, "x-vnd.", 6) == 0) {
		strlcpy(normalized, "application/", normalizedSize);
		strlcat(normalized, backendSignature, normalizedSize);
		return;
	}

	strlcpy(normalized, backendSignature, normalizedSize);
}


static status_t
find_backend_app_info(team_id team, app_info* info)
{
	if (team >= 0 || info == NULL || be_app == NULL)
		return B_BAD_VALUE;

	cosmoe_display_t display = be_app->Display();
	if (display == NULL)
		return B_BAD_VALUE;

	cosmoe_backend_app_info backendInfo;
	memset(&backendInfo, 0, sizeof(backendInfo));

	status_t error = cosmoe_display_get_app_info(display, (int32_t)team,
		&backendInfo);
	if (error != B_OK)
		return error;

	info->team = team;
	info->thread = -1;
	info->flags = backendInfo.flags;

	normalize_backend_signature(backendInfo.signature, info->signature, B_MIME_TYPE_LENGTH);

	/* If the signature resolves to a known app, prefer its real ref/flags. */
	if (info->signature[0] != '\0') {
		entry_ref ref;
		if (be_roster->FindApp(info->signature, &ref) == B_OK) {
			info->ref = ref;

			BFile appFile;
			if (appFile.SetTo(&info->ref, B_READ_ONLY) == B_OK) {
				BAppFileInfo appFileInfo;
				if (appFileInfo.SetTo(&appFile) == B_OK) {
					uint32 appFlags;
					if (appFileInfo.GetAppFlags(&appFlags) == B_OK)
						info->flags = appFlags;
				}
			}
		}
	}

#ifdef __linux__
	if ((info->ref.name == NULL || info->ref.name[0] == '\0')
		&& backendInfo.identifier[0] != '\0') {
		char canonicalSignature[B_MIME_TYPE_LENGTH];
		entry_ref ref;
		if (resolve_xdg_desktop_app(backendInfo.identifier, &ref,
				canonicalSignature, sizeof(canonicalSignature)) == B_OK) {
			info->ref = ref;
			if (canonicalSignature[0] != '\0') {
				strlcpy(info->signature, canonicalSignature, B_MIME_TYPE_LENGTH);
			}

			BFile appFile;
			if (appFile.SetTo(&info->ref, B_READ_ONLY) == B_OK) {
				BAppFileInfo appFileInfo;
				if (appFileInfo.SetTo(&appFile) == B_OK) {
					uint32 appFlags;
					if (appFileInfo.GetAppFlags(&appFlags) == B_OK)
						info->flags = appFlags;
				}
			}
		}
	}
#endif

	if (info->signature[0] == '\0') {
		snprintf(info->signature, B_MIME_TYPE_LENGTH,
			"application/x-vnd.cosmoe-%" B_PRId32, (int32)team);
	}

	return B_OK;
}

/*!	Checks whether or not an application can be used.

	Currently it is only checked whether the application is in the trash.

	\param ref An entry_ref referring to the application executable.

	\return A status code, \c B_OK on success oir other error codes specifying
	        why the application cannot be used.
	\retval B_OK The application can be used.
	\retval B_ENTRY_NOT_FOUND \a ref doesn't refer to and existing entry.
	\retval B_IS_A_DIRECTORY \a ref refers to a directory.
	\retval B_LAUNCH_FAILED_APP_IN_TRASH The application executable is in the
	        trash.
*/
static status_t
can_app_be_used(const entry_ref* ref)
{
	status_t error = (ref ? B_OK : B_BAD_VALUE);
	// check whether the file exists and is a file.
	BEntry entry;
	if (error == B_OK) {
		if (ref->name != NULL && ref->name[0] == '/')
			error = entry.SetTo(ref->name, true);
		else
			error = entry.SetTo(ref, true);
	}

	if (error == B_OK && !entry.Exists())
		error = B_ENTRY_NOT_FOUND;

	if (error == B_OK && !entry.IsFile())
		error = B_IS_A_DIRECTORY;

	// check whether the file is in trash
	BPath trashPath;
	BDirectory directory;
	BVolume volume;
	if (error == B_OK
		&& volume.SetTo(ref->device) == B_OK
		&& find_directory(B_TRASH_DIRECTORY, &trashPath, false, &volume)
			== B_OK
		&& directory.SetTo(trashPath.Path()) == B_OK
		&& directory.Contains(&entry)) {
		error = B_LAUNCH_FAILED_APP_IN_TRASH;
	}

	return error;
}


static bool
roster_has_app_watchers_locked()
{
	return !sAppWatchers.empty();
}


static status_t
roster_set_backend_app_watcher()
{
	if (be_app == NULL)
		return B_NO_INIT;

	cosmoe_display_t display = be_app->Display();
	if (display == NULL)
		return B_NO_INIT;

	status_t status = cosmoe_display_set_app_watcher(display,
		roster_app_watcher_callback, NULL);
	if (status == B_OK)
		sBackendAppWatcherInstalled = true;

	return status;
}


static status_t
roster_clear_backend_app_watcher()
{
	if (!sBackendAppWatcherInstalled)
		return B_OK;

	if (be_app == NULL)
		return B_NO_INIT;

	cosmoe_display_t display = be_app->Display();
	if (display == NULL)
		return B_NO_INIT;

	status_t status = cosmoe_display_clear_app_watcher(display);
	if (status == B_OK)
		sBackendAppWatcherInstalled = false;

	return status;
}


static void
roster_app_watcher_callback(cosmoe_display_t display, int32_t event,
	int32_t teamID, void* userData)
{
	(void)display;
	(void)userData;

	uint32 mask = 0;
	BMessage message;
	if (event == COSMOE_APP_WATCH_LAUNCHED) {
		app_info info;
		if (be_roster == NULL
			|| be_roster->GetRunningAppInfo((team_id)teamID, &info) != B_OK) {
			return;
		}

		message.what = B_SOME_APP_LAUNCHED;
		message.AddInt32("be:team", info.team);
		message.AddInt32("be:flags", (int32)info.flags);
		message.AddString("be:signature", info.signature);
		message.AddRef("be:ref", &info.ref);
		mask = B_REQUEST_LAUNCHED;
	} else if (event == COSMOE_APP_WATCH_QUIT) {
		message.what = B_SOME_APP_QUIT;
		message.AddInt32("be:team", (team_id)teamID);
		mask = B_REQUEST_QUIT;
	} else {
		return;
	}

	std::vector<BMessenger> watchers;
	{
		std::lock_guard<std::mutex> locker(sAppWatchersMutex);
		watchers.reserve(sAppWatchers.size());
		for (const app_watcher_entry& entry : sAppWatchers) {
			if ((entry.eventMask & mask) != 0)
				watchers.push_back(entry.target);
		}
	}

	for (const BMessenger& watcher : watchers)
		watcher.SendMessage(&message);
}


/*!	Compares the supplied version infos.

	\param info1 The first info.
	\param info2 The second info.

	\return \c -1, if the first info is less than the second one, \c 1, if
	        the first one is greater than the second one, and \c 0, if both
	        are equal.
*/
static int32
compare_version_infos(const version_info& info1, const version_info& info2)
{
	int32 result = 0;
	if (info1.major < info2.major)
		result = -1;
	else if (info1.major > info2.major)
		result = 1;
	else if (info1.middle < info2.middle)
		result = -1;
	else if (info1.middle > info2.middle)
		result = 1;
	else if (info1.minor < info2.minor)
		result = -1;
	else if (info1.minor > info2.minor)
		result = 1;
	else if (info1.variety < info2.variety)
		result = -1;
	else if (info1.variety > info2.variety)
		result = 1;
	else if (info1.internal < info2.internal)
		result = -1;
	else if (info1.internal > info2.internal)
		result = 1;

	return result;
}


/*!	Compares two applications to decide which one should be rather
	returned as a query result.

	First, it checks if both apps are in the path, and prefers the app that
	appears earlier.

	If both files have a version info, then those are compared.
	If one file has a version info, it is said to be greater. If both
	files have no version info, their modification times are compared.

	\param app1 An entry_ref referring to the first application.
	\param app2 An entry_ref referring to the second application.
	\return \c -1, if the first application version is less than the second
	        one, \c 1, if the first one is greater than the second one, and
	        \c 0, if both are equal.
*/
static int32
compare_queried_apps(const entry_ref* app1, const entry_ref* app2)
{
	BPath path1(app1);
	BPath path2(app2);

	// Check search path

	const char* searchPathes = getenv("PATH");
	if (searchPathes != NULL) {
		char* searchBuffer = strdup(searchPathes);
		if (searchBuffer != NULL) {
			char* last;
			const char* path = strtok_r(searchBuffer, ":", &last);
			while (path != NULL) {
				// Check if any app path matches
				size_t length = strlen(path);
				bool found1 = !strncmp(path, path1.Path(), length)
					&& path1.Path()[length] == '/';
				bool found2 = !strncmp(path, path2.Path(), length)
					&& path2.Path()[length] == '/';;

				if (found1 != found2) {
					free(searchBuffer);
					return found1 ? 1 : -1;
				}

				path = strtok_r(NULL, ":", &last);
			}

			free(searchBuffer);
		}
	}

	// Check system servers folder
	BPath path;
	find_directory(B_SYSTEM_SERVERS_DIRECTORY, &path);
	BString serverPath(path.Path());
	serverPath << '/';
	size_t length = serverPath.Length();

	bool inSystem1 = !strncmp(serverPath.String(), path1.Path(), length);
	bool inSystem2 = !strncmp(serverPath.String(), path2.Path(), length);
	if (inSystem1 != inSystem2)
		return inSystem1 ? 1 : -1;

	// Check version info

	BFile file1;
	file1.SetTo(app1, B_READ_ONLY);
	BFile file2;
	file2.SetTo(app2, B_READ_ONLY);

	BAppFileInfo appFileInfo1;
	appFileInfo1.SetTo(&file1);
	BAppFileInfo appFileInfo2;
	appFileInfo2.SetTo(&file2);

	time_t modificationTime1 = 0;
	time_t modificationTime2 = 0;

	file1.GetModificationTime(&modificationTime1);
	file2.GetModificationTime(&modificationTime2);

	int32 result = 0;

	version_info versionInfo1;
	version_info versionInfo2;
	bool hasVersionInfo1 = (appFileInfo1.GetVersionInfo(
		&versionInfo1, B_APP_VERSION_KIND) == B_OK);
	bool hasVersionInfo2 = (appFileInfo2.GetVersionInfo(
		&versionInfo2, B_APP_VERSION_KIND) == B_OK);

	if (hasVersionInfo1) {
		if (hasVersionInfo2)
			result = compare_version_infos(versionInfo1, versionInfo2);
		else
			result = 1;
	} else {
		if (hasVersionInfo2)
			result = -1;
		else if (modificationTime1 < modificationTime2)
			result = -1;
		else if (modificationTime1 > modificationTime2)
			result = 1;
	}

	return result;
}


/*!	Finds an app by signature on any mounted volume.

	\param signature The app's signature.
	\param appRef A pointer to a pre-allocated entry_ref to be filled with
	       a reference to the found application's executable.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE: \c NULL \a signature or \a appRef.
	\retval B_LAUNCH_FAILED_APP_NOT_FOUND: An application with this signature
	        could not be found.
*/
static status_t
query_for_app(const char* signature, entry_ref* appRef)
{
	if (signature == NULL || appRef == NULL)
		return B_BAD_VALUE;

	status_t error = B_LAUNCH_FAILED_APP_NOT_FOUND;
	bool caseInsensitive = false;

	while (true) {
		// search on all volumes
		BVolumeRoster volumeRoster;
		BVolume volume;
		while (volumeRoster.GetNextVolume(&volume) == B_OK) {
			if (!volume.KnowsQuery())
				continue;

			index_info info;
			if (fs_stat_index(volume.Device(), "BEOS:APP_SIG", &info) != 0) {
				// This volume doesn't seem to have the index we're looking for;
				// querying it might need a long time, and we don't care *that*
				// much...
				continue;
			}

			BQuery query;
			query.SetVolume(&volume);
			query.PushAttr("BEOS:APP_SIG");
			if (!caseInsensitive)
				query.PushString(signature);
			else {
				// second pass, create a case insensitive query string
				char string[B_MIME_TYPE_LENGTH * 4];
				strlcpy(string, "application/", sizeof(string));

				int32 length = strlen(string);
				const char* from = signature + length;
				char* to = string + length;

				for (; from[0]; from++) {
					if (isalpha(from[0])) {
						*to++ = '[';
						*to++ = tolower(from[0]);
						*to++ = toupper(from[0]);
						*to++ = ']';
					} else
						*to++ = from[0];
				}

				to[0] = '\0';
				query.PushString(string);
			}
			query.PushOp(B_EQ);

			query.Fetch();

			// walk through the query
			bool appFound = false;
			status_t foundAppError = B_OK;
			entry_ref ref;
			while (query.GetNextRef(&ref) == B_OK) {
				if ((!appFound || compare_queried_apps(appRef, &ref) < 0)
					&& (foundAppError = can_app_be_used(&ref)) == B_OK) {
					*appRef = ref;
					appFound = true;
				}
			}
			if (!appFound) {
				// If the query didn't return any hits, the error is
				// B_LAUNCH_FAILED_APP_NOT_FOUND, otherwise we return the
				// result of the last can_app_be_used().
				error = foundAppError != B_OK
					? foundAppError : B_LAUNCH_FAILED_APP_NOT_FOUND;
			} else
				return B_OK;
		}

		if (!caseInsensitive)
			caseInsensitive = true;
		else
			break;
	}

	return error;
}


//	#pragma mark - app_info


app_info::app_info()
	:
	thread(-1),
	team(-1),
	port(-1),
	flags(B_REG_DEFAULT_APP_FLAGS),
	ref()
{
	signature[0] = '\0';
}


app_info::~app_info()
{
}


//	#pragma mark - BRoster::ArgVector


class BRoster::ArgVector {
public:
								ArgVector();
								~ArgVector();

			status_t			Init(int argc, const char* const* args,
									const entry_ref* appRef,
									const entry_ref* docRef);
			void				Unset();
	inline	int					Count() const { return fArgc; }
	inline	const char* const*	Args() const { return fArgs; }

private:
			int					fArgc;
			const char**		fArgs;
			BPath				fAppPath;
			BPath				fDocPath;
};


//!	Creates an uninitialized ArgVector.
BRoster::ArgVector::ArgVector()
	:
	fArgc(0),
	fArgs(NULL),
	fAppPath(),
	fDocPath()
{
}


//!	Frees all resources associated with the ArgVector.
BRoster::ArgVector::~ArgVector()
{
	Unset();
}


/*!	Initilizes the object according to the supplied parameters.

	If the initialization succeeds, the methods Count() and Args() grant
	access to the argument count and vector created by this methods.
	\note The returned vector is valid only as long as the elements of the
	supplied \a args (if any) are valid and this object is not destroyed.
	This object retains ownership of the vector returned by Args().
	In case of error, the value returned by Args() is invalid (or \c NULL).

	The argument vector is created as follows: First element is the path
	of the entry \a appRef refers to, then follow all elements of \a args
	and then, if \a args has at least one element and \a docRef can be
	resolved to a path, the path of the entry \a docRef refers to. That is,
	if no or an empty \a args vector is supplied, the resulting argument
	vector contains only one element, the path associated with \a appRef.

	\param argc Specifies the number of elements \a args contains.
	\param args Argument vector. May be \c NULL.
	\param appRef entry_ref referring to the entry whose path shall be the
	       first element of the resulting argument vector.
	\param docRef entry_ref referring to the entry whose path shall be the
	       last element of the resulting argument vector. May be \c NULL.
	\return
	- \c B_OK: Everything went fine.
	- \c B_BAD_VALUE: \c NULL \a appRef.
	- \c B_ENTRY_NOT_FOUND or other file system error codes: \a appRef could
	  not be resolved to a path.
	- \c B_NO_MEMORY: Not enough memory to allocate for this operation.
*/
status_t
BRoster::ArgVector::Init(int argc, const char* const* args,
	const entry_ref* appRef, const entry_ref* docRef)
{
	// unset old values
	Unset();
	status_t error = appRef ? B_OK : B_BAD_VALUE;
	// get app path
	if (error == B_OK)
		error = fAppPath.SetTo(appRef);
	// determine number of arguments
	bool hasDocArg = false;
	if (error == B_OK) {
		fArgc = 1;
		if (argc > 0 && args) {
			fArgc += argc;
			if (docRef != NULL && fDocPath.SetTo(docRef) == B_OK) {
				fArgc++;
				hasDocArg = true;
			}
		}
		fArgs = new(nothrow) const char*[fArgc + 1];
			// + 1 for the terminating NULL
		if (!fArgs)
			error = B_NO_MEMORY;
	}
	// init vector
	if (error == B_OK) {
		fArgs[0] = fAppPath.Path();
		if (argc > 0 && args != NULL) {
			for (int i = 0; i < argc; i++)
				fArgs[i + 1] = args[i];
			if (hasDocArg)
				fArgs[fArgc - 1] = fDocPath.Path();
		}
		// NULL terminate (e.g. required by load_image())
		fArgs[fArgc] = NULL;
	}
	return error;
}


//!	Uninitializes the object.
void
BRoster::ArgVector::Unset()
{
	fArgc = 0;
	delete[] fArgs;
	fArgs = NULL;
	fAppPath.Unset();
	fDocPath.Unset();
}


//	#pragma mark - BRoster


BRoster::BRoster()
	:
	fNoRegistrar(false)
{
	_InitMessenger();
}


BRoster::~BRoster()
{
}


//	#pragma mark - Querying for apps


bool
BRoster::IsRunning(const char* signature) const
{
	return (TeamFor(signature) >= 0);
}


bool
BRoster::IsRunning(entry_ref* ref) const
{
	return (TeamFor(ref) >= 0);
}


team_id
BRoster::TeamFor(const char* signature) const
{
	team_id team;
	app_info info;
	status_t error = GetAppInfo(signature, &info);
	if (error == B_OK)
		team = info.team;
	else
		team = error;

	return team;
}


team_id
BRoster::TeamFor(entry_ref* ref) const
{
	team_id team;
	app_info info;
	status_t error = GetAppInfo(ref, &info);
	if (error == B_OK)
		team = info.team;
	else
		team = error;
	return team;
}


void
BRoster::GetAppList(BList* teamIDList) const
{
	if (teamIDList == NULL)
		return;
	teamIDList->MakeEmpty();

	if (be_app == NULL)
		return;

	cosmoe_display_t display = be_app->Display();
	if (display == NULL)
		return;

	const int32 kMaxTeams = 1024;
	team_id teams[kMaxTeams];
	int32 count = cosmoe_display_get_app_list(display, teams, kMaxTeams);
	if (count <= 0)
		return;

	if (count > kMaxTeams)
		count = kMaxTeams;

	for (int32 i = 0; i < count; i++)
		teamIDList->AddItem((void*)(addr_t)teams[i]);
}


void
BRoster::GetAppList(const char* signature, BList* teamIDList) const
{
	if (signature == NULL || teamIDList == NULL)
		return;

	GetAppList(teamIDList);

	for (int32 i = teamIDList->CountItems() - 1; i >= 0; i--) {
		team_id team = (team_id)(addr_t)teamIDList->ItemAt(i);
		app_info info;
		if (GetRunningAppInfo(team, &info) != B_OK
			|| strcmp(info.signature, signature) != 0) {
			teamIDList->RemoveItem(i);
		}
	}
}


status_t
BRoster::GetAppInfo(const char* signature, app_info* info) const
{
	if (signature == NULL || info == NULL)
		return B_BAD_VALUE;

	BList teams;
	GetAppList(signature, &teams);
	if (teams.IsEmpty())
		return B_ENTRY_NOT_FOUND;

	team_id team = (team_id)(addr_t)teams.ItemAt(0);
	return GetRunningAppInfo(team, info);
}


status_t
BRoster::GetAppInfo(entry_ref* ref, app_info* info) const
{
	if (ref == NULL || info == NULL)
		return B_BAD_VALUE;

	BEntry targetEntry;
	status_t status;
	if (ref->name != NULL && ref->name[0] == '/')
		status = targetEntry.SetTo(ref->name, true);
	else
		status = targetEntry.SetTo(ref, true);
	if (status != B_OK)
		return status;

	entry_ref targetRef;
	status = targetEntry.GetRef(&targetRef);
	if (status != B_OK)
		return status;

	BList teams;
	GetAppList(&teams);
	for (int32 i = 0; i < teams.CountItems(); i++) {
		team_id team = (team_id)(addr_t)teams.ItemAt(i);
		app_info runningInfo;
		if (GetRunningAppInfo(team, &runningInfo) != B_OK)
			continue;

		if (runningInfo.ref == targetRef) {
			*info = runningInfo;
			return B_OK;
		}
	}

	return B_ERROR;
}


status_t
BRoster::GetRunningAppInfo(team_id team, app_info* info) const
{
	if (info == NULL)
		return B_BAD_VALUE;

	*info = app_info();

	if (team < 0) {
		status_t backendInfoStatus = find_backend_app_info(team, info);
		if (backendInfoStatus == B_OK)
			return B_OK;
	}

	extern thread_id _main_thread_for_team(team_id);
	info->team = team;
	info->thread = _main_thread_for_team(team);
	if (info->thread < B_OK) {
		/*
		 * External apps discovered via backend app lists may only provide a
		 * process/team identifier (e.g. Linux PID) without an in-process
		 * thread mapping. Keep going and resolve metadata from the app ref.
		 */
		info->thread = -1;
	}

	status_t error = get_app_ref(team, &info->ref);
	if (error != B_OK)
		return error;

	BFile appFile;
	if (appFile.SetTo(&info->ref, B_READ_ONLY) == B_OK) {
		BAppFileInfo appFileInfo;
		if (appFileInfo.SetTo(&appFile) == B_OK) {
			if (appFileInfo.GetSignature(info->signature) != B_OK)
				info->signature[0] = '\0';

			if (appFileInfo.GetAppFlags(&info->flags) != B_OK)
				info->flags = B_REG_DEFAULT_APP_FLAGS;
		}
	}

	/* Prefer the live app signature for the current process if available. */
	if (be_app != NULL && team == be_app->Team()
		&& be_app->Signature() != NULL && be_app->Signature()[0] != '\0') {
		strlcpy(info->signature, be_app->Signature(), B_MIME_TYPE_LENGTH);
	}

	if (info->signature[0] == '\0') {
		/*
		 * Backend-discovered host apps may not carry a Haiku app signature.
		 * Provide a stable non-empty fallback so Deskbar does not merge all
		 * foreign apps into one entry.
		 */
		snprintf(info->signature, B_MIME_TYPE_LENGTH,
			"application/x-vnd.cosmoe-hostpid-%" B_PRId32, (int32)team);
	}

	return B_OK;
}


status_t
BRoster::GetActiveAppInfo(app_info* info) const
{
	if (info == NULL)
		return B_BAD_VALUE;

	// // compose the request message
	// BMessage request(B_REG_GET_APP_INFO);
	// // send the request
	// BMessage reply;
	// status_t error = fMessenger.SendMessage(&request, &reply);
	// // evaluate the reply
	// if (error == B_OK) {
	// 	if (reply.what == B_REG_SUCCESS)
	// 		error = find_message_app_info(&reply, info);
	// 	else if (reply.FindInt32("error", &error) != B_OK)
	// 		error = B_ERROR;
	// }
	// return error;

	return B_UNSUPPORTED;
}


status_t
BRoster::FindApp(const char* mimeType, entry_ref* app) const
{
	if (mimeType == NULL || app == NULL)
		return B_BAD_VALUE;

	return _ResolveApp(mimeType, NULL, app, NULL, NULL, NULL);
}


status_t
BRoster::FindApp(entry_ref* ref, entry_ref* app) const
{
	if (ref == NULL || app == NULL)
		return B_BAD_VALUE;

	entry_ref _ref(*ref);
	return _ResolveApp(NULL, &_ref, app, NULL, NULL, NULL);
}


//	#pragma mark - Launching, activating, and broadcasting to apps


status_t
BRoster::StartWatching(BMessenger target, uint32 eventMask) const
{
	const uint32 kSupportedEvents = B_REQUEST_LAUNCHED | B_REQUEST_QUIT;
	if (!target.IsValid() || eventMask == 0)
		return B_BAD_VALUE;
	if ((eventMask & ~kSupportedEvents) != 0)
		return B_UNSUPPORTED;

	bool installBackendWatcher = false;
	bool hadExistingEntry = false;
	uint32 previousMask = 0;
	{
		std::lock_guard<std::mutex> locker(sAppWatchersMutex);
		bool hadWatchers = roster_has_app_watchers_locked();
		for (app_watcher_entry& entry : sAppWatchers) {
			if (entry.target == target) {
				hadExistingEntry = true;
				previousMask = entry.eventMask;
				entry.eventMask = eventMask;
				break;
			}
		}

		if (!hadExistingEntry)
			sAppWatchers.emplace_back(target, eventMask);

		installBackendWatcher = !hadWatchers;
	}

	if (!installBackendWatcher)
		return B_OK;

	status_t status = roster_set_backend_app_watcher();
	if (status == B_OK)
		return B_OK;

	std::lock_guard<std::mutex> locker(sAppWatchersMutex);
	for (std::vector<app_watcher_entry>::iterator it = sAppWatchers.begin();
			it != sAppWatchers.end(); ++it) {
		if (!(it->target == target))
			continue;

		if (hadExistingEntry)
			it->eventMask = previousMask;
		else
			sAppWatchers.erase(it);
		break;
	}

	return status;
}


status_t
BRoster::StopWatching(BMessenger target) const
{
	if (!target.IsValid())
		return B_BAD_VALUE;

	bool clearBackendWatcher = false;
	{
		std::lock_guard<std::mutex> locker(sAppWatchersMutex);
		for (std::vector<app_watcher_entry>::iterator it = sAppWatchers.begin();
				it != sAppWatchers.end(); ++it) {
			if (!(it->target == target))
				continue;

			sAppWatchers.erase(it);
			clearBackendWatcher = !roster_has_app_watchers_locked();
			if (!clearBackendWatcher)
				return B_OK;
			break;
		}

		if (!clearBackendWatcher)
			return B_ENTRY_NOT_FOUND;
	}

	return roster_clear_backend_app_watcher();
}


status_t
BRoster::ActivateApp(team_id team) const
{
	return B_UNSUPPORTED;
}


status_t
BRoster::Launch(const char* mimeType, BMessage* initialMessage,
	team_id* _appTeam) const
{
	if (mimeType == NULL)
		return B_BAD_VALUE;

	BList messageList;
	if (initialMessage != NULL)
		messageList.AddItem(initialMessage);

	return _LaunchApp(mimeType, NULL, &messageList, 0, NULL,
		(const char**)environ, _appTeam, NULL, NULL, NULL, false);
}


status_t
BRoster::Launch(const char* mimeType, BList* messageList,
	team_id* _appTeam) const
{
	if (mimeType == NULL)
		return B_BAD_VALUE;

	return _LaunchApp(mimeType, NULL, messageList, 0, NULL,
		(const char**)environ, _appTeam, NULL, NULL, NULL, false);
}


status_t
BRoster::Launch(const char* mimeType, int argc, const char* const* args,
	team_id* _appTeam) const
{
	if (mimeType == NULL)
		return B_BAD_VALUE;

	return _LaunchApp(mimeType, NULL, NULL, argc, args, (const char**)environ,
		_appTeam, NULL, NULL, NULL, false);
}


status_t
BRoster::Launch(const entry_ref* ref, const BMessage* initialMessage,
	team_id* _appTeam) const
{
	if (ref == NULL)
		return B_BAD_VALUE;

	BList messageList;
	if (initialMessage != NULL)
		messageList.AddItem(const_cast<BMessage*>(initialMessage));

	return _LaunchApp(NULL, ref, &messageList, 0, NULL, (const char**)environ,
		_appTeam, NULL, NULL, NULL, false);
}


status_t
BRoster::Launch(const entry_ref* ref, const BList* messageList,
	team_id* appTeam) const
{
	if (ref == NULL)
		return B_BAD_VALUE;

	return _LaunchApp(NULL, ref, messageList, 0, NULL, (const char**)environ,
		appTeam, NULL, NULL, NULL, false);
}


status_t
BRoster::Launch(const entry_ref* ref, int argc, const char* const* args,
	team_id* appTeam) const
{
	if (ref == NULL)
		return B_BAD_VALUE;

	return _LaunchApp(NULL, ref, NULL, argc, args, (const char**)environ,
		appTeam, NULL, NULL, NULL, false);
}


//	#pragma mark - Recent document and app support


void
BRoster::GetRecentDocuments(BMessage* refList, int32 maxCount,
	const char* fileType, const char* signature) const
{
	if (refList == NULL)
		return;

	if (maxCount <= 0)
		return;

	refList->MakeEmpty();

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	int32 added = 0;
	for (int32 i = 0; i < sRecentDocuments.CountItems() && added < maxCount; i++) {
		entry_ref* ref = (entry_ref*)sRecentDocuments.ItemAt(i);
		if (ref == NULL)
			continue;

		// Filter by file type if specified
		if (fileType != NULL) {
			BNode node(ref);
			BNodeInfo nodeInfo(&node);
			char type[B_MIME_TYPE_LENGTH];
			if (nodeInfo.GetType(type) != B_OK
				|| strcasecmp(type, fileType) != 0) {
				continue;
			}
		}

		// Filter by signature if specified (not implemented - would need metadata)
		// For now we ignore the signature parameter

		if (refList->AddRef("refs", ref) == B_OK)
			added++;
	}
}


void
BRoster::GetRecentDocuments(BMessage* refList, int32 maxCount,
	const char* fileTypes[], int32 fileTypesCount,
	const char* signature) const
{
	if (refList == NULL)
		return;

	if (maxCount <= 0)
		return;

	refList->MakeEmpty();

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	int32 added = 0;
	for (int32 i = 0; i < sRecentDocuments.CountItems() && added < maxCount; i++) {
		entry_ref* ref = (entry_ref*)sRecentDocuments.ItemAt(i);
		if (ref == NULL)
			continue;

		// Filter by file types if specified
		bool matchesType = (fileTypes == NULL || fileTypesCount == 0);
		if (!matchesType && fileTypes != NULL) {
			BNode node(ref);
			BNodeInfo nodeInfo(&node);
			char type[B_MIME_TYPE_LENGTH];
			if (nodeInfo.GetType(type) == B_OK) {
				for (int32 j = 0; j < fileTypesCount; j++) {
					if (strcasecmp(type, fileTypes[j]) == 0) {
						matchesType = true;
						break;
					}
				}
			}
		}

		if (!matchesType)
			continue;

		// Filter by signature if specified (not implemented - would need metadata)

		if (refList->AddRef("refs", ref) == B_OK)
			added++;
	}
}


void
BRoster::GetRecentFolders(BMessage* refList, int32 maxCount,
	const char* signature) const
{
	if (refList == NULL)
		return;

	if (maxCount <= 0)
		return;

	refList->MakeEmpty();

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	int32 added = 0;
	for (int32 i = 0; i < sRecentFolders.CountItems() && added < maxCount; i++) {
		entry_ref* ref = (entry_ref*)sRecentFolders.ItemAt(i);
		if (ref == NULL)
			continue;

		// Filter by signature if specified (not implemented - would need metadata)
		// For now we ignore the signature parameter

		if (refList->AddRef("refs", ref) == B_OK)
			added++;
	}
}


void
BRoster::GetRecentApps(BMessage* refList, int32 maxCount) const
{
	if (refList == NULL)
		return;

	if (maxCount <= 0)
		return;

	refList->MakeEmpty();

	// TODO
}


void
BRoster::AddToRecentDocuments(const entry_ref* document,
	const char* signature) const
{
	if (document == NULL)
		return;

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Check if this document is already in the list
	for (int32 i = 0; i < sRecentDocuments.CountItems(); i++) {
		entry_ref* ref = (entry_ref*)sRecentDocuments.ItemAt(i);
		if (ref != NULL && *ref == *document) {
			// Already in list, move to front
			sRecentDocuments.RemoveItem(i);
			sRecentDocuments.AddItem(ref, 0);
			return;
		}
	}

	// Create a new entry_ref and add to front of list
	entry_ref* newRef = new (std::nothrow) entry_ref(*document);
	if (newRef == NULL)
		return;

	sRecentDocuments.AddItem(newRef, 0);

	// Trim list if too large
	while (sRecentDocuments.CountItems() > kMaxRecentItems) {
		entry_ref* oldRef = (entry_ref*)sRecentDocuments.RemoveItem(
			sRecentDocuments.CountItems() - 1);
		delete oldRef;
	}
}


void
BRoster::AddToRecentFolders(const entry_ref* folder,
	const char* signature) const
{
	if (folder == NULL)
		return;

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Check if this folder is already in the list
	for (int32 i = 0; i < sRecentFolders.CountItems(); i++) {
		entry_ref* ref = (entry_ref*)sRecentFolders.ItemAt(i);
		if (ref != NULL && *ref == *folder) {
			// Already in list, move to front
			sRecentFolders.RemoveItem(i);
			sRecentFolders.AddItem(ref, 0);
			return;
		}
	}

	// Create a new entry_ref and add to front of list
	entry_ref* newRef = new (std::nothrow) entry_ref(*folder);
	if (newRef == NULL)
		return;

	sRecentFolders.AddItem(newRef, 0);

	// Trim list if too large
	while (sRecentFolders.CountItems() > kMaxRecentItems) {
		entry_ref* oldRef = (entry_ref*)sRecentFolders.RemoveItem(
			sRecentFolders.CountItems() - 1);
		delete oldRef;
	}
}

//	#pragma mark - Private or reserved


/*!	Shuts down the system.

	When \c synchronous is \c true and the method succeeds, it doesn't return.

	\param reboot If \c true, the system will be rebooted instead of being
	       powered off.
	\param confirm If \c true, the user will be asked to confirm to shut down
	       the system.
	\param synchronous If \c false, the method will return as soon as the
	       shutdown process has been initiated successfully (or an error
	       occurred). Otherwise the method doesn't return, if successfully.

	\return A status code, \c B_OK on success or another error code in case
	        something went wrong.
	\retval B_SHUTTING_DOWN, when there's already a shutdown process in
	        progress,
	\retval B_SHUTDOWN_CANCELLED, when the user cancelled the shutdown process,
*/
status_t
BRoster::_ShutDown(bool reboot, bool confirm, bool synchronous) const
{
	status_t error = B_OK;

#if defined(__linux__)
	if (confirm) {
		BAlert* alert = new BAlert("confirm_shutdown",
			reboot ? "Are you sure you want to reboot the system?"
			       : "Are you sure you want to shut down the system?",
			"Cancel", reboot ? "Reboot" : "Shut Down");
		alert->SetShortcut(0, B_ESCAPE);
		int32 buttonIndex = alert->Go();
		if (buttonIndex != 1)
			return B_SHUTDOWN_CANCELLED;
	}

	// Trigger an IMMEDIATE shutdown/reboot via systemd
	const char* command = reboot ? "systemctl reboot" : "systemctl poweroff";
	system(command);
#endif

	return error;
}


void
BRoster::_AddToRecentApps(const char* signature) const
{
}


//	#pragma mark - Private or reserved

#if 0
/*!	(Pre-)Registers an application with the registrar.

	This methods is invoked either to register or to pre-register an
	application. Full registration is requested by supplying \c true via
	\a fullRegistration.

	A full registration requires \a signature, \a ref, \a flags, \a team,
	\a thread and \a port to contain valid values. No token will be return
	via \a pToken.

	For a pre-registration \a signature, \a ref, \a flags must be valid.
	\a team and \a thread are optional and should be set to -1, if they are
	unknown. If no team ID is supplied, \a pToken should be valid and, if the
	the pre-registration succeeds, will be filled with a unique token assigned
	by the roster.

	In both cases the registration may fail, if single/exclusive launch is
	requested and an instance of the application is already running. Then
	\c B_ALREADY_RUNNING is returned and the team ID of the running instance
	is passed back via \a otherTeam, if supplied.

	\param signature The application's signature
	\param ref An entry_ref referring to the app's executable
	\param flags The application's flags
	\param team The application's team ID
	\param thread The application's main thread
	\param port The application's looper port
	\param fullRegistration \c true for full, \c false for pre-registration
	\param pToken A pointer to a pre-allocated uint32 into which the token
	       assigned by the registrar is written (may be \c NULL)
	\param otherTeam A pointer to a pre-allocated team_id into which the
	       team ID of the already running instance of a single/exclusive
	       launch application is written (may be \c NULL)

	\return A status code
	\retval B_OK Everything went fine.
	\retval B_ENTRY_NOT_FOUND \a ref didn't refer to a file.
	\retval B_ALREADY_RUNNING The application requested a single/exclusive
	        launch and an instance was already running.
	\retval B_REG_ALREADY_REGISTERED An application with the team ID \a team
	        was already registered.
*/
status_t
BRoster::_AddApplication(const char* signature, const entry_ref* ref,
	uint32 flags, team_id team, thread_id thread, port_id port,
	bool fullRegistration, uint32* pToken, team_id* otherTeam) const
{
	status_t error = B_OK;

	// compose the request message
	BMessage request(B_REG_ADD_APP);
	if (error == B_OK && signature != NULL)
		error = request.AddString("signature", signature);

	if (error == B_OK && ref != NULL)
		error = request.AddRef("ref", ref);

	if (error == B_OK)
		error = request.AddInt32("flags", (int32)flags);

	if (error == B_OK && team >= 0)
		error = request.AddInt32("team", team);

	if (error == B_OK && thread >= 0)
		error = request.AddInt32("thread", thread);

	if (error == B_OK && port >= 0)
		error = request.AddInt32("port", port);

	if (error == B_OK)
		error = request.AddBool("full_registration", fullRegistration);

	// send the request
	BMessage reply;
	if (error == B_OK)
		error = fMessenger.SendMessage(&request, &reply);

	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS) {
			if (!fullRegistration && team < 0) {
				uint32 token;
				if (reply.FindInt32("token", (int32*)&token) == B_OK) {
					if (pToken != NULL)
						*pToken = token;
				} else
					error = B_ERROR;
			}
		} else {
			if (reply.FindInt32("error", &error) != B_OK)
				error = B_ERROR;

			// get team and token from the reply
			if (otherTeam != NULL
				&& reply.FindInt32("other_team", otherTeam) != B_OK) {
				*otherTeam = -1;
			}
			if (pToken != NULL
				&& reply.FindInt32("token", (int32*)pToken) != B_OK) {
				*pToken = 0;
			}
		}
	}

	return error;
}
#endif


/*!	Launches the application associated with the supplied MIME type or
	the entry referred to by the supplied entry_ref.

	The application to be started is searched the same way FindApp() does it.

	At least one of \a mimeType or \a ref must not be \c NULL. If \a mimeType
	is supplied, \a ref is ignored for finding the application.

	If \a ref does refer to an application executable, that application is
	launched. Otherwise the respective application is searched and launched,
	and \a ref is sent to it in a \c B_REFS_RECEIVED message, unless other
	arguments are passed via \a argc and \a args -- then the entry_ref is
	converted into a path (C-string) and added to the argument vector.

	\a messageList contains messages to be sent to the application
	"on launch", i.e. before ReadyToRun() is invoked on the BApplication
	object. The caller retains ownership of the supplied BList and the
	contained BMessages. In case the method fails with \c B_ALREADY_RUNNING
	the messages are delivered to the already running instance. The same
	applies to the \c B_REFS_RECEIVED message.

	The supplied \a argc and \a args are (if containing at least one argument)
	put into a \c B_ARGV_RECEIVED message and sent to the launched application
	"on launch". The caller retains ownership of the supplied \a args.
	In case the method fails with \c B_ALREADY_RUNNING the message is
	delivered to the already running instance. The same applies to the
	\c B_REFS_RECEIVED message, if no arguments are supplied via \a argc and
	\args.

	If \a launchSuspended is set to true, the main thread of the loaded app
	(returned in \a appThread) is kept in the suspended state and not
	automatically resumed.

	\param mimeType MIME type for which the application shall be launched.
	       May be \c NULL.
	\param ref entry_ref referring to the file for which an application shall
	       be launched. May be \c NULL.
	\param messageList Optional list of messages to be sent to the application
	       "on launch". May be \c NULL.
	\param argc Specifies the number of elements in \a args.
	\param args An array of C-strings to be sent as B_ARGV_RECEIVED messaged
	       to the launched application.
	\param appTeam Pointer to a pre-allocated team_id variable to be set to
	       the team ID of the launched application.
	\param appThread Pointer to a pre-allocated thread_id variable to
		   be set to the thread ID of the launched main thread.
	\param _appPort Pointer to a pre-allocated port_id variable to
		   be set to the port ID of the launched application.
	\param _appToken Pointer to a pre-allocated uint32 variable to
		   be set to the token of the launched application.
	\param launchSuspended Indicates whether to keep the app thread in the
		   suspended state or resume it.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \c NULL \a mimeType
	\retval B_LAUNCH_FAILED_NO_PREFERRED_APP Neither with the supplied type
	        nor with its supertype (if the supplied isn't a supertype itself)
	        a preferred application is associated.
	\retval B_LAUNCH_FAILED_APP_NOT_FOUND The supplied type is not installed
	        or its preferred application could not be found.
	\retval B_LAUNCH_FAILED_APP_IN_TRASH The supplied type's preferred
	        application was in the trash.
	\retval B_LAUNCH_FAILED_EXECUTABLE The found application was not
	        executable.
*/
status_t
BRoster::_LaunchApp(const char* mimeType, const entry_ref* ref,
	const BList* messageList, int argc, const char* const* args,
	const char** environment, team_id* _appTeam, thread_id* _appThread,
	port_id* _appPort, uint32* _appToken, bool launchSuspended) const
{
	DBG(OUT("BRoster::_LaunchApp()"));
	(void)messageList;
	(void)environment;
	(void)_appPort;
	(void)_appToken;
	(void)launchSuspended;

	if (_appTeam != NULL) {
		// we're supposed to set _appTeam to -1 on error; we'll
		// reset it later if everything goes well
		*_appTeam = -1;
	}
	if (_appThread != NULL)
		*_appThread = -1;

	if (mimeType == NULL && ref == NULL)
		return B_BAD_VALUE;

	entry_ref appRef;
	entry_ref documentRef;
	status_t error = B_OK;
	if (ref != NULL)
		documentRef = *ref;

	error = _ResolveApp(mimeType, ref != NULL ? &documentRef : NULL, &appRef,
		NULL, NULL, NULL);
	if (error != B_OK)
		return error;

	BPath appPath;
	error = appPath.SetTo(&appRef);
	if (error != B_OK)
		return error;

	const char* appPathString = appPath.Path();
	if (appPathString == NULL || appPathString[0] == '\0')
		return B_BAD_VALUE;

	std::vector<char*> launchArgv;
	launchArgv.reserve((argc > 0 ? argc : 0) + 2);
	launchArgv.push_back(const_cast<char*>(appPathString));
	for (int i = 0; i < argc; i++) {
		if (args != NULL && args[i] != NULL)
			launchArgv.push_back(const_cast<char*>(args[i]));
	}
	launchArgv.push_back(NULL);

#ifdef _WIN32
	intptr_t child = _spawnv(_P_NOWAIT, appPathString, launchArgv.data());
	if (child == -1)
		return B_ERROR;

	if (_appTeam != NULL)
		*_appTeam = (team_id)child;
	if (_appThread != NULL)
		*_appThread = (thread_id)child;
#else
	pid_t pid = fork();
	if (pid < 0)
		return B_ERROR;

	if (pid == 0) {
		execv(appPathString, launchArgv.data());
		_exit(1);
	}

	if (_appTeam != NULL)
		*_appTeam = (team_id)pid;
	if (_appThread != NULL)
		*_appThread = (thread_id)pid;
#endif

	DBG(OUT("BRoster::_LaunchApp() done: %s (%" B_PRIx32 ")\n",
		strerror(error), error));

	return error;
}


/*!	Finds an application associated with a MIME type or a file.

	It does also supply the caller with some more information about the
	application, like signature, app flags and whether the supplied
	MIME type/entry_ref already identified an application.

	At least one of \a inType or \a ref must not be \c NULL. If \a inType is
	supplied, \a ref is ignored.

	If \a ref refers to a link, it is updated with the entry_ref for the
	resolved entry.

	\see FindApp() for how the application is searched.

	\a signature is set to a string with length 0, if the found
	application has no signature.

	\param inType The MIME type for which an application shall be found.
	       May be \c NULL.
	\param ref The file for which an application shall be found.
	       May be \c NULL.
	\param appRef A pointer to a pre-allocated entry_ref to be filled with
	       a reference to the found application's executable. May be \c NULL.
	\param signature A pointer to a pre-allocated char buffer of at
	       least size \c B_MIME_TYPE_LENGTH to be filled with the signature of
	       the found application. May be \c NULL.
	\param appFlags A pointer to a pre-allocated uint32 variable to be filled
	       with the app flags of the found application. May be \c NULL.
	\param wasDocument A pointer to a pre-allocated bool variable to be set to
	       \c true, if the supplied file was not identifying an application,
	       to \c false otherwise. Has no meaning, if a \a inType is supplied.
	       May be \c NULL.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \c NULL \a inType and \a ref.

	\see FindApp() for other error codes.
*/
status_t
BRoster::_ResolveApp(const char* inType, entry_ref* ref,
	entry_ref* _appRef, char* _signature, uint32* _appFlags,
	bool* _wasDocument) const
{
	if ((inType == NULL && ref == NULL)
		|| (inType != NULL && strlen(inType) >= B_MIME_TYPE_LENGTH))
		return B_BAD_VALUE;

	// find the app
	BMimeType appMeta;
	BFile appFile;
	entry_ref appRef;
	status_t error;

	if (inType != NULL) {
		error = _TranslateType(inType, &appMeta, &appRef, &appFile);
		#ifdef __linux__
		if (error != B_OK) {
			char canonicalSignature[B_MIME_TYPE_LENGTH];
			status_t desktopError = resolve_xdg_desktop_app(inType, &appRef,
				canonicalSignature, sizeof(canonicalSignature));
			if (desktopError == B_OK) {
				appMeta.SetTo(canonicalSignature);
				error = appFile.SetTo(&appRef, B_READ_ONLY);
			}
		}
		#endif
		if (_wasDocument != NULL)
			*_wasDocument = !(appMeta == inType);
	} else {
		error = _TranslateRef(ref, &appMeta, &appRef, &appFile,
			_wasDocument);
	}

	// create meta mime
	if (!fNoRegistrar && error == B_OK) {
		BPath path;
		if (path.SetTo(&appRef) == B_OK)
			create_app_meta_mime(path.Path(), false, true, false);
	}

	// set the app hint on the type -- but only if the file has the
	// respective signature, otherwise unset the app hint
	BAppFileInfo appFileInfo;
	if (!fNoRegistrar && error == B_OK) {
		char signature[B_MIME_TYPE_LENGTH];
		if (appFileInfo.SetTo(&appFile) == B_OK
			&& appFileInfo.GetSignature(signature) == B_OK) {
			if (!strcasecmp(appMeta.Type(), signature)) {
				// Only set the app hint if there is none yet
				entry_ref dummyRef;
				if (appMeta.GetAppHint(&dummyRef) != B_OK)
					appMeta.SetAppHint(&appRef);
			} else {
				appMeta.SetAppHint(NULL);
				appMeta.SetTo(signature);
			}
		} else
			appMeta.SetAppHint(NULL);
	}

	// set the return values
	if (error == B_OK) {
		if (_appRef)
			*_appRef = appRef;

		if (_signature != NULL) {
			// there's no warranty, that appMeta is valid
			if (appMeta.IsValid()) {
				strlcpy(_signature, appMeta.Type(),
					B_MIME_TYPE_LENGTH);
			} else
				_signature[0] = '\0';
		}

		if (_appFlags != NULL) {
			// if an error occurs here, we don't care and just set a default
			// value
			if (appFileInfo.InitCheck() != B_OK
				|| appFileInfo.GetAppFlags(_appFlags) != B_OK) {
				*_appFlags = B_REG_DEFAULT_APP_FLAGS;
			}
		}
	} else {
		// unset the ref on error
		if (_appRef != NULL)
			*_appRef = appRef;
	}

	return error;
}


/*!	\brief Finds an application associated with a file.

	\a appMeta is left unmodified, if the file is executable, but has no
	signature.

	\see FindApp() for how the application is searched.

	If \a ref refers to a link, it is updated with the entry_ref for the
	resolved entry.

	\param ref The file for which an application shall be found.
	\param appMeta A pointer to a pre-allocated BMimeType to be set to the
	       signature of the found application.
	\param appRef A pointer to a pre-allocated entry_ref to be filled with
	       a reference to the found application's executable.
	\param appFile A pointer to a pre-allocated BFile to be set to the
	       executable of the found application.
	\param wasDocument A pointer to a pre-allocated bool variable to be set to
	       \c true, if the supplied file was not identifying an application,
	       to \c false otherwise. May be \c NULL.

	\return A status code.
	\retval B_OK: Everything went fine.
	\retval B_BAD_VALUE: \c NULL \a ref, \a appMeta, \a appRef or \a appFile.

	\see FindApp() for other error codes.
*/
status_t
BRoster::_TranslateRef(entry_ref* ref, BMimeType* appMeta,
	entry_ref* appRef, BFile* appFile, bool* _wasDocument) const
{
	if (ref == NULL || appMeta == NULL || appRef == NULL || appFile == NULL)
		return B_BAD_VALUE;

	entry_ref originalRef = *ref;

	// resolve ref, if necessary
	BEntry entry;
	status_t error = entry.SetTo(ref, false);
	if (error != B_OK)
		return error;

	if (entry.IsSymLink()) {
		// ref refers to a link
		if (entry.SetTo(ref, true) != B_OK || entry.GetRef(ref) != B_OK)
			return B_LAUNCH_FAILED_NO_RESOLVE_LINK;
	}

	// init node
	BNode node;
	error = node.SetTo(ref);
	if (error != B_OK)
		return error;

	// get permissions
	mode_t permissions;
	error = node.GetPermissions(&permissions);
	if (error != B_OK)
		return error;

	if ((permissions & S_IXUSR) != 0 && node.IsFile()) {
		// node is executable and a file
		error = appFile->SetTo(ref, B_READ_ONLY);
		if (error != B_OK)
			return error;

		// get the app's signature via a BAppFileInfo
		BAppFileInfo appFileInfo;
		error = appFileInfo.SetTo(appFile);
		if (error != B_OK)
			return error;

		// don't worry, if the file doesn't have a signature, just
		// unset the supplied object
		char type[B_MIME_TYPE_LENGTH];
		if (appFileInfo.GetSignature(type) == B_OK) {
			error = appMeta->SetTo(type);
			if (error != B_OK)
				return error;
		} else
			appMeta->Unset();

		// If the file type indicates that the file is an application, we've
		// definitely got what we're looking for.
		bool isDocument = true;
		if (_GetFileType(ref, &appFileInfo, type) == B_OK
			&& strcasecmp(type, B_APP_MIME_TYPE) == 0) {
			isDocument = false;
		}

		// If our file is not an application executable, we probably have a
		// script. Check whether the file has a preferred application set. If
		// so, we fall through and use the preferred app instead. Otherwise
		// we're done.
		char preferredApp[B_MIME_TYPE_LENGTH];
		if (!isDocument || appFileInfo.GetPreferredApp(preferredApp) != B_OK) {
			// If we were given a symlink, point appRef to it in case its name
			// or attributes are relevant.
			*appRef = originalRef;
			if (_wasDocument != NULL)
				*_wasDocument = isDocument;

			return B_OK;
		}

		// Executable file, but not an application, and it has a preferred
		// application set. Fall through...
	}

	// the node is not exectuable or not a file
	// init a node info
	BNodeInfo nodeInfo;
	error = nodeInfo.SetTo(&node);
	if (error != B_OK)
		return error;

	// if the file has a preferred app, let _TranslateType() find
	// it for us
	char preferredApp[B_MIME_TYPE_LENGTH];
	if (nodeInfo.GetPreferredApp(preferredApp) == B_OK
		&& _TranslateType(preferredApp, appMeta, appRef, appFile) == B_OK) {
		if (_wasDocument != NULL)
			*_wasDocument = true;

		return B_OK;
	}

	// no preferred app or existing one was not found -- we
	// need to get the file's type

	// get the type from the file
	char fileType[B_MIME_TYPE_LENGTH];
	error = _GetFileType(ref, &nodeInfo, fileType);
	if (error != B_OK)
		return error;

	// now let _TranslateType() do the actual work
	error = _TranslateType(fileType, appMeta, appRef, appFile);
	if (error != B_OK)
		return error;

	if (_wasDocument != NULL)
		*_wasDocument = true;

	return B_OK;
}


/*!	Finds an application associated with a MIME type.

	\see FindApp() for how the application is searched.

	\param mimeType The MIME type for which an application shall be found.
	\param appMeta A pointer to a pre-allocated BMimeType to be set to the
	       signature of the found application.
	\param appRef A pointer to a pre-allocated entry_ref to be filled with
	       a reference to the found application's executable.
	\param appFile A pointer to a pre-allocated BFile to be set to the
	       executable of the found application.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \c NULL \a mimeType, \a appMeta, \a appRef or
	        \a appFile.

	\see FindApp() for other error codes.
*/
status_t
BRoster::_TranslateType(const char* mimeType, BMimeType* appMeta,
	entry_ref* appRef, BFile* appFile) const
{
	if (mimeType == NULL || appMeta == NULL || appRef == NULL
		|| appFile == NULL || strlen(mimeType) >= B_MIME_TYPE_LENGTH) {
		return B_BAD_VALUE;
	}

	// Create a BMimeType and check, if the type is installed.
	BMimeType type;
	status_t error = type.SetTo(mimeType);

	// Get the preferred apps from the sub and super type.
	char primarySignature[B_MIME_TYPE_LENGTH];
	char secondarySignature[B_MIME_TYPE_LENGTH];
	primarySignature[0] = '\0';
	secondarySignature[0] = '\0';

	if (error == B_OK) {
		BMimeType superType;
		if (type.GetSupertype(&superType) == B_OK)
			superType.GetPreferredApp(secondarySignature);
		if (type.IsInstalled()) {
			if (type.GetPreferredApp(primarySignature) != B_OK) {
				// The type is installed, but has no preferred app.
				primarySignature[0] = '\0';
			} else if (!strcmp(primarySignature, secondarySignature)) {
				// Both types have the same preferred app, there is
				// no point in testing it twice.
				secondarySignature[0] = '\0';
			}
		} else {
			// The type is not installed. We assume it is an app signature.
			strlcpy(primarySignature, mimeType, sizeof(primarySignature));
		}
	}

	// We will use this BMessage "signatures" to hold all supporting apps
	// so we can iterator over them in the preferred order. We include
	// the supporting apps in such a way that the configured preferred
	// applications for the MIME type are in front of other supporting
	// applications for the sub and the super type respectively.
	const char* kSigField = "applications";
	BMessage signatures;
	bool addedSecondarySignature = false;
	if (error == B_OK) {
		if (primarySignature[0] != '\0')
			error = signatures.AddString(kSigField, primarySignature);
		else {
			// If there is a preferred app configured for the super type,
			// but no preferred type for the sub-type, add the preferred
			// super type handler in front of any other handlers. This way
			// we fall-back to non-preferred but supporting apps only in the
			// case when there is a preferred handler for the sub-type but
			// it cannot be resolved (misconfiguration).
			if (secondarySignature[0] != '\0') {
				error = signatures.AddString(kSigField, secondarySignature);
				addedSecondarySignature = true;
			}
		}
	}

	BMessage supportingSignatures;
	if (error == B_OK
		&& type.GetSupportingApps(&supportingSignatures) == B_OK) {
		int32 subCount;
		if (supportingSignatures.FindInt32("be:sub", &subCount) != B_OK)
			subCount = 0;
		// Add all signatures with direct support for the sub-type.
		const char* supportingType;
		if (!addedSecondarySignature) {
			// Try to add the secondarySignature in front of all other
			// supporting apps, if we find it among those.
			for (int32 i = 0; error == B_OK && i < subCount
					&& supportingSignatures.FindString(kSigField, i,
						&supportingType) == B_OK; i++) {
				if (strcmp(primarySignature, supportingType) != 0
					&& strcmp(secondarySignature, supportingType) == 0) {
					error = signatures.AddString(kSigField, supportingType);
					addedSecondarySignature = true;
					break;
				}
			}
		}

		for (int32 i = 0; error == B_OK && i < subCount
				&& supportingSignatures.FindString(kSigField, i,
					&supportingType) == B_OK; i++) {
			if (strcmp(primarySignature, supportingType) != 0
				&& strcmp(secondarySignature, supportingType) != 0) {
				error = signatures.AddString(kSigField, supportingType);
			}
		}

		// Add the preferred type of the super type here before adding
		// the other types supporting the super type, but only if we have
		// not already added it in case there was no preferred app for the
		// sub-type configured.
		if (error == B_OK && !addedSecondarySignature
			&& secondarySignature[0] != '\0') {
			error = signatures.AddString(kSigField, secondarySignature);
		}

		// Add all signatures with support for the super-type.
		for (int32 i = subCount; error == B_OK
				&& supportingSignatures.FindString(kSigField, i,
					&supportingType) == B_OK; i++) {
			// Don't add the signature if it's one of the preferred apps
			// already.
			if (strcmp(primarySignature, supportingType) != 0
				&& strcmp(secondarySignature, supportingType) != 0) {
				error = signatures.AddString(kSigField, supportingType);
			}
		}
	} else {
		// Failed to get supporting apps, just add the preferred apps.
		if (error == B_OK && secondarySignature[0] != '\0')
			error = signatures.AddString(kSigField, secondarySignature);
	}

	if (error != B_OK)
		return error;

	// Set an error in case we can't resolve a single supporting app.
	error = B_LAUNCH_FAILED_NO_PREFERRED_APP;

	// See if we can find a good application that is valid from the messege.
	const char* signature;
	for (int32 i = 0;
		signatures.FindString(kSigField, i, &signature) == B_OK; i++) {
		if (signature[0] == '\0')
			continue;

		error = appMeta->SetTo(signature);

		// Check, whether the signature is installed and has an app hint
		bool appFound = false;
		if (error == B_OK && appMeta->GetAppHint(appRef) == B_OK) {
			// Resolve symbolic links, if necessary
			BEntry entry;
			if (entry.SetTo(appRef, true) == B_OK && entry.IsFile()
				&& entry.GetRef(appRef) == B_OK) {
				appFound = true;
			} else {
				// Bad app hint -- remove it
				appMeta->SetAppHint(NULL);
			}
		}

		// In case there is no app hint or it is invalid, we need to query for
		// the app.
		if (error == B_OK && !appFound)
			error = query_for_app(appMeta->Type(), appRef);

		if (error == B_OK)
			error = appFile->SetTo(appRef, B_READ_ONLY);

		// check, whether the app can be used
		if (error == B_OK)
			error = can_app_be_used(appRef);

		if (error == B_OK)
			break;
	}

	return error;
}


/*!	Gets the type of a file either from the node info or by sniffing.

	The method first tries to get the file type from the supplied node info. If
	that didn't work, the given entry ref is sniffed.

	\param file An entry_ref referring to the file in question.
	\param nodeInfo A BNodeInfo initialized to the file.
	\param mimeType A pointer to a pre-allocated char buffer of at least size
	       \c B_MIME_TYPE_LENGTH to be filled with the MIME type sniffed for
	       the file.

	\return A status code.
	\retval B_OK Everything went fine.
	\retval B_BAD_VALUE \c NULL \a file, \a nodeInfo or \a mimeType.
*/
status_t
BRoster::_GetFileType(const entry_ref* file, BNodeInfo* nodeInfo,
	char* mimeType) const
{
	// first try the node info
	if (nodeInfo->GetType(mimeType) == B_OK)
		return B_OK;

	if (fNoRegistrar)
		return B_NO_INIT;

	// Try to update the file's MIME info and just read the updated type.
	// If that fails, sniff manually.
	BPath path;
	if (path.SetTo(file) != B_OK
		|| update_mime_info(path.Path(), false, true, false) != B_OK
		|| nodeInfo->GetType(mimeType) != B_OK) {
		BMimeType type;
		status_t error = BMimeType::GuessMimeType(file, &type);
		if (error != B_OK)
			return error;

		if (!type.IsValid())
			return B_BAD_VALUE;

		strlcpy(mimeType, type.Type(), B_MIME_TYPE_LENGTH);
	}

	return B_OK;
}


void
BRoster::_InitMessenger()
{
	DBG(OUT("BRoster::InitMessengers() done\n"));
}


//!	Sends a request to the roster to clear the recent documents list.
void
BRoster::_ClearRecentDocuments() const
{
	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Delete all entry_refs
	for (int32 i = 0; i < sRecentDocuments.CountItems(); i++) {
		entry_ref* ref = (entry_ref*)sRecentDocuments.ItemAt(i);
		delete ref;
	}
	sRecentDocuments.MakeEmpty();
}


//!	Sends a request to the roster to clear the recent documents list.
void
BRoster::_ClearRecentFolders() const
{
	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Delete all entry_refs
	for (int32 i = 0; i < sRecentFolders.CountItems(); i++) {
		entry_ref* ref = (entry_ref*)sRecentFolders.ItemAt(i);
		delete ref;
	}
	sRecentFolders.MakeEmpty();
}


//!	Sends a request to the roster to clear the recent apps list.
void
BRoster::_ClearRecentApps() const
{
	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Delete all entry_refs
	for (int32 i = 0; i < sRecentApps.CountItems(); i++) {
		entry_ref* ref = (entry_ref*)sRecentApps.ItemAt(i);
		delete ref;
	}
	sRecentApps.MakeEmpty();
}


/*!	Loads the system's recently used document, folder, and
	application lists from the specified file.

	\note The current lists are cleared before loading the new lists

	\param filename The name of the file to load from
*/
void
BRoster::_LoadRecentLists(const char* filename) const
{
	if (filename == NULL)
		return;

	BFile file(filename, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	BMessage archive;
	if (archive.Unflatten(&file) != B_OK)
		return;

	std::lock_guard<std::mutex> lock(sRecentListsMutex);

	// Clear existing lists
	for (int32 i = 0; i < sRecentDocuments.CountItems(); i++)
		delete (entry_ref*)sRecentDocuments.ItemAt(i);
	sRecentDocuments.MakeEmpty();

	for (int32 i = 0; i < sRecentFolders.CountItems(); i++)
		delete (entry_ref*)sRecentFolders.ItemAt(i);
	sRecentFolders.MakeEmpty();

	for (int32 i = 0; i < sRecentApps.CountItems(); i++)
		delete (entry_ref*)sRecentApps.ItemAt(i);
	sRecentApps.MakeEmpty();

	// Load documents
	entry_ref ref;
	for (int32 i = 0; archive.FindRef("documents", i, &ref) == B_OK; i++) {
		entry_ref* newRef = new (std::nothrow) entry_ref(ref);
		if (newRef != NULL)
			sRecentDocuments.AddItem(newRef);
	}

	// Load folders
	for (int32 i = 0; archive.FindRef("folders", i, &ref) == B_OK; i++) {
		entry_ref* newRef = new (std::nothrow) entry_ref(ref);
		if (newRef != NULL)
			sRecentFolders.AddItem(newRef);
	}

	// Load apps
	for (int32 i = 0; archive.FindRef("apps", i, &ref) == B_OK; i++) {
		entry_ref* newRef = new (std::nothrow) entry_ref(ref);
		if (newRef != NULL)
			sRecentApps.AddItem(newRef);
	}
}


/*!	Saves the system's recently used document, folder, and
	application lists to the specified file.

	\param filename The name of the file to save to
*/
void
BRoster::_SaveRecentLists(const char* filename) const
{
	if (filename == NULL)
		return;

	BMessage archive;

	{
		std::lock_guard<std::mutex> lock(sRecentListsMutex);

		// Save documents
		for (int32 i = 0; i < sRecentDocuments.CountItems(); i++) {
			entry_ref* ref = (entry_ref*)sRecentDocuments.ItemAt(i);
			if (ref != NULL)
				archive.AddRef("documents", ref);
		}

		// Save folders
		for (int32 i = 0; i < sRecentFolders.CountItems(); i++) {
			entry_ref* ref = (entry_ref*)sRecentFolders.ItemAt(i);
			if (ref != NULL)
				archive.AddRef("folders", ref);
		}

		// Save apps
		for (int32 i = 0; i < sRecentApps.CountItems(); i++) {
			entry_ref* ref = (entry_ref*)sRecentApps.ItemAt(i);
			if (ref != NULL)
				archive.AddRef("apps", ref);
		}
	}

	BFile file(filename, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		archive.Flatten(&file);
}
