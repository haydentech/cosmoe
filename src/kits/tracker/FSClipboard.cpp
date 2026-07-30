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
and shall be included in all copies or substantial portions of the Software..

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


#include "FSClipboard.h"

#include <Clipboard.h>
#include <Alert.h>
#include <Catalog.h>
#include <Entry.h>
#include <Locale.h>
#include <NodeMonitor.h>

#include "Commands.h"
#include "FSUtils.h"
#include "Tracker.h"


// prototypes
static void MakeNodeFromName(node_ref* node, char* name);
static inline void MakeRefName(char* refName, size_t refNameSize,
	const node_ref* node);
static inline void MakeModeName(char* modeName, size_t modeNameSize,
	const node_ref* node);
static inline void MakeModeNameFromRefName(char* modeName, char* refName);
static inline bool CompareModeAndRefName(const char* modeName,
	const char* refName);
static status_t GetNodeRefForRef(const entry_ref* ref, node_ref* node);

#if 0
static bool
FSClipboardCheckIntegrity()
{
	return true;
}
#endif

static void
MakeNodeFromName(node_ref* node, char* name)
{
	char* nodeString = strchr(name, '_');
	if (nodeString != NULL) {
		node->node = strtoll(nodeString + 1, (char**)NULL, 10);
		node->device = atoi(name + 1);
	}
}


static status_t
GetNodeRefForRef(const entry_ref* ref, node_ref* node)
{
	if (ref == NULL || node == NULL)
		return B_BAD_VALUE;

	BEntry entry(ref);
	if (entry.InitCheck() != B_OK)
		return entry.InitCheck();

	return entry.GetNodeRef(node);
}


static inline void
MakeRefName(char* refName, size_t refNameSize, const node_ref* node)
{
	snprintf(refName, refNameSize, "r%" B_PRIdDEV "_%" B_PRIdINO,
		node->device, node->node);
}


static inline void
MakeModeName(char* modeName, size_t modeNameSize, const node_ref* node)
{
	snprintf(modeName, modeNameSize, "m%" B_PRIdDEV "_%" B_PRIdINO,
		node->device, node->node);
}


static inline void
MakeModeName(char* name)
{
	name[0] = 'm';
}


static inline void
MakeModeNameFromRefName(char* modeName, char* refName)
{
	strcpy(modeName, refName);
	modeName[0] = 'm';
}


static inline bool
CompareModeAndRefName(const char* modeName, const char* refName)
{
	return !strcmp(refName + 1, modeName + 1);
}


//	#pragma mark - FSClipBoard


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "FSClipBoard"


bool
FSClipboardHasRefs()
{
	bool result = false;

	if (be_clipboard->Lock()) {
		BMessage* clip = be_clipboard->Data();
		if (clip != NULL) {
#ifdef B_BEOS_VERSION_DANO
			const
#endif
			char* refName;
#ifdef B_BEOS_VERSION_DANO
			const
#endif
			char* modeName;
			uint32 type;
			int32 count;
			if (clip->GetInfo(B_REF_TYPE, 0, &refName, &type, &count) == B_OK
				&& clip->GetInfo(B_INT32_TYPE, 0, &modeName, &type, &count)
					== B_OK) {
				result = CompareModeAndRefName(modeName, refName);
			}
		}
		be_clipboard->Unlock();
	}
	return result;
}


void
FSClipboardStartWatch(BMessenger target)
{
}


void
FSClipboardStopWatch(BMessenger target)
{
}


void
FSClipboardClear()
{
	if (!be_clipboard->Lock())
		return;

	be_clipboard->Clear();
	be_clipboard->Commit();
	be_clipboard->Unlock();
}


/** This function adds the given poses list to the clipboard, for both copy
 *	and cut. All poses in the list must have "directory" as parent.
 *	"moveMode" is either kMoveSelection or kCopySelection.
 *	It will check if the entries are already present, so that there can only
 *	be one reference to them in the clipboard.
 */
uint32
FSClipboardAddPoses(const node_ref* directory, PoseList* list,
	uint32 moveMode, bool clearClipboard)
{
	uint32 refsAdded = 0;

	return refsAdded;
}


uint32
FSClipboardRemovePoses(const node_ref* directory, PoseList* list)
{
	if (!be_clipboard->Lock())
		return 0;

	uint32 refsRemoved = 0;

	return refsRemoved;
}


/** Pastes entries from the clipboard to the target model's directory.
 *	Updates moveModes and notifies listeners if necessary.
 */
bool
FSClipboardPaste(Model* model, uint32 linksMode)
{
	if (!FSClipboardHasRefs())
		return false;

	return false;
}


// Seek node in clipboard, if found return it's moveMode
// else return 0
uint32
FSClipboardFindNodeMode(Model* model, bool autoLock, bool updateRefIfNeeded)
{
	int32 moveMode = 0;

	return (uint32)moveMode;
}


void
FSClipboardRemove(Model* model)
{
	BMessenger messenger(kTrackerSignature);
	if (messenger.IsValid()) {
		BMessage* report = new BMessage(kFSClipboardChanges);
		TClipboardNodeRef tcnode;
		tcnode.node = *model->NodeRef();
		tcnode.moveMode = kDelete;
		const entry_ref* ref = model->EntryRef();
		report->AddDevice("device", ref->device);
		report->AddInode("directory", ref->directory);
		report->AddBool("clearClipboard", false);
		report->AddData("tcnode", T_CLIPBOARD_NODE, &tcnode, sizeof(tcnode),
			true);
		messenger.SendMessage(report);
		delete report;
	}
}


//	#pragma mark -


BClipboardRefsWatcher::BClipboardRefsWatcher()
	:	BLooper("ClipboardRefsWatcher", B_LOW_PRIORITY, 4096),
	fNotifyList(10)
{
	watch_node(NULL, B_WATCH_MOUNT, this);
	fRefsInClipboard = FSClipboardHasRefs();
	be_clipboard->StartWatching(this);
}


BClipboardRefsWatcher::~BClipboardRefsWatcher()
{
	stop_watching(this);
	be_clipboard->StopWatching(this);
}


void
BClipboardRefsWatcher::AddToNotifyList(BMessenger target)
{
	if (Lock()) {
		// add the messenger if it's not already in the list
		// ToDo: why do we have to care about that?
		BMessenger* messenger;
		bool found = false;

		for (int32 index = 0; (messenger = fNotifyList.ItemAt(index)) != NULL;
				index++) {
			if (*messenger == target) {
				found = true;
				break;
			}
		}
		if (!found)
			fNotifyList.AddItem(new BMessenger(target));

		Unlock();
	}
}


void
BClipboardRefsWatcher::RemoveFromNotifyList(BMessenger target)
{
	if (Lock()) {
		BMessenger* messenger;

		for (int32 index = 0; (messenger = fNotifyList.ItemAt(index)) != NULL;
				index++) {
			if (*messenger == target) {
				delete fNotifyList.RemoveItemAt(index);
				break;
			}
		}
		Unlock();
	}
}


void
BClipboardRefsWatcher::AddRef(const entry_ref* ref)
{
	//TTracker::WatchRef(ref, B_WATCH_NAME, this);
	fRefsInClipboard = true;
}


void
BClipboardRefsWatcher::RemoveRef(const entry_ref* ref, const node_ref* node,
	bool removeFromClipboard)
{
	watch_path(ref->name, B_STOP_WATCHING, this);

	if (!removeFromClipboard)
		return;

	if (be_clipboard->Lock()) {
		BMessage* clip = be_clipboard->Data();
		if (clip != NULL) {
			char name[64];
			node_ref resolvedNode;
			if (node != NULL
				|| GetNodeRefForRef(ref, &resolvedNode) == B_OK) {
				const node_ref& keyNode = node != NULL ? *node : resolvedNode;
				MakeRefName(name, sizeof(name), &keyNode);
				clip->RemoveName(name);
				MakeModeName(name);
				clip->RemoveName(name);
			}

			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}
}


void
BClipboardRefsWatcher::RemoveRefsByDevice(dev_t device)
{
	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	if (clip != NULL) {
		char deviceName[6];
		snprintf(deviceName, sizeof(deviceName), "r%" B_PRIdDEV "_", device);

		int32 index = 0;
		char* refName;
		type_code type;
		int32 count;
		while (clip->GetInfo(B_REF_TYPE, index,
#ifdef B_BEOS_VERSION_DANO
			(const char**)
#endif
			&refName, &type, &count) == B_OK) {
			if (!strncmp(deviceName, refName, strlen(deviceName))) {
				clip->RemoveName(refName);
				MakeModeName(refName);
				clip->RemoveName(refName);

				node_ref node;
				MakeNodeFromName(&node, refName);
				watch_node(&node, B_STOP_WATCHING, this);
			}
			index++;
		}
		be_clipboard->Commit();
	}
	be_clipboard->Unlock();
}


void
BClipboardRefsWatcher::UpdateRef(const entry_ref* ref, const node_ref* node)
{
	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	if (clip != NULL) {
		char name[64];
		node_ref resolvedNode;
		if (node != NULL
			|| GetNodeRefForRef(ref, &resolvedNode) == B_OK) {
			const node_ref& keyNode = node != NULL ? *node : resolvedNode;
			MakeRefName(name, sizeof(name), &keyNode);
			if ((clip->ReplaceRef(name, ref)) != B_OK) {
				clip->RemoveName(name);
				MakeModeName(name);
				clip->RemoveName(name);

				RemoveRef(ref, &keyNode);
			}
		}
		be_clipboard->Commit();
	}
	be_clipboard->Unlock();
}


void
BClipboardRefsWatcher::Clear()
{
	stop_watching(this);
	watch_node(NULL, B_WATCH_MOUNT, this);

	BMessage message(kFSClipboardChanges);
	message.AddBool("clearClipboard", true);
	if (Lock()) {
		int32 items = fNotifyList.CountItems();
		for (int32 i = 0;i < items;i++) {
			fNotifyList.ItemAt(i)->SendMessage(&message);
		}
		Unlock();
	}
}


//void
//BClipboardRefsWatcher::UpdatePoseViews(bool clearClipboard,
//	const node_ref* node)
//{
//	BMessage message(kFSClipboardChanges);
//	message.AddInt32("device", node->device);
//	message.AddInt64("directory", node->node);
//	message.AddBool("clearClipboard", clearClipboard);
//
//	if (Lock()) {
//		int32 items = fNotifyList.CountItems();
//		for (int32 i = 0;i < items;i++) {
//			fNotifyList.ItemAt(i)->SendMessage(&message);
//		}
//		Unlock();
//	}
//}


void
BClipboardRefsWatcher::UpdatePoseViews(BMessage* reportMessage)
{
	if (Lock()) {
		// check if it was cleared, if so clear watching
		bool clearClipboard = false;
		if (reportMessage->FindBool("clearClipboard", &clearClipboard) == B_OK
			&& clearClipboard) {
			stop_watching(this);
			// FIXME: Need to figure out how to watch volumes
			//watch_node(NULL, B_WATCH_MOUNT, this);
		}

		// loop through reported node_ref's movemodes:
		// move or copy: start watching node_ref
		// remove: stop watching node_ref
		int32 index = 0;
		TClipboardNodeRef* tcnode = NULL;
		ssize_t size;
		while (reportMessage->FindData("tcnode", T_CLIPBOARD_NODE, index,
				(const void**)&tcnode, &size) == B_OK) {
			if (tcnode->moveMode == kDelete) {
				watch_node(&tcnode->node, B_STOP_WATCHING, this);
			} else {
				watch_node(&tcnode->node, B_STOP_WATCHING, this);
				//TTracker::WatchNode(&tcnode->node, B_WATCH_NAME, this);
				fRefsInClipboard = true;
			}
			index++;
		}

		// send report
		int32 items = fNotifyList.CountItems();
		for (int32 i = 0;i < items;i++) {
			fNotifyList.ItemAt(i)->SendMessage(reportMessage);
		}
		Unlock();
	}
}


void
BClipboardRefsWatcher::MessageReceived(BMessage* message)
{
	if (message->what == B_CLIPBOARD_CHANGED && fRefsInClipboard) {
		if (!(fRefsInClipboard = FSClipboardHasRefs()))
			Clear();
		return;
	} else if (message->what != B_NODE_MONITOR) {
		_inherited::MessageReceived(message);
		return;
	}

	switch (message->GetInt32("opcode", 0)) {
		case B_ENTRY_MOVED:
		{
			ino_t toDir;
			node_ref node;
			const char* name = NULL;
			message->FindInode("to directory", &toDir);
			message->FindInode("node", &node.node);
			message->FindDevice("device", &node.device);
			message->FindString("name", &name);
			entry_ref ref(node.device, toDir, name);
			UpdateRef(&ref, &node);
			break;
		}

		case B_DEVICE_UNMOUNTED:
		{
			dev_t device;
			message->FindDevice("device", &device);
			RemoveRefsByDevice(device);
			break;
		}

		case B_ENTRY_REMOVED:
		{
			node_ref node;
			const char* name = NULL;
			if (message->FindInode("node", &node.node) == B_OK
				&& message->FindDevice("device", &node.device) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				entry_ref ref(node.device, 0, name);
				RemoveRef(&ref, &node, true);
			}
			break;
		}
	}
}
