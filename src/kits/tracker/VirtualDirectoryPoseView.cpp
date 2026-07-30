/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold, ingo_weinhold@gmx.de
 */


#include "VirtualDirectoryPoseView.h"

#include <new>

#include <AutoLocker.h>
#include <NotOwningEntryRef.h>
#include <PathMonitor.h>
#include <storage_support.h>

#include "Commands.h"
#include "Tracker.h"
#include "VirtualDirectoryEntryList.h"
#include "VirtualDirectoryManager.h"


namespace BPrivate {

//	#pragma mark - VirtualDirectoryPoseView


VirtualDirectoryPoseView::VirtualDirectoryPoseView(Model* model)
	:
	BPoseView(model, kListMode),
	fDirectoryPaths(),
	fRootDefinitionFileRef(),
	fFileChangeTime(-1),
	fIsRoot(false)
{
	VirtualDirectoryManager* manager = VirtualDirectoryManager::Instance();
	if (manager == NULL)
		return;

	AutoLocker<VirtualDirectoryManager> managerLocker(manager);
	if (_UpdateDirectoryPaths() != B_OK)
		return;

	manager->GetRootDefinitionFile(*model->EntryRef(), fRootDefinitionFileRef);
	fIsRoot = fRootDefinitionFileRef == *model->EntryRef();
}


VirtualDirectoryPoseView::~VirtualDirectoryPoseView()
{
}


void
VirtualDirectoryPoseView::MessageReceived(BMessage* message)
{
	if (message->WasDropped())
		return _inherited::MessageReceived(message);

	switch (message->what) {
		// ignore all edit operations
		case B_CUT:
		case B_PASTE:
		case kCutMoreSelectionToClipboard:
		case kDeleteSelection:
		case kDuplicateSelection:
		case kIconMode:
		case kMiniIconMode:
		case kMoveSelectionToTrash:
		case kNewEntryFromTemplate:
		case kNewFolder:
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
VirtualDirectoryPoseView::AttachedToWindow()
{
	_inherited::AttachedToWindow();
	AddFilter(new TPoseViewFilter(this));
}


void
VirtualDirectoryPoseView::RestoreState(AttributeStreamNode* node)
{
	_inherited::RestoreState(node);
	fViewState->SetViewMode(kListMode);
}


void
VirtualDirectoryPoseView::RestoreState(const BMessage& message)
{
	_inherited::RestoreState(message);
	fViewState->SetViewMode(kListMode);
}


void
VirtualDirectoryPoseView::SavePoseLocations(BRect* frameIfDesktop)
{
}


void
VirtualDirectoryPoseView::SetViewMode(uint32 newMode)
{
}


EntryListBase*
VirtualDirectoryPoseView::InitDirentIterator(const entry_ref* ref)
{
	if (fRootDefinitionFileRef.name == NULL || *ref != *TargetModel()->EntryRef())
		return NULL;

	Model sourceModel(ref, false, true);
	if (sourceModel.InitCheck() != B_OK)
		return NULL;

	VirtualDirectoryEntryList* entryList
		= new(std::nothrow) VirtualDirectoryEntryList(
			*TargetModel()->EntryRef(), fDirectoryPaths);
	if (entryList == NULL || entryList->InitCheck() != B_OK) {
		delete entryList;
		return NULL;
	}

	return entryList;
}


void
VirtualDirectoryPoseView::StartWatching()
{
	// watch the directories
	int32 count = fDirectoryPaths.CountStrings();
	for (int32 i = 0; i < count; i++) {
		BString path = fDirectoryPaths.StringAt(i);
		BPathMonitor::StartWatching(path, B_WATCH_DIRECTORY | B_WATCH_CHILDREN
			| B_WATCH_NAME | B_WATCH_STAT | B_WATCH_INTERIM_STAT | B_WATCH_ATTR, this);
	}

	// watch the definition file
	TTracker::WatchRef(TargetModel()->EntryRef(),
		B_WATCH_NAME | B_WATCH_STAT | B_WATCH_ATTR, this);

	// also watch the root definition file
	if (!fIsRoot)
		TTracker::WatchRef(&fRootDefinitionFileRef, B_WATCH_STAT, this);
}


void
VirtualDirectoryPoseView::StopWatching()
{
	BPathMonitor::StopWatching(this);
	stop_watching(this);
}


bool
VirtualDirectoryPoseView::FSNotification(const BMessage* message)
{
	switch (message->GetInt32("opcode", 0)) {
		case B_ENTRY_CREATED:
			return _EntryCreated(message);

		case B_ENTRY_REMOVED:
			return _EntryRemoved(message);

		case B_ENTRY_MOVED:
			return _EntryMoved(message);

		case B_STAT_CHANGED:
			return _NodeStatChanged(message);

		default:
			return _inherited::FSNotification(message);
	}
}


bool
VirtualDirectoryPoseView::_EntryCreated(const BMessage* message)
{
	NotOwningEntryRef entryRef;
	node_ref nodeRef;

	if (message->FindDevice("device", &nodeRef.device) != B_OK
		|| message->FindInode("node", &nodeRef.node) != B_OK
		|| message->FindInode("directory", &entryRef.directory) != B_OK
		|| message->FindString("name", (const char**)&entryRef.name) != B_OK) {
		return true;
	}
	entryRef.device = nodeRef.device;

	// It might be one of our directories.
	BString path;
	if (message->FindString("path", &path) == B_OK
		&& fDirectoryPaths.HasString(path)) {
		// Cosmoe cannot open a BDirectory from a node_ref yet.
		return true;
	}

	// See, if this entry actually becomes visible. If not, we can simply ignore
	// it.
	struct stat st;
	entry_ref visibleEntryRef;
	if (!_GetEntry(entryRef.name, visibleEntryRef, &st)
		|| visibleEntryRef != entryRef) {
		return true;
	}

	// If it is a directory, translate it.
	VirtualDirectoryManager* manager = VirtualDirectoryManager::Instance();
	AutoLocker<VirtualDirectoryManager> managerLocker(manager);

	bool entryTranslated = S_ISDIR(st.st_mode);
	if (entryTranslated) {
		if (manager == NULL)
			return true;

		if (manager->TranslateDirectoryEntry(*TargetModel()->EntryRef(),
				entryRef) != B_OK) {
			return true;
		}
	}

	// The entry might replace another entry. If it does, we'll fake a removed
	// message for the old one first.
	BPose* pose = fPoseList->FindPoseByFileName(entryRef.name);
	if (pose != NULL) {
		if (nodeRef == *pose->TargetModel()->NodeRef()) {
			// apparently not really a new entry -- can happen for
			// subdirectories
			return true;
		}

		// It may be a directory, so tell the manager.
		if (manager != NULL)
			manager->DirectoryRemoved(*pose->TargetModel()->EntryRef());

		managerLocker.Unlock();

		BMessage removedMessage(B_NODE_MONITOR);
		_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_REMOVED,
			*pose->TargetModel()->EntryRef());
	} else
		managerLocker.Unlock();

	return entryTranslated
		? (_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_CREATED, entryRef), true)
		: _inherited::FSNotification(message);
}


bool
VirtualDirectoryPoseView::_EntryRemoved(const BMessage* message)
{
	NotOwningEntryRef entryRef;
	node_ref nodeRef;

	if (message->FindDevice("device", &nodeRef.device) != B_OK
		|| message->FindInode("node", &nodeRef.node) != B_OK
		|| message->FindInode("directory", &entryRef.directory)
			!= B_OK
		|| message->FindString("name", (const char**)&entryRef.name) != B_OK) {
		return true;
	}
	entryRef.device = nodeRef.device;

	// It might be our definition file.
	if (nodeRef == *TargetModel()->NodeRef())
		return _inherited::FSNotification(message);

	// It might be one of our directories.
	BString path;
	if (message->FindString("path", &path) == B_OK
		&& fDirectoryPaths.HasString(path)) {
		// Find all poses that stem from that directory and generate an
		// entry-removed message for each.
		PoseList poses;
		for (int32 i = 0; BPose* pose = fPoseList->ItemAt(i); i++) {
			NotOwningEntryRef poseEntryRef = *pose->TargetModel()->EntryRef();
			if (poseEntryRef.DirectoryNodeRef() == nodeRef)
				poses.AddItem(pose);
		}

		for (int32 i = 0; BPose* pose = poses.ItemAt(i); i++) {
			_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_REMOVED,
				*pose->TargetModel()->EntryRef(), NULL, false);
		}

		return true;
	}

	// If it is a directory, translate it.
	entry_ref* actualEntryRef = &entryRef;
	node_ref* actualNodeRef = &nodeRef;
	entry_ref definitionEntryRef;
	node_ref definitionNodeRef;

	VirtualDirectoryManager* manager = VirtualDirectoryManager::Instance();
	AutoLocker<VirtualDirectoryManager> managerLocker(manager);

	if (manager != NULL
		&& manager->GetSubDirectoryDefinitionFile(
			entryRef, entryRef.name, definitionEntryRef)) {
		actualEntryRef = &definitionEntryRef;
	}

	// Check the pose. It might have been an entry that wasn't visible anyway.
	// In that case we can just ignore the notification.
	BPose* pose = fPoseList->FindPoseByFileName(actualEntryRef->name);
	if (pose == NULL || *actualNodeRef != *pose->TargetModel()->NodeRef())
		return true;

	// See, if another entry becomes visible, now.
	struct stat st;
	entry_ref visibleEntryRef;
	if (_GetEntry(actualEntryRef->name, visibleEntryRef, &st)) {
		// If the new entry is a directory, translate it.
		if (S_ISDIR(st.st_mode)) {
			if (manager == NULL || manager->TranslateDirectoryEntry(
					*TargetModel()->EntryRef(), visibleEntryRef)
					!= B_OK) {
				return true;
			}

			// Effectively nothing changes, when the removed entry was a
			// directory as well.
			if (visibleEntryRef == *actualEntryRef)
				return true;
		}
	}

	if (actualEntryRef == &entryRef) {
		managerLocker.Unlock();
		if (_inherited::FSNotification(message))
			pendingNodeMonitorCache.Add(message);
	} else {
		// tell the manager that the directory has been removed
		manager->DirectoryRemoved(*actualEntryRef);
		managerLocker.Unlock();

		_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_REMOVED,
			*actualEntryRef);
	}

	_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_CREATED,
		visibleEntryRef);

	return true;
}


bool
VirtualDirectoryPoseView::_EntryMoved(const BMessage* message)
{
	NotOwningEntryRef fromEntryRef;
	NotOwningEntryRef toEntryRef;
	node_ref nodeRef;

	if (message->FindDevice("node device", &nodeRef.device) != B_OK
		|| message->FindInode("node", &nodeRef.node) != B_OK
		|| message->FindDevice("device", &fromEntryRef.device) != B_OK
		|| message->FindInode("from directory", &fromEntryRef.directory) != B_OK
		|| message->FindInode("to directory", &toEntryRef.directory) != B_OK
		|| message->FindString("from name", (const char**)&fromEntryRef.name)
			!= B_OK
		|| message->FindString("name", (const char**)&toEntryRef.name)
			!= B_OK) {
		return true;
	}
	toEntryRef.device = fromEntryRef.device;

	// TODO: That's the lazy approach. Ideally we'd analyze the situation and
	// forward a B_ENTRY_MOVED, if possible. There are quite a few cases to
	// consider, though.
	_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_REMOVED,
		fromEntryRef, message->GetString("from path", NULL), false);
	_DispatchEntryCreatedOrRemovedMessage(B_ENTRY_CREATED,
		toEntryRef, message->GetString("path", NULL), false);

	return true;
}


bool
VirtualDirectoryPoseView::_NodeStatChanged(const BMessage* message)
{
	entry_ref entryRef;
	// FIXME: need to pass entry_ref in message
	if (message->FindRef("ref", &entryRef) != B_OK) {
		return true;
	}

	if (entryRef == fRootDefinitionFileRef) {
		if ((message->GetInt32("fields", 0) & B_STAT_MODIFICATION_TIME) != 0) {
			VirtualDirectoryManager* manager
				= VirtualDirectoryManager::Instance();
			if (manager != NULL) {
				AutoLocker<VirtualDirectoryManager> managerLocker(manager);
				if (!manager->DefinitionFileChanged(
						*TargetModel()->EntryRef())) {
					// The definition file no longer exists. Ignore the message
					// -- we'll get a remove notification soon.
					return true;
				}

				bigtime_t fileChangeTime;
				manager->GetDefinitionFileChangeTime(*TargetModel()->EntryRef(),
					fileChangeTime);
				if (fileChangeTime != fFileChangeTime) {
					_UpdateDirectoryPaths();
					managerLocker.Unlock();
					Refresh();
						// TODO: Refresh() is rather radical. Or rather its
						// implementation is. Ideally it would just compare the
						// currently added poses with what a new dir iterator
						// returns and remove/add poses as needed.
				}
			}
		}

		if (!fIsRoot)
			return true;
	}

	return _inherited::FSNotification(message);
}


void
VirtualDirectoryPoseView::_DispatchEntryCreatedOrRemovedMessage(int32 opcode,
	const entry_ref& entryRef, const char* path,
	bool dispatchToSuperClass)
{
	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", opcode);
	message.AddString("name", entryRef.name);
	if (path != NULL && path[0] != '\0')
		message.AddString("path", path);
	bool result = dispatchToSuperClass
		? _inherited::FSNotification(&message)
		: FSNotification(&message);
	if (!result)
		pendingNodeMonitorCache.Add(&message);
}


bool
VirtualDirectoryPoseView::_GetEntry(const char* name, entry_ref& _ref,
	struct stat* _st)
{
	return VirtualDirectoryManager::GetEntry(fDirectoryPaths, name, &_ref, _st);
}


status_t
VirtualDirectoryPoseView::_UpdateDirectoryPaths()
{
	VirtualDirectoryManager* manager = VirtualDirectoryManager::Instance();
	Model* model = TargetModel();
	status_t error = manager->ResolveDirectoryPaths(
		*model->EntryRef(), fDirectoryPaths);
	if (error != B_OK)
		return error;

	manager->GetDefinitionFileChangeTime(*model->EntryRef(), fFileChangeTime);
	return B_OK;
}

} // namespace BPrivate
