/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold, ingo_weinhold@gmx.de
 */
#ifndef _VIRTUAL_DIRECTORY_MANAGER_H
#define _VIRTUAL_DIRECTORY_MANAGER_H


#include <dirent.h>

#include <map>

#include <Locker.h>
#include <Node.h>


class BStringList;


namespace BPrivate {

class Model;


class VirtualDirectoryManager {
public:
	static	VirtualDirectoryManager* Instance();

			bool				Lock()		{ return fLock.Lock(); }
			void				Unlock()	{ fLock.Unlock(); }

			status_t			ResolveDirectoryPaths(
									const entry_ref& definitionFileEntryRef,
									BStringList& _directoryPaths,
									entry_ref* _definitionFileEntryRef = NULL);

			bool				GetDefinitionFileChangeTime(
									const entry_ref& definitionFileRef,
									bigtime_t& _time) const;

			bool				GetRootDefinitionFile(
									const entry_ref& definitionFileRef,
									entry_ref& _rootDefinitionFileRef);
			bool				GetSubDirectoryDefinitionFile(
									const entry_ref& baseDefinitionRef,
									const char* subDirName,
									entry_ref& _entryRef);
			bool				GetParentDirectoryDefinitionFile(
									const entry_ref& subDirDefinitionRef,
									entry_ref& _entryRef);

			status_t			TranslateDirectoryEntry(
									const entry_ref& definitionFileRef,
									dirent* buffer);
			status_t			TranslateDirectoryEntry(
									const entry_ref& definitionFileRef,
									entry_ref& entryRef);

			bool				DefinitionFileChanged(
									const entry_ref& definitionFileRef);
									// returns whether the directory still
									// exists
			status_t			DirectoryRemoved(
									const entry_ref& definitionFileRef);

	static	bool				GetEntry(const BStringList& directoryPaths,
									const char* name, entry_ref* _ref,
						 			struct stat* _st);

private:
			class Info;
			class RootInfo;

			typedef std::map<entry_ref, Info*> EntryRefInfoMap;

private:
								VirtualDirectoryManager();

			Info*				_InfoForEntryRef(const entry_ref& entryRef) const;

			bool				_AddInfo(Info* info);
			void				_RemoveInfo(Info* info);

			void				_UpdateTree(RootInfo* root);
			void				_UpdateTree(Info* info);

			void				_RemoveDirectory(Info* info);

			status_t			_ResolveUnknownDefinitionFile(
									const entry_ref& definitionFileEntryRef,
									Info*& _info);
			status_t			_CreateRootInfo(
									const entry_ref& definitionFileEntryRef,
									Info*& _info);
			status_t			_ReadSubDirectoryDefinitionFileInfo(
									const entry_ref& entryRef,
									entry_ref& _rootDefinitionFileEntryRef,
									BString& _subDirPath);

private:
			BLocker				fLock;
			EntryRefInfoMap		fInfos;
};

} // namespace BPrivate


#endif	// _VIRTUAL_DIRECTORY_MANAGER_H
