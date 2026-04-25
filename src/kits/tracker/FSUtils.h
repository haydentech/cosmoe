/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/
#ifndef FS_UTILS_H
#define FS_UTILS_H


#include <FindDirectory.h>
#include <List.h>
#include <ObjectList.h>
#include <Point.h>
#include <StorageDefs.h>

#include <vector>

#include "Model.h"


// APIs/code in FSUtils.h and FSUtils.cpp is slated for a major cleanup
// -- in other words, you will find a lot of ugly cruft in here

class BDirectory;
class BEntry;
class BList;
class BFile;

namespace BPrivate {


#define B_DESKTOP_DIR_NAME "Desktop"
#define B_DISKS_DIR_NAME "Disks"
#define B_TRASH_DIR_NAME "Trash"

#ifndef _IMPEXP_TRACKER
#define _IMPEXP_TRACKER
#endif

status_t FSGetParentVirtualDirectoryAware(const BEntry& entry, entry_ref& _ref);
status_t FSGetParentVirtualDirectoryAware(const BEntry& entry, BEntry& _entry);

_IMPEXP_TRACKER status_t FSLaunchItem(const entry_ref* application,
	const BMessage* refsReceived, bool async, bool openWithOK);
	// Preferred way of launching; only pass an actual application in
	// <application>, not a document; to open documents with the preferred
	// app, pase 0 in <application> and stuff all the document refs into
	// <refsReceived> Consider having silent mode that does not show alerts,
	// just returns error code

status_t TrackerLaunch(const entry_ref* appRef, bool async);
status_t TrackerLaunch(const BMessage* refs, bool async,
	bool okToRunOpenWith = true);
status_t TrackerLaunch(const entry_ref* appRef, const BMessage* refs,
	bool async, bool okToRunOpenWith = true);

	// some extra directory_which values
// move these to FindDirectory.h
const uint32 B_USER_MAIL_DIRECTORY = 3500;
const uint32 B_USER_QUERIES_DIRECTORY = 3501;
const uint32 B_USER_PEOPLE_DIRECTORY = 3502;
const uint32 B_USER_DOWNLOADS_DIRECTORY = 3503;
const uint32 B_USER_DESKBAR_APPS_DIRECTORY = 3504;
const uint32 B_USER_DESKBAR_PREFERENCES_DIRECTORY = 3505;
const uint32 B_USER_DESKBAR_DEVELOP_DIRECTORY = 3506;
const uint32 B_BOOT_DISK = 3507;

class WellKnowEntryList {
	// matches up names, id's and node_refs of well known entries in the
	// system hierarchy
	public:
		struct WellKnownEntry {
			WellKnownEntry(const node_ref* node, directory_which which,
				const char* name)
				:
				node(*node),
				which(which),
				name(name)
			{
			}

			// mwcc needs these explicitly to use vector
			WellKnownEntry(const WellKnownEntry &clone)
				:
				node(clone.node),
				which(clone.which),
				name(clone.name)
			{
			}

			WellKnownEntry()
			{
			}

			node_ref node;
			directory_which which;
			BString name;
		};

		static directory_which Match(const node_ref*);
		static const WellKnownEntry* MatchEntry(const node_ref*);
		static void Quit();

	private:
		const WellKnownEntry* MatchEntryCommon(const node_ref*);
		WellKnowEntryList();
		void AddOne(directory_which, const char* name);
		void AddOne(directory_which, const char* path, const char* name);
		void AddOne(directory_which, directory_which base,
			const char* extension, const char* name);

		std::vector<WellKnownEntry> entries;
		static WellKnowEntryList* self;
};


#if B_BEOS_VERSION_DANO
#undef _IMPEXP_TRACKER
#endif

} // namespace BPrivate

using namespace BPrivate;

#endif	// FS_UTILS_H
