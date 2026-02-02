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
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

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
#include <Volume.h>
#include <VolumeRoster.h>

#include <locks.h>

#include <AppMisc.h>
#include <DesktopLink.h>
#include <MessengerPrivate.h>
#include <PortLink.h>
#include <RosterPrivate.h>
#include <ServerProtocol.h>


using namespace std;
using namespace BPrivate;


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
static const int32 kMaxRecentItems = 10;


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
	if (error == B_OK)
		error = entry.SetTo(ref, true);

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

#endif
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
{
	_InitMessenger();
}


BRoster::~BRoster()
{
}

#if 0
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
	status_t error = (teamIDList ? B_OK : B_BAD_VALUE);
	// compose the request message
	BMessage request(B_REG_GET_APP_LIST);

	// send the request
	BMessage reply;
	if (error == B_OK)
		error = fMessenger.SendMessage(&request, &reply);

	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS) {
			team_id team;
			for (int32 i = 0; reply.FindInt32("teams", i, &team) == B_OK; i++)
				teamIDList->AddItem((void*)(addr_t)team);
		} else {
			if (reply.FindInt32("error", &error) != B_OK)
				error = B_ERROR;
			DBG(OUT("Roster request unsuccessful: %s\n", strerror(error)));
			DBG(reply.PrintToStream());
		}
	} else {
		DBG(OUT("Sending message to roster failed: %s\n", strerror(error)));
	}
}


void
BRoster::GetAppList(const char* signature, BList* teamIDList) const
{
	status_t error = B_OK;
	if (signature == NULL || teamIDList == NULL)
		error = B_BAD_VALUE;

	// compose the request message
	BMessage request(B_REG_GET_APP_LIST);
	if (error == B_OK)
		error = request.AddString("signature", signature);

	// send the request
	BMessage reply;
	if (error == B_OK)
		error = fMessenger.SendMessage(&request, &reply);

	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS) {
			team_id team;
			for (int32 i = 0; reply.FindInt32("teams", i, &team) == B_OK; i++)
				teamIDList->AddItem((void*)(addr_t)team);
		} else if (reply.FindInt32("error", &error) != B_OK)
			error = B_ERROR;
	}
}


status_t
BRoster::GetAppInfo(const char* signature, app_info* info) const
{
	status_t error = B_OK;
	if (signature == NULL || info == NULL)
		error = B_BAD_VALUE;

	// compose the request message
	BMessage request(B_REG_GET_APP_INFO);
	if (error == B_OK)
		error = request.AddString("signature", signature);

	// send the request
	BMessage reply;
	if (error == B_OK)
		error = fMessenger.SendMessage(&request, &reply);

	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS)
			error = find_message_app_info(&reply, info);
		else if (reply.FindInt32("error", &error) != B_OK)
			error = B_ERROR;
	}

	return error;
}


status_t
BRoster::GetAppInfo(entry_ref* ref, app_info* info) const
{
	status_t error = (ref && info ? B_OK : B_BAD_VALUE);
	// compose the request message
	BMessage request(B_REG_GET_APP_INFO);
	if (error == B_OK)
		error = request.AddRef("ref", ref);

	// send the request
	BMessage reply;
	if (error == B_OK)
		error = fMessenger.SendMessage(&request, &reply);

	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS)
			error = find_message_app_info(&reply, info);
		else if (reply.FindInt32("error", &error) != B_OK)
			error = B_ERROR;
	}
	return error;
}


#endif
status_t
BRoster::GetRunningAppInfo(team_id team, app_info* info) const
{
	if (team != getpid()) {
		// For now, we only support querying the current app
		return B_BAD_VALUE;
	}

	extern thread_id _main_thread_for_team(team_id);
	info->team = be_app->Team();
	info->thread = _main_thread_for_team(info->team);

	if (be_app != NULL) {
		strncpy(info->signature, be_app->Signature(), B_MIME_TYPE_LENGTH - 1);
		info->signature[B_MIME_TYPE_LENGTH - 1] = '\0';
	} else {
		info->signature[0] = '\0';
	}

	// TODO: Read actual flags from app resources/attributes
	// For now, use B_MULTIPLE_LAUNCH as a reasonable default
	// since launch restrictions are not yet implemented anyway
	info->flags = B_MULTIPLE_LAUNCH;

	get_app_ref(&info->ref);

	return B_OK;
}
#if 0

status_t
BRoster::GetActiveAppInfo(app_info* info) const
{
	if (info == NULL)
		return B_BAD_VALUE;

	// compose the request message
	BMessage request(B_REG_GET_APP_INFO);
	// send the request
	BMessage reply;
	status_t error = fMessenger.SendMessage(&request, &reply);
	// evaluate the reply
	if (error == B_OK) {
		if (reply.what == B_REG_SUCCESS)
			error = find_message_app_info(&reply, info);
		else if (reply.FindInt32("error", &error) != B_OK)
			error = B_ERROR;
	}
	return error;
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
BRoster::ActivateApp(team_id team) const
{
	BPrivate::DesktopLink link;

	status_t status = link.InitCheck();
	if (status < B_OK)
		return status;

	// prepare the message
	status_t error = link.StartMessage(AS_ACTIVATE_APP);
	if (error != B_OK)
		return error;

	error = link.Attach(link.ReceiverPort());
	if (error != B_OK)
		return error;

	error = link.Attach(team);
	if (error != B_OK)
		return error;

	// send it
	status_t code;
	error = link.FlushWithReply(code);
	if (error != B_OK)
		return error;

	return code;
}
#endif

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

	if (_appTeam != NULL) {
		// we're supposed to set _appTeam to -1 on error; we'll
		// reset it later if everything goes well
		*_appTeam = -1;
	}

	if (mimeType == NULL && ref == NULL)
		return B_BAD_VALUE;

	// use a mutable copy of the document entry_ref
	entry_ref _docRef;
	entry_ref* docRef = NULL;
	if (ref != NULL) {
		_docRef = *ref;
		docRef = &_docRef;
	}

	status_t error = B_OK;
	#if 0
	uint32 otherAppFlags = B_REG_DEFAULT_APP_FLAGS;
	uint32 appFlags = B_REG_DEFAULT_APP_FLAGS;
	bool alreadyRunning = false;
	bool wasDocument = true;
	ArgVector argVector;
	team_id team = -1;
	thread_id appThread = -1;
	port_id appPort = -1;
	uint32 appToken = 0;
	entry_ref hintRef;

	while (true) {
		// find the app
		entry_ref appRef;
		char signature[B_MIME_TYPE_LENGTH];
		error = _ResolveApp(mimeType, docRef, &appRef, signature,
			&appFlags, &wasDocument);
		DBG(OUT("  find app: %s (%" B_PRIx32 ") %s \n", strerror(error), error,
			signature));

		if (error != B_OK)
			return error;

		// build an argument vector
		error = argVector.Init(argc, args, &appRef,
			wasDocument ? docRef : NULL);
		DBG(OUT("  build argv: %s (%" B_PRIx32 ")\n", strerror(error), error));
		if (error != B_OK)
			return error;

		// pre-register the app (but ignore scipts)
		app_info appInfo;
		bool isScript = wasDocument && docRef != NULL && *docRef == appRef;
		if (!isScript && !fNoRegistrar) {
			error = _AddApplication(signature, &appRef, appFlags, -1, -1, -1,
				false, &appToken, &team);
			if (error == B_ALREADY_RUNNING) {
				DBG(OUT("  already running\n"));
				alreadyRunning = true;

				// get the app flags for the running application
				//error = _IsAppRegistered(&appRef, team, appToken, NULL,
				//	&appInfo);
				if (error == B_OK) {
					otherAppFlags = appInfo.flags;
					appPort = appInfo.port;
					team = appInfo.team;
				}
			}
			DBG(OUT("  pre-register: %s (%" B_PRIx32 ")\n", strerror(error),
				error));
		}

		// launch the app
		if (error == B_OK && !alreadyRunning) {
			DBG(OUT("  token: %" B_PRIu32 "\n", appToken));
			// load the app image
			appThread = load_image(argVector.Count(),
				const_cast<const char**>(argVector.Args()), environment);

			// get the app team
			if (appThread >= 0) {
				thread_info threadInfo;
				error = get_thread_info(appThread, &threadInfo);
				if (error == B_OK)
					team = threadInfo.team;
			} else if (wasDocument && appThread == B_NOT_AN_EXECUTABLE)
				error = B_LAUNCH_FAILED_EXECUTABLE;
			else
				error = appThread;

			DBG(OUT("  load image: %s (%" B_PRIx32 ")\n", strerror(error),
				error));

			// resume the launched team
			if (error == B_OK && !launchSuspended)
				error = resume_thread(appThread);

			DBG(OUT("  resume thread: %s (%" B_PRIx32 ")\n", strerror(error),
				error));
			// on error: kill the launched team and unregister the app
			if (error != B_OK) {
				if (appThread >= 0)
					kill_thread(appThread);

				if (!isScript) {
					if (!wasDocument) {
						// Did we already try this?
						if (appRef == hintRef)
							break;

						// Remove app hint if it's this one
						BMimeType appType(signature);

						if (appType.InitCheck() == B_OK
							&& appType.GetAppHint(&hintRef) == B_OK
							&& appRef == hintRef) {
							appType.SetAppHint(NULL);
							// try again with the app hint removed
							continue;
						}
					}
				}
			}
		}
		// Don't try again
		break;
	}

	if (alreadyRunning && current_team() == team) {
		// The target team is calling us, so we don't send it the message
		// to prevent an endless loop
		error = B_BAD_VALUE;
	}

	// send "on launch" messages
	if (error == B_OK && !fNoRegistrar) {
		// If the target app is B_ARGV_ONLY, only if it is newly launched
		// messages are sent to it (namely B_ARGV_RECEIVED and B_READY_TO_RUN).
		// An already running B_ARGV_ONLY app won't get any messages.
		bool argvOnly = (appFlags & B_ARGV_ONLY) != 0
			|| (alreadyRunning && (otherAppFlags & B_ARGV_ONLY) != 0);
		const BList* _messageList = (argvOnly ? NULL : messageList);
		// don't send ref, if it refers to the app or is included in the
		// argument vector
		const entry_ref* _ref = argvOnly || !wasDocument
			|| argVector.Count() > 1 ? NULL : docRef;
		if (!(argvOnly && alreadyRunning)) {
			//_SendToRunning(team, argVector.Count(), argVector.Args(),
			//	_messageList, _ref, alreadyRunning);
		}
	}

	// set return values
	if (error == B_OK) {
		if (alreadyRunning)
			error = B_ALREADY_RUNNING;
		else if (_appTeam)
			*_appTeam = team;

		if (_appThread != NULL)
			*_appThread = appThread;
		if (_appPort != NULL)
			*_appPort = appPort;
		if (_appToken != NULL)
			*_appToken = appToken;
	}

#endif
	DBG(OUT("BRoster::_LaunchApp() done: %s (%" B_PRIx32 ")\n",
		strerror(error), error));

	return error;
}

#if 0

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
		if (_wasDocument != NULL)
			*_wasDocument = !(appMeta == inType);
	} else {
		error = _TranslateRef(ref, &appMeta, &appRef, &appFile,
			_wasDocument);
	}

#if 0
// Cosmoe FIXME: change entry_ref to path here
	// create meta mime
	if (!fNoRegistrar && error == B_OK) {
		BPath path;
		if (path.SetTo(&appRef) == B_OK)
			create_app_meta_mime(path.Path(), false, true, false);
	}
#endif

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
#if 0
// Cosmoe FIXME: change entry_ref to path here

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
#endif
	return B_OK;
}
#endif


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
	}

	BFile file(filename, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		archive.Flatten(&file);
}
